/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define INITGUID
#include <ndis.h>

//<defines>
#define __MODULE__ "NdisLwf"
#define DPF(x)	DbgPrint x

#define NDISLWF_NDIS_MAJOR_VERSION   6
#define NDISLWF_NDIS_MINOR_VERSION   0

#define NDISLWF_DRIVER_MAJOR_VERSION   1
#define NDISLWF_DRIVER_MINOR_VERSION   0

#define NDISLWF_FRIENDLY_NAME        L"CodeMachine NDIS LightWeight Filter"
#define NDISLWF_UNIQUE_NAME          L"{5cbf81bd-5055-47cd-9055-a76b2b4e3697}" // must match GUID in .INF
#define NDISLWF_SERVICE_NAME         L"NdisLwf" // must match service name

// global context
typedef struct _NDISLWF_GLOBAL
{
    NDIS_HANDLE         FilterDriverHandle; // NDIS handle for filter driver
} NDISLWF_GLOBAL, *PNDISLWF_GLOBAL;

// per adapter context
typedef struct _NDISLWF_CONTEXT
{
    NDIS_HANDLE                     FilterHandle;
    NET_IFINDEX                     MiniportIfIndex;
} NDISLWF_CONTEXT, *PNDISLWF_CONTEXT;


NDISLWF_GLOBAL	GlobalNDISLWF;

// implemented in NblChain.c
BOOLEAN 
ProcessNblChain ( 
    PNET_BUFFER_LIST  NetBufferLists );

VOID
DriverUnload (
    PDRIVER_OBJECT DriverObject );

NDIS_STATUS
NDISLWF_AttachHandler(
    NDIS_HANDLE NdisFilterHandle,
    NDIS_HANDLE  FilterDriverContext,
    PNDIS_FILTER_ATTACH_PARAMETERS AttachParameters );

VOID
NDISLWF_DetachHandler(
    NDIS_HANDLE FilterModuleContext );

NDIS_STATUS
NDISLWF_PauseHandler (
    NDIS_HANDLE                     FilterModuleContext,
    PNDIS_FILTER_PAUSE_PARAMETERS   PauseParameters );

NDIS_STATUS
NDISLWF_RestartHandler (
    NDIS_HANDLE                     FilterModuleContext,
    PNDIS_FILTER_RESTART_PARAMETERS RestartParameters );

VOID
NDISLWF_StatusHandler (
    NDIS_HANDLE FilterModuleContext,
    PNDIS_STATUS_INDICATION StatusIndication );

VOID
NDISLWF_ReceiveNetBufferListsHandler (
    NDIS_HANDLE         FilterModuleContext,
    PNET_BUFFER_LIST    NetBufferLists,
    NDIS_PORT_NUMBER    PortNumber,
    ULONG               NumberOfNetBufferLists,
    ULONG               ReceiveFlags );

VOID
NDISLWF_ReturnNetBufferListsHandler (
    NDIS_HANDLE         FilterModuleContext,
    PNET_BUFFER_LIST    NetBufferLists,
    ULONG               ReturnFlags );

