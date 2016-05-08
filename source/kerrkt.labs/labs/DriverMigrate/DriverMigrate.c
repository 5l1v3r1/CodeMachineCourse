/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"

#define __MODULE__  "DriverMigrate"
#define DPF(x)      DbgPrint x

#define VA_POOL_TAG 'CSMC'

#define TIME_INTERVAL_IN_SEC    10

#pragma warning( disable:4100 )
#pragma warning( disable:4055 )
#pragma warning( disable:4054 )
#pragma warning( disable:4057 )
#pragma warning( disable:4100 )

typedef struct _CONTEXT_DATA 
{
    PUCHAR      OrgDriverBase;
    PUCHAR      NewDriverBase;
    ULONG       DriverSize;

    PVOID       DbgPrint;
    PVOID       ExSystemTimeToLocalTime;
    PVOID       RtlTimeToTimeFields;

    PUCHAR      NewFormat;
    PUCHAR      NewContext;
    PUCHAR      NewRoutine;

    KTIMER      Timer;
    KDPC        Dpc;
} CONTEXT_DATA, *PCONTEXT_DATA;

//
// Globals
//

CONTEXT_DATA    g_ContextData = {0};
PCHAR           g_FormatString = "Timer Fired @ %04u%02u%02u-%02u%02u%02u.%04u\n";

VOID
TimerDpcRoutine (
    struct _KDPC  *Dpc,
    PVOID  DeferredContext,
    PVOID  SystemArgument1,
    PVOID  SystemArgument2 );

VOID
SetupTimer( 
    PCONTEXT_DATA Data );

BOOLEAN 
MigrateDriver (
    PUCHAR DriverBase,
    ULONG  DriverSize );

PVOID
GetNTOSExport ( 
    PWCHAR  Name )
{
    UNICODE_STRING FunctionName;

    RtlInitUnicodeString ( &FunctionName, Name );

    return (PVOID)MmGetSystemRoutineAddress ( &FunctionName );
} // GetNTOSImport()

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DPF(("%s!%s DriverBase=%p DriverSize=%x\n", __MODULE__, __FUNCTION__,
         DriverObject->DriverStart, DriverObject->DriverSize ));

    // call MigrateDriver() with the base and size of the original driver image in memory
    if ( MigrateDriver ( DriverObject->DriverStart, DriverObject->DriverSize ) != TRUE ) {
        DPF(("%s!%s MigrateDriver() FAIL\n", __MODULE__, __FUNCTION__ ));
        goto Exit;
    }

    // setup the timer that will periodically execute the migrated driver image
    SetupTimer ( &g_ContextData );

    // now that execution has been migrated to non-paged pool force the driver to unload
Exit :
    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
    return STATUS_UNSUCCESSFUL;
} // DriverEntry()

VOID
SetupTimer( 
    PCONTEXT_DATA Data )
{
    PCONTEXT_DATA NewData;
    LARGE_INTEGER DueTime;
    LONG Period;

    // retrieve a pointer to the global context area in the migrated image
    NewData = (PCONTEXT_DATA)Data->NewContext;

    DPF(("%s!%s Migrated g_ContextData=%p\n", __MODULE__, __FUNCTION__,
         Data->NewDriverBase ));

    // setup the timer
    KeInitializeTimer ( 
        &NewData->Timer );

    // setup the TimerDpcRoutine from the migrated image
    KeInitializeDpc ( 
        &NewData->Dpc, 
        (PKDEFERRED_ROUTINE)NewData->NewRoutine,
        Data->NewContext );

    // one shot and periodic timer
    DueTime.QuadPart = (ULONGLONG)( (-1) * 10 * 1000 * 1000 * TIME_INTERVAL_IN_SEC );
    Period = TIME_INTERVAL_IN_SEC * 1000;

    // start the timer
    KeSetTimerEx ( 
        &NewData->Timer, 
        DueTime, 
        Period, 
        &NewData->Dpc );
} // SetupTimer()

