/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
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
DriverThread (
    PVOID Context );

PVOID
CreateDriverThread (
    PKEVENT Event );

VOID
DestroyDriverThread (
    PVOID ThreadObject,
    PKEVENT Event );

KEVENT g_ThreadEvent;
PVOID g_ThreadObject = NULL;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // call the function that will create the system thread
    if ( ( g_ThreadObject = CreateDriverThread ( &g_ThreadEvent ) ) == NULL ) {
        Status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    DbgPrint ( "%s Driver=%p g_Thread=%p g_Event=%p\n", __FUNCTION__, 
        DriverObject, g_ThreadObject, &g_ThreadEvent );

    Status = STATUS_SUCCESS;

Exit :
    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    // call the function that will destroy the thread created by CreateDriverThread()
    DestroyDriverThread ( g_ThreadObject, &g_ThreadEvent );

    DbgPrint ( "%s DONE\n", __FUNCTION__ );
} // DriverUnload()

NTSTATUS
DriverThread (
    PVOID Context )
{
    PKEVENT Event = (PKEVENT)Context;

    DbgPrint ( "%s TID=%p Waiting\n", __FUNCTION__, PsGetCurrentThreadId() );

    // Step #1 : Block the thread on the Event (KeWaitForSingleObject())

    KeWaitForSingleObject(Event, Executive, KernelMode, TRUE, NULL);
    DbgPrint ( "%s TID=%p Unblocked\n", __FUNCTION__, PsGetCurrentThreadId() );
    
    // Step #2 : Terminate the thread  (PsTerminateSystemThread())
    PsTerminateSystemThread(STATUS_SUCCESS);

    return STATUS_SUCCESS;
} // DriverThread()

PVOID
CreateDriverThread (
    PKEVENT Event )
{
    NTSTATUS Status;
    HANDLE ThreadHandle;
    PVOID ThreadObject = NULL;

    // Step #3 : Initialize the event that would be signaled to terminate the created thread. (KeInitializeEvent())
    KeInitializeEvent(Event, NotificationEvent, FALSE);

    // Step #4 : Create a new system thread with the function DriverThread() as 
    // the thread function (PsCreateSystemThread())
    if (!NT_SUCCESS(Status = PsCreateSystemThread(&ThreadHandle, STANDARD_RIGHTS_ALL, NULL, NULL, NULL, DriverThread, (PVOID)Event)))
    {
        DbgPrint("%s PsCreateSystemThread Error (%d)\n", Status);
        goto Exit;
    }

    // Step #5 : Reference the thread handle to get a pointer to the thread (ObReferenceObjectByHandle())
    if (!NT_SUCCESS(Status = ObReferenceObjectByHandle(ThreadHandle, STANDARD_RIGHTS_ALL, *PsThreadType, KernelMode, &ThreadObject, NULL)))
    {
        DbgPrint("%s ObReferenceObjectByHandle Error (%d)\n", Status);
        goto Exit;
    }

    // Step #6 : Close the handle (ZwClose()) to the thread.
    ZwClose(ThreadHandle);

    // return the thread to the caller
    return ThreadObject;

Exit :
    // in case of error, destroy the thread that was created
    DestroyDriverThread ( ThreadObject, Event );

    return NULL;
} // CreateDriverThread()

VOID
DestroyDriverThread (
    PVOID ThreadObject,
    PKEVENT Event )
{
    // Step #7 : Signal the event that the thread is waiting on (KeSetEvent()) 
    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    if ( ThreadObject ) {
        // Step #8 : Wait for the thread to terminate (KeWaitForSingleObject())
        KeWaitForSingleObject(ThreadObject, Executive, KernelMode, TRUE, NULL);

        // Step #9 :Drop the reference to the thread (ObDereferenceObject())
        ObDereferenceObject(ThreadObject);
    }
} // DestroyDriverThread()