NDIS_STATUS 
DriverEntry(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PUNICODE_STRING     RegistryPath )
{
    NDIS_FILTER_DRIVER_CHARACTERISTICS  FilterCharacteristics;
    NDIS_STATUS NdisStatus;

    DPF(("%s!%s DriverObject=%p RegistryPath=%wZ\n", __MODULE__, __FUNCTION__, 
        DriverObject, RegistryPath )); 

    DriverObject->DriverUnload = DriverUnload;

    GlobalNDISLWF.FilterDriverHandle = NULL;

    // Setup the FilterCharacteristics structure
    // Handlers implemented by the driver are :
    // AttachHandler, DetachHandler, RestartHandler, PauseHandler, StatusHandler
    // ReceiveNetBufferListsHandler, ReturnNetBufferListsHandler
    NdisZeroMemory(&FilterCharacteristics, sizeof(NDIS_FILTER_DRIVER_CHARACTERISTICS));

    FilterCharacteristics.Header.Type = NDIS_OBJECT_TYPE_FILTER_DRIVER_CHARACTERISTICS;
    FilterCharacteristics.Header.Size = sizeof(NDIS_FILTER_DRIVER_CHARACTERISTICS);
    FilterCharacteristics.Header.Revision = NDIS_FILTER_CHARACTERISTICS_REVISION_1;
    FilterCharacteristics.MajorNdisVersion = NDISLWF_NDIS_MAJOR_VERSION;
    FilterCharacteristics.MinorNdisVersion = NDISLWF_NDIS_MINOR_VERSION;
    FilterCharacteristics.MajorDriverVersion = NDISLWF_DRIVER_MAJOR_VERSION;
    FilterCharacteristics.MinorDriverVersion = NDISLWF_DRIVER_MINOR_VERSION;
    FilterCharacteristics.Flags = 0;
    NdisInitUnicodeString ( &FilterCharacteristics.FriendlyName, NDISLWF_FRIENDLY_NAME );
    NdisInitUnicodeString ( &FilterCharacteristics.UniqueName, NDISLWF_UNIQUE_NAME);
    NdisInitUnicodeString ( &FilterCharacteristics.ServiceName, NDISLWF_SERVICE_NAME);
    FilterCharacteristics.AttachHandler = NDISLWF_AttachHandler;
    FilterCharacteristics.DetachHandler = NDISLWF_DetachHandler;
    FilterCharacteristics.RestartHandler = NDISLWF_RestartHandler;
    FilterCharacteristics.PauseHandler = NDISLWF_PauseHandler;
    FilterCharacteristics.StatusHandler = NDISLWF_StatusHandler;
    FilterCharacteristics.ReceiveNetBufferListsHandler = NDISLWF_ReceiveNetBufferListsHandler;
    FilterCharacteristics.ReturnNetBufferListsHandler = NDISLWF_ReturnNetBufferListsHandler;

    // Register the filter driver using the FilterCharacteristics structure
    // NdisFRegisterFilterDriver())
    NdisStatus = NdisFRegisterFilterDriver(
        DriverObject,
        (NDIS_HANDLE)DriverObject, // no driver-wide context
        &FilterCharacteristics, 
        &GlobalNDISLWF.FilterDriverHandle);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        DPF(("%s!%s NdisFRegisterFilterDriver() FAIL=%08x\n", __MODULE__, __FUNCTION__, NdisStatus));
        goto Exit1;
    }

    DPF(("%s!%s NdisFRegisterFilterDriver() FilterDriverHandle=%p\n", __MODULE__, __FUNCTION__, 
        GlobalNDISLWF.FilterDriverHandle )); 

Exit1 :
    return NdisStatus;
}

VOID
DriverUnload (
    PDRIVER_OBJECT DriverObject )
{
    DPF(("%s!%s DriverObject=%p\n", __MODULE__, __FUNCTION__, 
        DriverObject )); 

    // Deregister the filter driver (NdisFDeregisterFilterDriver())
    NdisFDeregisterFilterDriver(GlobalNDISLWF.FilterDriverHandle);

    DPF(("***** DRIVER UNLOADED *****\n")); 
}

NDIS_STATUS
NDISLWF_AttachHandler(
    NDIS_HANDLE NdisFilterHandle,
    NDIS_HANDLE  FilterDriverContext,
    PNDIS_FILTER_ATTACH_PARAMETERS AttachParameters )
{
    PNDISLWF_CONTEXT FilterContext;
    NDIS_STATUS NdisStatus;
    NDIS_FILTER_ATTRIBUTES FilterAttributes;

    DPF(("%s!%s [%x.%x] (FilterHandle=%p DriverContext=%p AttachParameters=%p)\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(), PsGetCurrentThreadId(), 
        NdisFilterHandle, FilterDriverContext, AttachParameters )); 

    // Allocate an instance of the NDISLWF_CONTEXT structure
    // Use the pool tag CMfc (NdisAllocateMemoryWithTagPriority()) and store
    // the context in FilterContext
    if ( ( FilterContext = NdisAllocateMemoryWithTagPriority (
        NdisFilterHandle,
        sizeof(NDISLWF_CONTEXT),
        'cfMC', //CodeMachineFilterContext
        NormalPoolPriority ) ) == NULL ) {
        
        NdisStatus = NDIS_STATUS_RESOURCES;
        DPF(("%s!%s NdisAllocateMemoryWithTagPriority() FAIL\n", __MODULE__, __FUNCTION__)); 
        goto Exit;
    }

    NdisZeroMemory(FilterContext, sizeof(NDISLWF_CONTEXT));
    FilterContext->FilterHandle = NdisFilterHandle;
    FilterContext->MiniportIfIndex = AttachParameters->BaseMiniportIfIndex;

    // Initialize the FilterAttribute structure
    NdisZeroMemory(&FilterAttributes, sizeof(NDIS_FILTER_ATTRIBUTES));
    FilterAttributes.Header.Revision = NDIS_FILTER_ATTRIBUTES_REVISION_1;
    FilterAttributes.Header.Size = sizeof(NDIS_FILTER_ATTRIBUTES);
    FilterAttributes.Header.Type = NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES;
    FilterAttributes.Flags = 0;

    // Register the filter attributes in FilterAttributes with NDIS
    // and register the FilterContext allocated above as the FilterModuleContext
    // (NdisFSetAttributes())
    NdisStatus = NdisFSetAttributes(
        NdisFilterHandle, 
        FilterContext,
        &FilterAttributes);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        DPF(("%s!%s NdisFSetAttributes() FAIL=%08x\n", __MODULE__, __FUNCTION__, NdisStatus)); 
        goto Exit;
    }

    return NdisStatus;

