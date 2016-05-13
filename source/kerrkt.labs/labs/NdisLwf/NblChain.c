/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4101)
#pragma warning(disable:4214)

#define NDIS60  1
#include "ndis.h"
#include "NetworkHeaders.h"

#define DPF(x)  DbgPrint x

// checks if the NBL chain contains any ICMP Echo Replies
BOOLEAN ProcessNblChain ( PNET_BUFFER_LIST  NetBufferLists )
{
    PNET_BUFFER_LIST  NetBufferList = NetBufferLists;
    PNET_BUFFER_LIST  NextNetBufferList;

    // Step #1 : Iterate through all the NET_BUFFER_LISTs in the NET_BUFFER_LIST chain
    for (NetBufferList = NetBufferLists; NetBufferList; NetBufferList = NET_BUFFER_LIST_NEXT_NBL(NetBufferList) ) {

        PNET_BUFFER NetBuffer;

        //  Step #2 : Iterate through all the NET_BUFFERs in the NET_BUFFER_LISTs passed to the function 
        for (NetBuffer = NET_BUFFER_LIST_FIRST_NB(NetBufferList); NetBuffer; NetBuffer = NET_BUFFER_NEXT_NB(NetBuffer)) {

            // Declare a storage buffer big enough to hold the MAC header (ETH_HEADER), 
            // IP Header (IP_HEADER) and ICMP Header (ICMP_HEADER)
            UCHAR LocalBuffer[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(ICMP_HEADER)];
            ULONG LocalBufferLength = sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(ICMP_HEADER);
            PUCHAR HeaderBuffer = NULL;
            PETH_HEADER EthHeader;
            PIP_HEADER IpHeader;
            PICMP_HEADER IcmpHeader;

            // Step #4 : Attempt to retrieve the pointer (NdisGetDataBuffer()) to the data area in HeaderBuffer
            // from the NB that contains the ETH, IP and ICMP headers. Pass in the storage buffer (LocalBuffer) 
            // where the data would be copied in case the ETH, IP and ICMP headers are not contiguous. 
            if (NULL == (HeaderBuffer = NdisGetDataBuffer(NetBuffer, LocalBufferLength, LocalBuffer, 1, 1)))
            {
                //  Step #3 : If the current NB is not large enough (NET_BUFFER_DATA_LENGTH()) to hold the required headers then skip this NB.
                continue;
            }

            // Step #5 : Initialize EthHeader to the point to the start of HeaderBuffer 
            EthHeader = (PETH_HEADER)HeaderBuffer;

            // Step #6 : If the layer 3 protocol (h_proto) in the ETH_HEADER is not IPv4 (ETH_TYPE_IP) then skip this NB.
            // Note h_proto is in network byte format and must be converted to host byte format (ntohs())
            if (ETH_TYPE_IP != ntohs(EthHeader->h_proto))
            {
                continue;
            }

            // Step #7 : Initialize IpHeader to point to HeaderBuffer after ETH_HEADER 
            IpHeader = (PIP_HEADER)((ULONG_PTR)EthHeader + sizeof(*EthHeader));

            // Step #8 : If the layer 4 protocol (ip_p) in the PIP_HEADER is not ICMP (IPPROTO_ICMP) then skip this NB
            if (IPPROTO_ICMP != IpHeader->ip_p)
            {
                continue;
            }

            // Step #9 : Initialize IcmpHeader to point to HeaderBuffer after ETH_HEADER and IP_HEADER
            IcmpHeader = (PICMP_HEADER)((ULONG_PTR)IpHeader + sizeof(*IpHeader));

            DPF (( "%s ICMP (%u) %u.%u.%u.%u < %u.%u.%u.%u\n", __FUNCTION__, 
                IcmpHeader->type,
                ((PUCHAR)(&IpHeader->ip_dst))[0], ((PUCHAR)(&IpHeader->ip_dst))[1],  ((PUCHAR)(&IpHeader->ip_dst))[2],  ((PUCHAR)(&IpHeader->ip_dst))[3],
                ((PUCHAR)(&IpHeader->ip_src))[0], ((PUCHAR)(&IpHeader->ip_src))[1],  ((PUCHAR)(&IpHeader->ip_src))[2],  ((PUCHAR)(&IpHeader->ip_src))[3] ));

            // Step #10 : If the IMCP type (type) in the ICMP_HEADER is echo reply (ICMP_ECHOREPLY) 
            // then return TRUE which will cause the caller to drop the packet
            if (ICMP_ECHOREPLY == IcmpHeader->type)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
} // ProcessNblChain()
