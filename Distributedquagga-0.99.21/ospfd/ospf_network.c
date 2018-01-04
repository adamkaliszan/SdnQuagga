/*
 * OSPF network related functions
 *   Copyright (C) 1999 Toshiaki Takada
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include "thread.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "sockunion.h"
#include "log.h"
#include "sockopt.h"
#include "privs.h"
#include "zclient.h"

extern struct zebra_privs_t ospfd_privs;

#include "ospfd/ospfd.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_dump.h"

static int CPA_setsockopt(int level, int optname, const void *optval, socklen_t optlen)
{
  struct stream *s;
  s = zclient->obuf;

  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, level);  
  stream_putl(s, optname);  
  stream_put(s, optval, optlen);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("CPA_setsockopt: Failed to send message");
  else
    zlog_debug("CPA_setsockopt: (%d, %d len %d)", level, optname, optlen);
  return 0;
}

static int setsockopt_ipv4_multicast_zebra(int optname, unsigned int mcast_addr, unsigned int ifindex)
{
//  ret = setsockopt_ipv4_multicast (top->fd, IP_ADD_MEMBERSHIP, htonl (OSPF_ALLSPFROUTERS), ifindex);
//  int setsockopt_ipv4_multicast(int sock, int optname, unsigned int mcast_addr, unsigned int ifindex)
  #ifdef HAVE_RFC3678
  struct group_req gr;
  struct sockaddr_in *si;
  int ret;
  memset (&gr, 0, sizeof(gr));
  si = (struct sockaddr_in *)&gr.gr_group;
  gr.gr_interface = ifindex;
  si->sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  si->sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */
  si->sin_addr.s_addr = mcast_addr;
  ret = CPA_setsockopt(IPPROTO_IP, (optname == IP_ADD_MEMBERSHIP) ? MCAST_JOIN_GROUP : MCAST_LEAVE_GROUP, (void *)(&gr), sizeof(gr));
  if ((ret < 0) && (optname == IP_ADD_MEMBERSHIP) && (errno == EADDRINUSE))
  {
    CPA_setsockopt(IPPROTO_IP, MCAST_LEAVE_GROUP, (void *)(&gr), sizeof(gr));
    ret = CPA_setsockopt(IPPROTO_IP, MCAST_JOIN_GROUP, (void *)(&gr), sizeof(gr));
  }
  return ret;

#elif defined(HAVE_STRUCT_IP_MREQN_IMR_IFINDEX) && !defined(__FreeBSD__)
  struct ip_mreqn mreqn;
  int ret;
  
  assert(optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP);
  memset (&mreqn, 0, sizeof(mreqn));

  mreqn.imr_multiaddr.s_addr = mcast_addr;
  mreqn.imr_ifindex = ifindex;
  
  ret = CPA_setsockopt(IPPROTO_IP, optname, (void *)&mreqn, sizeof(mreqn));
  if ((ret < 0) && (optname == IP_ADD_MEMBERSHIP) && (errno == EADDRINUSE))
  {
    /* see above: handle possible problem when interface comes back up */
    char buf[1][INET_ADDRSTRLEN];
    zlog_info("setsockopt_ipv4_multicast attempting to drop and re-add (mcast %s, ifindex %u)", inet_ntop(AF_INET, &mreqn.imr_multiaddr, buf[0], sizeof(buf[0])), ifindex);
    CPA_setsockopt(IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *)&mreqn, sizeof(mreqn));
    ret = CPA_setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreqn, sizeof(mreqn));
  }
  return ret;

  /* Example defines for another OS, boilerplate off other code in this
     function, AND handle optname as per other sections for consistency !! */
  /* #elif  defined(BOGON_NIX) && EXAMPLE_VERSION_CODE > -100000 */
  /* Add your favourite OS here! */

