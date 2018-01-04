/*
 * Copyright (C) 2003 Yasuhiro Ohara
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.
 * 
 * Control Plane Adapter functionality:
 * Adam Kaliszan 2012 adam.kaliszan@gmail.com
 *  
 */

#include <zebra.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

#include "log.h"
#include "memory.h"
#include "sockunion.h"
#include "sockopt.h"
#include "privs.h"

#include "lib/zclient.h"
#include "lib/stream.h"

#include "ospf6_top.h"
#include "ospf6_proto.h"
#include "ospf6_network.h"
#include "ospf6_interface.h"

extern struct zebra_privs_t ospf6d_privs;

#ifndef HAVE_ZEBRA_CPA
int  ospf6_sock;
#endif /* HAVE_ZEBRA_CPA */

struct in6_addr allspfrouters6;
struct in6_addr alldrouters6;

#ifdef HAVE_ZEBRA_CPA

static void createCPA_TP_socket(void);

static void sockopt_reuseaddr_zebra()
{
  struct stream *s = zclient->obuf;
  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  stream_putl(s, SOL_SOCKET);  
  stream_putl(s, SO_REUSEADDR);  
  stream_putl(s, 1);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("sockopt_reuseaddr_zebra: (%d, %d, %d)", SOL_SOCKET, SO_REUSEADDR, 1);
}


static uint16_t ospfv3SumCalc(size_t lenOspfMsg, struct in6_addr *srcAddr, struct in6_addr *dstAddr, uint8_t *buffOspfMsg)
{
#define OSPFv3_CRC_OFFSET 12
  uint16_t prot_ospfv3=89;
  uint16_t end=lenOspfMsg;
  uint16_t word16;
  uint32_t sum;    
  uint16_t *addrBuf;
  uint16_t *buff;
  int i;
    
  // Find out if the length of data is even or odd number. If odd, add a padding byte = 0 at the end of packet
  if ((lenOspfMsg & 0x01) == 0x01)
  {
    buffOspfMsg[lenOspfMsg]=0;
    end++;
  }
  end /=2;
    
  //initialize sum to zero
  sum=0;
    
  // make 16 bit words out of every two adjacent 8 bit words and 
  // calculate the sum of all 16 vit words

  buff = (uint16_t *)(buffOspfMsg);
  for (i=0; i<end; i++)
  {
    word16 = ntohs(buff[i]);
    sum = sum + (unsigned long)word16;
  }   
    // add the UDP pseudo header which contains the IP source and destinationn addresses
  
  if (srcAddr != NULL)
  {
    addrBuf = srcAddr->s6_addr16;
    for (i=0; i<8; i++)
      sum=sum+ntohs(addrBuf[i]); 
  }

  if (dstAddr != NULL)
  {
    addrBuf = dstAddr->s6_addr16;
    for (i=0; i<8; i++)
      sum=sum+ntohs(addrBuf[i]); 
  }
  // the protocol number and the length of the UDP packet
  sum = sum + prot_ospfv3 + (uint16_t)lenOspfMsg;

  // keep only the last 16 bits of the 32 bit calculated sum and add the carries
  while (sum>>16)
    sum = (sum & 0xFFFF)+(sum >> 16);
        
  // Take the one's complement of sum
  sum = ~sum;

  uint16_t wynik = (uint16_t) sum;
  if (wynik == 0x0000)
    wynik = 0xFFFF;
  uint16_t wynikNO = htons(wynik);
  
  uint16_t *tmpPtr = (uint16_t *)(&buffOspfMsg[OSPFv3_CRC_OFFSET]);
  *tmpPtr = wynikNO;
  
  return wynik;
#undef OSPFv3_CRC_OFFSET
}
#endif /*HAVE_ZEBRA_CPA */

/* setsockopt ReUseAddr to on */
void ospf6_set_reuseaddr (void)
{
#ifndef HAVE_ZEBRA_CPA
  u_int on = 0;  
  if (setsockopt (ospf6_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (u_int)) < 0)
    zlog_warn ("Network: set SO_REUSEADDR failed: %s", safe_strerror (errno));
#else
  sockopt_reuseaddr_zebra();
#endif /* HAVE_ZEBRA_CPA */
}

/* setsockopt MulticastLoop to off */
void ospf6_reset_mcastloop (void)
{
#ifndef HAVE_ZEBRA_CPA
  u_int off = 0;
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                  &off, sizeof (u_int)) < 0)
    zlog_warn ("Network: reset IPV6_MULTICAST_LOOP failed: %s",
               safe_strerror (errno));
