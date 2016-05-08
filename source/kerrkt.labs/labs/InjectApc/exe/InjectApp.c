/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <winioctl.h>

#pragma warning (disable:4054)

#define DEVICE_TYPE_INJECTAPC          0x434d
#define	DEVICE_NAME_INJECTAPC          "\\\\.\\InjectApc"
#define IOCTL_INJECTAPC                CTL_CODE(DEVICE_TYPE_INJECTAPC, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)
typedef struct _STRUCT_INJECTAPC
{
    HANDLE ProcessId;
    PVOID User32MessageBoxA;
} STRUCT_INJECTAPC, *PSTRUCT_INJECTAPC;

// calls the kernel mode driver to inject 
// the APC in the target process
ULONG
CallDriver ( 
    HANDLE ProcessId,
    PVOID User32MessageBoxA )
{
    HANDLE  FileHandle =  INVALID_HANDLE_VALUE;
    ULONG   BytesTransfered;
    ULONG   FunctionReturn  = 0;
    STRUCT_INJECTAPC InjectApc;

    // obtain a handle to the driver by device name
    if ( ( FileHandle = CreateFile ( 
        DEVICE_NAME_INJECTAPC,
        GENERIC_READ|GENERIC_WRITE,
        0,
        0, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL,
        0 ) ) == INVALID_HANDLE_VALUE ) {

        printf ( "CreateFile(%s) FAIL=%d\n", DEVICE_NAME_INJECTAPC, GetLastError() );
        goto Exit;
    }

    // populate the IOCTL structure
    InjectApc.ProcessId = ProcessId;
    InjectApc.User32MessageBoxA = User32MessageBoxA;

    if ( DeviceIoControl ( 
        FileHandle,
        (ULONG)IOCTL_INJECTAPC,
        &InjectApc,
        sizeof(InjectApc),
        NULL,
        0,
        &BytesTransfered,
        NULL ) == 0 ) {

        printf ( "DeviceIoControl(IOCTL_INJECTAPC) FAIL=%d\n", GetLastError() );
        goto Exit;
    }

Exit :
    // close the handle and exit out
    if ( FileHandle != INVALID_HANDLE_VALUE ) {
        CloseHandle( FileHandle );
    }

    return FunctionReturn;
} // CallDriver()


int __cdecl main( int argc, CHAR* argv[] )
{
    ULONG Pid;
    HMODULE Module;
    HANDLE ProcessId;
    PUCHAR FunctionPointer;

    if (argc < 2 ) {
        printf( "usage : InjectApp <ProcessId>\n" );
        goto Exit;
    }

    if ( sscanf ( argv[1], "%u", &Pid ) != 1 ) {
        printf( "usage : InjectApp <ProcessId>\n" );
        goto Exit;
    }

    printf( "Target Process = 0x%x (%u)\n", Pid, Pid );

    ProcessId = (HANDLE)Pid;

    // obtain the address of user32!MessageBoxA in the current process
    // the address should be the same across all processes
    Module = LoadLibrary ( "user32.dll" );
    if ( Module == NULL ) {
        printf ( "LoadLibrary(user32.dll) FAIL=%d\n", GetLastError() );
        goto Exit;
    }

    FunctionPointer = (PUCHAR)GetProcAddress ( Module, "MessageBoxA" );
    if ( FunctionPointer == NULL ) {
        printf ( "GetProcAddress(MessageBoxA) FAIL=%d\n", GetLastError() );
        goto Exit;
    }

    printf( "user32!MessageBoxA=%p\n", FunctionPointer );

    // call the driver to perform the code injection
    CallDriver ( ProcessId, (PVOID)FunctionPointer );

Exit :
    return 0;
} // main()