#elif defined(HAVE_BSD_STRUCT_IP_MREQ_HACK) /* #if OS_TYPE */ 
  /* standard BSD API */

  struct in_addr m;
  struct ip_mreq mreq;
  int ret;

  assert(optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP);

  m.s_addr = htonl(ifindex);

  memset (&mreq, 0, sizeof(mreq));
  mreq.imr_multiaddr.s_addr = mcast_addr;
  mreq.imr_interface = m;
  
  ret = CPA_setsockopt(IPPROTO_IP, optname, (void *)&mreq, sizeof(mreq));
  if ((ret < 0) && (optname == IP_ADD_MEMBERSHIP) && (errno == EADDRINUSE))
  {
    /* see above: handle possible problem when interface comes back up */
    char buf[1][INET_ADDRSTRLEN];
    zlog_info("setsockopt_ipv4_multicast attempting to drop and re-add (mcast %s, ifindex %u)", inet_ntop(AF_INET, &mreq.imr_multiaddr, buf[0], sizeof(buf[0])), ifindex);
    CPA_setsockopt(IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *)&mreq, sizeof(mreq));
    ret = CPA_setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq));
  }
  return ret;

#else
  #error "Unsupported multicast API"
#endif /* #if OS_TYPE */
}

static int setsockopt_ipv4_multicast_if_zebra(unsigned int ifindex)
{  
  #ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
  struct ip_mreqn mreqn;
  memset (&mreqn, 0, sizeof(mreqn));

  mreqn.imr_ifindex = ifindex;
  return CPA_setsockopt(IPPROTO_IP, IP_MULTICAST_IF, (void *)&mreqn, sizeof(mreqn));

  /* Example defines for another OS, boilerplate off other code in this
     function */
  /* #elif  defined(BOGON_NIX) && EXAMPLE_VERSION_CODE > -100000 */
  /* Add your favourite OS here! */
#elif defined(HAVE_BSD_STRUCT_IP_MREQ_HACK)
  struct in_addr m;

  m.s_addr = htonl(ifindex);

  return CPA_setsockopt (IPPROTO_IP, IP_MULTICAST_IF, (void *)&m, sizeof(m));
#else
  #error "Unsupported multicast API"
#endif
}