#else
  u_int off = 0;
  struct stream *s;
  s = zclient->obuf;
  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  stream_putl(s, IPPROTO_IPV6);  
  stream_putl(s, IPV6_MULTICAST_LOOP);  
  stream_putl(s, off);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("ospf6_reset_mcastloop: (%d, %d, %d)", IPPROTO_IPV6, IPV6_MULTICAST_LOOP, off);
#endif /*HAVE_ZEBRA_CPA */
}

void ospf6_set_pktinfo (void)
{
#ifndef HAVE_ZEBRA_CPA
  setsockopt_ipv6_pktinfo (ospf6_sock, 1);
#else
  struct stream *s;
  s = zclient->obuf;

#ifdef IPV6_RECVPKTINFO     /*2292bis-01*/
  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  stream_putl(s, IPPROTO_IPV6);  
  stream_putl(s, IPV6_RECVPKTINFO);  
  stream_putl(s, 1);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
#else   /*RFC2292*/  
  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  stream_putl(s, IPPROTO_IPV6);  
  stream_putl(s, IPV6_PKTINFO);  
  stream_putl(s, 1);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("ospf6_set_pktinfo: (%d, %d, %d)", IPPROTO_IPV6, IPV6_PKTINFO, 1);
#endif /* INIA_IPV6 */ 
#endif /*HAVE_ZEBRA_CPA */
}

void ospf6_set_transport_class (void)
{
#ifdef IPTOS_PREC_INTERNETCONTROL

#ifndef HAVE_ZEBRA_CPA
  setsockopt_ipv6_tclass (ospf6_sock, IPTOS_PREC_INTERNETCONTROL);
#else
  struct stream *s;
  s = zclient->obuf;

  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IPV6);  
  stream_putl(s, IPV6_TCLASS);  
  stream_putl(s, IPTOS_PREC_INTERNETCONTROL);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("ospf6_set_transport_class: (%d, %d, %d)", IPPROTO_IPV6, IPV6_TCLASS, IPTOS_PREC_INTERNETCONTROL);
#endif /* HAVE_ZEBRA_CPA */

#endif
}

void ospf6_set_checksum (void)
{

#ifndef DISABLE_IPV6_CHECKSUM
#ifndef HAVE_ZEBRA_CPA
  int offset = 12;
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_CHECKSUM, &offset, sizeof (offset)) < 0)
    zlog_warn ("Network: set IPV6_CHECKSUM failed: %s", safe_strerror (errno));
#else
//  int offset = 12;  
//  struct stream *s;
//  s = zclient->obuf;
// 
//  stream_reset(s);
//  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
//   
//  stream_putl(s, IPPROTO_IPV6);  
//  stream_putl(s, IPV6_CHECKSUM);  
//  stream_putl(s, offset);  
//   
//  stream_putw_at (s, 0, stream_get_endp (s));  
//  if (zclient_send_message(zclient) != 0)
//    zlog_err("ospf6_set_checksum: Failed to send message to zebra server");
//  else 
//    zlog_debug("ospf6_set_checksum: (%d, %d %d)", IPPROTO_IPV6, IPV6_CHECKSUM, offset);
#endif
  
#else
  zlog_warn ("Network: Don't set IPV6_CHECKSUM");
#endif /* DISABLE_IPV6_CHECKSUM */
}

#ifndef HAVE_ZEBRA_CPA

static int iov_count (struct iovec *iov)
{
  int i;
  for (i = 0; iov[i].iov_base; i++)
    ;
  return i;
}

static int iov_totallen (struct iovec *iov)
{
  int i;
  int totallen = 0;
  for (i = 0; iov[i].iov_base; i++)
    totallen += iov[i].iov_len;
  return totallen;
}

/* Make ospf6d's server socket. */
int ospf6_serv_sock (void)
{

  if (ospf6d_privs.change (ZPRIVS_RAISE))
    zlog_err ("ospf6_serv_sock: could not raise privs");

  ospf6_sock = socket (AF_INET6, SOCK_RAW, IPPROTO_OSPFIGP);
  if (ospf6_sock < 0)
    {
      zlog_warn ("Network: can't create OSPF6 socket.");
      if (ospf6d_privs.change (ZPRIVS_LOWER))
        zlog_err ("ospf_sock_init: could not lower privs");
      return -1;
    }
  if (ospf6d_privs.change (ZPRIVS_LOWER))
      zlog_err ("ospf_sock_init: could not lower privs");
  
  sockopt_reuseaddr (ospf6_sock);

  ospf6_reset_mcastloop ();
  ospf6_set_pktinfo ();
  ospf6_set_transport_class ();
  ospf6_set_checksum ();

  /* setup global in6_addr, allspf6 and alldr6 for later use */
  inet_pton (AF_INET6, ALLSPFROUTERS6, &allspfrouters6);
  inet_pton (AF_INET6, ALLDROUTERS6, &alldrouters6);
 
  return 0;
}
#else

