/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 1999-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

NTSTATUS 
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

HANDLE EventHandle = NULL;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING EventName;
    PKEVENT EventObject = NULL;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    DbgPrint ("RegistryPath=%wZ\n", RegistryPath );

    // Initialize the unicode string EventName with the 
    // event name "MyEvent" such that it can be shared with Win32
    // applications by name
    RtlInitUnicodeString ( &EventName, L"\\BaseNamedObjects\\MyEvent" );

    // Create a synchronization event with the name in EventName
    // and store the handle in EventHandle
    // The EventHandle returned is a kernel handle and is valid across
    // all process contexts
    EventObject = IoCreateSynchronizationEvent ( 
        &EventName,
        &EventHandle );

    if ( ! EventObject ) {
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        DbgPrint ("IoCreateSynchronizationEvent(%wZ) FAIL\n", &EventName );
        goto Exit;
    }

    DbgPrint ("EventName=%wZ EventObject=%p EventHandle=%p\n", 
        &EventName, EventObject, EventHandle );

    Status = STATUS_SUCCESS;

Exit :
    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    // Close the handle to the event
    ZwClose ( EventHandle );
} // DriverUnload()
