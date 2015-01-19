/*
  Copyright 2012-2014 Christopher E. Miller
  License: GPLv2, see LICENSE file.
*/

#ifndef _FILEIO_H_398173
#define _FILEIO_H_398173


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#if defined(WIN32) || defined(WIN64) || defined(WINNT)
#define _ON_WINDOWS_ 1
#include <windows.h>
int _widetoutf8(const WCHAR *w, int wlen, char *buf, int buflen);
int _utf8towide(const char *u8, int u8len, WCHAR *buf, int buflen);
#endif


#define FILEIO_FLAGS_ANSI 0x1000 /* Windows only; ignored otherwise. */
#define FILEIO_FLAGS_TEST 0x2000 /* Testing; fake curdir, etc. */
#define FILEIO_FLAGS_MASK 0xF000


#define LISTDIR_FLAGS_FILE 1
#define LISTDIR_FLAGS_DIRECTORY 2
#define LISTDIR_FLAGS_HIDDEN 4
#define LISTDIR_FLAGS_CURRENT 8 /* "." entries */
#define LISTDIR_FLAGS_PARENT 0x10 /* ".." entries */
#define LISTDIR_FLAGS_NAMEONLY 0x20 /* Want names of files and directories, not full path. */

/*
	Returns nonzero on error.
	callback returns 0 to stop.
	Input path can be relative or absolute.
	callback path will always be absolute, unless LISTDIR_FLAGS_NAMEONLY specified.
	Input flags is one or more LISTDIR_FLAGS_* controlling how to list the directory.
	callback flags will be one or more LISTDIR_FLAGS_* with attributes for the current path.
	UTF-8 by default, unless FILEIO_FLAGS_ANSI on Windows.
*/
int listdir(const char *path, int flags, int (*callback)(const char *, int cflags));


static int isabsolutepath(const char *path)
{
#ifdef _ON_WINDOWS_
	return (path[0] && ':' == path[1] && ('\\' == path[2] || '/' == path[2]))
		|| (path[0] == '\\' && path[1] == '\\')
		;
#else
	return '/' == path[0];
#endif
}


#ifdef _ON_WINDOWS_
#include <windows.h>
#define FILEIO_MAX_PATH_LENGTH MAX_PATH
#define FILEIO_DIRECTORY_SEPARATOR '\\'
#define FILEIO_DIRECTORY_SEPARATOR_ALT '/'
#define FILEIO_TEST_CURRENTDIRECTORY "C:\\"
#else
#include <unistd.h>
#include <sys/param.h>
#define FILEIO_MAX_PATH_LENGTH MAXPATHLEN
#define FILEIO_DIRECTORY_SEPARATOR '/'
#define FILEIO_DIRECTORY_SEPARATOR_ALT '/'
#define FILEIO_TEST_CURRENTDIRECTORY "/tmp/"
#endif

/*
	Returns 0 on success.
	If buflen is not large enough, the required length is returned (including nul byte).
	Other errors return negative.
	UTF-8 by default, unless FILEIO_FLAGS_ANSI on Windows.
*/
int getcurrentdirectory(int flags, char *buf, int buflen);


/*
	Returns 0 on success.
	Providing sufficient space, dirbuf will always end with a directory separator.
	UTF-8 by default, unless FILEIO_FLAGS_ANSI on Windows.
	Returns error: -1 if dirbuf is not large enough, -2 if filebuf is not large enough, or -3 for both.
	If dirbuf is NULL and dirbuflen is 0, the directory portion is not copied and it is not an error.
	If filebuf is NULL and filebuflen is 0, the file portion is not copied and it is not an error.
*/
/*
	flags for:
		preserving dir separators,
		don't make absolute,
int getpathparts(const char *path, int flags, char *dirbuf, int dirbuflen, char* filebuf, int filebuflen);
*/


#define GETABSOLUTEPATH_FLAGS_TRYNOTCOPY 1

/*
	Returns 0 on success.
	If buflen is not large enough, the required length is returned (including nul byte).
	Other errors return negative.
	If GETABSOLUTEPATH_FLAGS_TRYNOTCOPY is specified, returns -110 if path is already absolute.
	UTF-8 by default, unless FILEIO_FLAGS_ANSI on Windows.
*/
int getabsolutepath(const char *path, int flags, char *buf, int buflen);


static FILE *fileopen(const char *fn, const char *mode, int flags)
{
	char fp[FILEIO_MAX_PATH_LENGTH];
	if(0 != getabsolutepath(fn, flags & FILEIO_FLAGS_MASK, fp, sizeof(fp)))
	{
#ifdef _ON_WINDOWS_
		_set_errno(ENAMETOOLONG);
#else
		errno = ENAMETOOLONG;
#endif
		return NULL;
	}
#ifdef _ON_WINDOWS_
	if(!(flags & FILEIO_FLAGS_ANSI))
	{
		WCHAR wfp[FILEIO_MAX_PATH_LENGTH];
		WCHAR wmode[32];
		if(0 != _utf8towide(fn, -1, wfp, FILEIO_MAX_PATH_LENGTH)
			|| 0 != _utf8towide(mode, -1, wmode, 32))
		{
			_set_errno(ENAMETOOLONG);
			return NULL;
		}
		return _wfopen(wfp, wmode);
	}
#endif
	return fopen(fp, mode);
}


#ifdef _ON_WINDOWS_
#include <wchar.h>
#include <io.h>
#endif

static int fileprint(FILE *f, const char *s, int flags)
{
#ifdef _ON_WINDOWS_
	if(!(flags & FILEIO_FLAGS_ANSI))
	{
		if(f == stdout || f == stderr)
		{
			int result = EOF;
			UINT oldoutcp = GetConsoleOutputCP();
			/*UINT oldcp = GetConsoleCP();*/
			SetConsoleOutputCP(CP_UTF8);
			result = fputs(s, f);
			SetConsoleOutputCP(oldoutcp);
			/*SetConsoleCP(oldcp);*/
			return result;
		}
	}
#endif
	return fputs(s, f);
}


static int fileclose(FILE *f)
{
	return fclose(f);
}


#ifdef _DEBUG
void fileio_AllTests();
#endif


#endif
