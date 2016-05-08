/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
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

#define DEVICE_TYPE_USERIOCTL   0x1234

#define DEVICE_NAME_USERIOCTL   L"\\Device\\UserIoctlDevice"
#define SYMLINK_NAME_USERIOCTL  L"\\DosDevices\\UserIoctlLink"

// IOCTL codes
#define IOCTL_SEND_DATA         CTL_CODE(DEVICE_TYPE_USERIOCTL, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define IOCTL_RECV_DATA         CTL_CODE(DEVICE_TYPE_USERIOCTL, 0x0002, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define BUFFERSIZE_MAX_IOCTL    1024

// device extension structure allocated by IoCreateDevice()
// along with DEVICE_OBJECT
typedef struct _DEVICE_EXTENSION {
    LIST_ENTRY		List;
    KSPIN_LOCK		Lock;
}   DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// data buffer and metadata
typedef struct _DATA_BUFFER {
    ULONG           Size;
    LIST_ENTRY      Link;
    CHAR            Data[BUFFERSIZE_MAX_IOCTL];
}   DATA_BUFFER, *PDATA_BUFFER;

NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS
DispatchIoctl (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS 
EnqueueData ( 
    PDEVICE_EXTENSION DeviceExtension, 
    PUCHAR Data, 
    ULONG Size );

NTSTATUS 
DequeueData ( 
    PDEVICE_EXTENSION DeviceExtension, 
    PUCHAR Data, 
    PULONG Size );

VOID 
RundownData(
    PDEVICE_EXTENSION DeviceExtension );

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING DeviceKernel;
    UNICODE_STRING DeviceUser;
    PDEVICE_OBJECT DeviceObject = NULL;
    PDEVICE_EXTENSION DeviceExtension;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload	= DriverUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;

    // Step #1 : Register the function DispatchIoctl() as the dispatch
    // routine for IRP_MJ_DEVICE_CONTROL
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;

    // initialize the device name
    RtlInitUnicodeString ( &DeviceKernel, DEVICE_NAME_USERIOCTL );

    // create the device object
    Status = IoCreateDevice (
        DriverObject,
        sizeof(DEVICE_EXTENSION), // allocate space for device extension
        &DeviceKernel,
        0x1234,
        0,
        FALSE,
        &DeviceObject  );

    if ( ! NT_SUCCESS( Status) ) {
        DbgPrint ( "DriverEntry() IoCreateDevice() FAIL=%08x\n", 
            Status );
        goto Exit;
    }

    DbgPrint ( "%s DeviceName=%wS\n", __FUNCTION__, DEVICE_NAME_USERIOCTL );

    // retrieve the device extension from the device object and initialize it
    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory ( DeviceExtension, sizeof(DEVICE_EXTENSION) );

    // Step #2 : Initialize the List head (InitializeListHead())
    InitializeListHead(&DeviceExtension->List);

    // Step #3 : Initialize the List lock (KeInitializeSpinLock())
    KeInitializeSpinLock(&DeviceExtension->Lock);

    // initilize the symbolic link name
    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_USERIOCTL );

    // create the symbolic link
    Status = IoCreateSymbolicLink (
        &DeviceUser, &DeviceKernel );

    if ( ! NT_SUCCESS( Status) ) {
        DbgPrint ( "DriverEntry() IoCreateSymbolicLink() FAIL=%08x\n", 
            Status );
        goto Exit;
    }

    DbgPrint ( "%s SymbolicLinkName=%wS\n", __FUNCTION__, SYMLINK_NAME_USERIOCTL );

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
    UNICODE_STRING DeviceUser;
    PDEVICE_EXTENSION DeviceExtension;

    DbgPrint ( "%s DriverObject=%p DeviceObject=%p\n", __FUNCTION__, 
        DriverObject, DriverObject->DeviceObject );

    DeviceExtension = (PDEVICE_EXTENSION)DriverObject->DeviceObject->DeviceExtension;

    RundownData ( DeviceExtension );

    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_USERIOCTL );

    IoDeleteSymbolicLink ( &DeviceUser );
    
    IoDeleteDevice ( DriverObject->DeviceObject );
} // DriverUnload()


NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    DbgPrint ( "%s DeviceObject=%p Irp=%p\n",  __FUNCTION__, 
        DeviceObject, Irp );

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    
    IoCompleteRequest ( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
} // DispatchCreateClose()