/* Join to the OSPF ALL SPF ROUTERS multicast group. */
int ospf_if_add_allspfrouters (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  int ret;

#ifdef HAVE_ZEBRA_CPA
  ret = setsockopt_ipv4_multicast_zebra (IP_ADD_MEMBERSHIP, htonl (OSPF_ALLSPFROUTERS), ifindex);
#else
  ret = setsockopt_ipv4_multicast (top->fd, IP_ADD_MEMBERSHIP, htonl (OSPF_ALLSPFROUTERS), ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_ADD_MEMBERSHIP (fd %d, addr %s, ifindex %u, AllSPFRouters): %s; perhaps a kernel limit on # of multicast group memberships has been exceeded?", top->fd, inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
  else
    zlog_debug ("interface %s [%u] join AllSPFRouters Multicast group.", inet_ntoa (p->u.prefix4), ifindex);
#endif
  return ret;
}

int ospf_if_drop_allspfrouters (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  int ret;
  
#ifdef HAVE_ZEBRA_CPA
  ret = setsockopt_ipv4_multicast_zebra (IP_DROP_MEMBERSHIP, htonl (OSPF_ALLSPFROUTERS), ifindex);
#else
  ret = setsockopt_ipv4_multicast (top->fd, IP_DROP_MEMBERSHIP, htonl (OSPF_ALLSPFROUTERS), ifindex);  
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_DROP_MEMBERSHIP (fd %d, addr %s, ifindex %u, AllSPFRouters): %s", top->fd, inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
  else
    zlog_debug ("interface %s [%u] leave AllSPFRouters Multicast group.", inet_ntoa (p->u.prefix4), ifindex);
#endif

  return ret;
}

/* Join to the OSPF ALL Designated ROUTERS multicast group. */
int ospf_if_add_alldrouters (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  int ret;

#ifdef HAVE_ZEBRA_CPA
  ret = setsockopt_ipv4_multicast_zebra (IP_ADD_MEMBERSHIP, htonl (OSPF_ALLDROUTERS), ifindex);
#else
  ret = setsockopt_ipv4_multicast (top->fd, IP_ADD_MEMBERSHIP, htonl (OSPF_ALLDROUTERS), ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_ADD_MEMBERSHIP (fd %d, addr %s, ifindex %u, AllDRouters): %s; perhaps a kernel limit on # of multicast group memberships has been exceeded?", top->fd, inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
  else
    zlog_debug ("interface %s [%u] join AllDRouters Multicast group.", inet_ntoa (p->u.prefix4), ifindex);
#endif
  return ret;
}

int ospf_if_drop_alldrouters (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  int ret;

#ifdef HAVE_ZEBRA_CPA
  ret = setsockopt_ipv4_multicast_zebra (IP_DROP_MEMBERSHIP, htonl (OSPF_ALLDROUTERS), ifindex);
#else
  ret = setsockopt_ipv4_multicast (top->fd, IP_DROP_MEMBERSHIP, htonl (OSPF_ALLDROUTERS), ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_DROP_MEMBERSHIP (fd %d, addr %s, ifindex %u, AllDRouters): %s", top->fd, inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
  else
    zlog_debug ("interface %s [%u] leave AllDRouters Multicast group.", inet_ntoa (p->u.prefix4), ifindex);
#endif
  return ret;
}

int ospf_if_ipmulticast (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  u_char val;
  int ret, len;
  
  val = 0;
  len = sizeof (val);
  
#ifdef HAVE_ZEBRA_CPA
  //TODO zmodyfikować
  /* Prevent receiving self-origined multicast packets. */
  
  //ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void *)&val, len);
  struct stream *s;
  s = zclient->obuf;

  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IP);  
  stream_putl(s, IP_MULTICAST_LOOP);  
  stream_put(s, &val, len);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("ospf_if_ipmulticast: Failed to send message");
  else
    zlog_debug("ospf_if_ipmulticast: (%d, %d len %d) ifindex %d", IPPROTO_IP, IP_MULTICAST_LOOP, len, ifindex);
  ret = 0;
  
  /* Explicitly set multicast ttl to 1 -- endo. */
  
  val = 1;
  //ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&val, len);

  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IP);  
  stream_putl(s, IP_MULTICAST_TTL);  
  stream_put(s, &val, len);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("ospf_if_ipmulticast: Failed to send message");
  else
    zlog_debug("ospf_if_ipmulticast: (%d, %d len %d) ifindex %d", IPPROTO_IP, IP_MULTICAST_TTL, len, ifindex);
  ret = 0;


  ret = setsockopt_ipv4_multicast_if_zebra (ifindex);
  if (ret < 0)
    zlog_warn("can't setsockopt IP_MULTICAST_IF(addr %s, ifindex %u): %s", inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
#else
  /* Prevent receiving self-origined multicast packets. */
  ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void *)&val, len);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_MULTICAST_LOOP(0) for fd %d: %s", top->fd, safe_strerror(errno));
  
  /* Explicitly set multicast ttl to 1 -- endo. */
  val = 1;
  ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&val, len);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_MULTICAST_TTL(1) for fd %d: %s", top->fd, safe_strerror (errno));

  ret = setsockopt_ipv4_multicast_if (top->fd, ifindex);
  if (ret < 0)
    zlog_warn("can't setsockopt IP_MULTICAST_IF(fd %d, addr %s, ifindex %u): %s", top->fd, inet_ntoa(p->u.prefix4), ifindex, safe_strerror(errno));
#endif
  return ret;
}


