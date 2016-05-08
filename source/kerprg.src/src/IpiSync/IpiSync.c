/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////
#include "ntifs.h"

#define __MODULE__  "IpiSync"
#define DPF(x)      DbgPrint x

#define GROUP_MAX 4

typedef struct _GLOBAL {
    KAFFINITY   Affinity;
    ULONG       GroupCount;
    KAFFINITY   GroupAffinify[GROUP_MAX];

    ULONG       IpiCounter;
    ULONG       MasterDetected;

} GLOBAL, *PGLOBAL;


GLOBAL g_Context = {0};

ULONG_PTR 
IpiCallback( 
    ULONG_PTR PtrContext )
{
    PGLOBAL Context = (PGLOBAL)PtrContext;

    DPF(( "%s CPU=%u IpiCounter=%u\n", __FUNCTION__, 
        KeGetCurrentProcessorNumber(), Context->IpiCounter ));

    if ( InterlockedIncrement( (LONG *)&Context->IpiCounter) == 1 ) {
        Context->MasterDetected++;
    }

    return  Context->MasterDetected;
}

VOID 
IpiInit( 
    PGLOBAL Context )
{
    USHORT GroupIdx;
    ULONG_PTR IpiReturn;

    Context->Affinity =  KeQueryActiveProcessors();

    DPF(("KeQueryActiveProcessors() = %I64x\n", Context->Affinity ));

    Context->GroupCount = KeQueryActiveGroupCount();

    DPF(("KeQueryActiveGroupCount() = %I64x\n", Context->Affinity ));

    for ( GroupIdx = 0 ; ( GroupIdx < Context->GroupCount ) && ( GroupIdx < GROUP_MAX ) ; GroupIdx++ ) {
        Context->GroupAffinify[GroupIdx] = KeQueryGroupAffinity ( GroupIdx );
        DPF(("KeQueryGroupAffinity(%u) = %I64x\n", GroupIdx, Context->GroupAffinify[GroupIdx] ));
    }

    IpiReturn = KeIpiGenericCall( IpiCallback, (ULONG_PTR)&g_Context );

    DPF(("IpiReturn=%I64x\n", IpiReturn ));
    DPF(("IpiCounter=%08x\n", Context->IpiCounter ));
    DPF(("MasterDetected=%08x\n", Context->MasterDetected ));
}

VOID 
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{
    DPF(("---%s---\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    DPF(("+++%s+++\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload  = DriverUnload;
    IpiInit ( &g_Context );

    return STATUS_SUCCESS;
}

