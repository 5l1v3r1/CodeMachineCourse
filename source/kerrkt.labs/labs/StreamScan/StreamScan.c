/////////////////////////////////////////////////////////////////////////////
//
// (c) Copyright 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (http://www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define INITGUID
#define NDIS60	1 

#pragma warning (disable:4201)

#include "ntddk.h"
#include "ntstrsafe.h"
#include "fwpmk.h"
#include "fwpsk.h"
#include "netiodef.h"
#include <guiddef.h>

#define __MODULE__		"StreamScan"
#define DPF(x)			DbgPrint x


// {6EB8DF32-EB9E-42e2-8577-C60128D31B94}
DEFINE_GUID ( CODEMACHINE_STREAM_CALLOUT, 
    0x6eb8df32, 0xeb9e, 0x42e2, 0x85, 0x77, 0xc6, 0x1, 0x28, 0xd3, 0x1b, 0x94);

// {2EEA2747-7F79-4441-9378-8BF2086059AC}
DEFINE_GUID ( CODEMACHINE_STREAM_FILTER, 
    0x2eea2747, 0x7f79, 0x4441, 0x93, 0x78, 0x8b, 0xf2, 0x8, 0x60, 0x59, 0xac);

// {53804250-D9C8-4c59-92F7-3BFA5C91D20F}
DEFINE_GUID ( CODEMACHINE_STREAM_SUBLAYER, 
    0x53804250, 0xd9c8, 0x4c59, 0x92, 0xf7, 0x3b, 0xfa, 0x5c, 0x91, 0xd2, 0xf);

#define FILTER_PORT_NUMBER       60000
#define STREAM_BUFFER_SIZE       ( 65 * 1024 )


typedef struct _GLOBAL_CONTEXT {
    PDRIVER_OBJECT  DriverObject;
    PDEVICE_OBJECT  DeviceObject;

    BOOLEAN         Hooked;

    UINT32          Id;
    UINT32          CalloutId;
    UINT64          FilterId;
    PUCHAR          DataBuffer;
    ULONG           DataBufferLength;
} GLOBAL_CONTEXT, *PGLOBAL_CONTEXT;

PCHAR
IpV4AddressPrint(
    UINT32 IpV4Address );

VOID 
PatternScan (
    PCHAR PatternBuffer,
    ULONG PatternLength,
    PCHAR DataBuffer,
    ULONG DataLength,
    FWPS_STREAM_CALLOUT_IO_PACKET0* FwpsStreamCalloutIoPacket,
    FWPS_CLASSIFY_OUT* ClassifyOut );

NTSTATUS
Initialize (
    PGLOBAL_CONTEXT Context,
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject );

VOID
Terminate (
    PGLOBAL_CONTEXT Context );

NTSTATUS
Hook (
    PGLOBAL_CONTEXT Context );

NTSTATUS
Unhook (
    PGLOBAL_CONTEXT Context );

NTSTATUS
RegisterCallout (
    PDEVICE_OBJECT DeviceObject,
    PUINT32 CalloutId );

NTSTATUS
UnregisterCallout (
    UINT32 Id );

NTSTATUS
AddSublayer (
    HANDLE EngineHandle );

NTSTATUS
DeleteSublayer (
    HANDLE EngineHandle,
    const GUID *Key );

NTSTATUS
AddCallout (
    HANDLE EngineHandle,
    PUINT32 Id );

NTSTATUS
DeleteCallout (
    HANDLE EngineHandle,
    UINT32 Id );

NTSTATUS
AddFilters (
    HANDLE EngineHandle,
    PUINT64 Id );

NTSTATUS
DeleteFilters (
    HANDLE EngineHandle,
    UINT64 Id );

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject );

VOID NTAPI
StreamClassifyCallback (
    const FWPS_INCOMING_VALUES0  *inFixedValues,
    const FWPS_INCOMING_METADATA_VALUES0  *inMetaValues,
    VOID  *layerData,
    const void* classifyContext,
    const FWPS_FILTER1  *filter,
    UINT64  flowContext,
    FWPS_CLASSIFY_OUT0  *classifyOut );

VOID
StreamFlowDeleteCallback(
    IN UINT16  layerId,
    IN UINT32  calloutId,
    IN UINT64  flowContext );

