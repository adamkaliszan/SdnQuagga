/* Zebra daemon server header.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
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
 *
 * Control Plane Adapter functionality:
 * Adam Kaliszan 2012 adam.kaliszan@gmail.com
 */

#ifndef _ZEBRA_ZSERV_H
#define _ZEBRA_ZSERV_H

#include <stdint.h>

#include "rib.h"
#include "if.h"
#include "workqueue.h"
#include "zclient.h"

#define ALLSPFROUTERS6 "ff02::5"
#define ALLDROUTERS6   "ff02::6"
#define IPPROTO_OSPFIGP 89

/* Default port information. */
#define ZEBRA_VTY_PORT                2601

/* Default configuration filename. */
#define DEFAULT_CONFIG_FILE "zebra.conf"


struct in6_addr allspfrouters6;
struct in6_addr alldrouters6;

/* Client structure. */
struct zserv
{
  /* Client file descriptor. */
  int sock;
  
  /** Socket for sending and ceceiving routing messages via Data Plane interfaces */
  int sock_net;
  int sock_net_domain; 
  int sock_net_type; 
  int sock_net_protocol; 

  /* Input/output buffer to the client. */
  struct stream *ibuf;
  struct stream *obuf;
  
  
  /** NET packed received buffer */
  struct iovec message_net[2];
  u_char message_net_buffer[ZEBRA_MAX_PACKET_SIZ];
  
  
  /* Buffer of data waiting to be written to client. */
  struct buffer *wb;

  /* Threads for read/write. */
  struct thread *t_read;
  struct thread *t_read_net;
  struct thread *t_write;

  /* Thread for delayed close. */
  struct thread *t_suicide;

  /* default routing table this client munges */
  int rtm_table;

  /* This client's redistribute flag. */
  u_char redist[ZEBRA_ROUTE_MAX];

  /* Redistribute default route flag. */
  u_char redist_default;

  /* Interface information. */
  u_char ifinfo;

  /* Router-id information. */
  u_char ridinfo;
};

/* Zebra instance */
struct zebra_t
{
  /* Thread master */
  struct thread_master *master;
  struct list *client_list;

  /* default table */
  int rtm_table_default;

  /* rib work queue */
  struct work_queue *ribq;
  struct meta_queue *mq;
};

/* Count prefix size from mask length */
#define PSIZE(a) (((a) + 7) / (8))

/* Prototypes. */
extern void zebra_init (void);
extern void zebra_if_init (void);
extern void zebra_zserv_socket_init (char *path);
extern void hostinfo_get (void);
extern void rib_init (void);
extern void interface_list (void);
extern void kernel_init (void);
extern void route_read (void);
extern void zebra_route_map_init (void);
extern void zebra_snmp_init (void);
extern void zebra_vty_init (void);

extern int zsend_interface_add (struct zserv *, struct interface *);
extern int zsend_interface_delete (struct zserv *, struct interface *);
extern int zsend_interface_address (int, struct zserv *, struct interface *,
                                    struct connected *);
extern int zsend_interface_update (int, struct zserv *, struct interface *);
extern int zsend_route_multipath (int, struct zserv *, struct prefix *, 
                                  struct rib *, uint16_t virtualNetworkNo);
extern int zsend_router_id_update(struct zserv *, struct prefix *, uint16_t virtualNetworkNo);

extern pid_t pid;

#endif /* _ZEBRA_ZEBRA_H */