Exit :
    if ( FilterContext ) {
        // Free the filter context in FilterContext (NdisFreeMemory())
        NdisFreeMemory (
            FilterContext,
            sizeof(NDISLWF_CONTEXT),
            0 );
    }

    return NdisStatus;
} // NDISLWF_AttachHandler()

VOID
NDISLWF_DetachHandler(
    NDIS_HANDLE FilterModuleContext )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    // Free the filter context (NdisFreeMemory())
    NdisFreeMemory (
        FilterContext,
        sizeof(NDISLWF_CONTEXT),
        0 );
} // NDISLWF_DetachHandler()


NDIS_STATUS
NDISLWF_PauseHandler (
    NDIS_HANDLE                     FilterModuleContext,
    PNDIS_FILTER_PAUSE_PARAMETERS   PauseParameters )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    DPF(("%s!%s [%x.%x] (Context=%p PauseParameters=%p)\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(), PsGetCurrentThreadId(), 
        FilterContext, PauseParameters )); 

    return NDIS_STATUS_SUCCESS;
} // NDISLWF_PauseHandler()

NDIS_STATUS
NDISLWF_RestartHandler (
    NDIS_HANDLE                     FilterModuleContext,
    PNDIS_FILTER_RESTART_PARAMETERS RestartParameters )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    DPF(("%s!%s [%x.%x] (Context=%p RestartParameters=%p)\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(), PsGetCurrentThreadId(), 
        FilterContext, RestartParameters )); 

    return NDIS_STATUS_SUCCESS;
} //NDISLWF_RestartHandler()


VOID
NDISLWF_StatusHandler (
    NDIS_HANDLE FilterModuleContext,
    PNDIS_STATUS_INDICATION StatusIndication )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    DPF(("%s!%s [%x.%x] (Context=%p StatusIndication=%p)\n", __MODULE__, __FUNCTION__, 
        PsGetCurrentProcessId(), PsGetCurrentThreadId(), 
        FilterModuleContext, 
        StatusIndication )); 

    NdisFIndicateStatus(FilterContext->FilterHandle, StatusIndication);
} //NDISLWF_StatusHandler()

VOID
NDISLWF_ReceiveNetBufferListsHandler (
    NDIS_HANDLE         FilterModuleContext,
    PNET_BUFFER_LIST    NetBufferLists,
    NDIS_PORT_NUMBER    PortNumber,
    ULONG               NumberOfNetBufferLists,
    ULONG               ReceiveFlags )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    // process the NBL chain to determine if should be allowed or rejected
    if ( ProcessNblChain ( NetBufferLists ) ) {
        DPF(("%s!%s [%x.%x] NBL=%p BLOCKED\n", __MODULE__, __FUNCTION__, 
            PsGetCurrentProcessId(), PsGetCurrentThreadId(), NetBufferLists ));

        // Step #1 : Return the NBL chain to the caller instead of indicating it up to 
        // the driver above (NdisFReturnNetBufferLists())
        // ensure that the ReceiveFlags are properly translated to ReturnFlags

    } else {
        // Step #2 : Indicate the NBL chain to the driver above (NdisFIndicateReceiveNetBufferLists())

    }
} // NDISLWF_ReceiveNetBufferListsHandler()

VOID
NDISLWF_ReturnNetBufferListsHandler (
    NDIS_HANDLE         FilterModuleContext,
    PNET_BUFFER_LIST    NetBufferLists,
    ULONG               ReturnFlags )
{
    PNDISLWF_CONTEXT FilterContext = (PNDISLWF_CONTEXT)FilterModuleContext;

    // Step #3 : Return the NBL chain (NdisFReturnNetBufferLists()) to the driver
    // that indicated the NBL

} // NDISLWF_ReturnNetBufferListsHandler()
