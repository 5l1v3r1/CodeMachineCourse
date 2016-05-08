/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999-2014 CodeMachine Incorporated. All rights Reserved.
// Developed by CodeMachine Incorporated. (www.codemachine.com)
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _NETWORKHEADERS_H_
#define _NETWORKHEADERS_H_

#define ntohl(x)	((((x) & 0x000000ff) << 24) | \
    (((x) & 0x0000ff00) <<  8) | \
    (((x) & 0x00ff0000) >>  8) | \
    (((x) & 0xff000000) >> 24))

#define htonl(x)	((((x) & 0x000000ff) << 24) | \
    (((x) & 0x0000ff00) <<  8) | \
    (((x) & 0x00ff0000) >>  8) | \
    (((x) & 0xff000000) >> 24))

#define ntohs(x)	((((x) & 0x00ff) << 8) | \
    (((x) & 0xff00) >> 8))

#define htons(x)	((((x) & 0x00ff) << 8) | \
    (((x) & 0xff00) >> 8))

#define	NTOHL(x)	(x) = ntohl((unsigned long)(x))
#define	NTOHS(x)	(x) = ntohs((unsigned short)(x))
#define	HTONL(x)	(x) = htonl((unsigned long)(x))
#define	HTONS(x)	(x) = htons((unsigned short)(x))

//
// ETHERNET
//

#define ETH_ALEN            6

#define ETH_TYPE_IP         0x0800
#define ETH_TYPE_ARP        0x0806
#define ETH_TYPE_REVARP     0x8035
#define ETH_TYPE_IPV6       0x86dd

#define ETH_HEADER_LENGTH        (sizeof(ETH_HEADER))

//
// ARP
//
#define ARPHRD_ETHER        1
#define ARPHRD_IEEE802      6
#define ARPHRD_FRELAY       15
#define ARPHRD_IEEE1394     24

#define	ARPOP_REQUEST       1
#define	ARPOP_REPLY         2
#define	ARPOP_REVREQUEST    3
#define	ARPOP_REVREPLY      4
#define	ARPOP_INVREQUEST    8
#define	ARPOP_INVREPLY      9
#define	ARPOP_NAK           10

//
// IPV4
//
#define	IP_DF               0x4000
#define	IP_MF               0x2000
#define	IP_OFFMASK          0x1fff


#define  IPPROTO_IP              0
#define  IPPROTO_ICMP            1
#define  IPPROTO_TCP             6
#define  IPPROTO_UDP             17

//
// UDP
//
#define	LEN_UDP_HEADER          sizeof(UDP_HEADER)

//
// TCP
//
#define	TH_FIN	  0x01
#define	TH_SYN	  0x02
#define	TH_RST	  0x04
#define	TH_PSH	  0x08
#define	TH_ACK	  0x10
#define	TH_URG	  0x20
#define	TH_ECE	  0x40
#define	TH_CWR	  0x80

#define  TCP_FLAGS_MASK         0xFFF
#define  LEN_TCP_HEADER         sizeof(TCP_HEADER)

//
// ICMP
//

#define ICMP_ECHOREPLY          0
#define ICMP_DEST_UNREACH       3
#define ICMP_SOURCE_QUENCH      4
#define ICMP_REDIRECT           5
#define ICMP_ECHO               8
#define ICMP_TIME_EXCEEDED      11
#define ICMP_PARAMETERPROB      12
#define ICMP_TIMESTAMP          13
#define ICMP_TIMESTAMPREPLY     14
#define ICMP_INFO_REQUEST       15
#define ICMP_INFO_REPLY         16
#define ICMP_ADDRESS            17
#define ICMP_ADDRESSREPLY       18
#define NR_ICMP_TYPES           18

#pragma pack( push, 1 )

typedef struct _ETH_HEADER
{
    unsigned char   h_dest[ETH_ALEN];
    unsigned char   h_source[ETH_ALEN];
    unsigned short  h_proto;
} ETH_HEADER, *PETH_HEADER;

typedef struct	_ARP_HEADER {
    unsigned short     ar_hrd;
    unsigned short     ar_pro;
    unsigned char      ar_hln;
    unsigned char      ar_pln;
    unsigned short     ar_op;
    unsigned char      ar_sha[6];
    unsigned char      ar_spa[4];
    unsigned char      ar_tha[6];
    unsigned char      ar_tpa[4];
}ARP_HEADER,*PARP_HEADER;

typedef struct _IP_HEADER {
    unsigned char   ip_hl:4;
    unsigned char   ip_v:4;
    unsigned char   ip_tos;
    short           ip_len;
    unsigned short  ip_id;
    short           ip_off;
    unsigned char   ip_ttl;
    unsigned char   ip_p;
    unsigned short  ip_sum;
    unsigned int    ip_src;
    unsigned int    ip_dst;
} IP_HEADER, *PIP_HEADER;

typedef struct _UDP_HEADER {
    unsigned short uh_sport;
    unsigned short uh_dport;
    unsigned short uh_ulen;
    unsigned short uh_sum;
}UDP_HEADER, *PUDP_HEADER;

typedef struct _TCP_HEADER
{
   unsigned short       src_port;
   unsigned short       dst_port;
   unsigned long        seq_num;
   unsigned long        ack_num;
   unsigned char        th_off;
   unsigned char        flags;
   unsigned short       window;
   unsigned short       sum;
   unsigned short       urg_pointer;
}TCP_HEADER, *PTCP_HEADER;

typedef struct _ICMP_HEADER {
    unsigned char       type;
    unsigned char       code;
    unsigned short      checksum;
    unsigned char       data[4];
}ICMP_HEADER, *PICMP_HEADER;

typedef  struct   _L4_HEADER {
    unsigned short sport;
    unsigned short dport;
} L4_HEADER, *PL4_HEADER;

#pragma pack(pop)

#endif // _NETWORKHEADERS_H_