#ifndef HAVE_ZEBRA_CPA
int ospf_sock_init (void)
{
  int ospf_sock;
  int ret, hincl = 1;

  if ( ospfd_privs.change (ZPRIVS_RAISE) )
    zlog_err ("ospf_sock_init: could not raise privs, %s", safe_strerror (errno) );
    
  ospf_sock = socket (AF_INET, SOCK_RAW, IPPROTO_OSPFIGP);
  if (ospf_sock < 0)
  {
    int save_errno = errno;
    if ( ospfd_privs.change (ZPRIVS_LOWER) )
      zlog_err ("ospf_sock_init: could not lower privs, %s", safe_strerror (errno) );
    zlog_err ("ospf_read_sock_init: socket: %s", safe_strerror (save_errno));
    exit(1);
  }
    
#ifdef IP_HDRINCL
  /* we will include IP header with packet */
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof (hincl));
  if (ret < 0)
    {
      int save_errno = errno;
      if ( ospfd_privs.change (ZPRIVS_LOWER) )
        zlog_err ("ospf_sock_init: could not lower privs, %s",
                   safe_strerror (errno) );
      zlog_warn ("Can't set IP_HDRINCL option for fd %d: %s",
      		 ospf_sock, safe_strerror(save_errno));
    }
#elif defined (IPTOS_PREC_INTERNETCONTROL)
#warning "IP_HDRINCL not available on this system"
#warning "using IPTOS_PREC_INTERNETCONTROL"
  ret = setsockopt_ipv4_tos(ospf_sock, IPTOS_PREC_INTERNETCONTROL);
  if (ret < 0)
  {
    int save_errno = errno;
    if ( ospfd_privs.change (ZPRIVS_LOWER) )
      zlog_err ("ospf_sock_init: could not lower privs, %s", safe_strerror (errno) );
    zlog_warn ("can't set sockopt IP_TOS %d to socket %d: %s", tos, ospf_sock, safe_strerror(save_errno));
    close (ospf_sock);	/* Prevent sd leak. */
    return ret;
  }
#else /* !IPTOS_PREC_INTERNETCONTROL */
#warning "IP_HDRINCL not available, nor is IPTOS_PREC_INTERNETCONTROL"
  zlog_warn ("IP_HDRINCL option not available");
#endif /* IP_HDRINCL */

  ret = setsockopt_ifindex (AF_INET, ospf_sock, 1);

  if (ret < 0)
     zlog_warn ("Can't set pktinfo option for fd %d", ospf_sock);

  if (ospfd_privs.change (ZPRIVS_LOWER))
  {
    zlog_err ("ospf_sock_init: could not lower privs, %s", safe_strerror (errno) );
  }
 
  return ospf_sock;
}
#else
void createCPA_TP_socket(void)
{ 
  zlog_debug("createCPA_TP_socket START");

  int ret, hincl = 1;
  int val=1;
  struct stream *s;
  s= zclient->obuf;
  stream_reset(s);
  
  zclient_create_header (s, ZEBRA_CREATE_NET_SCK, 0);
  
  stream_putl(s, AF_INET);  
  stream_putl(s, SOCK_RAW);  
  stream_putl(s, IPPROTO_OSPFIGP);  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("Failed to send message to zebra server");
  else
    zlog_debug("createCPA_TP_socket (%d, %d, %d)", AF_INET, SOCK_RAW, IPPROTO_OSPFIGP);
   

#ifdef IP_HDRINCL
  /* we will include IP header with packet */
  //ret = setsockopt (ospf_sock, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof (hincl));
  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IP);  
  stream_putl(s, IP_HDRINCL);  
  stream_put(s, &hincl, sizeof (hincl));  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("createCPA_TP_socket: Failed to send message");
  else
    zlog_debug("createCPA_TP_socket: (%d, %d len %d)", IPPROTO_IP, IP_HDRINCL, sizeof (hincl));
    
