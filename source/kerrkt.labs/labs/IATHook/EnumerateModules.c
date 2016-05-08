/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

#define __MODULE__  "EnumereateModule"
#define DPF(x)      DbgPrint x

NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation (
    ULONG  SystemInformationClass,
    OUT PVOID  SystemInformation,
    IN ULONG  Length,
    OUT PULONG 	ResultLength );

#define SystemModuleInformation     11

typedef struct _SYSTEM_MODULE_INFORMATION_ENTRY
{
    PVOID   Section;
    PVOID   MappedBase;
    PVOID   ModuleBaseAddress;
    ULONG   Size;
    ULONG   Flags;
    USHORT  Index;
    USHORT  NameLength;
    USHORT  LoadCount;
    USHORT  ModuleNameOffset;
    CHAR  ModuleName[256];
} SYSTEM_MODULE_INFORMATION_ENTRY, *PSYSTEM_MODULE_INFORMATION_ENTRY;

typedef struct _SYSTEM_MODULE_INFORMATION
{
    ULONG Count;
    SYSTEM_MODULE_INFORMATION_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, *PSYSTEM_MODULE_INFORMATION;


typedef BOOLEAN (*PENUM_MODULE_CALLBACK) (
    PVOID Context,
    PVOID ModuleBase,
    ULONG ModuleSize,
    PCHAR ModuleName ); // will be NULL terminated

NTSTATUS 
EnumerateModules ( 
    PENUM_MODULE_CALLBACK Callback,
    PVOID Context )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVOID ModuleBase = NULL;
    PULONG SystemInfoBuffer = NULL;
    PSYSTEM_MODULE_INFORMATION_ENTRY SysModuleEntry;
    ULONG SystemInfoBufferSize = 0;
    ULONG i;

    __try
    {
        Status = ZwQuerySystemInformation(
            SystemModuleInformation,
            &SystemInfoBufferSize,
            0,
            &SystemInfoBufferSize);

        if (!SystemInfoBufferSize) {
            goto  Exit;
        }

        SystemInfoBuffer = (PULONG)ExAllocatePoolWithTag(
            NonPagedPool, 
            SystemInfoBufferSize*2, '00MC');

        if (!SystemInfoBuffer) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto  Exit;
        }

        memset(SystemInfoBuffer, 0, SystemInfoBufferSize*2);

        Status = ZwQuerySystemInformation(
            SystemModuleInformation,
            SystemInfoBuffer,
            SystemInfoBufferSize*2,
            &SystemInfoBufferSize);

        if (!NT_SUCCESS(Status)) {
            goto  Exit;
        }

        DPF( ("[%u] Modules\n", ((PSYSTEM_MODULE_INFORMATION)(SystemInfoBuffer))->Count ));

        SysModuleEntry = ((PSYSTEM_MODULE_INFORMATION)(SystemInfoBuffer))->Module;

        for (i = 0; i <((PSYSTEM_MODULE_INFORMATION)(SystemInfoBuffer))->Count; i++) {
            
            if ( Callback (
                Context, 
                SysModuleEntry[i].ModuleBaseAddress,
                SysModuleEntry[i].Size,
                SysModuleEntry[i].ModuleName + SysModuleEntry[i].ModuleNameOffset ) == TRUE ) {
                    // if the callback returned TRUE, it found what it needed, so abort
                    break;
                }

            //DPF(("%p [%s]\n", 
            //    SysModuleEntry[i].ModuleBaseAddress,
            //    SysModuleEntry[i].ModuleName + SysModuleEntry[i].ModuleNameOffset ));
        }

        Status = STATUS_SUCCESS;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        ModuleBase = NULL;
        Status = STATUS_UNSUCCESSFUL;
    }

Exit :
    if(SystemInfoBuffer) {
        ExFreePool(SystemInfoBuffer);
    }

    return Status;
}


typedef struct _GET_MODULE_BYNAME_DATA {
    PCHAR ModuleName;
    PUCHAR ModuleBase;
    ULONG ModuleSize;
} GET_MODULE_BYNAME_DATA, *PGET_MODULE_BYNAME_DATA;

BOOLEAN GetModuleBaseCallback ( 
    PVOID Context, 
    PVOID ModuleBase, 
    ULONG ModuleSize, 
    PCHAR ModuleName )
{
    PGET_MODULE_BYNAME_DATA Data = Context;

    if (  _stricmp ( Data->ModuleName, ModuleName ) == 0 ) {
        Data->ModuleBase = ModuleBase;
        Data->ModuleSize = ModuleSize;
        return TRUE;
    }

    return FALSE;
}


NTSTATUS 
GetModuleBaseByName ( 
    PCHAR ModuleName,
    PUCHAR *ModuleBase ) 
{
    NTSTATUS Status;
    GET_MODULE_BYNAME_DATA Data = {0};

    Data.ModuleName = ModuleName;

    Status = EnumerateModules ( GetModuleBaseCallback, &Data );

    if ( ! NT_SUCCESS ( Status ) ) {
        return Status;
    }

    *ModuleBase = Data.ModuleBase;

    return STATUS_SUCCESS;
}