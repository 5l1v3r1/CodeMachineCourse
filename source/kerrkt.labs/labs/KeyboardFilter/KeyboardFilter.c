/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"
#include "ntddkbd.h"

#define __MODULE__  "KeyboardFilter"
#define DPF(x)      DbgPrint x

typedef struct _DEVICE_EXTENSION 
{
    PDEVICE_OBJECT	StackDeviceObject; // AttachedDeviceObject
    KSPIN_LOCK Lock;
    KEVENT Event;
    ULONG References;
    BOOLEAN InRundown;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


NTSTATUS 
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

NTSTATUS 
FilterDispatch(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp);

NTSTATUS 
ReadDispatch(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp);

NTSTATUS 
ReadCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp, 
    IN PVOID Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif


NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING KeyboardDeviceName;
    PDEVICE_EXTENSION DeviceExtension = NULL;
    PDEVICE_OBJECT FilterDeviceObject = NULL;
    ULONG Index;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload  = DriverUnload;

    // Step #1 : Set all IRP dispatch routines to the "forward
    // and forget" function FilterDispatch()
    // Total number of dispatch routines is IRP_MJ_MAXIMUM_FUNCTION

    // Step #2 : Set the IRP_MJ_READ dispatch routine to ReadDispatch()


    // Step #3 : Create the filter device object and store the FiDO pointer in FilterDeviceObject
    // Use an auto-generated name, allocate storage for the DEVICE_EXTENSION structure, use
    // FILE_DEVICE_KEYBOARD as the DeviceType, and make device Exclusive (IoCreateDevice())

    // obtain a pointer to the device extension that was allocated
    // by IoCreateDevice()
    DeviceExtension = FilterDeviceObject->DeviceExtension;

    // initialize the fields of the device extension
    RtlZeroMemory ( DeviceExtension, sizeof(DEVICE_EXTENSION) );
    KeInitializeEvent ( &DeviceExtension->Event, NotificationEvent, TRUE );
    DeviceExtension->References = 0;
    DeviceExtension->InRundown = FALSE;
    DeviceExtension->StackDeviceObject = NULL;

    // Step #4 : Initialize the unicode stirng KeyboardDeviceName with the 
    // device name "KeyboardClass0" (RtlInitUnicodeString())

    // Step #5 : Attach the FiDO to "KeyboardClass0" and store the top of stack
    // device object in DeviceExtension->StackDeviceObject (IoAttachDevice())

    // Set the FiDO flags based on the Flags settings from the top of stack device object
    FilterDeviceObject->Flags |= (DeviceExtension->StackDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE));

    return STATUS_SUCCESS;

Exit :
    // error handling
    if ( FilterDeviceObject ) {
        if ( DeviceExtension->StackDeviceObject ) {
            // Step #6 : Detach the FiDO from the device stack (IoDetachDevice())

        }

        // Step #7 : Delete the FiDO (IoDeleteDevice())

    }

    return Status;
}

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
    PDEVICE_EXTENSION DeviceExtension = 
        (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    // set the rundown flag which will prevent any more IRP
    // completions from being intercepted by the completion routine
    DeviceExtension->InRundown = TRUE;

    // Step #8 : Detach the FiDO from the device stack (IoDetachDevice())

    // wait for reference count to drop to 0
    KeWaitForSingleObject ( 
        &DeviceExtension->Event, Executive, KernelMode, FALSE, NULL );

    // Step #9 : Delete the FiDO (IoDeleteDevice())

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
}

// non-read dispatch routine
NTSTATUS 
FilterDispatch(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp)
{
    PDEVICE_EXTENSION DeviceExtension = 
        (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    IoSkipCurrentIrpStackLocation ( Irp );

    return IoCallDriver ( DeviceExtension->StackDeviceObject, Irp );
}
 
NTSTATUS
ReadDispatch(
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp)
{
    PDEVICE_EXTENSION DeviceExtension = 
        (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    if ( ! DeviceExtension->InRundown ) {
        KIRQL Irql;

        IoCopyCurrentIrpStackLocationToNext(Irp);

        IoSetCompletionRoutine(Irp,
            ReadCompletion,
            NULL,
            TRUE,
            TRUE,
            TRUE);

        // take a reference, if this is a 0 -> 1 transition
        // then clear the event so that the device object cannot be deleted
        KeAcquireSpinLock ( &DeviceExtension->Lock, &Irql );

        if ( InterlockedIncrement ( (PLONG)&DeviceExtension->References ) == 1 ) {
            KeClearEvent( &DeviceExtension->Event );
        }

        KeReleaseSpinLock ( &DeviceExtension->Lock, Irql );
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
    }

    return IoCallDriver(DeviceExtension->StackDeviceObject, Irp);
}

NTSTATUS 
ReadCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp, 
    IN PVOID Context)
{
    PDEVICE_EXTENSION DeviceExtension = 
        (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PKEYBOARD_INPUT_DATA KeyboardInputData;
    ULONG Idx;
    ULONG NumberOfKeys;
    KIRQL Irql;

    UNREFERENCED_PARAMETER(Context);

    if( Irp->PendingReturned ) {
        IoMarkIrpPending( Irp );
    }

    if ( Irp->IoStatus.Status != STATUS_SUCCESS ) {
        goto Exit;
    }

    // get the number of key scan codes that have been collected
    NumberOfKeys = (ULONG)(Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA));

    // get a pointer to the scan code buffer
    KeyboardInputData = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;

    // iterate through the scan code structures and display the scan codes
    for( Idx = 0 ; Idx < NumberOfKeys ; Idx++) {
        DPF(("%u %s [%02x] \n",
            KeyboardInputData[Idx].UnitId,
            KeyboardInputData[Idx].Flags & KEY_BREAK ? "BREAK" : "MAKE ",
            KeyboardInputData[Idx].MakeCode ));
    }

Exit:
    // drop a reference, if this is a 1 -> 0 transition
    // then set the event so that the device object can be deleted
    KeAcquireSpinLock ( &DeviceExtension->Lock, &Irql );

    if ( InterlockedDecrement ( (PLONG)&DeviceExtension->References ) == 0 ) {
        KeSetEvent( &DeviceExtension->Event, IO_NO_INCREMENT, FALSE );
    }

    KeReleaseSpinLock ( &DeviceExtension->Lock, Irql );

    return STATUS_SUCCESS;
} // ReadCompletion()
