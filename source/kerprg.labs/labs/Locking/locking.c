/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"

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

VOID
ThreadNotifyCallback (
    HANDLE  ProcessId,
    HANDLE  ThreadId,
    BOOLEAN  Create );

#define TID_ARRAY_SIZE 256
ERESOURCE   g_TidLock;
HANDLE      g_TidArray[TID_ARRAY_SIZE];
ULONG       g_TidIndex = 0;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS            Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload  = DriverUnload;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    // Step #1 : Initialize the lock that protects the g_Tidxxx globals (ExInitializeResourceLite())
    ExInitializeResourceLite(&g_TidLock);

    Status = PsSetCreateThreadNotifyRoutine( ThreadNotifyCallback ); 
    if ( ! NT_SUCCESS ( Status ) ) {
        DbgPrint ( "%s PsSetCreateThreadNotifyRoutine() FAIL=%08x\n", __FUNCTION__, Status );
        goto Exit;
    }

    Status = STATUS_SUCCESS;

Exit :
    return Status;
}

VOID
ThreadNotifyCallback (
    HANDLE  ProcessId,
    HANDLE  ThreadId,
    BOOLEAN  Create )
{
    DbgPrint ( "%s ProcessId=%x ThreadId=%x %s\n", __FUNCTION__,
        ProcessId,
        ThreadId,
        Create ? "CREATED" : "DESTROYED" );

    if ( Create ) {

        // Step #2 : Acquire the lock that protects the g_Tidxxx globals 
        // (KeEnterCriticalRegion() and ExAcquireResourceExclusiveLite())
        KeEnterCriticalRegion();
        ExAcquireResourceExclusiveLite(&g_TidLock, TRUE);

        g_TidArray[g_TidIndex] = ThreadId;
        g_TidIndex++;
        if ( g_TidIndex >= TID_ARRAY_SIZE ) {
            g_TidIndex = 0;
        }

        // Step #3 : Release the lock that protects the g_Tidxxx globals 
        // (KeLeaveCriticalRegion() and ExReleaseResourceLite())
        ExReleaseResourceLite(&g_TidLock);
        KeLeaveCriticalRegion();
    }
}

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    NTSTATUS            Status;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    Status = PsRemoveCreateThreadNotifyRoutine ( ThreadNotifyCallback );
    if ( ! NT_SUCCESS ( Status ) ) {
        DbgPrint ( "%s PsRemoveCreateThreadNotifyRoutine() FAIL=%08x\n", __FUNCTION__, Status );
    }

    // Step #4 : Uninitialize the lock that protects the g_Tidxxx globals (ExDeleteResourceLite())
    ExDeleteResourceLite(&g_TidLock);
}

