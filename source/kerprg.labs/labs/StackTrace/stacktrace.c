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

#define RET_ADDR_COUNT  5

typedef void (*PRET_ADDR)();

typedef struct  _BACKTRACE_ENTRY {
    ULONG Hash;
    HANDLE  ProcessId;
    PRET_ADDR Address[RET_ADDR_COUNT];
}   BACKTRACE_ENTRY, *PBACKTRACE_ENTRY;

#define BACKTRACE_COUNT 16

ULONG g_BackTraceIndex = 0;
BACKTRACE_ENTRY g_BackTraceHistory[BACKTRACE_COUNT];

VOID CaptureStack ( HANDLE  ProcessId )
{
    ULONG Index;
    PBACKTRACE_ENTRY Entry;

    // Step 1 : Compute the index of the next entry of the array g_BackTraceIndex
    Index = g_BackTraceIndex;
    g_BackTraceIndex++;
    if (g_BackTraceIndex >= BACKTRACE_COUNT)
    {
        g_BackTraceIndex = 0;
    }

    // Step 2 : Get the entry corresponding to the index into the array g_BackTraceHistory in Entry
    Entry = &g_BackTraceHistory[Index];
    RtlZeroMemory(Entry, sizeof(*Entry));

    // Step 3 : Capture the ProcessId in Entry->ProcessId
    Entry->ProcessId = ProcessId;

    // Step 4 : Capture the stack backtrace in Entry->Address (RtlCaptureStackBackTrace())
    RtlCaptureStackBackTrace(1, 3, (PVOID *)&Entry->Address, &Entry->Hash);
}

VOID
ImageNotifyRoutine (
    PUNICODE_STRING  FullImageName,
    HANDLE  ProcessId,
    PIMAGE_INFO  ImageInfo)
{
    UNREFERENCED_PARAMETER(FullImageName);
    UNREFERENCED_PARAMETER(ImageInfo);

    CaptureStack ( ProcessId );
}


NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    RtlZeroMemory ( g_BackTraceHistory, sizeof(BACKTRACE_ENTRY) * BACKTRACE_COUNT );

    PsSetLoadImageNotifyRoutine(ImageNotifyRoutine);

    return STATUS_SUCCESS;
}

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    PsRemoveLoadImageNotifyRoutine(ImageNotifyRoutine);
}