int ospf6_serv_sock(void)
{
  inet_pton (AF_INET6, ALLSPFROUTERS6, &allspfrouters6);
  inet_pton (AF_INET6, ALLDROUTERS6, &alldrouters6);

  if (ospf6d_privs.change (ZPRIVS_RAISE))
    zlog_err ("ospf6_serv_sock: could not raise privs");
  

  zclient->NetworkTxcInit = createCPA_TP_socket; 

  return 0;
}

static void createCPA_TP_socket(void)
{ 
  struct stream *s;
  s= zclient->obuf;
  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_CREATE_NET_SCK, 0);
  
  stream_putl(s, AF_INET6);  
  stream_putl(s, SOCK_RAW);  
  stream_putl(s, IPPROTO_OSPFIGP);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("createCPA_TP_socket (%d, %d, %d)", AF_INET6, SOCK_RAW, IPPROTO_OSPFIGP);
   
  sockopt_reuseaddr_zebra(); 

  /* set socket options */
  ospf6_reset_mcastloop ();
  
  ospf6_set_pktinfo ();
  //ospf6_set_transport_class ();
  ospf6_set_checksum ();  
}

#endif /* HAVE_ZEBRA_CPA */

/* ospf6 set socket option */
void ospf6_sso (u_int ifindex, struct in6_addr *group, int option)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, group, sizeof (struct in6_addr));

#ifndef HAVE_ZEBRA_CPA
  int ret;
  ret = setsockopt (ospf6_sock, IPPROTO_IPV6, option,
                    &mreq6, sizeof (mreq6));
  if (ret < 0)
    zlog_err ("Network: setsockopt (%d) on ifindex %d failed: %s",
              option, ifindex, safe_strerror (errno));
#else
  struct stream *s;
  s = zclient->obuf;

  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IPV6);  
  stream_putl(s, option);  
  stream_put(s, &mreq6, (size_t)sizeof (mreq6));  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("ospf6_sso: Failed to send message");
  else
    zlog_debug("ospf6_sso: (%d, %d len %d) ifindex %d", IPPROTO_IPV6, option, sizeof(mreq6), ifindex);
#endif /* HAVE_ZEBRA_CPA */
}

#ifdef HAVE_ZEBRA_CPA


static int ospf6_sendmsgViaZebra (struct in6_addr *src, struct in6_addr *dst, unsigned int *ifindex, struct iovec *message)
{
  int sdu_size = message[0].iov_len;
  struct stream *s;
  
  s = zclient->obuf;
  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_TXC, 0);
  stream_putl (s, *ifindex);
  
  struct ip6_hdr iph;

  memset(&iph, 0, sizeof(struct ip6_hdr));
  
  iph.ip6_ctlun.ip6_un1.ip6_un1_flow = htonl(6<<28);              // = IPVERSION;
  iph.ip6_ctlun.ip6_un1.ip6_un1_plen = sdu_size;
  iph.ip6_ctlun.ip6_un1.ip6_un1_nxt = 89;
  iph.ip6_ctlun.ip6_un1.ip6_un1_hlim = 64;
  
  if (src)
    memcpy (&iph.ip6_src, src, sizeof (struct in6_addr));
  else
  {
    struct ospf6_interface *oi = ospf6_interface_lookup_by_ifindex ((int)(*ifindex));
    struct interface *ifp = if_lookup_by_index((int)(*ifindex));
    if (oi && ifp)
    {
      if (oi->linklocal_addr)
        memcpy (&iph.ip6_src, oi->linklocal_addr, sizeof (struct in6_addr));
      else                                                                      //TODO Adam spróbować zdobyć w inny sposób ten adres lokalny
      {
        if (!if_is_loopback(ifp))
          zlog_warn ("No source address %s", ifp->name);
        return -2; //inet_pton (AF_INET6, "::0", &iph->ip6_src);
      }
    }
  }
  memcpy (&iph.ip6_dst, dst, sizeof (struct in6_addr));

  //ospfv3SumCalc(sdu_size, src, dst, message[0].iov_base);
  
  stream_put(s, &iph, sizeof(struct ip6_hdr));
  stream_put(s, message[0].iov_base, (size_t) sdu_size);
  
  int len = stream_get_endp (s);
  
  stream_putw_at (s, 0, len);
  if (zclient_send_message(zclient) != 0)
    return -1;

  return len - ZEBRA_HEADER_SIZE -4 - sizeof(struct ip6_hdr);
}
#else
static int ospf6_sendmsgDirect (struct in6_addr *src, struct in6_addr *dst, unsigned int *ifindex, struct iovec *message)
{
  int retval;
  struct msghdr smsghdr;
  struct cmsghdr *scmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 dst_sin6;

  assert (dst);
  assert (*ifindex);

  scmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));
  memset (&dst_sin6, 0, sizeof (struct sockaddr_in6));

  /* source address */
  pktinfo->ipi6_ifindex = *ifindex;
  if (src)
    memcpy (&pktinfo->ipi6_addr, src, sizeof (struct in6_addr));
  else
    memset (&pktinfo->ipi6_addr, 0, sizeof (struct in6_addr));

  /* destination address */
  dst_sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  dst_sin6.sin6_len = sizeof (struct sockaddr_in6);
