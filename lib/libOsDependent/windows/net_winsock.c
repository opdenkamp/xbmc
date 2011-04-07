/*
 *  Networking under WINDOWS
 *  Copyright (C) 2007-2008 Andreas Öman
 *  Copyright (C) 2007-2008 Joakim Plate
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <winsock2.h>
#include <Ws2tcpip.h>
//#include "msvc.h"
#include "../os-dependent_net.h"

#define EINPROGRESS WSAEINPROGRESS
#define ECONNRESET  WSAECONNRESET
#define ETIMEDOUT   WSAETIMEDOUT
#define EAGAIN      WSAEWOULDBLOCK

#define snprintf _snprintf

#ifndef MSG_WAITALL
#define MSG_WAITALL 0x8
#endif

static int recv_fixed (SOCKET s, char * buf, int len, int flags)
{
  char* org = buf;
  int   res = 1;

  if((flags & MSG_WAITALL) == 0)
    return recv(s, buf, len, flags);

  flags &= ~MSG_WAITALL;
  while(len > 0 && res > 0)
  {
    res = recv(s, buf, len, flags);
    if(res < 0)
      return res;

    buf += res;
    len -= res;
  }
  return buf - org;
}
#define recv(s, buf, len, flags) recv_fixed(s, buf, len, flags)

/**
 *
 */
socket_t
tcp_connect_addr(struct addrinfo* addr, char *errbuf, size_t errbufsize,
	    int timeout)
{
  socket_t fd;
  int r, err, val;
  socklen_t errlen = sizeof(int);

  fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if(fd == -1) {
    snprintf(errbuf, errbufsize, "Unable to create socket: %s",
	     strerror(WSAGetLastError()));
    return -1;
  }

  /**
   * Switch to nonblocking
   */
  val = 1;
  ioctlsocket(fd, FIONBIO, &val);

  r = connect(fd, addr->ai_addr, addr->ai_addrlen);

  if(r == -1) {
    if(WSAGetLastError() == EINPROGRESS ||
       WSAGetLastError() == EAGAIN) {
      fd_set fd_write, fd_except;
      struct timeval tv;

      tv.tv_sec  =         timeout / 1000;
      tv.tv_usec = 1000 * (timeout % 1000);

      FD_ZERO(&fd_write);
      FD_ZERO(&fd_except);

      FD_SET(fd, &fd_write);
      FD_SET(fd, &fd_except);

      r = select((int)fd+1, NULL, &fd_write, &fd_except, &tv);

      if(r == 0) {
        /* Timeout */
        snprintf(errbuf, errbufsize, "Connection attempt timed out");
        closesocket(fd);
        return -1;
      }

      if(r == -1) {
        snprintf(errbuf, errbufsize, "select() error: %s", strerror(WSAGetLastError()));
        closesocket(fd);
        return -1;
      }

      getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);
    } else {
      err = WSAGetLastError();
    }
  } else {
    err = 0;
  }

  if(err != 0) {
    snprintf(errbuf, errbufsize, "%s", strerror(err));
    closesocket(fd);
    return -1;
  }

  val = 0;
  ioctlsocket(fd, FIONBIO, &val);

  val = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&val, sizeof(val));

  return fd;
}


socket_t
tcp_connect(const char *hostname, int port, char *errbuf, size_t errbufsize,
	    int timeout)
{
  struct   addrinfo hints;
  struct   addrinfo *result, *addr;
  char     service[33];
  int      res;
  socket_t fd = INVALID_SOCKET;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  sprintf(service, "%d", port);

  res = getaddrinfo(hostname, service, &hints, &result);
  if(res) {
    switch(res) {
    case EAI_NONAME:
      snprintf(errbuf, errbufsize, "The specified host is unknown");
      break;

    case EAI_FAIL:
      snprintf(errbuf, errbufsize, "A nonrecoverable failure in name resolution occurred");
      break;

    case EAI_MEMORY:
      snprintf(errbuf, errbufsize, "A memory allocation failure occurred");
      break;

    case EAI_AGAIN:
      snprintf(errbuf, errbufsize, "A temporary error occurred on an authoritative name server");
      break;

    default:
      snprintf(errbuf, errbufsize, "Unknown error %d", res);
      break;
    }
    return INVALID_SOCKET;
  }

  for(addr = result; addr; addr = addr->ai_next) {
    fd = tcp_connect_addr(addr, errbuf, errbufsize, timeout);
    if(fd != INVALID_SOCKET)
      break;
  }

  freeaddrinfo(result);
  return fd;
}



/**
 *
 */
int
tcp_read(socket_t fd, void *buf, size_t len)
{
  int x = recv(fd, buf, len, MSG_WAITALL);

  if(x == -1)
    return WSAGetLastError();
  if(x != len)
    return ECONNRESET;
  return 0;

}

/**
 *
 */
int
tcp_read_timeout(socket_t fd, char *buf, size_t len, int timeout)
{
  int x, tot = 0, val, err;
  fd_set fd_read;
  struct timeval tv;

  assert(timeout > 0);

  while(tot != len) {

    tv.tv_sec  =         timeout / 1000;
    tv.tv_usec = 1000 * (timeout % 1000);

    FD_ZERO(&fd_read);
    FD_SET(fd, &fd_read);

    x = select((int)fd+1, &fd_read, NULL, NULL, &tv);

    if(x == 0)
      return ETIMEDOUT;

    val = 1;
    ioctlsocket(fd, FIONBIO, &val);

    x   = recv(fd, buf + tot, len - tot, 0);
    err = WSAGetLastError();

    val = 0;
    ioctlsocket(fd, FIONBIO, &val);

    if(x == 0)
      return ECONNRESET;
    else if(x == -1)
    {
      if(err == EAGAIN)
        continue;
      return err;
    }

    tot += x;
  }
  return 0;
}

/**
 *
 */
void
tcp_close(socket_t fd)
{
  closesocket(fd);
}
