#pragma once
/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#ifndef LIBPLATFORM_OS_POSIX_H__
#define LIBPLATFORM_OS_POSIX_H__

#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
//#include <sys/signal.h>
//#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#ifndef __APPLE__
#include <sys/prctl.h> 
#endif
#include <pthread.h>
#include <poll.h>
#include <semaphore.h>

typedef int bool_t;
//
// Success codes
#define S_OK                             0L
#define S_FALSE                          1L
//
// Error codes
#define ERROR_FILENAME_EXCED_RANGE       206L
#define E_OUTOFMEMORY                    0x8007000EL

#define LIBTYPE

#define console_vprintf vprintf
#define console_printf printf
#define THREAD_FUNC_PREFIX void *

#ifndef __STL_CONFIG_H
template<class T> inline T min(T a, T b) { return a <= b ? a : b; }
template<class T> inline T max(T a, T b) { return a >= b ? a : b; }
template<class T> inline int sgn(T a) { return a < 0 ? -1 : a > 0 ? 1 : 0; }
template<class T> inline void swap(T &a, T &b) { T t = a; a = b; b = t; }
#endif

#define Sleep(t) usleep(t*1000)

typedef pthread_mutex_t criticalsection_t;
typedef sem_t waitevent_t;

#define PATH_SEPARATOR_CHAR '/'

static inline uint64_t getcurrenttime(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return ((uint64_t)t.tv_sec * 1000) + (t.tv_usec / 1000);
}

#endif /* LIBPLATFORM_OS_POSIX_H__ */
