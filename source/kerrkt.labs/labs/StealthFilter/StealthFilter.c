/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#include "ntifs.h"
#include "stdio.h"
#include "ntddkbd.h"

#define __MODULE__  "StealthFilter"
#define DPF(x)      DbgPrint x

// name of the keyboard class driver
#define DRIVER_NAME_KBDCLASS    L"\\Driver\\KbdClass"

// backup of the filter driver's driver object
PDRIVER_OBJECT  g_FilterDriverObject = NULL;

// backup of original items
PDRIVER_OBJECT  g_OriginalDriverObject = NULL;
PDRIVER_DISPATCH g_OriginalDispatchRead = (PDRIVER_DISPATCH)NULL;

// driver object type export in NTOSKRNL
extern POBJECT_TYPE *IoDriverObjectType;

// undocumented APIs
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
HookDispatchRead (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS
HookReadCompletion(
    PDEVICE_OBJECT  DeviceObject,
    PIRP  Irp,
    PVOID  Context );

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    UNICODE_STRING KbdDriverName;
    ULONG Idx;
    PDEVICE_OBJECT DeviceObject;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload = DriverUnload;

    // store the filters driver object
    g_FilterDriverObject = DriverObject;

    RtlInitUnicodeString ( &KbdDriverName, DRIVER_NAME_KBDCLASS );

    // Using the name KbdDriverName obtain a pointer to the driver
    // object and store it in g_OriginalDriverObject
    Status = ObReferenceObjectByName (
        &KbdDriverName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        &g_OriginalDriverObject );

    if ( ! NT_SUCCESS(Status) ) {
        DPF(("%s!%s ObReferenceObjectByName(%wZ) : FAIL=%08x\n" , __MODULE__, __FUNCTION__,
            &KbdDriverName, Status ));
        goto Exit;
    }

    DPF(( "%s!%s g_OriginalDriverObject=%p\n", __MODULE__, __FUNCTION__, 
        g_OriginalDriverObject ));

    // Step #1 : Copy all the dispatch routines (IRP_MJ_CREATE - IRP_MJ_PNP) pointer
    // from the keyboard driver object to the filter driver object

    // Save the original IRP_MJ_READ dispatch routine into g_OriginalDispatchRead
    // and setup HookDispatchRead() as the new dispatch routine
    g_OriginalDispatchRead = g_FilterDriverObject->MajorFunction[IRP_MJ_READ];
    g_FilterDriverObject->MajorFunction[IRP_MJ_READ] = HookDispatchRead;

    // Step #2 : For all the device objects created by the keyboard class driver 
    // -force them to point to the filter driver's driver object
    // -increment their StackSize field

    return STATUS_SUCCESS;

Exit :
    if ( g_OriginalDriverObject ) {
        // Dereference the original driver object (ObDereferenceObject())
        ObDereferenceObject ( g_OriginalDriverObject );
        g_OriginalDriverObject = NULL;
    }

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    PDEVICE_OBJECT DeviceObject;

    UNREFERENCED_PARAMETER(DriverObject);

    // Step #3 : For all the device objects created by the keyboard class driver 
    // restore the original driver object pointer and restore back the StackSize

    if ( g_OriginalDriverObject ) {
        ObDereferenceObject ( g_OriginalDriverObject );
        g_OriginalDriverObject = NULL;
    }

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
} // DriverUnload()


NTSTATUS
HookDispatchRead (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    NTSTATUS Status;

    DPF(( "%s!%s DeviceObject=%p Irp=%p\n", __MODULE__, __FUNCTION__, 
        DeviceObject, Irp ));

    // Copy the contents of the current IOSL to the next IOSL
    IoCopyCurrentIrpStackLocationToNext(Irp);

    // Setup HookReadCompletion() as the completion routine in the IRP
    IoSetCompletionRoutine ( Irp, HookReadCompletion, NULL, TRUE, TRUE, TRUE );

    // Step #4 : Emulate the action of IoCallDriver() and forward the IRP down to 
    // the original driver by directly invoking its original dispatch routine 
    // in g_OriginalDispatchRead

    return Status;
} // HookDispatchRead()

NTSTATUS
HookReadCompletion(
    PDEVICE_OBJECT  DeviceObject,
    PIRP  Irp,
    PVOID  Context )
{
    PKEYBOARD_INPUT_DATA KeyboardInputData;
    ULONG Idx;
    ULONG NumberOfKeys;

    UNREFERENCED_PARAMETER(Context);

    DPF(( "%s!%s DeviceObject=%p Irp=%p\n", __MODULE__, __FUNCTION__, 
        DeviceObject, Irp ));

    // propagate the pending returned flag
    if ( Irp->PendingReturned ) {
        IoMarkIrpPending ( Irp );
    }

    // check if we have any data
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

Exit :
    return STATUS_SUCCESS;
} // HookReadCompletion()
