/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "wdm.h"
#include "mountmgr.h"
#include "mountdev.h"

#define MOUNTDEV_NAME_MAXIMUM   256
#define MOUNTDEV_NAME_TAG       'nmMM'

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


NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS            Status;
    UNICODE_STRING      DriveLetter;
    PFILE_OBJECT        FileObject = NULL;
    PDEVICE_OBJECT      DeviceObject = NULL;
    IO_STATUS_BLOCK     IoStatusBlock;
    KEVENT              Event;
    PIRP                Irp;
    PMOUNTDEV_NAME      MountDevName = NULL;
    UNICODE_STRING      MountedDeviceName;
    PIO_STACK_LOCATION IoStackLocation;

    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload  = DriverUnload;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    KeInitializeEvent ( &Event, NotificationEvent, FALSE );

    // Store the UNC path to the 'C' drive in DriveLetter
    RtlInitUnicodeString ( &DriveLetter, L"\\??\\C:" );

    // Step #1 : Obtain a pointer to the device obeject corresponding 
    // to the string in DriveLetter (IoGetDeviceObjectPointer())
    if (!NT_SUCCESS(Status = IoGetDeviceObjectPointer(&DriveLetter, FILE_READ_DATA, &FileObject, &DeviceObject)))
    {
        DbgPrint("%s IoGetDeviceObjectPointer Error (%d)\n", __FUNCTION__, Status);
        goto Exit;
    }

    // Step #2 : Allocate a buffer from non-paged pool to hold the volume name
    // and store the buffer pointer in MountDevName (ExAllocatePoolWithTag()).
    MountDevName = (PMOUNTDEV_NAME)ExAllocatePoolWithTag(NonPagedPool, MOUNTDEV_NAME_MAXIMUM, MOUNTDEV_NAME_TAG);

    // Step #3 : Build a device I/O control IRP to send down to the underlying 
    // device object (in DeviceObject) with the IOCTL code 
    // IOCTL_MOUNTDEV_QUERY_DEVICE_NAME (IoBuildDeviceIoControlRequest()).
    // Store the IRP pointer in IRP.
    // The status of the I/O will be returned in IoStatusBlock
    if (NULL == (Irp = IoBuildDeviceIoControlRequest(IOCTL_MOUNTDEV_QUERY_DEVICE_NAME, DeviceObject, NULL, 0, MountDevName, MOUNTDEV_NAME_MAXIMUM, FALSE, &Event, &IoStatusBlock)))
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // Step #4 : Retrieve a pointer to the current stack location
    // (IoGetNextIrpStackLocation()) and store it in IoStackLocation
    // Populate the FileObject field of the IO_STACK_LOCATION
    IoStackLocation = IoGetNextIrpStackLocation(Irp);
    IoStackLocation->FileObject = FileObject;

    // Step #5 : Pass the IRP down to underlying device object (IoCallDriver()).
    // And wait for the IRP to complete (KeWaitForSingleObject()) if the driver
    // pends the IRP. Then check if the IRP completed successfully
    Status = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Status)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    if (!NT_SUCCESS(Status))
    {
        DbgPrint("%s IoCallDriver Error (%d)\n", __FUNCTION__, Status);
        goto Exit;
    }

    // Display the volume device object name in the debugger(DbgPrint()).
    MountedDeviceName.Buffer = MountDevName->Name;
    MountedDeviceName.Length = MountDevName->NameLength;
    MountedDeviceName.MaximumLength = MountDevName->NameLength + sizeof(UNICODE_NULL);

    DbgPrint ( "%s MOUNTEDNAME : %wZ -> %wZ\n", 
        __FUNCTION__, &DriveLetter, &MountedDeviceName );

Exit:
    // Step #6: Dereference the file object (ObDereferenceObject()).
    if (FileObject)
    {
        ObDereferenceObject(FileObject);
    }
    

    // Step #7: Free the buffer allocated in MountDevName (ExFreePool()).
    if (NULL != MountDevName)
    {
        ExFreePool(MountDevName);
    }
    
    return Status;
}

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );
}