NTSTATUS
DispatchIoctl (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    ULONG BytesReturned = 0;
    PIO_STACK_LOCATION IoStackLocation;
    PDEVICE_EXTENSION DeviceExtension;

    DbgPrint ( "%s DeviceObject=%p Irp=%p\n",  __FUNCTION__, 
        DeviceObject, Irp );

    // retrieve the current I/O Stack Location from the IRP
    IoStackLocation =  IoGetCurrentIrpStackLocation(Irp);

    // retrieve the device extension pointer from the device object
    DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    // process the IOCTL code provided in the call to DeviceIoControl()
    switch (IoStackLocation->Parameters.DeviceIoControl.IoControlCode) {
        case  IOCTL_SEND_DATA : {
            PUCHAR InputBuffer;
            ULONG InputLength;

            // retrieve the pointer to the I/O Manager allocated system buffer and length
            InputBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
            InputLength = IoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
            // validate the parameters supplied by the user mode caller
            if ( ( InputBuffer == NULL ) || ( InputLength == 0 ) || ( InputLength >= BUFFERSIZE_MAX_IOCTL ) ) {
                BytesReturned = 0;
                Status = STATUS_INVALID_PARAMETER;
            } else {
                // call the function to copy the application supplied data and queue it
                Status = EnqueueData ( DeviceExtension, InputBuffer, InputLength );
            }
        }
        break;

        case  IOCTL_RECV_DATA : {
            PUCHAR OutputBuffer;
            ULONG OutputLength;

            // retrieve the pointer to the I/O Manager allocated system buffer and length
            OutputBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
            OutputLength = IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

            // validate the parameters supplied by the user mode caller
            if ( ( OutputBuffer == NULL ) || ( OutputLength < BUFFERSIZE_MAX_IOCTL ) ) {
                BytesReturned = 0;
                Status = STATUS_INVALID_PARAMETER;
            } else {
                // call the function to dequeue any available data and return it to the application
                Status = DequeueData ( DeviceExtension, OutputBuffer, &BytesReturned );
            }
        }
        break;

        default :
        break;
    }

    // complete the I/O request synchronously
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = BytesReturned;
    IoCompleteRequest ( Irp, IO_NO_INCREMENT );

    return Status;
} // DispatchIoctl()

NTSTATUS 
EnqueueData ( 
    PDEVICE_EXTENSION DeviceExtension, 
    PUCHAR Data, 
    ULONG Size )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    KIRQL Irql;
    PDATA_BUFFER DataBuffer;

    // Step #4 : Allocate DataBuffer, copy the Data into it, and insert DataBuffer in the DeviceExtension->List atomically
    // (ExAllocatePoolWithTag(), RtlCopyMemory(), KeAcquireSpinLock(), InsertTailList(), KeReleaseSpinLock())
    if (NULL != (DataBuffer = ExAllocatePoolWithTag(PagedPool, sizeof(*DataBuffer), 'tciu')))
    {
        DataBuffer->Size = Size;
        RtlCopyMemory(DataBuffer->Data, Data, Size);
        KeAcquireSpinLock(&DeviceExtension->Lock, &Irql);
        InsertTailList(&DeviceExtension->List, &DataBuffer->Link);
        KeReleaseSpinLock(&DeviceExtension->Lock, Irql);
        Status = STATUS_SUCCESS;
    }

    return Status;
} // EnqueueData()

NTSTATUS 
DequeueData ( 
    PDEVICE_EXTENSION DeviceExtension, 
    PUCHAR Data, 
    PULONG Size )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    KIRQL Irql;
    PLIST_ENTRY ListEntry = NULL;
    PDATA_BUFFER DataBuffer = NULL;

    // Step #5 : Remove the first DataBuffer (if any) from DeviceExtension->List atomically
    if (!IsListEmpty(&DeviceExtension->List))
    {
        KeAcquireSpinLock(&DeviceExtension->Lock, &Irql);
        if (!IsListEmpty(&DeviceExtension->List))
        {
            ListEntry = RemoveHeadList(&DeviceExtension->List);
        }
        KeReleaseSpinLock(&DeviceExtension->Lock, Irql);
    }

    // Copy the data contents to Data 
    // Return the size of the buffer in Size
    // free the DataBuffer
    // (KeAcquireSpinLock(), IsListEmpty(), RemoveHeadList(), KeReleaseSpinLock(), RtlCopyMemory(), ExFreePool())

    if (NULL != ListEntry)
    {
        DataBuffer = CONTAINING_RECORD(ListEntry, DATA_BUFFER, Link);
        RtlCopyMemory(Data, DataBuffer->Data, DataBuffer->Size);
        *Size = DataBuffer->Size;
        ExFreePool(DataBuffer);
        Status = STATUS_SUCCESS;
    }
    else
    {
        *Size = 0;
        Status = STATUS_END_OF_FILE;
    }

    return Status;
} // DequeueData()

VOID 
RundownData(
    PDEVICE_EXTENSION DeviceExtension )
{
    KIRQL Irql;
    PLIST_ENTRY ListEntry = NULL;
    PDATA_BUFFER DataBuffer = NULL;

    // Step #6 : Drain any DataBuffers from DeviceExtension->List atomically
    // and free DataBuffer
    // (IsListEmpty(), KeAcquireSpinLock(), IsListEmpty(), RemoveHeadList(), KeReleaseSpinLock(), ExFreePool())
    if (!IsListEmpty(&DeviceExtension->List))
    {
        KeAcquireSpinLock(&DeviceExtension->Lock, &Irql);
        while(!IsListEmpty(&DeviceExtension->List))
        {
            ListEntry = RemoveHeadList(&DeviceExtension->List);
            DataBuffer = CONTAINING_RECORD(ListEntry, DATA_BUFFER, Link);
            ExFreePool(DataBuffer);
        }
        KeReleaseSpinLock(&DeviceExtension->Lock, Irql);
    }
} // RundownData()
