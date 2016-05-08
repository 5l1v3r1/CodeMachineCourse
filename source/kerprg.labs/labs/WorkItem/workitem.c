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

VOID 
WorkerRotuineEx (
    PVOID IoObject,
    PVOID Context,
    PIO_WORKITEM IoWorkItem );

// for  DeviceType parameter to IoCreateDevice()
#define DEVICE_TYPE_WORKITEM    0x1234
#define DEVICE_NAME_WORKITEM    L"\\Device\\WorkItemDevice"
#define WAIT_TIME_IN_SEC        10

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject = NULL;
    UNICODE_STRING DeviceKernel;
    PIO_WORKITEM    WorkItem;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    RtlInitUnicodeString ( &DeviceKernel, DEVICE_NAME_WORKITEM );

    Status = IoCreateDevice(
        DriverObject,
        0,
        &DeviceKernel,
        DEVICE_TYPE_WORKITEM,
        0,
        FALSE,
        &DeviceObject  );

    if ( ! NT_SUCCESS( Status) ) {
        DbgPrint ( "%s IoCreateDevice() FAIL=%08x\n", 
            __FUNCTION__, Status );
        goto Exit;
    }

    // Step #1 : Allocate a work item structure (IoAllocateWorkItem())
    WorkItem = IoAllocateWorkItem(DeviceObject);

    // Step #2 : Queue the work item to run the function WorkerRoutineEx (IoQueueWorkItemEx())
    IoQueueWorkItemEx(WorkItem, WorkerRotuineEx, NormalWorkQueue, NULL);

    return STATUS_SUCCESS;

Exit :
    if ( DeviceObject ) {
        IoDeleteDevice ( DeviceObject );
    }

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p DeviceObject=%p\n", __FUNCTION__, 
        DriverObject, DriverObject->DeviceObject );

    IoDeleteDevice ( DriverObject->DeviceObject );

    DbgPrint ( "%s Done\n", __FUNCTION__ );

} // DriverUnload()


VOID 
WorkerRotuineEx (
    PVOID IoObject,
    PVOID Context,
    PIO_WORKITEM IoWorkItem )
{
    LARGE_INTEGER Interval;

    UNREFERENCED_PARAMETER(IoObject);
    UNREFERENCED_PARAMETER(Context);

    DbgPrint ( "%s Waiting for %u seconds\n", __FUNCTION__,
        WAIT_TIME_IN_SEC );

    // Step #3 : Wait for WAIT_TIME_IN_SEC (KeDelayExecutionThread())
    Interval.QuadPart = (-1 * WAIT_TIME_IN_SEC * 1000 * 1000 * 10);
    KeDelayExecutionThread(KernelMode, TRUE, &Interval);

    DbgPrint ( "%s Wait complete\n", __FUNCTION__ );

    // Step #4 : Free the Work Item (IoFreeWorkItem())
    IoFreeWorkItem(IoWorkItem);

} // WorkerRotuineEx()
