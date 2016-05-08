/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#define INITGUID
#define NDIS60	1 
#include "ndis.h"
#include "ntstrsafe.h"

#define __MODULE__		"NdisHook"
#define DPF(x)			DbgPrint x

#pragma warning (disable:4054)
#pragma warning (disable:4055)
#pragma warning (disable:4152)

typedef struct _GLOBAL_CONTEXT {
    PDRIVER_OBJECT                      DriverObject;
    NDIS_HANDLE                         NdisProtocolHandle;
    PVOID                               TcpIpProtocolBlock;
    RECEIVE_NET_BUFFER_LISTS_HANDLER    ProtocolReceiveNetBufferListsOriginal;
} GLOBAL_CONTEXT, *PGLOBAL_CONTEXT;

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject );

GLOBAL_CONTEXT Globals = {0};


NDIS_STATUS
NdisprotBindAdapter(
    IN NDIS_HANDLE                  ProtocolDriverContext,
    IN NDIS_HANDLE                  BindContext,
    IN PNDIS_BIND_PARAMETERS        BindParameters )
{
    NDIS_STATUS                     Status =  NDIS_STATUS_NOT_SUPPORTED;

    DPF (( "%s!%s ProtocolDriverContext=%p BindContext=%p BindParameters=%p\n", __MODULE__, __FUNCTION__, 
        ProtocolDriverContext, BindContext, BindParameters ));

    return Status;
}

