/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <malloc.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((LONG)(Status)) >= 0)
#endif

// Error code: (NTSTATUS) 0xc00000f0 (3221225712) - 
// An invalid parameter was passed to a service or function as the second argument.

// Error code: (NTSTATUS) 0xc0000018 (3221225496) - 
// {Conflicting Address Range}  The specified address range conflicts with the address space.

typedef LONG 
( *PNT_ALLOCATE_VIRTUAL_MEMORY ) (
    HANDLE    ProcessHandle,
    PVOID     *BaseAddress,
    ULONG_PTR ZeroBits,
    PSIZE_T   RegionSize,
    ULONG     AllocationType,
    ULONG     Protect );

VOID AllocPage( PUCHAR Address, ULONG Count )
{
    SYSTEM_INFO SystemInfo;
    PUCHAR *AddressArray;
    ULONG AddressArraySize;
    ULONG Idx;
    HMODULE NtDll = NULL;
    PNT_ALLOCATE_VIRTUAL_MEMORY NtAllocateVirtualMemoryX = NULL;

    GetSystemInfo ( &SystemInfo );

    if ( ( NtDll = GetModuleHandle ( "ntdll.dll" ) ) == NULL ) {
        printf ( "[-] GetModuleHandle(ntdll.dll) : FAIL=%08x\n", GetLastError() );
        goto Exit;
    }

    if ( ( NtAllocateVirtualMemoryX = (PNT_ALLOCATE_VIRTUAL_MEMORY)GetProcAddress ( 
            NtDll, "NtAllocateVirtualMemory" ) ) == NULL ) {
        printf ( "[-] GetProcAddress(NtAllocateVirtualMemory) : FAIL=%08x\n", GetLastError() );
        goto Exit;
    }

    AddressArraySize = sizeof (PUCHAR) * Count;

    if ( ( AddressArray = (PUCHAR *)malloc ( AddressArraySize ) ) == NULL ) {
        printf ( "[-] malloc (%d) FAIL\n", AddressArraySize );
        goto Exit;
    }

    memset ( AddressArray, 0, AddressArraySize );

    for ( Idx = 0 ; Idx < Count ; Idx++ ) {
        ULONG Status;
        SIZE_T RegionSize = (SIZE_T)SystemInfo.dwPageSize;

        AddressArray[Idx] = (Address + 1);

        Status = NtAllocateVirtualMemoryX ( 
            GetCurrentProcess(),
            &AddressArray[Idx],
            0,
            &RegionSize, 
            MEM_RESERVE | MEM_COMMIT, // | MEM_TOP_DOWN, 
            PAGE_EXECUTE_READWRITE );

        if ( NT_SUCCESS(Status) ) {
            printf ( "[+] (%d) VA=%p Alloc OK ", Idx, AddressArray[Idx] );
            __try {
                volatile UCHAR Content = *((PUCHAR)AddressArray[Idx]);
                UNREFERENCED_PARAMETER(Content);
                printf ( "| Access OK\n" );
            } __except ( EXCEPTION_EXECUTE_HANDLER ) {
                printf ( "| Access FAIL\n" );
            }

        } else {
            printf ( "[-] (%d) VA=%p Alloc FAIL=%08x\n", Idx, AddressArray[Idx], Status );
        }



        Address += SystemInfo.dwPageSize;
    }

    //for ( Idx = 0 ; Idx < Count ; Idx++ ) {
    //    if ( AddressArray[Idx] ) {
    //        VirtualFree ( AddressArray[Idx], 0, MEM_RELEASE );
    //    }
    //}

Exit :
    return;
}

VOID Usage ( VOID )
{
    printf ( "\nusage : AllocPage [<Address>] [PageCount]\n" );
}

int _cdecl 
main ( 
    int argc, char *argv[] ) 
{
    ULONG UserAddress = 0;
    ULONG PageCount  = 1 ;

    if ( argc > 1 ) {

        if ( strcmp ( argv[1], "/?" ) == 0 ) {
            Usage();
            goto Exit;
        }

        if ( strcmp ( argv[1], "-?" ) == 0 ) {
            Usage();
            goto Exit;
        }

        if ( sscanf ( argv[1], "%x", &UserAddress ) != 1 ) {
            Usage();
            goto Exit;
        }
    }

    if (  argc > 2 ) {
        if ( sscanf ( argv[2], "%d", &PageCount ) != 1 ) {
            Usage();
            goto Exit;
        }
        if ( PageCount == 0 ){
            PageCount = 1;
        }
    }

    AllocPage( (PUCHAR)UserAddress, PageCount );

Exit :
    return 0;
}