#pragma once
/*
 *      Copyright (C) 2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef LIBTCPSOCKET_NET_WINSOCK_H
#define LIBTCPSOCKET_NET_WINSOCK_H

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#pragma warning(disable:4005) // Disable "warning C4005: '_WINSOCKAPI_' : macro redefinition"
#include <winsock2.h>
#pragma warning(default:4005)
typedef SOCKET socket_t;


typedef unsigned short in_port_t;
typedef unsigned short int ushort;
typedef unsigned int in_addr_t;
typedef int socklen_t;
typedef int uid_t;
typedef int gid_t;

#include <ws2spi.h>
#include <ws2ipdef.h>
#if defined(_MSC_VER) /* Microsoft C Compiler ONLY */
#pragma warning (pop)
#endif

#include <io.h>

/* Unix socket emulation macros */
#define __close closesocket
/*
#undef FD_CLR
#define FD_CLR(fd, set) do { \
    u_int __i; \
	   SOCKET __sock = _get_osfhandle(fd); \
    for (__i = 0; __i < ((fd_set FAR *)(set))->fd_count ; __i++) { \
        if (((fd_set FAR *)(set))->fd_array[__i] == __sock) { \
            while (__i < ((fd_set FAR *)(set))->fd_count-1) { \
                ((fd_set FAR *)(set))->fd_array[__i] = \
                    ((fd_set FAR *)(set))->fd_array[__i+1]; \
                __i++; \
            } \
            ((fd_set FAR *)(set))->fd_count--; \
            break; \
        } \
    } \
} while(0)

#undef FD_SET
#define FD_SET(fd, set) do { \
    u_int __i; \
    SOCKET __sock = _get_osfhandle(fd); \
    for (__i = 0; __i < ((fd_set FAR *)(set))->fd_count; __i++) { \
        if (((fd_set FAR *)(set))->fd_array[__i] == (__sock)) { \
            break; \
        } \
    } \
    if (__i == ((fd_set FAR *)(set))->fd_count) { \
        if (((fd_set FAR *)(set))->fd_count < FD_SETSIZE) { \
            ((fd_set FAR *)(set))->fd_array[__i] = (__sock); \
            ((fd_set FAR *)(set))->fd_count++; \
        } \
    } \
} while(0)

#undef FD_ISSET
#define FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(_get_osfhandle(fd)), (fd_set FAR *)(set))

#ifndef THREADLOCAL
#define THREADLOCAL __declspec(thread)
#endif

extern THREADLOCAL int ws32_result;
#define __poll(f,n,t) \
	(SOCKET_ERROR == WSAPoll(f,n,t) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __socket(f,t,p) \
	(INVALID_SOCKET == ((SOCKET)(ws32_result = (int)socket(f,t,p))) ? \
	((WSAEMFILE == (errno = WSAGetLastError()) ? errno = EMFILE : -1), -1) : \
	(SOCKET)_open_osfhandle(ws32_result,0))
#define __accept(s,a,l) \
	(INVALID_SOCKET == ((SOCKET)(ws32_result = (int)accept(_get_osfhandle(s),a,l))) ? \
	((WSAEMFILE == (errno = WSAGetLastError()) ? errno = EMFILE : -1), -1) : \
	(SOCKET)_open_osfhandle(ws32_result,0))
#define __bind(s,n,l) \
	(SOCKET_ERROR == bind(_get_osfhandle(s),n,l) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __connect(s,n,l) \
	(SOCKET_ERROR == connect(_get_osfhandle(s),n,l) ? \
	(WSAEMFILE == (errno = WSAGetLastError()) ? errno = EMFILE : -1, -1) : 0)
#define __listen(s,b) \
	(SOCKET_ERROR == listen(_get_osfhandle(s),b) ? \
	(WSAEMFILE == (errno = WSAGetLastError()) ? errno = EMFILE : -1, -1) : 0)
#define __shutdown(s,h) \
	(SOCKET_ERROR == shutdown(_get_osfhandle(s),h) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __select(n,r,w,e,t) \
	(SOCKET_ERROR == (ws32_result = select(n,r,w,e,t)) ? \
	(errno = WSAGetLastError()), -1 : ws32_result)
#define __recv(s,b,l,f) \
	(SOCKET_ERROR == (ws32_result = recv(_get_osfhandle(s),b,l,f)) ? \
  (errno = WSAGetLastError()), -1 : ws32_result)
#define __recvfrom(s,b,l,f,fr,frl) \
	(SOCKET_ERROR == (ws32_result = recvfrom(_get_osfhandle(s),b,l,f,fr,frl)) ? \
	(errno = WSAGetLastError()), -1 : ws32_result)
#define __send(s,b,l,f) \
	(SOCKET_ERROR == (ws32_result = send(_get_osfhandle(s),b,l,f)) ? \
	(errno = WSAGetLastError()), -1 : ws32_result)
#define __sendto(s,b,l,f,t,tl) \
	(SOCKET_ERROR == (ws32_result = sendto(_get_osfhandle(s),b,l,f,t,tl)) ? \
	(errno = WSAGetLastError()), -1 : ws32_result)
#define __getsockname(s,n,l) \
	(SOCKET_ERROR == getsockname(_get_osfhandle(s),n,l) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __getpeername(s,n,l) \
	(SOCKET_ERROR == getpeername(_get_osfhandle(s),n,l) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __getsockopt(s,l,o,v,n) \
	(Sleep(1), SOCKET_ERROR == getsockopt(_get_osfhandle(s),l,o,(char*)v,n) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __setsockopt(s,l,o,v,n) \
	(SOCKET_ERROR == setsockopt(_get_osfhandle(s),l,o,v,n) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __ioctlsocket(s,c,a) \
	(SOCKET_ERROR == ioctlsocket(_get_osfhandle(s),c,a) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __gethostname(n,l) \
	(SOCKET_ERROR == gethostname(n,l) ? \
	(errno = WSAGetLastError()), -1 : 0)
#define __gethostbyname(n) \
	(NULL == ((HOSTENT FAR*)(ws32_result = (int)gethostbyname(n))) ? \
	(errno = WSAGetLastError()), NULL : (HOSTENT FAR*)ws32_result)
#define __getservbyname(n,p) \
	(NULL == ((SERVENT FAR*)(ws32_result = (int)getservbyname(n,p))) ? \
	(errno = WSAGetLastError()), NULL : (SERVENT FAR*)ws32_result)
#define __gethostbyaddr(a,l,t) \
	(NULL == ((HOSTENT FAR*)(ws32_result = (int)gethostbyaddr(a,l,t))) ? \
	(errno = WSAGetLastError()), NULL : (HOSTENT FAR*)ws32_result)
extern THREADLOCAL int _so_err;
extern THREADLOCAL int _so_err_siz;
#define __read(fd,buf,siz) \
	(_so_err_siz = sizeof(_so_err), \
	__getsockopt((fd),SOL_SOCKET,SO_ERROR,&_so_err,&_so_err_siz) \
	== 0 ? __recv((fd),(char *)(buf),(siz),0) : _read((fd),(char *)(buf),(siz)))
#define __write(fd,buf,siz) \
	(_so_err_siz = sizeof(_so_err), \
	__getsockopt((fd),SOL_SOCKET,SO_ERROR,&_so_err,&_so_err_siz) \
	== 0 ? __send((fd),(const char *)(buf),(siz),0) : _write((fd),(const char *)(buf),(siz)))
*/
#endif /* LIBTCPSOCKET_NET_WINSOCK_H */

