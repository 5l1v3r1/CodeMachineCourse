/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "stdio.h"

#define __MODULE__  "StealthCommDrv"
#define DPF(x)      DbgPrint x

#define DEVICE_TYPE_NULL                0x0015

// IOCTL codes
#define IOCTL_STEALTHCOMM         CTL_CODE(DEVICE_TYPE_NULL, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)

NTSYSAPI
NTSTATUS NTAPI
ObReferenceObjectByName(
    PUNICODE_STRING ObjectPath,
    ULONG Attributes,
    PACCESS_STATE PassedAccessState,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID ParseContext,
    OUT PVOID *ObjectPtr );


VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

NTSTATUS
HookDispatchIoctl (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

//
// Globals
//
PDRIVER_OBJECT  g_NullDriverObject = NULL;
PDRIVER_DISPATCH g_OriginalDispatchIoctl = (PDRIVER_DISPATCH)NULL;
extern POBJECT_TYPE *IoDriverObjectType; // POBJECT_TYPE 

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING NullDriverName;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload	= DriverUnload;

    // Step #1 : Initialize the unicode string NullDriverName with the 
    // name of the NULL driver's driver object. (RtlInitUnicodeString())

    // Step #2 : Using the name NullDriverName obtain a pointer to the driver
    // object and store it in g_NullDriverObject (ObReferenceObjectByName())

    DPF(( "%s!%s g_NullDriverObject=%p\n", __MODULE__, __FUNCTION__, 
        g_NullDriverObject ));

    // Step #3 : Save the pointer to the original IRP_MJ_DEVICE_CONTROL dispatch 
    // routine in g_OriginalDispatchIoctl

    // Step #4 : Set the NULL drivers IRP_MJ_DEVICE_CONTROL dispatch routine to the 
    // function HookDispatchIoctl()

    return STATUS_SUCCESS;

Exit :
    // error handling
    if ( g_NullDriverObject ) {
        // Step #5 : Drop the reference on the NULL driver's driver object
        // (ObDereferenceObject())

    }

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    // Step #6 : Restore back the original IRP_MJ_DEVICE_CONTROL dispatch routine

    if ( g_NullDriverObject ) {
        // Step #7 : Drop the reference on the NULL driver's driver object
        // (ObDereferenceObject())

    }

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
} // DriverUnload()


NTSTATUS
HookDispatchIoctl (
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
        case  IOCTL_STEALTHCOMM : {
            DPF(( "%s!%s IOCTL_STEALTHCOMM\n",  __MODULE__, __FUNCTION__, 
                DeviceObject, Irp ));

            // complete the I/O request synchronously
            Irp->IoStatus.Status = Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = BytesReturned;
            IoCompleteRequest ( Irp, IO_NO_INCREMENT );
        }
        break;

        default :
        if ( g_OriginalDispatchIoctl ) {
            Status = g_OriginalDispatchIoctl ( DeviceObject, Irp );
        }
        break;
    }

    return Status;
} // HookDispatchIoctl()
