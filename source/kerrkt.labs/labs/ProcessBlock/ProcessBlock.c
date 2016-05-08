/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

#define __MODULE__  "ProcessBlock"
#define DPF(x)      DbgPrint x

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

VOID
ProcessNotifyCallbackEx (
    PEPROCESS  Process,
    HANDLE  ProcessId,
    PPS_CREATE_NOTIFY_INFO  CreateInfo );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

EX_RUNDOWN_REF g_RunDownRef = {0};

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload	= DriverUnload;

    // Step #1 : Initialize the global variable g_RunDownRef (ExInitializeRundownProtection())

    // Step #2 : Install the function ProcessNotifyCallbackEx() as a process notification callback
    // (PsSetCreateProcessNotifyRoutineEx())

    return STATUS_SUCCESS;

Exit :
    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DriverObject);

    // Step #3 : Remove the process notification callback (PsSetCreateProcessNotifyRoutineEx())

    // Step #4 : Wait for rundown references to drop to zero (ExWaitForRundownProtectionRelease())


Exit :
    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));

} // DriverUnload()

VOID
ProcessNotifyCallbackEx (
    PEPROCESS  Process,
    HANDLE  ProcessId,
    PPS_CREATE_NOTIFY_INFO  CreateInfo )
{
    UNICODE_STRING ExecutableBlocked[2] = {
        // Step #5 : Initialize the path to all the executables that need to be blocked
        // Use the macro RTL_CONSTANT_STRING()
    };
    UNREFERENCED_PARAMETER(ProcessId);
    UNREFERENCED_PARAMETER(Process);

    // Step #6 : Take a rundown reference (ExAcquireRundownProtection())


    if ( CreateInfo ) {
        ULONG Idx;

        // Step #7 : Iterate through all the elements of the ExecutableBlocked[] array
        // and check if any of the paths match the executable being started and if so
        // fail the process creation with STATUS_INSUFFICIENT_RESOURCES

    }

    // Step #8 : Drop a rundown reference (ExReleaseRundownProtection())

    return;
} // ProcessNotifyCallbackEx()

