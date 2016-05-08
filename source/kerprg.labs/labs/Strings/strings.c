/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
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
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status = STATUS_SUCCESS;
    UNICODE_STRING ConcatString = {0};
    UNICODE_STRING ServiceString = {0};
    UNICODE_STRING HelloString = {0};
    PWCHAR ServiceStringTemp = NULL;

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // Step #1 : Extract the service name from RegistryPath by scanning backwards 
    // for the path seperator character L'\\' (wcsrchr()) and store the pointer in 
    // ServiceStringTemp.
    ServiceStringTemp = wcsrchr(RegistryPath->Buffer, L'\\');

    // Step #2 : Initialize the Unicode string ServiceString to the service name.(RtlInitUnicodeString()) 
    // skip over the leading '\\'
    RtlInitUnicodeString(&ServiceString, ServiceStringTemp + 1);

    // Step #3 : Initialize the Unicode string HelloString with the string "Hello ".(RtlInitUnicodeString()) 
    RtlInitUnicodeString(&HelloString, L"Hello ");

    // Step #4 : Allocate the buffer of ConcatString large enough to hold 
    // HelloString and ServiceString (ExAllocatePoolWithTag())
    RtlInitUnicodeString(&ConcatString, NULL);
    ConcatString.MaximumLength = ServiceString.Length + HelloString.Length + sizeof(WCHAR);
    ConcatString.Buffer = (PWCH)ExAllocatePoolWithTag(PagedPool, ConcatString.MaximumLength, 'oleh');

    // Step #5 : Copy the contents of HelloString to ConcatString (RtlCopyUnicodeString()).
    RtlCopyUnicodeString(&ConcatString, &HelloString);

    // Step #6 : Append ServiceName to ConcatString (RtlAppendUnicodeStringToString())
    RtlAppendUnicodeStringToString(&ConcatString, &ServiceString);

    DbgPrint ( "%wZ\n", &ConcatString );

    goto Exit;
Exit :
    // Step #7 : Free the ConcatString buffer (ExFreePool())
    ExFreePool(ConcatString.Buffer);

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );
} // DriverUnload()

