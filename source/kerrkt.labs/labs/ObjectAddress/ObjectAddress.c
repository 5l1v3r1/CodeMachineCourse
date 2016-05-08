/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <malloc.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((LONG)(Status)) >= 0)
#endif

#define SystemHandleInformation 16

#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

#define ARRAY_SIZE 1024

#pragma warning(disable:4305)

// SYSTEM_HANDLE_TABLE_ENTRY_INFO for 
// SystemHandleInformation information class
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
    USHORT UniqueProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR ObjectTypeIndex;
    UCHAR HandleAttributes;
    USHORT HandleValue;
    PVOID Object;
    ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

// SYSTEM_HANDLE_INFORMATION for 
// SystemHandleInformation information class
typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG NumberOfHandles;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

// NTSTATUS WINAPI NtQuerySystemInformation (
//  SYSTEM_INFORMATION_CLASS SystemInformationClass,
//  PVOID                    SystemInformation,
//  ULONG                    SystemInformationLength,
//  PULONG                   ReturnLength );

typedef LONG
( *PNT_QUERY_SYSTEM_INFORMATION) (
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength );

PVOID 
GetKernelObjectFromHandle ( 
    HANDLE Pid,
    HANDLE Handle )
{
    PVOID ObjectPointer = NULL;
    HMODULE NtDll = NULL;
    PNT_QUERY_SYSTEM_INFORMATION NtQuerySystemInformationX = NULL;
    PSYSTEM_HANDLE_TABLE_ENTRY_INFO HandleTableEntryInfo;
    ULONG Index;
    LONG Status;
    ULONG ReturnLength = 0;

    PSYSTEM_HANDLE_INFORMATION InformationBuffer = NULL;
    ULONG InformationSize = 1024 * 1024; // 1 MB Buffer

    if ( ( NtDll = GetModuleHandle ( "ntdll.dll" ) ) == NULL ) {
        printf ( "[-] GetModuleHandle(ntdll.dll) : FAIL=%08x\n", GetLastError() );
        goto Exit;
    }

    if ( ( NtQuerySystemInformationX = (PNT_QUERY_SYSTEM_INFORMATION)GetProcAddress ( NtDll, "NtQuerySystemInformation" ) ) == NULL ) {
        printf ( "[-] GetProcAddress(NtQuerySystemInformation) : FAIL=%08x\n", GetLastError() );
        goto Exit;
    }

    // Step #1 : Allocate the initial buffer in InformationBuffer based on InformationSize
    if (NULL == (InformationBuffer = (PSYSTEM_HANDLE_INFORMATION)malloc(InformationSize)))
    {
        printf("ERROR Out of memory!\n");
        goto Exit;
    }

    // Repeatedly retrieve the handle table for all processes until it succeeds.
    // Double the buffer size at every call to accommodate all the handles.
    while ( TRUE ) {
        // Step #2 : Get the table for all the processes
        if (!NT_SUCCESS(Status = NtQuerySystemInformationX(SystemHandleInformation, InformationBuffer, InformationSize, &ReturnLength)))
        {
            // Step #3 : Deal with failures appropriately
            if (STATUS_INFO_LENGTH_MISMATCH == Status)
            {
                InformationSize *= 2;
                if (NULL == (InformationBuffer = (PSYSTEM_HANDLE_INFORMATION)realloc(InformationBuffer, InformationSize)))
                {
                    printf("ERROR Out of memory!\n");
                    goto Exit;
                }
            }
            else
            {
                printf("Error NtQuerySystemInformation (%x)\n", Status);
                goto Exit;
            }
        }
        else
        {
            break;
        }
    }

    printf ( "[+] Total Handle Count = %u\n", InformationBuffer->NumberOfHandles );


    HandleTableEntryInfo = InformationBuffer->Handles;
    // Iterate through the entire table and find the required handle information
    for ( Index = 0; Index < InformationBuffer->NumberOfHandles; Index++, HandleTableEntryInfo++ ) {

        // Step #4 : Check if this handle table entry is for the process identified by Pid
        // if not then proceed to the next handle table entry
        if ((HANDLE)HandleTableEntryInfo->UniqueProcessId != Pid)
        {
            continue;
        }

        // Step #5 : Check if this handle table entry is for the handle in Handle
        // and if so cache then object corresponding to the handle in ObjectPointer
        if ((HANDLE)HandleTableEntryInfo->HandleValue == Handle)
        {
            ObjectPointer = HandleTableEntryInfo->Object;
            break;
        }
    }

Exit :
    // Step #6 : Free the handle table entry buffer
    if (NULL != InformationBuffer)
    {
        free(InformationBuffer);
    }
    
    return ObjectPointer;
}


int main( void )
{
    PVOID TokenObject = NULL;
    HANDLE TokenHandle = NULL;

    if ( ! OpenProcessToken ( GetCurrentProcess(), TOKEN_QUERY, &TokenHandle ) ) {
        printf( "[-] OpenProcessToken() FAIL=0x%08x\n", GetLastError() );
        goto Exit;
    }

    printf( "[+] OpenProcessToken() Token Handle=%p\n", TokenHandle );

    TokenObject = GetKernelObjectFromHandle ( 
        (HANDLE)GetCurrentProcessId(),
        TokenHandle );

    if ( TokenHandle ) {
        printf( "[+] GetKernelObjectFromHandle() Token Object Pointer=%p\n", TokenObject );
    } else {
        printf( "[-] GetKernelObjectFromHandle() FAIL\n" );
    }

    printf( "[+] Press [ENTER] to exit >\n" );
    getchar();

Exit :
    if ( TokenHandle ) {
        CloseHandle ( TokenHandle );
    }

    printf( "[+] Done\n" );

    return TokenObject;
}