NTSTATUS
StreamNotifyCallback(
    IN FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
    IN const GUID  *filterKey,
    IN const FWPS_FILTER1  *filter );

// global
GLOBAL_CONTEXT g_Context = {0};
PCHAR g_PatternString = "Virus";


NTSTATUS
Hook (
    PGLOBAL_CONTEXT Context )
{
    NTSTATUS Status = STATUS_SUCCESS;
    NTSTATUS CleanupStatus;
    HANDLE EngineHandle = 0;
    FWPM_SESSION0 FwpmSession;

    if ( Context->Hooked ) {
        goto Exit1;
    }

    RtlZeroMemory(&FwpmSession,sizeof(FWPM_SESSION0));
    FwpmSession.displayData.name = L"CODEMACHINE_SESSION";
    FwpmSession.displayData.description = L"CodeMachine WFP Driver";

    Status = FwpmEngineOpen0(
        NULL, 
        RPC_C_AUTHN_DEFAULT, 
        NULL, 
        &FwpmSession, 
        &EngineHandle);
    
    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s FwpmEngineOpen0() FAIL=%08x\n", __MODULE__, __FUNCTION__,
            Status ));
        goto Exit1;
    }

    Status = RegisterCallout (
        Context->DeviceObject,
        &Context->CalloutId );
    if ( ! NT_SUCCESS ( Status ) ) {
        DPF(( "%s!%s RegisterCallout() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit2;
    }

    Status = AddSublayer (
        EngineHandle );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s AddSublayer() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            Status ));
        goto Exit3;
    }

    Status = AddCallout (
        EngineHandle,
        &Context->Id );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s AddCallout() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            Status ));
        goto Exit4;
    }

    Status = AddFilters (
        EngineHandle,
        &Context->FilterId );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s AddFilters() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            Status ));
        goto Exit5;
    }

    Status = FwpmEngineClose0(EngineHandle);
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmEngineClose0() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            Status ));
    }

    Context->Hooked = TRUE;

    return STATUS_SUCCESS;

Exit5 :
    CleanupStatus  = DeleteCallout (EngineHandle, Context->Id );
    if ( !NT_SUCCESS(CleanupStatus) ) {
        DPF(( "%s!%s DeleteCallout() FAIL=%08x\n",__MODULE__, __FUNCTION__, 
            CleanupStatus ));
    }

Exit4 :
    CleanupStatus = DeleteSublayer ( EngineHandle, &CODEMACHINE_STREAM_SUBLAYER );
    if ( !NT_SUCCESS(CleanupStatus) ) {
        DPF(( "%s!%s DeleteSublayer() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            CleanupStatus ));
    }

Exit3 :
    CleanupStatus = UnregisterCallout(Context->CalloutId);
    if ( !NT_SUCCESS(CleanupStatus) ) {
        DPF(( "%s!%s UnregisterCallout() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            CleanupStatus ));
    }

Exit2 :
    CleanupStatus = FwpmEngineClose0(EngineHandle);
    if ( !NT_SUCCESS(CleanupStatus) ) {
        DPF(( "%s!%s FwpmEngineClose0() FAIL=%08x\n",__MODULE__, __FUNCTION__,
            CleanupStatus ));
    }

Exit1:
    // return the original status instead of the one during cleanup
    return Status;
} // Hook()

NTSTATUS
Unhook (
    PGLOBAL_CONTEXT Context )
{
    NTSTATUS Status = STATUS_SUCCESS;
    HANDLE EngineHandle;
    FWPM_SESSION0 FwpmSession;

    if ( ! Context->Hooked ) {
        goto Exit;
    }

    RtlZeroMemory(&FwpmSession,sizeof(FWPM_SESSION0));
    FwpmSession.displayData.name = L"CODEMACHINE_SESSION";
    FwpmSession.displayData.description = L"CodeMachine WFP Driver";

    Status = FwpmEngineOpen0(
        NULL,
        RPC_C_AUTHN_DEFAULT, 
        NULL,
        &FwpmSession, 
        &EngineHandle);

    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s FwpmEngineOpen0() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
        goto Exit;
    }

    Status = DeleteFilters ( EngineHandle, Context->FilterId  );

    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s DeleteFilters() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
    }

    Status  = DeleteSublayer ( EngineHandle, &CODEMACHINE_STREAM_SUBLAYER );

    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s DeleteSublayer() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
    }

    Status  = DeleteCallout (EngineHandle, Context->Id );

    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s DeleteCallout() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
    }

    Status = UnregisterCallout(Context->CalloutId);

    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s UnregisterCallout() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
    }

    Status = FwpmEngineClose0(EngineHandle);
    if (!NT_SUCCESS(Status)) {
        DPF(( "%s!%s FwpmEngineClose0() FAIL=%08x\n",  __MODULE__, __FUNCTION__,
            Status ));
    }

    Status = STATUS_SUCCESS;

