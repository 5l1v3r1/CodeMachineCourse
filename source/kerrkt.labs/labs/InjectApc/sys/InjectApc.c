/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"

#define __MODULE__  "InjectApc"
#define DPF(x)      DbgPrint x

#define DEVICE_TYPE_INJECTAPC   0x434d

#define DEVICE_NAME_INJECTAPC   L"\\Device\\InjectApc"
#define SYMLINK_NAME_INJECTAPC  L"\\DosDevices\\InjectApc"

#define     STRING_SIZE 256

// IOCTL codes
#define IOCTL_INJECTAPC         CTL_CODE(DEVICE_TYPE_INJECTAPC, 0x0001, METHOD_BUFFERED, FILE_ALL_ACCESS)

#pragma warning(disable:4054)
#pragma warning(disable:4055)

typedef VOID KRUNDOWN_ROUTINE(
    __in struct _KAPC *Apc );
typedef KRUNDOWN_ROUTINE *PKRUNDOWN_ROUTINE;

typedef VOID KNORMAL_ROUTINE(
    __in_opt PVOID NormalContext,
    __in_opt PVOID SystemArgument1,
    __in_opt PVOID SystemArgument2 );
typedef KNORMAL_ROUTINE *PKNORMAL_ROUTINE;

typedef VOID KKERNEL_ROUTINE(
    struct _KAPC *Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2);
typedef KKERNEL_ROUTINE *PKKERNEL_ROUTINE;

typedef struct _STRUCT_INJECTAPC
{
    HANDLE ProcessId;
    PVOID User32MessageBoxA;
} STRUCT_INJECTAPC, *PSTRUCT_INJECTAPC;

typedef int (*MESSAGEBOXA)( PVOID hWnd, PCHAR lpText, PCHAR lpCaption, ULONG Type ); 

typedef struct _CONTEXT_DATA 
{
    PUCHAR      ShellCodeBase;
    ULONG       ShellCodeSize;

    // import function pointers
    MESSAGEBOXA MessageBox;

    // strings for printing
    CHAR        Text[STRING_SIZE];
    CHAR        Caption[STRING_SIZE];
} CONTEXT_DATA, *PCONTEXT_DATA;

// missing enumerated types for APCs
typedef enum _KAPC_ENVIRONMENT {
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT;

// kernel API prototypes
NTKERNELAPI
VOID
KeInitializeApc (
    IN PRKAPC Apc,
    IN PKTHREAD Thread,
    IN UCHAR Environment,
    IN PKKERNEL_ROUTINE KernelRoutine,
    IN PKRUNDOWN_ROUTINE RundownRoutine OPTIONAL,
    IN PKNORMAL_ROUTINE NormalRoutine OPTIONAL,
    IN KPROCESSOR_MODE ApcMode,
    IN PVOID NormalContext );

NTKERNELAPI
BOOLEAN 
KeInsertQueueApc(
    PKAPC Apc,
    PVOID SystemArgument1,
    PVOID SystemArgument2,
    KPRIORITY Increment);

NTSTATUS NTAPI 
PsSuspendProcess(
    PEPROCESS Process);

NTSTATUS NTAPI 
PsResumeProcess (
    PEPROCESS Process );

BOOLEAN
KeTestAlertThread (
    KPROCESSOR_MODE AlertMode );

// utils.c
HANDLE GetThreadForProcess ( 
    HANDLE ProcessId );

// utils.c
ULONG 
Getx64FunctionSize ( 
    PVOID Function );

// forward declerations
NTSTATUS
RequestApc(
   HANDLE ProcessId,
   PVOID ShellcodeBase,
   PVOID ContextData );

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );

NTSTATUS
DispatchIoctl (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp );


// this code for this function is copied in to UVAS
// it runs user mode as KAPC.NormalRoutine
VOID
UserApcNormalRoutine (
    PVOID NormalContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2 )
{
    PCONTEXT_DATA    Context = (PCONTEXT_DATA)NormalContext;

    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    Context->MessageBox ( NULL, Context->Text, Context->Caption, 0x00000000 );
}

// runs in kernel mode as KAPC.KernelRoutine
VOID
UserApcKernelRoutine (
    PKAPC Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2 )
{
    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    DPF(("%s!%s CID=%x.%x Irql=%u Apc=%p NormalRoutine=%p\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(),
        PsGetCurrentThreadId(),
        KeGetCurrentIrql(),
        Apc,
        *NormalRoutine ));

    ExFreePool ( Apc );
}

// runs in kernel mode as KAPC.NormalRoutine
VOID
KernelApcNormalRoutine (
    PVOID NormalContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2 )
{
    BOOLEAN Alerted = FALSE;

    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // KeTestAlertThread() sets the following flag
    // KTHREAD.ApcState.UserApcPending for the 
    // current thread
    Alerted = KeTestAlertThread ( UserMode );

    DPF(("%s!%s Alerted=%s\n", __MODULE__, __FUNCTION__, 
        Alerted ? "TRUE" : "FALSE" ));
}

