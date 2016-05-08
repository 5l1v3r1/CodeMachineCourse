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

VOID
TimerDpcRoutine (
    struct _KDPC  *Dpc,
    PVOID  DeferredContext,
    PVOID  SystemArgument1,
    PVOID  SystemArgument2 );

VOID
GetCurrentTime ( 
    PTIME_FIELDS  TimeFields );

KTIMER g_Timer;
KDPC g_Dpc;
LARGE_INTEGER RequestTime;

#define TIME_INTERVAL_IN_SEC    10

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    LARGE_INTEGER DueTime;
    LONG Period;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // Step #1 : Initialize the global variable g_Timer (KeInitializeTimer())
    KeInitializeTimer(&g_Timer);

    // Step #2 : Initialize the global variable g_Dpc, (KeInitializeDpc())
    KeInitializeDpc(&g_Dpc, TimerDpcRoutine, NULL);

    // Step #3 : Initialize the variable DueTime to TIME_INTERVAL_IN_SEC 
    DueTime.QuadPart = (-1 * TIME_INTERVAL_IN_SEC * 1000 * 1000 * 10);

    // Step #4 : Initialize the variable Period to TIME_INTERVAL_IN_SEC 
    Period = TIME_INTERVAL_IN_SEC * 1000;

    // Step #5 : Get the current time and store it in the global variable RequestTime (KeQuerySystemTime())
    KeQuerySystemTime(&RequestTime);

    // Step #6 : Set the Timer to fire once after DueTime and periodically after Period (KeSetTimerEx())

    DbgPrint ( "%s PID=%p TID=%p Set Timer %u seconds\n", __FUNCTION__,
        PsGetCurrentProcessId(), PsGetCurrentThreadId(),
        TIME_INTERVAL_IN_SEC );

    KeSetTimerEx(&g_Timer, DueTime, Period, &g_Dpc);

    return STATUS_SUCCESS;
} // DriverEntry()

VOID 
DriverUnload (
	PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    // Step #7 : Cancel any running timers (KeCancelTimer())
    KeCancelTimer(&g_Timer);

} // DriverUnload()

VOID
TimerDpcRoutine (
    struct _KDPC  *Dpc,
    PVOID  DeferredContext,
    PVOID  SystemArgument1,
    PVOID  SystemArgument2 )
{
    LARGE_INTEGER FireTime;
    LARGE_INTEGER DiffTime;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // Step #8 : Get the current time and store it in the variable FireTime (KeQuerySystemTime())
    KeQuerySystemTime(&FireTime);

    // 100 nS to mSec
    DiffTime.QuadPart = ( ( FireTime.QuadPart - RequestTime.QuadPart ) / ( 10 * 1000 ) );

    // Step #9 : Get the current time and store it in the global variable RequestTime (KeQuerySystemTime())
    KeQuerySystemTime(&RequestTime);

    DbgPrint ( "%s PID=%p TID=%p Interval=%I64u ms\n", __FUNCTION__,
        PsGetCurrentProcessId(), PsGetCurrentThreadId(),
        DiffTime.QuadPart );
} // TimerDpcRoutine()
