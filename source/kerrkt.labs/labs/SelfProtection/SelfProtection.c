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

NTSTATUS
DispatchShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS
ProtectBinary(
    VOID );
    
NTSTATUS
ProtectService(
    VOID );
    
#define DEVICE_TYPE_SHUTDOWN    0x1234
#define DEVICE_NAME_SHUTDOWN    L"\\Device\\SelfProtection"

#define DRIVER_BINARY_PATH      L"\\??\\c:\\Windows\\System32\\Drivers\\SelfProtection.sys"
#define DRIVER_SERVICE_KEY      L"\\REGISTRY\\Machine\\System\\CurrentControlSet\\Services\\SelfProtection"
#define DRIVER_SERVICE_IMAGE    L"System32\\Drivers\\SelfProtection.sys"
#define DRIVER_SERVICE_START    3   // driver start type
#define DRIVER_SERVICE_TYPE     1   // kernel mode driver

HANDLE g_FileHandle = NULL;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceKernel;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    // Do not register a driver unload so that driver cannot be unloaded

    // Step #1 : Register the function DispatchShutdown() as the dispatch
    // routine for IRP_MJ_SHUTDOWN

    // initialize the name of the shutdown device
    RtlInitUnicodeString ( &DeviceKernel, DEVICE_NAME_SHUTDOWN );

    // create the device object for which shutdown notifications will be enabled
    Status = IoCreateDevice(
        DriverObject,
        0,
        &DeviceKernel,
        DEVICE_TYPE_SHUTDOWN,
        0,
        FALSE,
        &DeviceObject  );

    if ( ! NT_SUCCESS( Status) ) {
        DbgPrint ( "%s IoCreateDevice() FAIL=%08x\n", 
            __FUNCTION__, Status );
        goto Exit;
    }

    // Step #2 : Register DeviceObject for shutdown notification (IoRegisterShutdownNotification())


    Status = ProtectBinary();
    if ( NT_SUCCESS( Status) ) {
        DbgPrint ( "%s ProtectBinary() DONE\n", __FUNCTION__ );
    }

    return STATUS_SUCCESS;

Exit :
    if ( DeviceObject ) {
        IoDeleteDevice ( DeviceObject );
    }

    return Status;
} // DriverEntry()


NTSTATUS
DispatchShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    NTSTATUS Status;
    
    DbgPrint ( "%s DeviceObject=%p Irp=%p\n", __FUNCTION__,
        DeviceObject, Irp );

    Status = ProtectService();
    if ( NT_SUCCESS( Status) ) {
        DbgPrint ( "%s ProtectService() DONE\n", __FUNCTION__ );
    }

    // complete the shutdown IRP
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest ( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
} // DispatchShutdown()


NTSTATUS
ProtectBinary (
    VOID )
{
    NTSTATUS Status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    UNICODE_STRING FilePath;

    RtlInitUnicodeString ( &FilePath, DRIVER_BINARY_PATH );

    InitializeObjectAttributes ( 
        &ObjectAttributes, 
        &FilePath, 
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL );

    // Step #3 : Open a handle to the driver binary file using the path in 
    // ObjectAttributes (ZwCreateFile()) and store the file handle in g_FileHandle

    return Status;
} // ProtectBinary()

NTSTATUS
ProtectService(
    VOID )
{
    NTSTATUS Status;
    HANDLE RegistryHandle = NULL;
    UNICODE_STRING ValueNameU;
    UNICODE_STRING ValueDataU;
    ULONG ValueDataUlong;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Disposition;
    UNICODE_STRING RegistryPath;

    RtlInitUnicodeString ( &RegistryPath, L"\\REGISTRY\\Machine\\System\\CurrentControlSet\\Services\\SelfProtection" );
    
    InitializeObjectAttributes(
        &ObjectAttributes,
        &RegistryPath,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL );

    // Step #4: Open a handle to the service key of the driver in using the path in ObjectAttributes (ZwCreateKey())
    // and store the handle in RegistryHandle

    // Start (REG_DWORD) : 3
    RtlInitUnicodeString (&ValueNameU, L"Start");
    ValueDataUlong = DRIVER_SERVICE_START; // demand start

    // Step #5 : Set the value identified by ValueNameU to the DWORD type data in ValueDataUlong (ZwSetValueKey())
    // Use the registry key handle in RegistryHandle

    // Type (REG_DWORD) : 1
    RtlInitUnicodeString (&ValueNameU, L"Type");
    ValueDataUlong = DRIVER_SERVICE_TYPE;

    // Step #6 : Set the value identified by ValueNameU to the DWORD type data in ValueDataUlong (ZwSetValueKey())
    // Use the registry key handle in RegistryHandle

    // ImagePath (REG_EXPAND_SZ) : System32\Drivers\SelfProtection.sys
    RtlInitUnicodeString (&ValueNameU, L"ImagePath");
    RtlInitUnicodeString (&ValueDataU, DRIVER_SERVICE_IMAGE);

    // Step #7 : Set the value identified by ValueNameU to the EXPAND_SZ type data in ValueDataU (ZwSetValueKey())
    // Use the registry key handle in RegistryHandle

Exit:
    if ( RegistryHandle ) {
        ZwClose ( RegistryHandle );
    }

    return Status;
} // ProtectService()
