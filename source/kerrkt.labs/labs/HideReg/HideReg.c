/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntifs.h"
#include "stdio.h"

#define __MODULE__  "HideReg"
#define DPF(x)      DbgPrint x

#pragma warning(disable:4305)

// global context structure used to store driver wide information 
typedef struct _GLOBAL_CONTEXT {
    PDRIVER_OBJECT DriverObject;
    UNICODE_STRING Altitude;
    LARGE_INTEGER Cookie;
} GLOBAL_CONTEXT, *PGLOBAL_CONTEXT;

PCHAR PsGetProcessImageFileName(
    PEPROCESS Process);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject);

NTSTATUS
RegistryCallback(
    IN PVOID  CallbackContext,
    IN PVOID  Argument1,
    IN PVOID  Argument2);

BOOLEAN
CheckPolicy(
    PUNICODE_STRING KeyFullPath);

BOOLEAN
CheckProcess(
    VOID);

BOOLEAN
CheckKeys(
    PVOID RootObject,
    PUNICODE_STRING CompleteName);

GLOBAL_CONTEXT  g_GlobalContext = { 0 };

// Initialize the g_PolicyKeyArray for :
// CurrentControlSet and ControlSet001 - ControlSet003
// Set g_PolicyKeyCount appropriately
// HKLM\System\CurrentControlSet\Services\HideReg
UNICODE_STRING g_PolicyKeyArray[] = {
    RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\CurrentControlSet\\Services\\HideReg"),
    RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\ControlSet001\\Services\\HideReg"),
    RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\ControlSet002\\Services\\HideReg"),
    RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\ControlSet003\\Services\\HideReg")
};
ULONG g_PolicyKeyCount = sizeof(g_PolicyKeyArray) / sizeof(UNICODE_STRING);

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DPF(("*** %s.sys Loaded ***\n", __MODULE__));

    DriverObject->DriverUnload = DriverUnload;

    RtlZeroMemory(&g_GlobalContext, sizeof(GLOBAL_CONTEXT));

    g_GlobalContext.DriverObject = DriverObject;

    // Initialize the _GlobalContext.Altitude
    RtlInitUnicodeString(&g_GlobalContext.Altitude, L"");

    // Step #1 : Register RegistryCallback() as the registry access callback 
    // function using the information in g_GlobalContext and pass a pointer to
    // g_GlobalContext as the Context (CmRegisterCallbackEx())
    if (!NT_SUCCESS(Status = CmRegisterCallbackEx(RegistryCallback, &g_GlobalContext.Altitude, DriverObject, &g_GlobalContext, &g_GlobalContext.Cookie, NULL)))
    {
        DbgPrint("ERROR CmRegisterCallbackEx (%x)\n", Status);
        goto Exit;
    }

    return STATUS_SUCCESS;

Exit:
    return Status;
} // DriverEntry()


NTSTATUS
RegistryCallback(
    IN PVOID  CallbackContext,
    IN PVOID  Argument1,
    IN PVOID  Argument2)
{
    NTSTATUS Status = STATUS_SUCCESS;
    REG_NOTIFY_CLASS RegNotifyClass = (REG_NOTIFY_CLASS)Argument1;

    UNREFERENCED_PARAMETER(CallbackContext);

    // if the calling process matches the bypass list then allow the 
    // operation
    if (CheckProcess()) {
        return STATUS_SUCCESS;
    }

    // Step #2 : Check for registry key pre-create (RegNtPreCreateKeyEx) and 
    // pre-open (RegNtPreOpenKeyEx) operations
    // For these operations extract the RootObject and the CompleteName from the
    // appropriate structures and pass them to the function CheckKeys() to 
    // determine if access should be allowed (STATUS_SUCCESS) or 
    // denied (STATUS_ACCESS_DENIED).
    if (RegNtPreCreateKeyEx == RegNotifyClass || RegNtPreOpenKeyEx == RegNotifyClass)
    {
        //PREG_CREATE_KEY_INFORMATION == PREG_OPEN_KEY_INFORMATION
        PREG_CREATE_KEY_INFORMATION RegInformation = (PREG_CREATE_KEY_INFORMATION)Argument2;

        if (CheckKeys(RegInformation->RootObject, RegInformation->CompleteName))
        {
            Status = STATUS_ACCESS_DENIED; //Key matched, so deny access
        }
    }
    //else if (RegNtPreEnumerateKey == RegNotifyClass)
    //{
    //    PREG_ENUMERATE_KEY_INFORMATION RegInformation = (PREG_ENUMERATE_KEY_INFORMATION)Argument2;

    //    //DbgPrint("HIDEREG: Enumerate Key\n");

    //    //DbgPrint("Enumerate Info:\n");
    //    //DbgPrint("  Idx: %d\n", RegInformation->Index);
    //    //DbgPrint("  KeyInformationClass: %d\n", RegInformation->KeyInformationClass);
    //    if (KeyBasicInformation == RegInformation->KeyInformationClass)
    //    {
    //        PKEY_BASIC_INFORMATION KeyInformation = RegInformation->KeyInformation;
    //        DbgPrint("[LAB]: Key Name: %.*S\n", (KeyInformation->NameLength/2), KeyInformation->Name);
    //    }
    //}
    
    return Status;
} // RegistryCallback()



