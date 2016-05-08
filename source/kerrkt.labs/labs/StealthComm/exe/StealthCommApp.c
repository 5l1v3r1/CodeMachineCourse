/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <winioctl.h>

#define __MODULE__                      "StealthCommApp"

#define DEVICE_TYPE_NULL                0x0015
#define	DEVICE_NAME_NULL                "\\\\.\\NUL"
#define IOCTL_STEALTHCOMM               CTL_CODE(DEVICE_TYPE_NULL, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)


BOOL
CallDriver ( 
    VOID )
{
    BOOL    Result = FALSE;
    HANDLE  FileHandle =  INVALID_HANDLE_VALUE;
    ULONG   BytesTransfered;

    // obtain a handle to the driver by device name
    if ( ( FileHandle = CreateFile ( 
        DEVICE_NAME_NULL,
        GENERIC_READ|GENERIC_WRITE,
        0,
        0, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL,
        0 ) ) == INVALID_HANDLE_VALUE ) {
        
        printf ( "CreateFile(%s) FAIL=%d\n", DEVICE_NAME_NULL, GetLastError() );
        goto Exit;
    }

    if ( DeviceIoControl ( 
        FileHandle,
        (ULONG)IOCTL_STEALTHCOMM,
        NULL,
        0,
        NULL,
        0,
        &BytesTransfered,
        NULL ) == 0 ) {

        printf ( "DeviceIoControl(IOCTL_STEALTHCOMM) FAIL=%d\n", GetLastError() );
        goto Exit;
    }

    Result = TRUE;

Exit :
    // close the handle and exit out
    if ( FileHandle != INVALID_HANDLE_VALUE ) {
        CloseHandle( FileHandle );
    }

    return Result;
} // CallDriver()




int __cdecl main( )
{
    CallDriver ( );
    return 0;
}
