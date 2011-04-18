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

#ifndef LIBPLATFORM_OS_WIN_H
#define LIBPLATFORM_OS_WIN_H

#pragma warning(disable:4005) // Disable "warning C4005: '_WINSOCKAPI_' : macro redefinition"
#include <winsock2.h>
#pragma warning(default:4005)

#include "../pthread_win32/pthread.h"

#if defined _FILE_OFFSET_BITS && _FILE_OFFSET_BITS == 64
# define __USE_FILE_OFFSET64	1
#endif

//typedef CRITICAL_SECTION pthread_mutex_t;

//typedef int ssize_t;
typedef int mode_t;
typedef int bool_t;
//typedef __int8 int8_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;
typedef signed __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#if defined __USE_FILE_OFFSET64
typedef int64_t off_t;
typedef uint64_t ino_t;
#else
typedef long off_t;
#endif

#define NAME_MAX         255   /* # chars in a file name */
#define MAXPATHLEN       255
//#define INT64_MAX _I64_MAX
//#define INT64_MIN _I64_MIN

#ifndef S_ISLNK
# define S_ISLNK(x) 0
#endif

#ifndef S_ISREG
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#ifndef DEFFILEMODE
#define DEFFILEMODE 0
#endif

#define alloca _alloca
#define chdir _chdir
#define dup _dup
#define dup2 _dup2
#define fdopen _fdopen
#define fileno _fileno
#define getcwd _getcwd
#define getpid _getpid
#define ioctl ioctlsocket
#define mkdir(p) _mkdir(p)
#define mktemp _mktemp
#define open _open
#define pclose _pclose
#define popen _popen
#define putenv _putenv
#define setmode _setmode
#define sleep(t) Sleep((t)*1000)
#define usleep(t) Sleep((t)/1000)
#define snprintf _snprintf
#define strcasecmp _stricmp
#define strdup _strdup
#define strlwr _strlwr
#define strncasecmp _strnicmp
#define tempnam _tempnam
#define umask _umask
#define unlink _unlink
#define close _close

#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND

#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL

#define O_TEXT          _O_TEXT
#define O_BINARY        _O_BINARY
#define O_RAW           _O_BINARY
#define O_TEMPORARY     _O_TEMPORARY
#define O_NOINHERIT     _O_NOINHERIT
#define O_SEQUENTIAL    _O_SEQUENTIAL
#define O_RANDOM        _O_RANDOM
#define O_NDELAY	0

#define S_IRWXO 007
//#define	S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
//#define	S_ISREG(m) (((m) & _S_IFREG) == _S_IFREG)

#ifndef SIGHUP
#define	SIGHUP	1	/* hangup */
#endif
#ifndef SIGBUS
#define	SIGBUS  7	/* bus error */
#endif
#ifndef SIGKILL
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#endif
#ifndef	SIGSEGV
#define	SIGSEGV 11      /* segment violation */
#endif
#ifndef SIGPIPE
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#endif
#ifndef SIGCHLD
#define	SIGCHLD	20	/* to parent on child stop or exit */
#endif
#ifndef SIGUSR1
#define SIGUSR1 30	/* user defined signal 1 */
#endif
#ifndef SIGUSR2
#define SIGUSR2 31	/* user defined signal 2 */
#endif

typedef unsigned short in_port_t;
typedef unsigned short int ushort;
typedef unsigned int in_addr_t;
typedef int socklen_t;
typedef int uid_t;
typedef int gid_t;

#if defined __USE_FILE_OFFSET64
#define stat _stati64
#define lseek _lseeki64
#define fstat _fstati64
#define tell _telli64
#else
#define stat _stat
#define lseek _lseek
#define fstat _fstat
#define tell _tell
#endif

#define atoll _atoi64
#ifndef va_copy
#define va_copy(x, y) x = y
#endif

#include <stddef.h>
#include <process.h>
#if defined(_MSC_VER) /* Microsoft C Compiler ONLY */
#pragma warning (push)
/* Hack to suppress compiler warnings on FD_SET() & FD_CLR() */
#pragma warning (disable:4142)
/* Suppress compiler warnings about double definition of _WINSOCKAPI_ */
#pragma warning (disable:4005)
#endif
/* prevent inclusion of wingdi.h */
#define NOGDI
#if defined(_MSC_VER) /* Microsoft C Compiler ONLY */
#pragma warning (pop)
#endif
#include <io.h>
#include <stdlib.h>
#include <errno.h>

typedef char * caddr_t;

#undef FD_CLOSE
#undef FD_OPEN
#undef FD_READ
#undef FD_WRITE

#undef h_errno
#define h_errno errno /* we'll set it ourselves */

struct timezone
{
  int	tz_minuteswest;	/* minutes west of Greenwich */
  int	tz_dsttime;	/* type of dst correction */
};

extern int gettimeofday(struct timeval *, struct timezone *);

#if !defined(__MINGW32__)
#define strtok_r( _s, _sep, _lasts ) \
	( *(_lasts) = strtok( (_s), (_sep) ) )
#endif /* !__MINGW32__ */

#define asctime_r( _tm, _buf ) \
	( strcpy( (_buf), asctime( (_tm) ) ), \
	  (_buf) )

#define ctime_r( _clock, _buf ) \
	( strcpy( (_buf), ctime( (_clock) ) ),  \
	  (_buf) )

#define gmtime_r( _clock, _result ) \
	( *(_result) = *gmtime( (_clock) ), \
	  (_result) )

#define localtime_r( _clock, _result ) \
	( *(_result) = *localtime( (_clock) ), \
	  (_result) )

#define rand_r( _seed ) \
	( _seed == _seed? rand() : rand() )

#endif /* LIBPLATFORM_OS_WIN_H */