BOOLEAN
CheckKeys(
    PVOID RootObject,
    PUNICODE_STRING CompleteName)
{
    PUNICODE_STRING RootObjectName;
    ULONG_PTR RootObjectID;
    BOOLEAN Matched = FALSE;
    NTSTATUS Status;
    UNICODE_STRING KeyPath = { 0 };

    if (RootObject) {

        // Step #3 : Translate the RootObject to a registry path and store it in 
        // RootObjectName (CmCallbackGetKeyObjectID())
        if (!NT_SUCCESS(Status = CmCallbackGetKeyObjectID(&g_GlobalContext.Cookie, RootObject, &RootObjectID, &RootObjectName)))
        {
            DbgPrint("ERROR CmCallbackGetKeyObjectID (%x)\n", Status);
            goto Exit;
        }

        // If the CompleteName contains a valid unicode string then join the RootObjectName
        // and the CompleteName to create the full path string in KeyPath 
        if (CompleteName->Length && CompleteName->Buffer) {

            KeyPath.MaximumLength = RootObjectName->Length + CompleteName->Length + (sizeof(WCHAR) * 2);

            KeyPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, KeyPath.MaximumLength, 'pkMC');

            if (!KeyPath.Buffer) {
                DPF(("ExAllocatePool() FAIL\n"));
                goto Exit;
            }

            swprintf(KeyPath.Buffer, L"%wZ\\%wZ", RootObjectName, CompleteName);
            KeyPath.Length = RootObjectName->Length + CompleteName->Length + (sizeof(WCHAR));

            // Call CheckPolicy() with the full path to the registry key to determine
            // is it matches any of the keys under policy
            Matched = CheckPolicy(&KeyPath);
        }
        else {
            Matched = CheckPolicy(RootObjectName);

        }
    }
    else {
        // If the RootObjectName is not valid then just use the CompleteName as the full path
        Matched = CheckPolicy(CompleteName);
    }

Exit:
    // if a buffer was allocated in KeyPath.Buffer then free it 
    if (KeyPath.Buffer) {
        ExFreePool(KeyPath.Buffer);
    }
    return Matched;
} //CheckKeys

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(DriverObject);

    // Step #4 : Unregister the registry callback function using the cookie 
    // that was returned in g_GlobalContext.Cookie during registration
    // (CmUnRegisterCallback()
    if (!NT_SUCCESS(Status = CmUnRegisterCallback(g_GlobalContext.Cookie)))
    {
        DbgPrint("ERROR CmUnRegisterCallback (%x)\n", Status);
    }

    DPF(("*** %s.sys Unloaded ***\n", __MODULE__));
} // DriverUnload()

// check if the calling process meets our bypass policy
BOOLEAN
CheckProcess(
    VOID)
{
    PEPROCESS  Process;
    PCHAR ImageFileName;

    // get a pointer to the EPROCSS structure of the calling process
    Process = PsGetCurrentProcess();

    // get the name of the process
    ImageFileName = PsGetProcessImageFileName(Process);

    // if the process is services.exe or svchost.exe
    // then allow it to access the keys
    // this is required to load and unload the driver
    if (_stricmp(ImageFileName, "services.exe") == 0) {
        return TRUE;
    }

    if (_stricmp(ImageFileName, "svchost.exe") == 0) {
        return TRUE;
    }

    return FALSE;
} // CheckProcess

// check if the given full registry key path is in policy
BOOLEAN
CheckPolicy(
    PUNICODE_STRING KeyFullPath)
{
    BOOLEAN Matched = FALSE;
    ULONG Idx;

    // Iterate through all the ControlSets and check if the given 
    // full registry path matches any of them
    for (Idx = 0; Idx < g_PolicyKeyCount; Idx++) {
        if (RtlEqualUnicodeString(KeyFullPath, &g_PolicyKeyArray[Idx], TRUE)) {
            Matched = TRUE;
            break;
        }
    }

    if (Matched) {
        DPF(("[%x.%x] BLOCK %wZ\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            KeyFullPath));
    }

    return Matched;
} // CheckPolicy
