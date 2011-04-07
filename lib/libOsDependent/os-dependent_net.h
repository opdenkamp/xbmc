/*
 *  Networking
 *  Copyright (C) 2007-2008 Andreas Öman
 *  Copyright (C) 2011 Lucian Muresan
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef OSDEP_NET_H__
#define OSDEP_NET_H__

#include <sys/types.h>
#include <stdint.h>

#ifdef _MSC_VER
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

socket_t tcp_connect(const char *hostname, int port, char *errbuf,
		size_t errbufsize, int timeout);

int tcp_read(socket_t fd, void *buf, size_t len);

int tcp_read_timeout(socket_t fd, void *buf, size_t len, int timeout);

void tcp_close(socket_t fd);

#endif /* OSDEP_NET_H__ */