// runs in kernel mode as KAPC.KernelRoutine
VOID
KernelApcKernelRoutine (
    PKAPC Apc,
    PKNORMAL_ROUTINE *NormalRoutine,
    PVOID *NormalContext,
    PVOID *SystemArgument1,
    PVOID *SystemArgument2 )
{
    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    DPF(("%s!%s CID=%x.%x Irql=%u Apc=%p NormalRoutine=%p\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(),
        PsGetCurrentThreadId(),
        KeGetCurrentIrql(),
        Apc,
        *NormalRoutine ));

    ExFreePool ( Apc );
}


// injects the function UserApcNormalRoutine() as a user mode APC
// into the process selected by Process Pid
NTSTATUS
InjectApc(
    HANDLE ProcessId,
    PVOID User32MessageBoxA )
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    HANDLE ProcessHandle = NULL;
    PEPROCESS ProcessObject = NULL;
    BOOLEAN Attached = FALSE;
    BOOLEAN Suspended = FALSE;
    PCONTEXT_DATA ContextData = NULL;
    PUCHAR AllocationBase = NULL;
    SIZE_T AllocationSize;
    ULONG ShellcodeSize;
    PUCHAR ShellcodeBase;
    KAPC_STATE ApcState;

    // get the size of the shell code that will be copied into user VAS
    ShellcodeSize = (ULONG)Getx64FunctionSize ( (PVOID)UserApcNormalRoutine );

    if ( ! ShellcodeSize ) {
        DPF(("%s!%s Getx64FunctionSize(UserApcNormalRoutine) FAIL\n", __MODULE__, __FUNCTION__ ));
        goto Exit;
    }

    // Step #1 : Obtain the EPROCESS pointer from ProcessId (PsLookupProcessByProcessId())
    // and store it in ProcessObject

    // Step #2 : Suspend the target process (PsSuspendProcess())

    Suspended = TRUE;

    // Step #3 : Attach to the target process (KeStackAttachProcess())

    Attached = TRUE;

    AllocationSize = ShellcodeSize + sizeof ( CONTEXT_DATA ) ;

    // Step #4 : Allocate memory in the target process large enough
    // to hold the shellcode and CONTEXT_DATA (ZwAllocateVirtualMemory())
    // and store the result in AllocationBase

    ShellcodeBase = AllocationBase;
    ContextData = (PCONTEXT_DATA)(AllocationBase + ShellcodeSize);

    // Step #5 : Copy the user mode APC code into the newly allocated 
    // memory (RtlCopyMemory()) i.e. @ ShellCodeBase

    //setup the context structure with data that will be required by the APC routine
    ContextData->ShellCodeBase = ShellcodeBase;
    ContextData->ShellCodeSize = ShellcodeSize;
    strncpy (  ContextData->Text, "Hello from Kernel", STRING_SIZE );
    strncpy (  ContextData->Caption, "P0wned", STRING_SIZE );
    //user32.dll base + offset of MessageBoxA;
    ContextData->MessageBox = (MESSAGEBOXA)User32MessageBoxA; 

    // queue the APC
    Status = RequestApc(
       ProcessId,
       ShellcodeBase,
       ContextData );

    if ( ! NT_SUCCESS( Status ) ) {
        DPF(("%s!%s RequestApc() FAIL=%08x\n", __MODULE__, __FUNCTION__,
             Status ));
        goto Exit;
    }


Exit :
    // in case of an error free the memory that was allocated
    if ( ! NT_SUCCESS ( Status ) ) {

        if ( AllocationBase ) {
            AllocationSize = 0;

            // Step #6 : Free the virtual memory that was allocated in the 
            // target process (ZwFreeVirtualMemory())
        }
    }


    if ( Attached ) {
        // Step #7 : Detach from the target process (KeUnstackDetachProcess())
    }

    if ( Suspended ) {
        // Step #8 : Resume the target process (PsResumeProcess())
    }

    if ( ProcessObject ) {
        // Step #9 : Dereference the process object (ObDereferenceObject())
    }

    return Status;
}


