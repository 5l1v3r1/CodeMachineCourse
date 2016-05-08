/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "ntimage.h"

#define __MODULE__      "UTILS"
#define DPF(x)          DbgPrint x

#define SystemProcessInformation    5

// number of processes
#define PROCESS_COUNT               100
// number of threads in process
#define THREAD_COUNT                100

typedef struct _SYSTEM_THREAD_INFORMATION {
    ULONGLONG KernelTime;
    ULONGLONG UserTime;
    ULONGLONG CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    CLIENT_ID ClientId;
    ULONG Priority;
    ULONG BasePriority;
    ULONG ContextSwitches;
    ULONG ThreadState;
    ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    ULONGLONG WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    ULONGLONG CreateTime;
    ULONGLONG UserTime;
    ULONGLONG KernelTime;
    UNICODE_STRING ImageName;
    ULONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    PVOID UniqueProcessKey;
    ULONGLONG PeakVirtualSize;
    ULONGLONG VirtualSize;
    ULONG PageFaultCount;
    ULONGLONG PeakWorkingSetSize;
    ULONGLONG WorkingSetSize;
    ULONGLONG QuotaPeakPagedPoolUsage;
    ULONGLONG QuotaPagedPoolUsage;
    ULONGLONG QuotaPeakNonPagedPoolUsage;
    ULONGLONG QuotaNonPagedPoolUsage;
    ULONGLONG PagefileUsage;
    ULONGLONG PeakPagefileUsage;
    ULONGLONG PrivatePageCount;
    ULONGLONG ReadOperations;
    ULONGLONG WriteOperations;
    ULONGLONG OtherOperations;
    ULONGLONG ReadTransfers;
    ULONGLONG WriteTransfers;
    ULONGLONG OtherTransfers;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

NTSYSAPI
NTSTATUS
NTAPI
ZwQuerySystemInformation(
    IN ULONG SystemInformationClass,
    IN OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL );

PIMAGE_RUNTIME_FUNCTION_ENTRY 
RtlLookupFunctionEntry(
    ULONGLONG ControlPc,
    PULONGLONG ImageBase,
    PULONGLONG TargetGp );

// returns the thread ID of the first thread 
// in the process identified by the process ID
HANDLE GetThreadForProcess ( 
    HANDLE ProcessId )
{
    PVOID SystemInformationBuffer = NULL;
    ULONG SystemInformationLength = 0;
    NTSTATUS Status;
    ULONG ReturnLength;
    PSYSTEM_PROCESS_INFORMATION SystemProcess;
    ULONG ProcessIdx;
    HANDLE ThreadId = NULL;

    SystemInformationLength = 
        ( sizeof ( SYSTEM_PROCESS_INFORMATION ) + ( sizeof ( SYSTEM_THREAD_INFORMATION ) * THREAD_COUNT ) ) * PROCESS_COUNT;

    // allocate memory for about 100 threads in 100 processes
    SystemInformationBuffer = ExAllocatePoolWithTag ( 
        NonPagedPool, 
        SystemInformationLength,
        'tsMC' );

    if ( ! SystemInformationBuffer ) {
        goto Exit;
    }

    // retrieve a list of all the processes and threads
    // in the system
    Status = ZwQuerySystemInformation(
        SystemProcessInformation,
        SystemInformationBuffer,
        SystemInformationLength,
        &ReturnLength );

    if ( ! NT_SUCCESS( Status ) ) {
        DPF(("%s!%s ZwQuerySystemInformation() FAIL=%08x\n", __MODULE__, __FUNCTION__,
             Status ));
        goto Exit;
    }

    SystemProcess = (PSYSTEM_PROCESS_INFORMATION)SystemInformationBuffer;

    // iterate through all the processes and find one that matches the PID
    for ( ProcessIdx = 0 ;  ; ProcessIdx++ ) {

        if ( SystemProcess->UniqueProcessId == ProcessId ) {
            // ensure that there is at least one thread
            if ( SystemProcess->NumberOfThreads ) {
                PSYSTEM_THREAD_INFORMATION SystemThreads;
                // return the first thread in the matching process
                SystemThreads = (PSYSTEM_THREAD_INFORMATION)(SystemProcess + 1);
                ThreadId = SystemThreads->ClientId.UniqueThread;
                goto Exit;
            }
        }

        // if this is the last process then abort
        if ( ! SystemProcess->NextEntryOffset ) {
            break;
        }

        // move on to the next process in the list
        SystemProcess = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)SystemProcess + SystemProcess->NextEntryOffset);
    }

Exit :
    if ( SystemInformationBuffer ) {
        ExFreePool ( SystemInformationBuffer );
    }

    return ThreadId;
}


// returns the size (number of bytes) in an X64 function
ULONG 
Getx64FunctionSize ( 
    PVOID Function )
{
    PIMAGE_RUNTIME_FUNCTION_ENTRY  RuntimeFunction;
    ULONGLONG ImageBase = 0;
    ULONG FunctionSize = 0;

    // RtlLookupFunctionEntry() locates the start and end of the function
    // using the X64 exception table information in the PE file
    // RtlLookupFunctionEntry() is only available on X64 systems
    // using it makes this this module incompatible with x86 systems
    RuntimeFunction = (PIMAGE_RUNTIME_FUNCTION_ENTRY)RtlLookupFunctionEntry(
        (ULONGLONG)Function,
        &ImageBase,
        NULL );

    // if there is no valid runtime entry structure then the extents of 
    // the functions cannot be determined
    if ( ! RuntimeFunction ) {
        DPF(("%s!%s RtlLookupFunctionEntry(%p)\n", __MODULE__, __FUNCTION__,
             Function ));
        goto Exit;
    }

    // compute the number of functions in the function body based on the 
    // functions extents, note this logic will not work for BBT'd functions
    FunctionSize = RuntimeFunction->EndAddress - RuntimeFunction->BeginAddress;

    DPF(("%s!%s PRUNTIME_FUNCTION(BeginAddress=0x%08x EndAddress=0x%08x) Size=%u\n", __MODULE__, __FUNCTION__,
        RuntimeFunction->BeginAddress, RuntimeFunction->EndAddress, FunctionSize ));

Exit :
    return FunctionSize;
} // Getx64FunctionSize()

