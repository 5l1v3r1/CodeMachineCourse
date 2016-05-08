/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#define _CRT_SECURE_NO_WARNINGS

#include "windows.h"
#include "stdio.h"
#include "winioctl.h"

#define DEVICE_TYPE_USERIOCTL   0x1234
#define	DEVICE_NAME_COMM    "\\\\.\\UserIoctlLink"
#define IOCTL_SEND_DATA         CTL_CODE(DEVICE_TYPE_USERIOCTL, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define IOCTL_RECV_DATA         CTL_CODE(DEVICE_TYPE_USERIOCTL, 0x0002, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define BUFFERSIZE_MAX_IOCTL    1024

VOID Usage( VOID )
{
    printf ( "usage : AppIoctl send <String>\n" );
    printf ( "usage : AppIoctl recv\n" );
}

int __cdecl main ( int argc, char *argv[] )
{
    HANDLE FileHandle =  INVALID_HANDLE_VALUE;
    BOOL   SendOrRecv = FALSE;
    ULONG  BytesTransfered;
    ULONG  SendLength = 0;
    CHAR   Data[BUFFERSIZE_MAX_IOCTL] = {0};

    if ( argc < 2 ) {
        Usage();
        goto Exit;
    }

    if ( _stricmp ( argv[1], "send" ) == 0 ) {
        SendOrRecv = TRUE;
        if ( argc < 3 ) {
            Usage();
            goto Exit;
        }
        strncpy ( Data, argv[2], BUFFERSIZE_MAX_IOCTL ); 
        Data[BUFFERSIZE_MAX_IOCTL-1] = 0;
        SendLength =  (ULONG)strlen ( Data );
    } else if ( _stricmp ( argv[1], "recv" ) == 0 ) {
        SendOrRecv = FALSE;
    } else {
        Usage();
        goto Exit;
    }

    if ( ( FileHandle = CreateFile ( 
        DEVICE_NAME_COMM,
        GENERIC_READ|GENERIC_WRITE,
        0,
        0, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL,
        0 ) ) == INVALID_HANDLE_VALUE ) {
        
        printf ( "CreateFile(%s) FAIL=%d\n", DEVICE_NAME_COMM, GetLastError() );
        goto Exit;
    }

    if ( SendOrRecv )  {
        if ( DeviceIoControl ( 
            FileHandle,
            (ULONG)IOCTL_SEND_DATA,
            Data,
            SendLength,
            NULL,
            0,
            &BytesTransfered,
            NULL ) == 0 ) {

            printf ( "DeviceIoControl(IOCTL_SEND_DATA) FAIL=%d\n", GetLastError() );
            goto Exit;
        }
    } else {
        ULONG Idx;

        if ( DeviceIoControl ( 
            FileHandle,
            (ULONG)IOCTL_RECV_DATA,
            NULL,
            0,
            Data,
            BUFFERSIZE_MAX_IOCTL,
            &BytesTransfered,
            NULL ) == 0 ) {

            printf ( "DeviceIoControl(IOCTL_RECV_DATA) FAIL=%d\n", GetLastError() );
            goto Exit;
        }

        for ( Idx = 0 ; Idx < BytesTransfered ; Idx++ ) {
            putchar ( Data[Idx] );
        }
        putchar ( '\n' );
    }

Exit :
    if ( FileHandle != INVALID_HANDLE_VALUE ) {
        CloseHandle( FileHandle );
    }

    return 0;
} // main()
