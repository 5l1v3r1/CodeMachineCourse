/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define POOL_NX_OPTIN   1
#include "ntifs.h"

#define __MODULE__  "NxPool"
#define DPF(x)      DbgPrint x

PUCHAR g_PoolBlock = NULL;

VOID 
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{
    DPF(("---%s---\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(DriverObject);

    if (g_PoolBlock) {
        ExFreePool(g_PoolBlock);
        g_PoolBlock = NULL;
    }
}

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    DPF(("+++%s+++\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(RegistryPath);

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    DriverObject->DriverUnload = DriverUnload;

    g_PoolBlock = ExAllocatePoolWithTag(NonPagedPool, 100, 'tpMC');
    if (!g_PoolBlock) {
        DPF(("%s: ExAllocatePoolWithTag() FAIL\n", __MODULE__  ));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DPF(("%s: g_PoolBlock=%p\n", __MODULE__, g_PoolBlock ));

    return STATUS_SUCCESS;
}