VOID
NdisprotOpenAdapterComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN NDIS_STATUS                  Status )
{
    DPF (( "%s!%s ProtocolBindingContext=%p Status=%08x\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, Status ));
}


NDIS_STATUS
NdisprotUnbindAdapter(
    IN NDIS_HANDLE                  UnbindContext,
    IN NDIS_HANDLE                  ProtocolBindingContext )
{
    DPF (( "%s!%s UnbindContext=%p ProtocolBindingContext=%p\n", __MODULE__, __FUNCTION__, 
        UnbindContext, ProtocolBindingContext ));

    return NDIS_STATUS_SUCCESS;
}

VOID
NdisprotCloseAdapterComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext )
{
    DPF (( "%s!%s ProtocolBindingContext=%p\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext ));
}

VOID
NdisprotSendComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_BUFFER_LIST             pNetBufferList,
    IN ULONG                        SendCompleteFlags )
{
    DPF (( "%s!%s ProtocolBindingContext=%p pNetBufferList=%p SendCompleteFlags=%08x\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, pNetBufferList, SendCompleteFlags ));
}

VOID
NdisprotReceiveNetBufferLists(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_BUFFER_LIST             pNetBufferLists,
    IN NDIS_PORT_NUMBER             PortNumber,
    IN ULONG                        NumberOfNetBufferLists,
    IN ULONG                        ReceiveFlags )
{
    DPF (( "%s!%s ProtocolBindingContext=%p pNetBufferLists=%p NumberOfNetBufferLists=%u PortNumber=%u ReceiveFlags=%08x\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, pNetBufferLists, NumberOfNetBufferLists, PortNumber, ReceiveFlags ));
}

VOID
NdisprotRequestComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNDIS_OID_REQUEST            pNdisRequest,
    IN NDIS_STATUS                  Status )
{
    DPF (( "%s!%s ProtocolBindingContext=%p pNdisRequest=%p Status=%08x\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, pNdisRequest, Status ));
}

VOID
NdisprotStatus(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNDIS_STATUS_INDICATION      StatusIndication )
{
    DPF (( "%s!%s ProtocolBindingContext=%p StatusIndication=%p\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, StatusIndication ));
}

NDIS_STATUS
NdisprotPnPEventHandler(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_PNP_EVENT_NOTIFICATION  pNetPnPEventNotification )
{
    NDIS_STATUS                       Status = NDIS_STATUS_SUCCESS;

    DPF (( "%s!%s ProtocolBindingContext=%p pNetPnPEventNotification=%p\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, pNetPnPEventNotification ));

    return Status;
}

// ProtocolReceiveNetBufferLists() hook function that will 
// scan NBL chains to find specific packets
VOID
ProtocolReceiveNetBufferListsHook (
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNET_BUFFER_LIST  NetBufferLists,
    IN NDIS_PORT_NUMBER  PortNumber,
    IN ULONG  NumberOfNetBufferLists,
    IN ULONG  ReceiveFlags )
{
    DPF (( "%s!%s ProtocolBindingContext=%p Nbls=%p Count=%u Port=%u Flags=%08x\n", __MODULE__, __FUNCTION__, 
        ProtocolBindingContext, NetBufferLists, NumberOfNetBufferLists, PortNumber, ReceiveFlags ));

    // if there is an original handler invoke it
    if ( Globals.ProtocolReceiveNetBufferListsOriginal ) {
        Globals.ProtocolReceiveNetBufferListsOriginal ( 
            ProtocolBindingContext, 
            NetBufferLists,
            PortNumber,
            NumberOfNetBufferLists, 
            ReceiveFlags );
    }
} // ProtocolReceiveNetBufferListsHook()

//
// Step #1 : Create MACROs to access NDIS internal data structure fields
//

// Begin Step #1

// find the offset of _NDIS_PROTOCOL_BLOCK.Name and use it in the following macro
#define NDIS_PROTOCOL_BLOCK_NAME(p) (PUNICODE_STRING)( ((PUCHAR)p) + 0x?? )

// find the offset of _NDIS_PROTOCOL_BLOCK.NextProtocol and use it in the following macro
#define NDIS_PROTOCOL_BLOCK_NEXT(p) (PVOID*)( ((PUCHAR)p) + 0x?? )

// find the offset of _NDIS_PROTOCOL_BLOCK.ReceiveNetBufferListsHandler and use it in the following macro
#define NDIS_PROTOCOL_BLOCK_RECVNBL(p) (PVOID*)( ((PUCHAR)p) + 0x?? )

// find the offset of _NDIS_PROTOCOL_BLOCK.OpenQueue and use it in the following macro
#define NDIS_PROTOCOL_BLOCK_OPEN(p) (PVOID*)( ((PUCHAR)p) + 0x?? )

// find the offset of _NDIS_OPEN_BLOCK.ProtocolNextOpen and use it in the following macro
#define NDIS_OPEN_BLOCK_NEXT(p) (PVOID*)( ((PUCHAR)p) + 0x?? )

// find the offset of _NDIS_OPEN_BLOCK.ReceiveNetBufferLists and use it in the following macro
#define NDIS_OPEN_BLOCK_RECVNBL(p) (PVOID*)( ((PUCHAR)p) + 0x?? )

// End Step #1

// returns NDIS_PROTOCOL_BLOCK
PVOID FindTcpIpProtcolBlock( PVOID ProtocolBlock )
{
    UNICODE_STRING TcpIp  = RTL_CONSTANT_STRING( L"TCPIP");

    // Step #2 : Iterate through all the protocol blocks and find the one for TCPIP
    // (RtlEqualUnicodeString())

    return NULL;
} // FindTcpIpProtcolBlock()


VOID UpdateProtocolOpenBlockRecvHandler ( PVOID ProtocolBlock, PVOID OrgHandler, PVOID NewHandler )
{
    PVOID OpenBlock = *NDIS_PROTOCOL_BLOCK_OPEN(ProtocolBlock);

    // Step #3 : Iterate through all the NDIS_OPEN_BLOCK for the given NDIS_PROTOCOL_BLOCK 
    // i.e. ProtocolBlock. If the existing ProtocolReceiveNetBufferLists() handler matches the 
    // the one in OrgHandler then replace the handler with the new one i.e. NewHandler

} // UpdateProtocolOpenBlockRecvHandler()

NTSTATUS
DriverEntry (
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath )
{
    NTSTATUS Status;
    NDIS_PROTOCOL_DRIVER_CHARACTERISTICS NdisProtocolDriverCharacteristics;
    NDIS_STRING ProtocolName = NDIS_STRING_CONST("FAKEPROT");

    UNREFERENCED_PARAMETER(RegistryPath);

    Globals.DriverObject = DriverObject;

    DriverObject->DriverUnload = DriverUnload;

    NdisZeroMemory(&NdisProtocolDriverCharacteristics,sizeof(NDIS_PROTOCOL_DRIVER_CHARACTERISTICS));

    NdisProtocolDriverCharacteristics.Header.Type                = NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS,
    NdisProtocolDriverCharacteristics.Header.Size                = sizeof(NDIS_PROTOCOL_DRIVER_CHARACTERISTICS);
    NdisProtocolDriverCharacteristics.Header.Revision            = NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;

    NdisProtocolDriverCharacteristics.MajorNdisVersion            = 6;
    NdisProtocolDriverCharacteristics.MinorNdisVersion            = 0;
    NdisProtocolDriverCharacteristics.Name                        = ProtocolName;
    NdisProtocolDriverCharacteristics.SetOptionsHandler           = NULL;
    NdisProtocolDriverCharacteristics.OpenAdapterCompleteHandlerEx  = NdisprotOpenAdapterComplete;
    NdisProtocolDriverCharacteristics.CloseAdapterCompleteHandlerEx = NdisprotCloseAdapterComplete;
    NdisProtocolDriverCharacteristics.SendNetBufferListsCompleteHandler = NdisprotSendComplete;
    NdisProtocolDriverCharacteristics.OidRequestCompleteHandler   = NdisprotRequestComplete;
    NdisProtocolDriverCharacteristics.StatusHandlerEx             = NdisprotStatus;
    NdisProtocolDriverCharacteristics.UninstallHandler            = NULL;
    NdisProtocolDriverCharacteristics.ReceiveNetBufferListsHandler = NdisprotReceiveNetBufferLists;
    NdisProtocolDriverCharacteristics.NetPnPEventHandler          = NdisprotPnPEventHandler;
    NdisProtocolDriverCharacteristics.BindAdapterHandlerEx        = NdisprotBindAdapter;
    NdisProtocolDriverCharacteristics.UnbindAdapterHandlerEx      = NdisprotUnbindAdapter;


    Status = NdisRegisterProtocolDriver(
        (NDIS_HANDLE)&Globals,
        &NdisProtocolDriverCharacteristics,
        &Globals.NdisProtocolHandle);

    if ( ! NT_SUCCESS(Status ) ) {
        DPF (("%s!%s NdisRegisterProtocolDriver() FAIL=%08x\n", __MODULE__, __FUNCTION__, Status ));
        goto Exit;
    }

    // find the NDIS_PROTOCOL_BLOCK for the protocol driver TCPIP.sys
    Globals.TcpIpProtocolBlock = FindTcpIpProtcolBlock (Globals.NdisProtocolHandle);

    if ( ! Globals.TcpIpProtocolBlock ) {
        Status = NDIS_STATUS_FAILURE;
        DPF (("%s!%s FindTcpIpProtcolBlock() FAIL\n", __MODULE__, __FUNCTION__ ));
        goto Exit;
    }

    DPF (( "%s!%s TcpIpProtocolBlock=%p\n", __MODULE__, __FUNCTION__, 
        Globals.TcpIpProtocolBlock ));

    // Step #4 : Save a copy of the original ProtocolReceiveNetBufferLists handler in TCPIP

    UpdateProtocolOpenBlockRecvHandler( 
        Globals.TcpIpProtocolBlock, 
        (PVOID)Globals.ProtocolReceiveNetBufferListsOriginal, 
        (PVOID)ProtocolReceiveNetBufferListsHook );

    return STATUS_SUCCESS;

Exit:
    if (  Globals.TcpIpProtocolBlock ) {
        UpdateProtocolOpenBlockRecvHandler( 
            Globals.TcpIpProtocolBlock, 
            (PVOID)ProtocolReceiveNetBufferListsHook,
            (PVOID)Globals.ProtocolReceiveNetBufferListsOriginal );
    }

    if ( Globals.NdisProtocolHandle ) {
        NdisDeregisterProtocolDriver ( Globals.NdisProtocolHandle );
    }

    return Status;
} // DriverEntry()

VOID
DriverUnload(
    PDRIVER_OBJECT DriverObject )
{
    UNREFERENCED_PARAMETER(DriverObject);

    DPF(( "%s!%s DriverUnload()\n", __MODULE__, __FUNCTION__ ));

    if (  Globals.TcpIpProtocolBlock ) {
        UpdateProtocolOpenBlockRecvHandler( 
            Globals.TcpIpProtocolBlock, 
            (PVOID)ProtocolReceiveNetBufferListsHook,
            (PVOID)Globals.ProtocolReceiveNetBufferListsOriginal );
    }

    if ( Globals.NdisProtocolHandle ) {
        NdisDeregisterProtocolDriver ( Globals.NdisProtocolHandle );
    }
} // DriverUnload()