#elif defined (IPTOS_PREC_INTERNETCONTROL)
#warning "IP_HDRINCL not available on this system"
#warning "using IPTOS_PREC_INTERNETCONTROL"
//TODO convert function setsockopt_ipv4_tos
#error not implemented
  ret = setsockopt_ipv4_tos(ospf_sock, IPTOS_PREC_INTERNETCONTROL);
  if (ret < 0)
  {
    int save_errno = errno;
    if ( ospfd_privs.change (ZPRIVS_LOWER) )
      zlog_err ("ospf_sock_init: could not lower privs, %s", safe_strerror (errno) );
    zlog_warn ("can't set sockopt IP_TOS %d to socket %d: %s", tos, ospf_sock, safe_strerror(save_errno));
    close (ospf_sock);	/* Prevent sd leak. */
    return ret;
  }
#else /* !IPTOS_PREC_INTERNETCONTROL */
#warning "IP_HDRINCL not available, nor is IPTOS_PREC_INTERNETCONTROL"
  zlog_warn ("IP_HDRINCL option not available");
#endif /* IP_HDRINCL */

  
#if defined (IP_PKTINFO)
  //ret = setsockopt (sock, IPPROTO_IP, IP_PKTINFO, &val, sizeof (val));
  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IP);  
  stream_putl(s, IP_PKTINFO);  
  stream_put(s, &val, sizeof (val));  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("createCPA_TP_socket: Failed to send message");
  else
    zlog_debug("createCPA_TP_socket: (%d, %d len %d)", IPPROTO_IP, IP_PKTINFO, sizeof (val));
#elif defined (IP_RECVIF)
  //ret = setsockopt (sock, IPPROTO_IP, IP_RECVIF, &val, sizeof (val));
  stream_reset(s);
  zclient_create_header (s, ZEBRA_SOCKOPT, 0);
  
  stream_putl(s, IPPROTO_IP);  
  stream_putl(s, IP_RECVIF);  
  stream_put(s, &val, sizeof (val));  
  
  stream_putw_at (s, 0, stream_get_endp (s));  
  if (zclient_send_message(zclient) != 0)
    zlog_err("createCPA_TP_socket: Failed to send message");
  else
    zlog_debug("createCPA_TP_socket: (%d, %d len %d)", IPPROTO_IP, IP_RECVIF, sizeof (val));
#else
#warning "Neither IP_PKTINFO nor IP_RECVIF is available."
#warning "Will not be able to receive link info."
#warning "Things might be seriously broken.."
  /* XXX Does this ever happen?  Should there be a zlog_warn message here? */
  ret = -1;
#endif  
}
#endif

#ifndef HAVE_ZEBRA_CPA
void ospf_adjust_sndbuflen (struct ospf * ospf, int buflen)
{
  int ret, newbuflen;
  /* Check if any work has to be done at all. */
  if (ospf->maxsndbuflen >= buflen)
    return;
  if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
    zlog_debug ("%s: adjusting OSPF send buffer size to %d", __func__, buflen);
  if (ospfd_privs.change (ZPRIVS_RAISE))
    zlog_err ("%s: could not raise privs, %s", __func__, safe_strerror (errno));
  /* Now we try to set SO_SNDBUF to what our caller has requested
   * (the MTU of a newly added interface). However, if the OS has
   * truncated the actual buffer size to somewhat less size, try
   * to detect it and update our records appropriately. The OS
   * may allocate more buffer space, than requested, this isn't
   * a error.
   */
  ret = setsockopt_so_sendbuf (ospf->fd, buflen);
  newbuflen = getsockopt_so_sendbuf (ospf->fd);
  if (ret < 0 || newbuflen < buflen)
    zlog_warn ("%s: tried to set SO_SNDBUF to %d, but got %d", __func__, buflen, newbuflen);
  if (newbuflen >= 0)
    ospf->maxsndbuflen = newbuflen;
  else
    zlog_warn ("%s: failed to get SO_SNDBUF", __func__);
  if (ospfd_privs.change (ZPRIVS_LOWER))
    zlog_err ("%s: could not lower privs, %s", __func__,
      safe_strerror (errno));
}
#else
void ospf_adjust_sndbuflen (struct ospf * ospf, int buflen)
{
  //TODO implement it
}
#endif
