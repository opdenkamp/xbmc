/*
 *  Networking under POSIX
 *  Copyright (C) 2007-2008 Andreas Öman
 *  Copyright (C) 2011 Team XBMC
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

#include <netdb.h>
#ifdef __APPLE__
/* Needed on Mac OS/X */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#include "OSXGNUReplacements.h"
#elif defined(__FreeBSD__)
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#else
#include <sys/epoll.h>
#endif
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#if defined(__FreeBSD__)
#include <sys/socket.h>
#endif

#include "../os-dependent_socket.h"

socket_t
tcp_connect_addr(struct addrinfo* addr, char *errbuf, size_t errbufsize,
    int timeout)
{
  socket_t fd;
  int r, err, val;
  socklen_t errlen = sizeof(int);

  fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if(fd == -1) {
    snprintf(errbuf, errbufsize, "Unable to create socket: %s", strerror(errno));
    return -1;
  }

  /**
   * Switch to nonblocking
   */
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  r = connect(fd, addr->ai_addr, addr->ai_addrlen);

  if(r == -1) {
    if(errno == EINPROGRESS) {
      struct pollfd pfd;

      pfd.fd = fd;
      pfd.events = POLLOUT;
      pfd.revents = 0;

      r = poll(&pfd, 1, timeout);
      if(r == 0) {
        /* Timeout */
        snprintf(errbuf, errbufsize, "Connection attempt timed out");
        close(fd);
        return -1;
      }

      if(r == -1) {
        snprintf(errbuf, errbufsize, "poll() error: %s", strerror(errno));
        close(fd);
        return -1;
      }

      getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);
    } else {
      err = errno;
    }
  } else {
    err = 0;
  }

  if(err != 0) {
    snprintf(errbuf, errbufsize, "%s", strerror(err));
    close(fd);
    return -1;
  }

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

  val = 1;
  setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val));

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
  socket_t fd = -1;

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
    return -1;
  }

  for(addr = result; addr; addr = addr->ai_next) {
    fd = tcp_connect_addr(addr, errbuf, errbufsize, timeout);
    if(fd != -1)
      break;
  }

  freeaddrinfo(result);
  return fd;
}

int
tcp_read(socket_t fd, void *buf, size_t len)
{
  int x = recv(fd, buf, len, MSG_WAITALL);

  if(x == -1)
    return errno;
  if(x != (int)len)
    return ECONNRESET;
  return 0;
}

int
tcp_read_timeout(socket_t fd, void *buf, size_t len, int timeout)
{
  int x, tot = 0;
  struct pollfd fds;

  if(timeout > 0)
    return EINVAL;

  fds.fd = fd;
  fds.events = POLLIN;
  fds.revents = 0;

  while(tot != (int)len) {

    x = poll(&fds, 1, timeout);
    if(x == 0)
      return ETIMEDOUT;

    x = recv(fd, buf + tot, len - tot, MSG_DONTWAIT);
    if(x == -1) {
      if(errno == EAGAIN)
        continue;
      return errno;
    }

    if(x == 0)
      return ECONNRESET;

    tot += x;
  }
  return 0;
}

void
tcp_close(socket_t fd)
{
  close(fd);
}
