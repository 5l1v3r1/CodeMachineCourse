/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2016 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#include "ntddk.h"

#pragma warning(disable:4311)

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

NTSTATUS RegisterCallback(
    PCALLBACK_FUNCTION  CallbackFunction,
    PVOID CallbackContext,
    PCALLBACK_OBJECT*  CallbackObject,
    PVOID* CallbackRegistration );

VOID DeregisterCallback(
    PCALLBACK_OBJECT  CallbackObject,
    PVOID CallbackRegistration );

VOID
PowerStateCallback(
    PVOID Context,
    PVOID Argument1,
    PVOID Argument2 );

PCALLBACK_OBJECT  g_CallbackObject = NULL;
PVOID g_CallbackRegistration = NULL;

NTSTATUS 
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    DriverObject->DriverUnload = DriverUnload;

    // call the function to register the power callback
    Status = RegisterCallback (
        PowerStateCallback,
        DriverObject,
        &g_CallbackObject,
        &g_CallbackRegistration );

    if ( ! NT_SUCCESS(Status) ) {
        DbgPrint("%s RegisterCallback() FAIL\n", __FUNCTION__);
    } else {
        DbgPrint("%s g_CallbackObject=%p g_CallbackRegistration=%p\n", __FUNCTION__,
            g_CallbackObject, g_CallbackRegistration);
    }

    return Status;
} // DriverEntry()

VOID 
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DbgPrint ( "%s DriverObject=%p\n", __FUNCTION__, DriverObject );

    // call the function to deregister the power callback
    DeregisterCallback(
        g_CallbackObject,
        g_CallbackRegistration );
} // DriverUnload()

VOID
PowerStateCallback(
    PVOID Context,
    PVOID Argument1,
    PVOID Argument2 )
{
    UNREFERENCED_PARAMETER(Context);

    if ( ((ULONG)Argument1) == PO_CB_SYSTEM_STATE_LOCK ) {
        if ( ((ULONG)Argument2) == 1 ) {
            DbgPrint ("%s: Power Up\n", __FUNCTION__ );
        } else {
            DbgPrint ("%s: Shutdown or Standby\n", __FUNCTION__ );
        }
    }
} // PowerStateCallback()


NTSTATUS RegisterCallback(
    PCALLBACK_FUNCTION  CallbackFunction,
    PVOID CallbackContext,
    PCALLBACK_OBJECT*  CallbackObject,
    PVOID* CallbackRegistration )
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING String;
    NTSTATUS Status;
    PCALLBACK_OBJECT CallbackObjectTemp;
    PVOID* CallbackRegistrationTemp;

    // Step #1 : Initialize String with \Callback\PowerState (RtlInitUnicodeString()) 
    RtlInitUnicodeString(&String, L"\\Callback\\PowerState");

    // Step #2 : Initialize OBJECT_ATTRIBUTES with String (InitializeObjectAttributes()) 
    InitializeObjectAttributes(&ObjectAttributes, &String, OBJ_CASE_INSENSITIVE, NULL, NULL);

    // Step #3 : Create the callback (ExCreateCallback()) 
    if (!NT_SUCCESS(Status = ExCreateCallback(&CallbackObjectTemp, &ObjectAttributes, FALSE, TRUE)))
    {
        DbgPrint("ERROR ExCreateCallback (%d)\n", Status);
        goto Exit;
    }

    // Step #4 : Register the callback (ExRegisterCallback()) 
    if (NULL == (CallbackRegistrationTemp = ExRegisterCallback(CallbackObjectTemp, CallbackFunction, CallbackContext)))
    {
        DbgPrint("ERROR ExRegisterCallback\n");
        goto Exit;
    }

    *CallbackObject = CallbackObjectTemp;
    *CallbackRegistration = CallbackRegistrationTemp;
    return STATUS_SUCCESS;

Exit :
    // Step #5 : Dereference the callback object (ObDereferenceObject()) 
    if (NULL != CallbackObjectTemp)
    {
        ObDereferenceObject(CallbackObject);
    }

    return Status;
} // RegisterCallback()

VOID DeregisterCallback(
    PCALLBACK_OBJECT  CallbackObject,
    PVOID CallbackRegistration )
{
    // Step #6 : Deregister the callback (ExUnregisterCallback()) 
    ExUnregisterCallback(CallbackRegistration);

    // Step #7 : Dereference the callback object (ObDereferenceObject()) 
    ObDereferenceObject(CallbackObject);
} // DeregisterCallback()
