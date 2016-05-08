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
    IN PUNICODE_STRING RegistryPath);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject);

// place the DriverEntry() in the .INIT section
// such that it is discarded after execution
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    // display the current function name and the driver object
    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    // Step #1 : Register a DriverUnload() routine
    DriverObject->DriverUnload = DriverUnload;

    // Step #2 : Print the registry path in the debugger
    DbgPrint("%s %wZ\n", __FUNCTION__, RegistryPath);

    return STATUS_SUCCESS;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);
} // DriverUnload()