NTSTATUS
RequestApc(
   HANDLE ProcessId,
   PVOID ShellcodeBase,
   PVOID ContextData )
{
    NTSTATUS Status;
    PKAPC UserApc = NULL;
    PKAPC KernelApc = NULL;
    HANDLE ThreadId = NULL;
    PKTHREAD ThreadObject = NULL;


    ThreadId = GetThreadForProcess ( ProcessId );

    if ( ! ThreadId ) {
        DPF(("%s!%s GetThreadForProcess() FAIL\n", __MODULE__, __FUNCTION__ ));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // get the ETHREAD/KTHREAD for ThreadId
    Status = PsLookupThreadByThreadId ( ThreadId, &ThreadObject );

    if ( ! NT_SUCCESS( Status ) ) {
        DPF(("%s!%s PsLookupThreadByThreadId(%p) FAIL=%08x\n", __MODULE__, __FUNCTION__,
                ThreadId, Status ));
        goto Exit;
    }

    // allocate memory for the user mode APC object
    UserApc = ExAllocatePoolWithTag ( NonPagedPool, sizeof (KAPC), 'auMC' );
    if ( ! UserApc ) {
        DPF(("%s!%s ExAllocatePoolWithTag(UserApc) FAIL=%08x\n", __MODULE__, __FUNCTION__ ));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // allocate memory for the kernel mode APC
    KernelApc = ExAllocatePoolWithTag ( NonPagedPool, sizeof (KAPC), 'akMC' );
    if ( ! KernelApc ) {
        DPF(("%s!%s ExAllocatePoolWithTag(KernelApc) FAIL=%08x\n", __MODULE__, __FUNCTION__ ));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }
    
    // initialize the user mode APC obect with the 
    // APC routines UserApcKernelRoutine and UserApcNormalRoutine
    KeInitializeApc ( 
        UserApc,
        ThreadObject,
        OriginalApcEnvironment,
        (PKKERNEL_ROUTINE)UserApcKernelRoutine,
        (PKRUNDOWN_ROUTINE)NULL,
        (PKNORMAL_ROUTINE)ShellcodeBase, // user routine UserApcNormalRoutine()
        UserMode,
        ContextData );

    // queue the user mode APC
    // note that this APC will not be delivered until the thread is alerted
    // when the kernel mode APC calls KeTestAlertThread()
    KeInsertQueueApc( 
        UserApc,
        NULL,
        NULL,
        IO_NO_INCREMENT );

    // initialize the kernel mode APC obect with the 
    // APC routines KernelApcKernelRoutine and KernelApcNormalRoutine
    KeInitializeApc ( 
        KernelApc,
        ThreadObject,
        OriginalApcEnvironment,
        (PKKERNEL_ROUTINE)KernelApcKernelRoutine,
        (PKRUNDOWN_ROUTINE)NULL,
        (PKNORMAL_ROUTINE)KernelApcNormalRoutine,
        KernelMode,
        NULL );

    // queue the kernel mode APC which will alert the target thread
    KeInsertQueueApc( 
        KernelApc,
        NULL,
        NULL,
        IO_NO_INCREMENT );


Exit :
    if ( ThreadObject ) {
        ObDereferenceObject ( ThreadObject );
    }

    if ( ! NT_SUCCESS ( Status ) ) {
        if ( UserApc ) {
            ExFreePool ( UserApc );
        }
        
        if ( KernelApc ) {
            ExFreePool ( KernelApc );
        }
    }

    return Status;
} // RequestApc()


NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
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
    RtlInitUnicodeString ( &DeviceKernel, DEVICE_NAME_INJECTAPC );

    // create the device object
    Status = IoCreateDevice (
        DriverObject,
        0,
        &DeviceKernel,
        DEVICE_TYPE_INJECTAPC,
        0,
        FALSE,
        &DeviceObject  );

    if ( ! NT_SUCCESS( Status) ) {
        DPF(( "%s:% IoCreateDevice() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
        goto Exit;
    }

    // initialize the symbolic link name
    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_INJECTAPC );

    // create the symbolic link
    Status = IoCreateSymbolicLink (
        &DeviceUser, &DeviceKernel );

    if ( ! NT_SUCCESS( Status) ) {
        DPF(( "%s!%s IoCreateSymbolicLink() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
        goto Exit;
    }

    DPF(( "%s SymbolicLinkName=%wS\n", __FUNCTION__, SYMLINK_NAME_INJECTAPC ));

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

    RtlInitUnicodeString ( &DeviceUser, SYMLINK_NAME_INJECTAPC );

    IoDeleteSymbolicLink ( &DeviceUser );
    
    if ( DriverObject->DeviceObject ) {
        IoDeleteDevice ( DriverObject->DeviceObject );
    }

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));

} // DriverUnload()


NTSTATUS
DispatchCreateClose (
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp )
{
    DPF(( "%s DeviceObject=%p Irp=%p\n",  __FUNCTION__, 
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

    DPF(( "%s DeviceObject=%p Irp=%p\n",  __FUNCTION__, 
        DeviceObject, Irp ));

    // retrieve the current I/O Stack Location from the IRP
    IoStackLocation =  IoGetCurrentIrpStackLocation(Irp);

    // process the IOCTL code provided in the call to DeviceIoControl()
    switch (IoStackLocation->Parameters.DeviceIoControl.IoControlCode) {
        case  IOCTL_INJECTAPC : {
            PSTRUCT_INJECTAPC StructInjectApc = (PSTRUCT_INJECTAPC)Irp->AssociatedIrp.SystemBuffer;

            if ( IoStackLocation->Parameters.DeviceIoControl.InputBufferLength < sizeof ( STRUCT_INJECTAPC ) ) {
                BytesReturned = 0;
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            Status = InjectApc(
                StructInjectApc->ProcessId,
                StructInjectApc->User32MessageBoxA );
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