Exit:
    return Status;
} // Unhook()


NTSTATUS
RegisterCallout (
    PDEVICE_OBJECT DeviceObject,
    PUINT32 CalloutId )
{
    NTSTATUS Status;
    FWPS_CALLOUT1 FwpsCallout;

    RtlZeroMemory ( &FwpsCallout, sizeof(FwpsCallout) );
    FwpsCallout.flags = 0;
    FwpsCallout.calloutKey = CODEMACHINE_STREAM_CALLOUT;
    FwpsCallout.classifyFn = StreamClassifyCallback;
    FwpsCallout.notifyFn = StreamNotifyCallback;
    FwpsCallout.flowDeleteFn = StreamFlowDeleteCallback;

    Status = FwpsCalloutRegister1 ( 
        DeviceObject,
        &FwpsCallout,
        CalloutId );

    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpsCalloutRegister0() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
    }

    return Status;
} // RegisterCallout()

NTSTATUS
UnregisterCallout (
    UINT32 Id )
{
    NTSTATUS Status;

    Status = FwpsCalloutUnregisterById0 ( Id );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpsCalloutUnregisterById0(%x) FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Id, Status ));
    }

    return Status;
} // UnregisterCallout()


NTSTATUS
AddSublayer (
    HANDLE EngineHandle )
{
    NTSTATUS Status;
    FWPM_SUBLAYER0 FwpmSubLayer;

    RtlZeroMemory ( &FwpmSubLayer, sizeof(FWPM_SUBLAYER0) );
    FwpmSubLayer.subLayerKey = CODEMACHINE_STREAM_SUBLAYER;
    FwpmSubLayer.displayData.name = L"CODEMACHINE_SUBLAYER";
    FwpmSubLayer.displayData.description = L"CodeMachine WFP Sublayer";
    FwpmSubLayer.weight = 255;
    FwpmSubLayer.flags = 0;

    Status = FwpmSubLayerAdd0 ( EngineHandle, &FwpmSubLayer, NULL);
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmSubLayerAdd0() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
    }

    return Status;
} // AddSublayer()

NTSTATUS
DeleteSublayer (
    HANDLE EngineHandle,
    const GUID *Key )
{
    NTSTATUS Status;

    Status = FwpmSubLayerDeleteByKey0 ( EngineHandle, Key );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmSubLayerDeleteByKey0() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
    }

    return Status;
} // DeleteSublayer()

NTSTATUS
AddCallout (
    HANDLE EngineHandle,
    PUINT32 Id )
{
    NTSTATUS Status;
    FWPM_CALLOUT0 FwpmCallout;

    RtlZeroMemory ( &FwpmCallout, sizeof(FWPM_CALLOUT0) );
    FwpmCallout.calloutKey = CODEMACHINE_STREAM_CALLOUT;
    FwpmCallout.flags = 0;
    FwpmCallout.displayData.name = L"CODEMACHINE_CALLOUT";
    FwpmCallout.displayData.description = L"CodeMachine WFP Callout";
    FwpmCallout.applicableLayer = FWPM_LAYER_STREAM_V4;

    Status = FwpmCalloutAdd0 ( EngineHandle, &FwpmCallout, NULL, Id );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmCalloutAdd0() FAIL=%08x\n",__MODULE__, __FUNCTION__, 
            Status ));
    }

    return Status;
} // AddCallout()

