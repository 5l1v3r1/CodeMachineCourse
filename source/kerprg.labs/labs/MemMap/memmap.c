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
    PUNICODE_STRING RegistryPath);

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif

BOOLEAN
MapMemory(
    PVOID Address,
    ULONG Length,
    PVOID* MappedAddressPtr,
    PMDL* MdlPtr);

VOID UnmapMemory(
    PVOID Address,
    PMDL Mdl);

PUCHAR g_MappedAddress = NULL;
PMDL g_MappedMdl = NULL;
PCHAR g_String = "Hi!";

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    PUCHAR DriverStart;
    ULONG DriverSize;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    DriverObject->DriverUnload = DriverUnload;

    DriverStart = DriverObject->DriverStart;
    DriverSize = DriverObject->DriverSize;

    DbgPrint("%s DriverStart=%p DriverSize=%x\n", __FUNCTION__, DriverStart, DriverSize);

    // map the entire driver image with read write attributes
    if (!MapMemory(
        DriverStart,
        DriverSize,
        &g_MappedAddress,
        &g_MappedMdl)) {

        Status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    DbgPrint("%s MappedAddress=%p MappedMdl=%p\n", __FUNCTION__,
        g_MappedAddress,
        g_MappedMdl);

    DbgPrint("%s (1) String=[%s]\n", __FUNCTION__,
        g_String);

    // change the Hi! to a Bye
    memcpy(g_MappedAddress + ((PUCHAR)g_String - DriverStart), "Bye", 3);

    DbgPrint("%s (2) String=[%s]\n", __FUNCTION__,
        g_String);

    Status = STATUS_SUCCESS;

Exit:
    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject)
{
    DbgPrint("%s DriverObject=%p\n", __FUNCTION__, DriverObject);

    // unmap the memory that was mapped
    UnmapMemory(g_MappedAddress, g_MappedMdl);
} // DriverUnload()

BOOLEAN
MapMemory(
    PUCHAR Address,
    ULONG Length,
    PVOID* MappedAddressPtr,
    PMDL* MdlPtr)
{
    PMDL Mdl = NULL;
    PUCHAR MappedAddress = NULL;

    // Step #1 : Allocate a MDL for Address and Length (IoAllocateMdl())
    Mdl = IoAllocateMdl(Address, Length, FALSE, FALSE, NULL);
    if (NULL == Mdl)
    {
        goto Exit;
    }

    __try
    {
        // Step #2 : Lock the pages in the MDL with IoModifyAccess (MmProbeAndLockPages())
        // make sure you use a __try/__except
        MmProbeAndLockPages(Mdl, KernelMode, IoModifyAccess);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("ERROR: Stuff went wrong\n");
        goto Exit;
    }

    // Step #3 : Map the locked pages to a Kernel VA (MmMapLockedPagesSpecifyCache())
    if (NULL == (MappedAddress = MmMapLockedPagesSpecifyCache(Mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority)))
    {
        DbgPrint("ERROR: MmMapLockedPages - Out of resources\n");
        goto Exit;
    }

    // return the mapped address and the MDL to the caller
    *MappedAddressPtr = MappedAddress;
    *MdlPtr = Mdl;

    return TRUE;

Exit:
    // Step #4 : Unmap the locked pages (MmUnmapLockedPages())
    if (MappedAddress != NULL && Mdl->MdlFlags & MDL_PAGES_LOCKED)
    {
        MmUnmapLockedPages(MappedAddress, Mdl);
    }

    // Step #5 : Unlock the pages (MmUnlockPages())
    MmUnlockPages(Mdl);

    // Step #6 : Free the MDL (IoFreeMdl())
    IoFreeMdl(Mdl);
    *MappedAddressPtr = NULL;
    *MdlPtr = NULL;

    return FALSE;
} // MapMemory()

VOID UnmapMemory(
    PUCHAR Address,
    PMDL Mdl)
{
    // Step #7 : Unmap the locked pages (MmUnmapLockedPages())
    if (Address != NULL && Mdl->MdlFlags & MDL_PAGES_LOCKED)
    {
        MmUnmapLockedPages(Address, Mdl);
    }

    // Step #8 : Unlock the pages (MmUnlockPages())
    MmUnlockPages(Mdl);

    // Step #9 : Free the MDL (IoFreeMdl())
    IoFreeMdl(Mdl);
} // UnmapMemory()
