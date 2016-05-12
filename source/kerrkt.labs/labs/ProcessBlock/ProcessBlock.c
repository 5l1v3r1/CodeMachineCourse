/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

//#include "ntddk.h"
#include <ntifs.h>

#define __MODULE__  "ProcessBlock"
#define DPF(x)      DbgPrint x

//BOOLEAN
//FsRtlIsNameInExpression(
//    _In_ PUNICODE_STRING Expression,
//    _In_ PUNICODE_STRING Name,
//    _In_ BOOLEAN IgnoreCase,
//    _In_opt_ PWCH UpcaseTable
//);

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
    ExInitializeRundownProtection(&g_RunDownRef);

    // Step #2 : Install the function ProcessNotifyCallbackEx() as a process notification callback
    // (PsSetCreateProcessNotifyRoutineEx())
    if (!NT_SUCCESS(Status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallbackEx, FALSE)))
    {
        DbgPrint("ERROR PsSetCreateProcessNotifyRoutineEx (%x)\n", Status);
        goto Exit;
    }

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
    if (!NT_SUCCESS(Status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyCallbackEx, TRUE)))
    {
        DbgPrint("ERROR PsSetCreateProcessNotifyRoutineEx (%x)\n", Status);
        goto Exit;
    }

    // Step #4 : Wait for rundown references to drop to zero (ExWaitForRundownProtectionRelease())
    ExWaitForRundownProtectionRelease(&g_RunDownRef);


Exit :
    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));

} // DriverUnload()

VOID
ProcessNotifyCallbackEx (
    PEPROCESS  Process,
    HANDLE  ProcessId,
    PPS_CREATE_NOTIFY_INFO  CreateInfo )
{
    UNICODE_STRING ExecutableBlocked = RTL_CONSTANT_STRING(L"*CALC*.EXE");
    
    UNREFERENCED_PARAMETER(ProcessId);
    UNREFERENCED_PARAMETER(Process);

    // Step #6 : Take a rundown reference (ExAcquireRundownProtection())
    ExAcquireRundownProtection(&g_RunDownRef);

    if ( CreateInfo ) {

        // Step #7 : Iterate through all the elements of the ExecutableBlocked[] array
        // and check if any of the paths match the executable being started and if so
        // fail the process creation with STATUS_INSUFFICIENT_RESOURCES
        if (!FsRtlIsNameInExpression(&ExecutableBlocked, (PUNICODE_STRING)CreateInfo->ImageFileName, TRUE, NULL))
        {
            DbgPrint("Starting Process: %wZ\n", CreateInfo->ImageFileName);
        }
        else
        {
            DbgPrint("Preventing Process (%wZ) Execution\n", CreateInfo->ImageFileName);
            CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
        }
    }

    // Step #8 : Drop a rundown reference (ExReleaseRundownProtection())
    ExReleaseRundownProtection(&g_RunDownRef);

    return;
} // ProcessNotifyCallbackEx()