NTSTATUS
DeleteCallout (
    HANDLE EngineHandle,
    UINT32 Id )
{
    NTSTATUS Status;

    Status = FwpmCalloutDeleteById0 ( EngineHandle, Id );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmCalloutDeleteById0(%x) FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Id, Status ));
    }

    return Status;

} // DeleteCallout()

NTSTATUS
AddFilters (
    HANDLE EngineHandle,
    PUINT64 Id )
{
    NTSTATUS Status;
    FWPM_FILTER0 FwpmFilter;
    FWPM_FILTER_CONDITION0 FwpmFilterCondition[1];

    RtlZeroMemory ( &FwpmFilter, sizeof(FWPM_FILTER0) );
    FwpmFilter.displayData.name = L"CODEMACHINE_FILTER";
    FwpmFilter.displayData.description = L"CodeMachine WFP Filter";
    FwpmFilter.layerKey = FWPM_LAYER_STREAM_V4; // pre-defined layer id for TCP Streams
    FwpmFilter.filterKey = CODEMACHINE_STREAM_FILTER; //identifies the filter
    FwpmFilter.subLayerKey = CODEMACHINE_STREAM_SUBLAYER; // identifies the sublayer
    FwpmFilter.action.type = FWP_ACTION_CALLOUT_UNKNOWN; //terminating + inspection
    FwpmFilter.action.calloutKey = CODEMACHINE_STREAM_CALLOUT; // identifies the installed callout functions
    FwpmFilter.weight.type = FWP_EMPTY; // sublayer weight - auto-weight.
    FwpmFilter.numFilterConditions = 1; // number of filters that will be applied to the sublayer
    FwpmFilter.filterCondition = &FwpmFilterCondition[0]; // filter structure

    // Filter : ( LocalIPPort == FILTER_PORT_NUMBER )
    RtlZeroMemory ( FwpmFilterCondition, 1 * sizeof(FWPM_FILTER_CONDITION0) );
    FwpmFilterCondition[0].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT; // local IP port
    FwpmFilterCondition[0].matchType = FWP_MATCH_EQUAL; // == 
    FwpmFilterCondition[0].conditionValue.type = FWP_UINT16; // USHORT
    FwpmFilterCondition[0].conditionValue.uint16 = FILTER_PORT_NUMBER; // Port number

    Status = FwpmFilterAdd0 ( EngineHandle, &FwpmFilter, NULL, Id );
    if ( !NT_SUCCESS(Status) ) {
        DPF(( "%s!%s FwpmFilterAdd0(%wS) FAIL=%08x\n", __MODULE__, __FUNCTION__,
            FwpmFilter.displayData.name, Status ));
    }

    return Status;
} // AddFilters()

NTSTATUS
DeleteFilters (
    HANDLE EngineHandle,
    UINT64 Id )
{
    NTSTATUS Status = STATUS_SUCCESS;

    if ( Id ) {
        Status = FwpmFilterDeleteById0 ( EngineHandle, Id );
        if ( !NT_SUCCESS (Status) ) {
            DPF(( "%s!%s FwpmFilterDeleteById0(%I64x) FAIL=%08x\n",__MODULE__, __FUNCTION__, 
                Id, Status ));
        }
    }

    return Status;
} // DeleteFilters()

