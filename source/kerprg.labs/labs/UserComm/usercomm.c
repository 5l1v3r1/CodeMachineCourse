/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

NTSTATUS
DispatchCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp);

#define DEVICE_TYPE_USERCOMM    0x1234
// device names
#define DEVICE_NAME_USERCOMM    L"\\Device\\UserCommDevice"
#define SYMLINK_NAME_USERCOMM   L"\\DosDevices\\UserCommLink"

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    UNICODE_STRING DeviceKernel;
    UNICODE_STRING DeviceUser;
    PDEVICE_OBJECT DeviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    DriverObject->DriverUnload = DriverUnload;

    // Step #1 : Register the function DispatchCreateClose() as the dispatch
    // routine for IRP_MJ_CREATE and IRP_MJ_CLOSE
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;

    RtlInitUnicodeString(&DeviceKernel, DEVICE_NAME_USERCOMM);

    // Step #2 : Create a device object with the name in DeviceKernel and 
    // type DEVICE_TYPE_USERCOMM (IoCreateDevice()).
    // Device must provide exclusive access to callers. 
    // Store the pointer to the device object in DeviceObject
    DbgPrint("%s DeviceName=%wS\n", __FUNCTION__, DEVICE_NAME_USERCOMM);
    if (!NT_SUCCESS(Status = IoCreateDevice(DriverObject, 0, &DeviceKernel, DEVICE_TYPE_USERCOMM, 0, TRUE, &DeviceObject)))
    {
        goto Exit;
    }

    RtlInitUnicodeString(&DeviceUser, SYMLINK_NAME_USERCOMM);

    // Step #3 : Create a symbolic link DeviceUser to DeviceKernel (IoCreateSymbolicLink())

    DbgPrint("%s SymbolicLinkName=%wS\n", __FUNCTION__, SYMLINK_NAME_USERCOMM);
    if (!NT_SUCCESS(Status = IoCreateSymbolicLink(&DeviceUser, &DeviceKernel)))
    {
        goto Exit;
    }

    return STATUS_SUCCESS;

Exit:
    if (DeviceObject) {
        IoDeleteDevice(DeviceObject);
    }

    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DeviceUser;

    DbgPrint("%s DriverObject=%p DeviceObject=%p\n", __FUNCTION__,
        DriverObject, DriverObject->DeviceObject);

    RtlInitUnicodeString(&DeviceUser, SYMLINK_NAME_USERCOMM);

    // Step #4 : Delete the symbolic link DeviceUser (IoDeleteSymbolicLink())
    IoDeleteSymbolicLink(&DeviceUser);

    // Step #5 : Delete the device object that was created in DriverEntry()
    // The pointer to the device object is in DriverObject->DeviceObject
    IoDeleteDevice(DriverObject->DeviceObject);

} // DriverUnload()


NTSTATUS
DispatchCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    DbgPrint("%s DeviceObject=%p Irp=%p\n", __FUNCTION__,
        DeviceObject, Irp);

    // Step #6 : Setup the appropriate values in IRP.IoStatus
    Irp->IoStatus.Pointer = NULL;
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    // Step #7 : Complete the IRP
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
} // DispatchCreateClose()
