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

NTSTATUS
TrackDriverLoad (
    PUNICODE_STRING RegistryPath );

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // call the function that will track driver loads
    Status = TrackDriverLoad ( RegistryPath );

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );
} // DriverUnload()

NTSTATUS
TrackDriverLoad (
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status = STATUS_INSUFFICIENT_RESOURCES;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG Disposition;
    ULONG BytesXfered;
    UNICODE_STRING ValueName;
    UNICODE_STRING Parameters = {0};
    HANDLE RegistryHandle = NULL;
    HANDLE ParametersHandle = NULL;
    UCHAR PartialInformation[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
    ULONG LoadCount = 0;

    // Step #1 : Setup the OBJECT_ATTRIBUTES (ObjectAttributes) structure with RegistryPath (InitializeObjectAttributes())
    // For attributes specify case-insensitive and the handle accessible from kernel mode only
    InitializeObjectAttributes(&ObjectAttributes, RegistryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    // Step #2 : Obtain a handle to RegistryPath (ZWCreateKey())
    if (!NT_SUCCESS(Status = ZwCreateKey(&RegistryHandle, KEY_READ | KEY_WRITE , &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Disposition)))
    {
        DbgPrint("ERROR ZwCreateKey (%x)\n", Status);
        goto Exit;
    }

    // Step #3 : Initialize a Unciode string with the string "Parameters".(RtlInitUnicodeString()) 
    RtlInitUnicodeString(&Parameters, L"Parameters");

    // Step #4 : Setup the OBJECT_ATTRIBUTES (ObjectAttributes) structure with Parameters and 
    // the handle to the RegistryPath to perform a relative open (InitializeObjectAttributes())
    // For attributes specify case-insensitive and the handle accessible from kernel mode only
    InitializeObjectAttributes(&ObjectAttributes, &Parameters, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, RegistryHandle, NULL);

    // Step #5 : Obtain a handle to Paremeters sub-key (ZWCreateKey())
    if (!NT_SUCCESS(Status = ZwCreateKey(&ParametersHandle, KEY_READ | KEY_WRITE, &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Disposition)))
    {
        DbgPrint("ERROR ZwCreateKey (%x)\n", Status);
        goto Exit;
    }

    // Step #6 : Initialize a Unciode string with the string "LoadCount".(RtlInitUnicodeString()) 
    RtlInitUnicodeString(&ValueName, L"LoadCount");

    // Step #7 : Read the current setting of the value “LoadCount” into the PartialInformation buffer. (ZwQueryValueKey())
    if (NT_SUCCESS(Status = ZwQueryValueKey(ParametersHandle, &ValueName, KeyValuePartialInformation, (PVOID)PartialInformation, sizeof(PartialInformation), &BytesXfered)))
    {
        // Step #8 : If the above call succeeded then 
        // verify the amount of data read ((PKEY_VALUE_PARTIAL_INFORMATION)PartialInformation)->DataLength) 
        // and read the value from ((PKEY_VALUE_PARTIAL_INFORMATION)PartialInformation)->Data) 
        // into the variable LoadCount and increment it
        if (sizeof(ULONG) == ((PKEY_VALUE_PARTIAL_INFORMATION)PartialInformation)->DataLength)
        {
            RtlCopyMemory(&LoadCount, ((PKEY_VALUE_PARTIAL_INFORMATION)PartialInformation)->Data, sizeof(LoadCount));
            LoadCount++;
        }
    }

    // Step #9 :Write back the updated value of LoadCount to the registry (ZwSetValueKey())
    if (!NT_SUCCESS(Status = ZwSetValueKey(ParametersHandle, &ValueName, 0, REG_DWORD, (PVOID)&LoadCount, sizeof(LoadCount))))
    {
        DbgPrint("ERROR ZwSetValueKey (%d)\n", Status);
        goto Exit;
    }

    DbgPrint ( "%s LoadCount=%u\n", __FUNCTION__, LoadCount );
    
Exit :
    // Step #10 :Close the handle to the registry path (ZwClose())
    ZwClose(RegistryHandle);

    // Step #11 :Close the handle to the parameters key (ZwClose())
    ZwClose(ParametersHandle);

    return Status;
} // TrackDriverLoad()