BOOLEAN 
MigrateDriver (
    PUCHAR DriverBase,
    ULONG  DriverSize )
{
    PCONTEXT_DATA Data = &g_ContextData;

    Data->OrgDriverBase     = (PUCHAR)DriverBase;
    Data->DriverSize        = DriverSize;

    UINT_PTR offset = 0;

    // Step #1 : Allocate memory from non-paged pool to migrate the entire driver image
    // (ExAllocatePoolWithTag())
    if (NULL == (Data->NewDriverBase = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, Data->DriverSize, 'gmvd')))
    {
        DbgPrint("Error OUT OF MEMORY\n");
        goto Exit;
    }

    DPF(("%s!%s NewDriverBase=%p\n", __MODULE__, __FUNCTION__,
         Data->NewDriverBase ));

    // Step #2 : Retrieve pointers to the NTOSKrnl functions that the TimeDpcRoutine() will call
    // The NTOSKrnl functions are DbgPrint(), ExSystemTimeToLocalTime(), RtlTimeToTimeFields().
    // Use the function GetNTOSExport() to perform this step
    Data->DbgPrint = GetNTOSExport(L"DbgPrint");
    Data->ExSystemTimeToLocalTime = GetNTOSExport(L"ExSystemTimeToLocalTime");
    Data->RtlTimeToTimeFields = GetNTOSExport(L"RtlTimeToTimeFields");

    // Step #3 : Compute the address of g_FormatString, g_ContextData and TimerDpcRoutine()
    // in the migrated image and save them in NewFormat, NewContext and NewRoutine respectively.
    offset = (UINT_PTR)g_FormatString - (UINT_PTR)Data->OrgDriverBase;
    DbgPrint("Offset 1 (%x)\n", offset);
    Data->NewFormat = (PUCHAR)(offset + Data->NewDriverBase);

    offset = (UINT_PTR)&g_ContextData - (UINT_PTR)Data->OrgDriverBase;
    DbgPrint("Offset 2 (%x)\n", offset);
    Data->NewContext = (PUCHAR)(offset + Data->NewDriverBase);

    offset = (UINT_PTR)TimerDpcRoutine - (UINT_PTR)Data->OrgDriverBase;
    DbgPrint("Offset 3 (%x)\n", offset);
    Data->NewRoutine = (PUCHAR)(offset + Data->NewDriverBase);

    // Step #4 : Copy the loaded driver image to the memory allocated in non-paged pool
    RtlCopyMemory(Data->NewDriverBase, Data->OrgDriverBase, Data->DriverSize);

    // After this point any changes made to g_ContextData in the 
    // original driver image will not be migrated to the new image
    return TRUE;

Exit :
    if ( Data->NewDriverBase ) {
        // Step #5 : Free the memory that was allocated from non-paged pool
        ExFreePoolWithTag(Data->NewDriverBase, 'gmvd');
        Data->NewDriverBase = NULL;
    }
    return FALSE;
} //MigrateDriver ()


// Typedef function signatures that will be used to call 
// DbgPrint(), ExSystemTimeToLocalTime(), RtlTimeToTimeFields()
typedef ULONG (*DBGPRINT)( PCHAR  Format, ... ); 
typedef VOID (*EXSYSTEMTIMETOLOCALTIME) ( PLARGE_INTEGER  SystemTime, PLARGE_INTEGER  LocalTime );
typedef VOID (*RTLTIMETOTIMEFIELDS) ( PLARGE_INTEGER  Time, PTIME_FIELDS  TimeFields );

// all the data this function has to refer to must be stored in 
// CONTEXT_DATA which is passed to this function as DeferredContext
VOID
TimerDpcRoutine (
    struct _KDPC  *Dpc,
    PVOID  DeferredContext,
    PVOID  SystemArgument1,
    PVOID  SystemArgument2 )
{
    PCONTEXT_DATA Data = (PCONTEXT_DATA)DeferredContext;
    LARGE_INTEGER  SystemTime;
    LARGE_INTEGER  LocalTime;
    TIME_FIELDS LocalTimeFields;

    // retrieve the current system time in UTC
    // KeQuerySystemTime() is a macro in wdm.h
    KeQuerySystemTime ( &SystemTime );

    // Step #6 : Convert the system time to local time
    // Ensure that the call to the API does not use the Import Table of the driver
    ((EXSYSTEMTIMETOLOCALTIME)Data->ExSystemTimeToLocalTime)(&SystemTime, &LocalTime);

    // Step #7 : Retrieve the time fields of the local time
    // Ensure that the call to the API does not use the Import Table of the driver
    ((RTLTIMETOTIMEFIELDS)Data->RtlTimeToTimeFields)(&LocalTime, &LocalTimeFields);

    // Step #8 : Display the local time using the time fields in the format YYYYMMDD-HHMMSS.sss
    // using the format string in PCONTEXT_DATA->NewFormat
    // Ensure that the call to the API does not use the Import Table of the driver
    ((DBGPRINT)Data->DbgPrint)(Data->NewFormat, LocalTimeFields.Year, LocalTimeFields.Month, LocalTimeFields.Day, LocalTimeFields.Hour, LocalTimeFields.Minute, LocalTimeFields.Second, LocalTimeFields.Milliseconds);

} // TimerDpcRoutine()

