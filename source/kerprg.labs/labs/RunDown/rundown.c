/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

// amount of time for which the work item 
// will be blocked, giving the user enough
// to initiate a driver unload operation
#define WAIT_TIME_IN_SEC        10

VOID WorkerRoutine( IN PVOID Parameter );

EX_RUNDOWN_REF g_RunDownRef;
WORK_QUEUE_ITEM  g_WorkQueueItem;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // initialize the legacy work item
    ExInitializeWorkItem ( &g_WorkQueueItem, WorkerRoutine, &g_WorkQueueItem );

    // Step #1 : Initialize the global variable g_RunDownRef (ExInitializeRundownProtection())
    ExInitializeRundownProtection(&g_RunDownRef);

    // Step #2 : Take a rundown reference (ExAcquireRundownProtection())
    ExAcquireRundownProtection(&g_RunDownRef);

    // queue the legacy work item
    ExQueueWorkItem ( &g_WorkQueueItem, DelayedWorkQueue );

    DbgPrint ( "%s EX_RUNDOWN_REF=%p WORK_QUEUE_ITEM=%p\n", __FUNCTION__, &g_RunDownRef, &g_WorkQueueItem );

    return STATUS_SUCCESS;
} // DriverEntry()

VOID 
DriverUnload (
	PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DbgPrint ( "%s Waiting for Rundown Protection\n", __FUNCTION__ );

    // Step #3 : Wait for rundown references to drop to zero (ExWaitForRundownProtectionRelease())
    ExWaitForRundownProtectionRelease(&g_RunDownRef);
    DbgPrint ( "%s Done\n", __FUNCTION__ );
} // DriverUnload()

// legacy work item routine
VOID WorkerRoutine( IN PVOID Parameter )
{
    LARGE_INTEGER Interval;

    UNREFERENCED_PARAMETER(Parameter);

    DbgPrint ( "%s Waiting for %u seconds\n", __FUNCTION__,
        WAIT_TIME_IN_SEC );

    // block the work item routine for 10 seconds giving the 
    // user a chance to initiate driver unload
    Interval.QuadPart = (ULONGLONG)( -1 * ( WAIT_TIME_IN_SEC * 1000 * 1000 * 10 ) );
    KeDelayExecutionThread ( KernelMode, FALSE, &Interval );

    DbgPrint ( "%s Wait complete\n", __FUNCTION__ );

    // Step #4 : Drop a rundown reference (ExReleaseRundownProtection())
    ExReleaseRundownProtection(&g_RunDownRef);
} // WorkerRoutine()