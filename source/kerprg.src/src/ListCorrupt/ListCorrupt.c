/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"

#define __MODULE__  "ListCorrupt"
#define DPF(x)      DbgPrint x

LIST_ENTRY g_ListHead;
LIST_ENTRY g_ListNode;

VOID 
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{
    DPF(("---%s---\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(DriverObject);

    RemoveEntryList(&g_ListNode);
}

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    DPF(("+++%s+++\n", __MODULE__ ));

    UNREFERENCED_PARAMETER(RegistryPath);

    InitializeListHead(&g_ListHead);

    InsertTailList(&g_ListHead, &g_ListNode);

    DriverObject->DriverUnload = DriverUnload;

    DPF(("%s: g_ListHead=%p g_ListNode=%p\n", 
        __MODULE__, &g_ListHead, &g_ListNode ));

    return STATUS_SUCCESS;
}

