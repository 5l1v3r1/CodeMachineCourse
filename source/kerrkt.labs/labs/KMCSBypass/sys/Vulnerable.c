/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

#define __MODULE__ "Vulnerable"
#define DPF(x)      DbgPrint x

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

#define DEVICE_TYPE_KMCSBYPASS   0x434d

#define DEVICE_NAME_KMCSBYPASS   L"\\Device\\KMCSBypass"
#define SYMLINK_NAME_KMCSBYPASS  L"\\DosDevices\\KMCSBypass"

// IOCTL codes
#define IOCTL_WRITEWHATWHERE        CTL_CODE(DEVICE_TYPE_KMCSBYPASS, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)
#define IOCTL_TRIGGERVULNERABILITY  CTL_CODE(DEVICE_TYPE_KMCSBYPASS, 0x0002, METHOD_BUFFERED, FILE_ALL_ACCESS)

typedef ULONG(*PFUNCTION)(VOID);

PFUNCTION g_FunctionPointer = NULL;

NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS
DispatchIoctl (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING DeviceKernel;
    UNICODE_STRING DeviceUser;
    PDEVICE_OBJECT DeviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload	= DriverUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;

    // initialize the device name
    RtlInitUnicodeString ( &DeviceKernel, DEVICE_NAME_KMCSBYPASS );

    // create the device object
    Status = IoCreateDevice (
        DriverObject,
        0,
        &DeviceKernel,
        DEVICE_TYPE_KMCSBYPASS,
        0,
        FALSE,
        &DeviceObject  );

    if ( ! NT_SUCCESS( Status) ) {
        DPF(( "%s!%s IoCreateDevice() FAIL=%08x\n", __MODULE__, __FUNCTION__,
            Status ));
        goto Exit;
    }

    DPF(( "%s!%s DeviceName=%wS\n", __MODULE__, __FUNCTION__,
        DEVICE_NAME_KMCSBYPASS ));

    // initialize the symbolic link name
    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_KMCSBYPASS );

    // create the symbolic link
    Status = IoCreateSymbolicLink (
        &DeviceUser, &DeviceKernel );

    if ( ! NT_SUCCESS( Status) ) {
        DPF(( "%s!%s IoCreateSymbolicLink() FAIL=%08x\n", __MODULE__, __FUNCTION__,
            Status ));
        goto Exit;
    }

    DPF(( "%s!%s SymbolicLinkName=%wS\n", __MODULE__, __FUNCTION__, 
        SYMLINK_NAME_KMCSBYPASS ));

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

    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_KMCSBYPASS );

    IoDeleteSymbolicLink ( &DeviceUser );
    
    IoDeleteDevice ( DriverObject->DeviceObject );

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));

} // DriverUnload()


NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    DPF(( "%s!%s DeviceObject=%p Irp=%p\n",  __MODULE__, __FUNCTION__,
        DeviceObject, Irp ));

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

    DPF(( "%s!%s DeviceObject=%p Irp=%p\n",  __MODULE__, __FUNCTION__,
        DeviceObject, Irp ));

    // retrieve the current I/O Stack Location from the IRP
    IoStackLocation =  IoGetCurrentIrpStackLocation(Irp);

    // process the IOCTL code provided in the call to DeviceIoControl()
    switch (IoStackLocation->Parameters.DeviceIoControl.IoControlCode) {
        case  IOCTL_WRITEWHATWHERE : {
            if ( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof ( PFUNCTION ) ) {
                BytesReturned = 0;
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            g_FunctionPointer = *((PFUNCTION *)Irp->AssociatedIrp.SystemBuffer);
            Status = STATUS_SUCCESS;
        }
        break;

        case  IOCTL_TRIGGERVULNERABILITY : {
            if ( IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength < sizeof ( ULONG ) ) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if ( ! g_FunctionPointer ) {
                Status = STATUS_UNSUCCESSFUL;
                break;
            }

            *((ULONG *)Irp->AssociatedIrp.SystemBuffer) = g_FunctionPointer();
            BytesReturned = sizeof (ULONG);
            Status = STATUS_SUCCESS;
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