VOID NTAPI
StreamClassifyCallback (
    const FWPS_INCOMING_VALUES0  *inFixedValues,
    const FWPS_INCOMING_METADATA_VALUES0  *inMetaValues,
    VOID  *layerData,
    const void* classifyContext,
    const FWPS_FILTER1  *filter,
    UINT64  flowContext,
    FWPS_CLASSIFY_OUT0  *classifyOut )
{
    FWPS_STREAM_CALLOUT_IO_PACKET0* FwpsStreamCalloutIoPacket;
    FWPS_STREAM_DATA0 *FwpsStreamData;
    SIZE_T DataLength;
    SIZE_T DataActual;

    UNREFERENCED_PARAMETER(filter);
    UNREFERENCED_PARAMETER(classifyContext);
    UNREFERENCED_PARAMETER(flowContext);

    FwpsStreamCalloutIoPacket = (FWPS_STREAM_CALLOUT_IO_PACKET0*)layerData;
    FwpsStreamData = FwpsStreamCalloutIoPacket->streamData;
    DataLength = FwpsStreamData->dataLength;

    // decode and display the FWPS_INCOMING_VALUES0 and FWPS_INCOMING_METADATA_VALUES0
    DPF(( "%s!%s Flow=%I64x Pid=%I64x %s:%u %s %s:%u %s Nbl=%p Len=%u %s %s %s\n", __MODULE__, __FUNCTION__,
        inMetaValues->currentMetadataValues & FWPS_METADATA_FIELD_FLOW_HANDLE ? inMetaValues->flowHandle : (UINT64)0,
        inMetaValues->currentMetadataValues & FWPS_METADATA_FIELD_PROCESS_ID ? inMetaValues->processId : (UINT64)PsGetCurrentProcessId(),
        IpV4AddressPrint(inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_IP_LOCAL_ADDRESS].value.uint32),
        inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_IP_LOCAL_PORT].value.uint16,
        inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_DIRECTION].value.uint32 == FWP_DIRECTION_INBOUND ? "<-" :
        inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_DIRECTION].value.uint32 == FWP_DIRECTION_OUTBOUND ? "->" : "??",
        IpV4AddressPrint(inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_IP_REMOTE_ADDRESS].value.uint32),
        inFixedValues->incomingValue[FWPS_FIELD_STREAM_V4_IP_REMOTE_PORT].value.uint16,
        ( FwpsStreamData->flags & FWPS_STREAM_FLAG_RECEIVE ) ? "RECV" : "SEND",
        FwpsStreamData->netBufferListChain,
        FwpsStreamData->dataLength,
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_SEND_ABORT) ? "SEND-ABRT" :
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_RECEIVE_ABORT) ? "RECV-ABRT" : "",
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_SEND_EXPEDITED) ? "SEND-EXPD" :
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_RECEIVE_EXPEDITED) ? "RECV-EXPD" : "",
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_SEND_DISCONNECT) ? "SEND-DISC" :
        (FwpsStreamData->flags & FWPS_STREAM_FLAG_RECEIVE_DISCONNECT) ? "RECV-DISC" : "" ));

    if ( FwpsStreamData->dataLength ) {
        ULONG PatternLength;

        FwpsCopyStreamDataToBuffer0 (
            FwpsStreamData,
            g_Context.DataBuffer,
            g_Context.DataBufferLength,
            &DataActual );

        PatternLength = (ULONG)strlen (g_PatternString );

        // scan the buffer for the pattern 
        PatternScan ( 
            g_PatternString,
            PatternLength, 
            (PCHAR)g_Context.DataBuffer, 
            (ULONG)DataActual, 
            FwpsStreamCalloutIoPacket, 
            classifyOut );
    } else {
        classifyOut->actionType = FWP_ACTION_CONTINUE;
    }

    return;
} // StreamClassifyCallback()


VOID
StreamFlowDeleteCallback(
    IN UINT16  layerId,
    IN UINT32  calloutId,
    IN UINT64  flowContext )
{
    UNREFERENCED_PARAMETER(layerId);
    UNREFERENCED_PARAMETER(calloutId);
    UNREFERENCED_PARAMETER(flowContext);
} // StreamFlowDeleteCallback()

NTSTATUS
StreamNotifyCallback(
    IN FWPS_CALLOUT_NOTIFY_TYPE  notifyType,
    IN const GUID  *filterKey,
    IN const FWPS_FILTER1  *filter )
{
    UNREFERENCED_PARAMETER(filterKey);
    UNREFERENCED_PARAMETER(filter);

    DPF(( "NOTIFY:%s\n", 
        notifyType == FWPS_CALLOUT_NOTIFY_ADD_FILTER ? "FWPS_CALLOUT_NOTIFY_ADD_FILTER" : "FWPS_CALLOUT_NOTIFY_DELETE_FILTER" ));

    return STATUS_SUCCESS;
} // StreamNotifyCallback()