#endif /*SIN6_LEN*/
  memcpy (&dst_sin6.sin6_addr, dst, sizeof (struct in6_addr));
#ifdef HAVE_SIN6_SCOPE_ID
  dst_sin6.sin6_scope_id = *ifindex;
#endif

  /* send control msg */
  scmsgp->cmsg_level = IPPROTO_IPV6;
  scmsgp->cmsg_type = IPV6_PKTINFO;
  scmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
  /* scmsgp = CMSG_NXTHDR (&smsghdr, scmsgp); */

  /* send msg hdr */
  memset (&smsghdr, 0, sizeof (smsghdr));
  smsghdr.msg_iov = message;
  smsghdr.msg_iovlen = iov_count (message);
  smsghdr.msg_name = (caddr_t) &dst_sin6;
  smsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  smsghdr.msg_control = (caddr_t) cmsgbuf;
  smsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = sendmsg (ospf6_sock, &smsghdr, 0);
  if (retval != iov_totallen (message))
    zlog_warn ("sendmsg failed: ifindex: %d: %s (%d)",
               *ifindex, safe_strerror (errno), errno);

  return retval;
}
#endif

int ospf6_sendmsg (struct in6_addr *src, struct in6_addr *dst, unsigned int *ifindex, struct iovec *message)
{
#ifndef HAVE_ZEBRA_CPA
  return ospf6_sendmsgDirect(src, dst, ifindex, message);
#else
  return ospf6_sendmsgViaZebra(src, dst, ifindex, message);
#endif
}

#ifndef HAVE_ZEBRA_CPA
int ospf6_recvmsg (struct in6_addr *src, struct in6_addr *dst, unsigned int *ifindex, struct iovec *message)
{
  int retval;
  struct msghdr rmsghdr;
  struct cmsghdr *rcmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 src_sin6;

  rcmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(rcmsgp));
  memset (&src_sin6, 0, sizeof (struct sockaddr_in6));

  /* receive control msg */
  rcmsgp->cmsg_level = IPPROTO_IPV6;
  rcmsgp->cmsg_type = IPV6_PKTINFO;
  rcmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
  /* rcmsgp = CMSG_NXTHDR (&rmsghdr, rcmsgp); */

  /* receive msg hdr */
  memset (&rmsghdr, 0, sizeof (rmsghdr));
  rmsghdr.msg_iov = message;
  rmsghdr.msg_iovlen = iov_count (message);
  rmsghdr.msg_name = (caddr_t) &src_sin6;
  rmsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  rmsghdr.msg_control = (caddr_t) cmsgbuf;
  rmsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = recvmsg (ospf6_sock, &rmsghdr, 0);
  if (retval < 0)
    zlog_warn ("recvmsg failed: %s", safe_strerror (errno));
  else if (retval == iov_totallen (message))
    zlog_warn ("recvmsg read full buffer size: %d", retval);

  /* source address */
  assert (src);
  memcpy (src, &src_sin6.sin6_addr, sizeof (struct in6_addr));

  /* destination address */
  if (ifindex)
    *ifindex = pktinfo->ipi6_ifindex;
  if (dst)
    memcpy (dst, &pktinfo->ipi6_addr, sizeof (struct in6_addr));

  return retval;
}
#endif /* HAVE_ZEBRA_CPA */

