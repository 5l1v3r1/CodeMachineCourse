/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

#define __MODULE__  "InlineHook"
#define DPF(x)      DbgPrint x

#pragma warning(disable:4054)

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

// implemented in amd64\Trampoline.asm
VOID Trampoline(VOID);

// implemented in MemCpyWP.c
VOID MemCpyWP ( 
    PUCHAR Destination, 
    PUCHAR Source, 
    ULONG Size );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

#pragma pack(push,1)
typedef struct _HOOK_OPCODES {
    // Step #1 : Create an instruction template structure for hook instructions
    // in which instruction operands are easy to modify.
    // Purpose of the hook instructions is to transfer execution control to the trampoline.
    // For each instruction in the hook add an UCHAR array to 
    // represent the instruction opcodes.
    // For absolute addresses use a ULONGLONG.
    // At the end add NOPs to pad the hook instruction up to an
    // instruction boundary in the original function.
} HOOK_OPCODES, *PHOOK_OPCODES;
#pragma pack(pop)

extern POBJECT_TYPE *IoDriverObjectType; // defined in NTOSKRNL.exe
PUCHAR g_OrignalFunction = NULL;
PUCHAR g_OrignalResume = NULL;
PUCHAR g_HookFunction = NULL;
UCHAR g_OriginalOpcodes[sizeof(HOOK_OPCODES)];


NTSTATUS HookDispatchWrite ( 
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    PIO_STACK_LOCATION Iosl;

    Iosl = IoGetCurrentIrpStackLocation(Irp);

    // check if this is a write operation
    if ( Iosl->MajorFunction == IRP_MJ_WRITE ) {
        // display the DO, IRP, Write Byte Offset and Write Length
        DPF(( "DISK WRITE DO=%p Irp=%p Offset=%I64u Length=%u\n", 
            DeviceObject, 
            Irp, 
            Iosl->Parameters.Write.ByteOffset,
            Iosl->Parameters.Write.Length ));
    }
    return STATUS_SUCCESS;
}

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UNICODE_STRING TargetDeviceName;
    PDRIVER_OBJECT TargetDriverObject = NULL;
    PFILE_OBJECT TargetFileObject = NULL;
    PDEVICE_OBJECT TargetDeviceObject = NULL;

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    HOOK_OPCODES HookOpCodes = { 
        // Step # 2 : Fill up the op-codes that will be used to 
        // detour the original function to the trampoline function 
        // Trampoline() defined in amd64\Trampoline.asm
    };

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload = DriverUnload;

    // name of one the device objects created by disk.sys
    RtlInitUnicodeString ( &TargetDeviceName, L"\\Device\\Harddisk0\\DR0" );

    // open this device object by name (i.e. create a FILE_OBJECT)
    Status = IoGetDeviceObjectPointer (
        &TargetDeviceName,
        FILE_READ_DATA,
        &TargetFileObject,
        &TargetDeviceObject );

    if ( ! NT_SUCCESS(Status) ) {
        DPF(("%s!%s IoGetDeviceObjectPointer(%wZ) : FAIL=%08x\n" , __MODULE__, __FUNCTION__,
            &TargetDeviceName, Status ));
        Status = STATUS_NOT_FOUND;
        goto Exit;
    }

    DPF(( "%s!%s TargetDeviceObject=%p DRVO=%p\n", __MODULE__, __FUNCTION__, 
        TargetDeviceObject,
        TargetDeviceObject->DriverObject ));

    DPF(( "%s!%s TargetDeviceObject=%p DO=%p DRVO=%p\n", __MODULE__, __FUNCTION__, 
        TargetFileObject, 
        TargetFileObject->DeviceObject, 
        TargetFileObject->DeviceObject->DriverObject ));

    // extract the DRIVER_OBJECT from the DEVICE_OBJECT that is pointed to by the FILE_OBJECT
    TargetDriverObject = TargetFileObject->DeviceObject->DriverObject;

    DPF(( "%s!%s TargetDriverObject=%p\n", __MODULE__, __FUNCTION__, 
        TargetDriverObject ));

    // Store the address of the disk.sys IRP_MJ_WRITE entry point in g_OrignalFunction
    // This is the original function that will be detoured
    g_OrignalFunction = (PUCHAR)TargetDriverObject->MajorFunction[IRP_MJ_WRITE];

    // Store the address of the function HookDispatchWrite() in g_HookFunction
    g_HookFunction = (PUCHAR)HookDispatchWrite;

    // Store the address within the original function where execution 
    // will resume after HookDispatch() has been called, taking care of
    // instruction boundaries. Execution will resume at the instruction 
    // right after the hooked instructions
    g_OrignalResume = g_OrignalFunction + sizeof (HOOK_OPCODES);

    // Before overwriting the opcodes at the beginning of the original 
    // function, save them in g_OriginalOpcodes
    memcpy ( g_OriginalOpcodes, g_OrignalFunction, sizeof(g_OriginalOpcodes) );

    // replace the instructions in the original function with the hook instructions
    MemCpyWP ( g_OrignalFunction, (PUCHAR)&HookOpCodes, sizeof(HookOpCodes) );

    // drop the reference that was taken by IoGetDeviceObjectPointer()
    if ( TargetFileObject ) {
        ObDereferenceObject( TargetFileObject );
    }
    return STATUS_SUCCESS;

Exit :
    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    // restore back the original opcodes in the original function
    MemCpyWP ( g_OrignalFunction, g_OriginalOpcodes, sizeof(g_OriginalOpcodes) );

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
} // DriverUnload()