NTSTATUS
Initialize (
    PGLOBAL_CONTEXT Context,
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject )
{
    NTSTATUS Status = STATUS_SUCCESS;

    RtlZeroMemory ( Context, sizeof (GLOBAL_CONTEXT) );

    Context->Hooked = FALSE;

    Context->DriverObject  = DriverObject;
    Context->DeviceObject = DeviceObject;
    Context->CalloutId = 0;
    Context->Id = 0;
    Context->FilterId = 0;
    Context->DataBufferLength = STREAM_BUFFER_SIZE;

    if ( ( Context->DataBuffer  = ExAllocatePoolWithTag ( 
        NonPagedPool, 
        Context->DataBufferLength,
        'bsMC' ) ) == NULL  ) {

        DPF(("%s!%s ExAllocatePoolWithTag(StreamBuffer) : FAIL\n", __MODULE__, __FUNCTION__ ));
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
} // Initialize()

VOID
Terminate (
    PGLOBAL_CONTEXT Context )
{
    UNREFERENCED_PARAMETER(Context);
} // Terminate()

PCHAR
IpV4AddressPrint(
    UINT32 IpAddress )
{
    static CHAR LocalBuffer[3][32];
    static ULONG Index = 0;
    Index = ( Index + 1 ) % 3;
    RtlStringCbPrintfA ( LocalBuffer[Index], 32, "%u.%u.%u.%u", 
        ((PUCHAR)&IpAddress)[3],
        ((PUCHAR)&IpAddress)[2],
        ((PUCHAR)&IpAddress)[1],
        ((PUCHAR)&IpAddress)[0] );
    return LocalBuffer[Index];
} // IpV4AddressPrint()

// scan for PatternBuffer in DataBuffer and requests WFP to drop the 
// bytes in the DataBuffer where PatternBuffer bytes are present
VOID 
PatternScan (
    PCHAR PatternBuffer,
    ULONG PatternLength,
    PCHAR DataBuffer,
    ULONG DataLength,
    FWPS_STREAM_CALLOUT_IO_PACKET0* FwpsStreamCalloutIoPacket,
    FWPS_CLASSIFY_OUT* ClassifyOut )
{
    ULONG BufferIdx;

    FwpsStreamCalloutIoPacket->streamAction = FWPS_STREAM_ACTION_NONE;
    ClassifyOut->actionType = FWP_ACTION_CONTINUE;

    // Step #1 : Scan the buffer described by DataBuffer and DataLength 
    // to look for the pattern at PatternBuffer and PatternLength.
    // And set the following fields appropriately
    //   FwpsStreamCalloutIoPacket->countBytesEnforced
    //   ClassifyOut->actionType
    //   ClassifyOut->rights
    //   ClassifyOut->flags

} // PatternScan()

NTSTATUS
DriverEntry (
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    UNREFERENCED_PARAMETER(RegistryPath);
    DPF(( "*** %s.sys Loaded ***\n", __MODULE__ ));

    DriverObject->DriverUnload = DriverUnload;

    Status = IoCreateDevice (
        DriverObject,
        0,
        NULL,
        0x1234,
        0,
        FALSE,
        &DeviceObject );

    if ( ! NT_SUCCESS( Status) ) {
        DPF (( "%s!%s IoCreateDevice() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit1;
    }

    // initialize g_Context
    Status = Initialize ( 
        &g_Context, 
        DriverObject,
        DeviceObject );

    if ( ! NT_SUCCESS(Status ) ) {
        DPF (("%s!%s Initialize() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit2;
    }

    // register the WFP filter
    Status = Hook ( &g_Context );

    if ( ! NT_SUCCESS(Status) ) {
        DPF (("%s!%s ook() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
        goto Exit3;
    }

    return STATUS_SUCCESS;

    // error handling

Exit3:
    // undo Initialize()
    Terminate ( &g_Context );

Exit2:
    IoDeleteDevice ( DeviceObject );

Exit1:
    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{
    NTSTATUS Status;

    // unregister WFP filter
    Status = Unhook ( &g_Context );

    if ( ! NT_SUCCESS(Status) ) {
        DPF (("%s!%s Unhook() FAIL=%08x\n", __MODULE__, __FUNCTION__, 
            Status ));
    }

    // undo Initialize()
    Terminate ( &g_Context );

    IoDeleteDevice ( DriverObject->DeviceObject );

    DPF(( "*** %s.sys Unloaded ***\n", __MODULE__ ));
} // DriverUnload()

