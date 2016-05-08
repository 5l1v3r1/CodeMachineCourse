/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "stdio.h"

#define	DEVICE_NAME_COMM    "\\\\.\\UserCommLink"

int __cdecl main ( )
{
    HANDLE FileHandle;

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

    printf ( "CreateFile(%s) FileHandle=%p\n", DEVICE_NAME_COMM, FileHandle );

    CloseHandle( FileHandle );

    printf ( "CloseFile(%p)\n", FileHandle );

Exit :
    return 0;
}
