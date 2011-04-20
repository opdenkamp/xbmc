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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>

typedef int socket_t;
typedef socket_t SOCKET;

static inline int setsocktimeout(int s, int level, int optname, uint64_t timeout)
{
      struct timeval t;
      t.tv_sec = timeout / 1000;
      t.tv_usec = (timeout % 1000) * 1000;
      return setsockopt(s, level, optname, (char *)&t, sizeof(t));
}
