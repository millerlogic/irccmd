/*
  Copyright 2012-2014 Christopher E. Miller
  License: GPLv3, see LICENSE file.
*/

#include "fileio.h"

#include <string.h>


#ifdef _ON_WINDOWS_
#include <windows.h>
#else
#include <dirent.h>
#endif


#ifdef _ON_WINDOWS_
/* wlen can be -1 if null-terminated */
int _widetoutf8(const WCHAR *w, int wlen, char *buf, int buflen)
{
	int i = WideCharToMultiByte(CP_UTF8, 0, w, wlen, buf, buflen, NULL, NULL);
	if(!i)
		return -1;
	if(i + 1 > buflen)
		return i + 1;
	buf[i] = '\0';
	return 0;
}

/* u8len can be -1 if null-terminated.
	buflen is number of WCHAR.
*/
int _utf8towide(const char *u8, int u8len, WCHAR *buf, int buflen)
{
	int i = MultiByteToWideChar(CP_UTF8, 0, u8, u8len, buf, buflen);
	if(!i)
		return -1;
	if(i + 1 > buflen)
		return i + 1;
	buf[i] = '\0';
	return 0;
}
#endif


int listdir(const char *path, int flags, int (*callback)(const char *, int cflags))
{
	char fbuf[FILEIO_MAX_PATH_LENGTH + 32]; /* always starts with fpath ending with dir sep. */
#ifdef _ON_WINDOWS_
	union _Fi
	{
		WIN32_FIND_DATAW w;
		WIN32_FIND_DATAA a;
	}fi;
	HANDLE h;
#else
	DIR *dir;
#endif
	char fpath[FILEIO_MAX_PATH_LENGTH];
	int fpathlen, fbufbaselen;
	int initcflags = flags & (FILEIO_FLAGS_MASK | LISTDIR_FLAGS_NAMEONLY);
	int i;
	if(0 == (flags & (LISTDIR_FLAGS_FILE | LISTDIR_FLAGS_DIRECTORY)))
		return 0;
	i = getabsolutepath(path, (flags & FILEIO_FLAGS_MASK), fpath, FILEIO_MAX_PATH_LENGTH);
	if(0 != i)
		return i;
	fpathlen = strlen(fpath);
	if(!fpathlen) /* Strange, empty path even when absolute. */
		return -78;
	fbufbaselen = fpathlen;
	memcpy(fbuf, fpath, fpathlen);
	if(FILEIO_DIRECTORY_SEPARATOR != fbuf[fbufbaselen - 1]
		&& FILEIO_DIRECTORY_SEPARATOR_ALT != fbuf[fbufbaselen - 1]
		)
	{
		fbuf[fbufbaselen++] = FILEIO_DIRECTORY_SEPARATOR;
	}
	fbuf[fbufbaselen] = '\0';
	if(flags & LISTDIR_FLAGS_DIRECTORY)
	{
		/* TODO: only callback for these after the dir is confirmed to exist. */
		/* TODO: only callback for ".." when there ACTUALLY IS A PARENT! (e.g. not at root!). */
		/* TODO: need to check if dir is hidden and add LISTDIR_FLAGS_HIDDEN,
			only if flags contains LISTDIR_FLAGS_HIDDEN */
		if(flags & LISTDIR_FLAGS_CURRENT)
		{
			return -53; /* NOT SUPPORTED YET */
			if(flags & LISTDIR_FLAGS_NAMEONLY)
			{
				if(!callback(".", initcflags | LISTDIR_FLAGS_CURRENT | LISTDIR_FLAGS_DIRECTORY))
					return 0;
			}
			else
			{
				fbuf[fbufbaselen] = '.';
				fbuf[fbufbaselen + 1] = '\0';
				if(!callback(fbuf, initcflags | LISTDIR_FLAGS_CURRENT | LISTDIR_FLAGS_DIRECTORY))
					return 0;
			}
		}
		if(flags & LISTDIR_FLAGS_PARENT)
		{
			return -53; /* NOT SUPPORTED YET */
			if(flags & LISTDIR_FLAGS_NAMEONLY)
			{
				if(!callback("..", initcflags | LISTDIR_FLAGS_PARENT | LISTDIR_FLAGS_DIRECTORY))
					return 0;
			}
			else
			{
				fbuf[fbufbaselen] = '.';
				fbuf[fbufbaselen + 1] = '.';
				fbuf[fbufbaselen + 2] = '\0';
				if(!callback(fbuf, initcflags | LISTDIR_FLAGS_PARENT | LISTDIR_FLAGS_DIRECTORY))
					return 0;
			}
		}
	}
#ifdef _ON_WINDOWS_
	fbuf[fbufbaselen] = '*';
	fbuf[fbufbaselen + 1] = '\0';
	if(flags & FILEIO_FLAGS_ANSI)
	{
		h = FindFirstFileA(fbuf, &fi.a);
		if(INVALID_HANDLE_VALUE == h)
			return -441;
		while(1)
		{
			const char *cp = fi.a.cFileName;
			const char *cx = cp;
			if('.' == cx[0] && '\0' == cx[1])
			{
			}
			else if('.' == cx[0] && '.' == cx[1] && '\0' == cx[2])
			{
			}
			else
			{
				DWORD fiattribs = fi.a.dwFileAttributes;
				int cflags = initcflags;
				char ok = 1;
				if(fiattribs & FILE_ATTRIBUTE_DIRECTORY)
				{
					cflags |= LISTDIR_FLAGS_DIRECTORY;
					if(!(flags & LISTDIR_FLAGS_DIRECTORY))
						ok = 0;
				}
				else
				{
					cflags |= LISTDIR_FLAGS_FILE;
					if(!(flags & LISTDIR_FLAGS_FILE))
						ok = 0;
				}
				if(fiattribs & FILE_ATTRIBUTE_HIDDEN)
				{
					if(!(flags & LISTDIR_FLAGS_HIDDEN))
						ok = 0;
					cflags |= LISTDIR_FLAGS_HIDDEN;
				}
				if(ok)
				{
					if(flags & LISTDIR_FLAGS_NAMEONLY)
					{
						if(!callback(cx, cflags))
							break;
					}
					else
					{
						if(fbufbaselen + strlen(cx) < sizeof(fbuf))
						{
							strcpy(fbuf + fbufbaselen, cx);
							if(!callback(fbuf, cflags))
								break;
						}
					}
				}
			}
			if(!FindNextFileA(h, &fi.a))
				break;
		}
	}
	else
	{
		/* h = FindFirstFileW(fbuf, &fi.w); */ /* wrong string type */
		if(1)
		{
			WCHAR wfbuf[MAX_PATH];
			if(0 != _utf8towide(fbuf, -1, wfbuf, sizeof(wfbuf)))
				return -445;
			h = FindFirstFileW(wfbuf, &fi.w);
		}
		if(INVALID_HANDLE_VALUE == h)
			return -441;
		while(1)
		{
			const WCHAR *cwp = fi.w.cFileName;
			const WCHAR *cx = cwp;
			if('.' == cx[0]&& '\0' == cx[1])
			{
			}
			else if('.' == cx[0] && '.' == cx[1] && '\0' == cx[2])
			{
			}
			else
			{
				DWORD fiattribs = fi.w.dwFileAttributes;
				int cflags = initcflags;
				char ok = 1;
				if(fiattribs & FILE_ATTRIBUTE_DIRECTORY)
				{
					cflags |= LISTDIR_FLAGS_DIRECTORY;
					if(!(flags & LISTDIR_FLAGS_DIRECTORY))
						ok = 0;
				}
				else
				{
					cflags |= LISTDIR_FLAGS_FILE;
					if(!(flags & LISTDIR_FLAGS_FILE))
						ok = 0;
				}
				if(fiattribs & FILE_ATTRIBUTE_HIDDEN)
				{
					if(!(flags & LISTDIR_FLAGS_HIDDEN))
						ok = 0;
					cflags |= LISTDIR_FLAGS_HIDDEN;
				}
				if(ok)
				{
					if(flags & LISTDIR_FLAGS_NAMEONLY)
					{
						/*
						if(0 == _widetoutf8(cx, -1, fbuf + fbufbaselen, sizeof(fbuf) - fbufbaselen))
						{
							if(!callback(fbuf + fbufbaselen, cflags))
								break;
						}
						*/
						/* WARNING: rewriting all of fbuf! */
						if(0 == _widetoutf8(cx, -1, fbuf, sizeof(fbuf)))
						{
							if(!callback(fbuf, cflags))
								break;
						}
					}
					else
					{
						if(0 == _widetoutf8(cx, -1, fbuf + fbufbaselen, sizeof(fbuf) - fbufbaselen))
						{
							if(!callback(fbuf, cflags))
								break;
						}
					}
				}
			}
			if(!FindNextFileW(h, &fi.w))
				break;
		}
	}
	FindClose(h);
	h = INVALID_HANDLE_VALUE;
#else
	/* dir = opendir(); */
	return -52;
#endif
	return 0;
}


int getcurrentdirectory(int flags, char *buf, int buflen)
{
	int i;
	if(flags & FILEIO_FLAGS_TEST)
	{
		i = strlen(FILEIO_TEST_CURRENTDIRECTORY) + 1;
		if(buflen < i)
			return i;
		memcpy(buf, FILEIO_TEST_CURRENTDIRECTORY, i);
		return 0;
	}
#ifdef _ON_WINDOWS_
	if(flags & FILEIO_FLAGS_ANSI)
	{
		i = GetCurrentDirectoryA(buflen, buf);
		if(!i)
			return -1;
		if(i > buflen)
			return i;
	}
	else
	{
		WCHAR wbuf[FILEIO_MAX_PATH_LENGTH];
		i = GetCurrentDirectoryW(FILEIO_MAX_PATH_LENGTH, wbuf);
		if(!i)
			return -1;
		if(i > buflen)
			return FILEIO_MAX_PATH_LENGTH;
		return _widetoutf8(wbuf, i, buf, buflen);
	}
#else
	char *p = getcwd(buf, buflen);
	if(!p)
	{
		if(ERANGE == errno)
			return FILEIO_MAX_PATH_LENGTH;
	}
#endif
	return 0;
}


int getabsolutepath(const char *path, int flags, char *buf, int buflen)
{
	int i;
	if(isabsolutepath(path))
	{
		if(flags & GETABSOLUTEPATH_FLAGS_TRYNOTCOPY)
			return -110;
		for(i = 0;; i++)
		{
			if(i < buflen)
				buf[i] = path[i];
			if(!path[i])
				break;
		}
		if(i <= buflen)
			return 0; /* Success. */
		return i; /* buflen not large enough. */
	}
	else
	{
#ifdef _ON_WINDOWS_
		if(path[0] && ':' == path[1])
		{
			/* TODO: Handle case of: "x:foo" to be relative to curdir of drive x. */
			return -55;
		}
		if(FILEIO_DIRECTORY_SEPARATOR == path[0]
			|| FILEIO_DIRECTORY_SEPARATOR_ALT == path[0])
		{
			/* TODO: Handle case of "\foo" to be relative to current drive. */
			return -56;
		}
#endif
		i = getcurrentdirectory(flags & FILEIO_FLAGS_MASK, buf, buflen);
		if(0 != i)
		{
			if(i < 0)
				return i;
			return i + 1 + strlen(path);
		}
		if(!path[0]) /* If empty input path, all I need is curdir. */
			return 0;
		i = strlen(buf);
		if(!i) /* Curdir is empty, that's strange. */
			return -77;
		if(FILEIO_DIRECTORY_SEPARATOR != buf[i - 1]
			&& FILEIO_DIRECTORY_SEPARATOR_ALT != buf[i - 1]
			&& FILEIO_DIRECTORY_SEPARATOR != path[0]
			&& FILEIO_DIRECTORY_SEPARATOR_ALT != path[0]
			)
		{
			if(i + 1 >= buflen)
				return i + 1 + strlen(path);
			buf[i++] = FILEIO_DIRECTORY_SEPARATOR;
		}
		for(;; path++, i++)
		{
			if(i < buflen)
				buf[i] = *path;
			if(!*path)
				break;
		}
		if(i <= buflen)
			return 0;
		return i; /* buflen not large enough. */
	}
}


#ifdef _DEBUG
#include <assert.h>
#include <stdio.h>

static int _printfilescallback(const char *f, int cflags)
{
	char cftype = '?';
	char cfh = ' ';
	char extra[200] = "";
	if(cflags & LISTDIR_FLAGS_FILE)
		cftype = 'F';
	else if(cflags & LISTDIR_FLAGS_DIRECTORY)
		cftype = 'D';
	if(cflags & LISTDIR_FLAGS_HIDDEN)
		cfh = 'H';
	if(cflags & LISTDIR_FLAGS_NAMEONLY)
		strcat(extra, " NameOnly");
	if(cflags & FILEIO_FLAGS_TEST)
		strcat(extra, " Test");
	if(cflags & FILEIO_FLAGS_ANSI)
		strcat(extra, " ANSI");
	fprintf(stderr, "    `%s` %c %c \t%s\n", f, cftype, cfh, extra);
	return 1; /* Continue. */
}

void fileio_AllTests()
{
	char buf[FILEIO_MAX_PATH_LENGTH];
	int curdirlen = strlen(FILEIO_TEST_CURRENTDIRECTORY);

	assert(0 == getcurrentdirectory(FILEIO_FLAGS_TEST, buf, sizeof(buf)));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY));

	assert(0 == getcurrentdirectory(FILEIO_FLAGS_TEST, buf, curdirlen + 1));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY));

	assert(getcurrentdirectory(FILEIO_FLAGS_TEST, buf, 0) >= curdirlen + 1);

	assert(getcurrentdirectory(FILEIO_FLAGS_TEST, buf, curdirlen) >= curdirlen + 1);

	assert(0 == getabsolutepath("", FILEIO_FLAGS_TEST, buf, sizeof(buf)));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY));

	assert(0 == getabsolutepath(FILEIO_TEST_CURRENTDIRECTORY "foo", FILEIO_FLAGS_TEST, buf, sizeof(buf)));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY "foo"));

	assert(0 == getabsolutepath("foo", FILEIO_FLAGS_TEST, buf, sizeof(buf)));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY "foo"));
	
	assert(getabsolutepath("foo", FILEIO_FLAGS_TEST, buf, 0) >= strlen(FILEIO_TEST_CURRENTDIRECTORY "foo"));

	assert(0 == getabsolutepath(".", FILEIO_FLAGS_TEST, buf, sizeof(buf)));
	assert(0 == strcmp(buf, FILEIO_TEST_CURRENTDIRECTORY "."));

	return;

	/* Real curdir test. */
	assert(0 == getcurrentdirectory(0, buf, sizeof(buf)));
	assert(buf && *buf);
	fprintf(stderr, "curdir=`%s`\n", buf);

	fprintf(stderr, "Dir list LISTDIR_FLAGS_FILE\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_FILE, &_printfilescallback));

	fprintf(stderr, "Dir list LISTDIR_FLAGS_FILE|LISTDIR_FLAGS_HIDDEN\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_FILE | LISTDIR_FLAGS_HIDDEN, &_printfilescallback));

	fprintf(stderr, "Dir list LISTDIR_FLAGS_DIRECTORY\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_DIRECTORY, &_printfilescallback));

	fprintf(stderr, "Dir list LISTDIR_FLAGS_DIRECTORY|LISTDIR_FLAGS_HIDDEN\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_DIRECTORY | LISTDIR_FLAGS_HIDDEN, &_printfilescallback));

	fprintf(stderr, "Dir list LISTDIR_FLAGS_FILE|LISTDIR_FLAGS_DIRECTORY|LISTDIR_FLAGS_HIDDEN\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_FILE | LISTDIR_FLAGS_DIRECTORY | LISTDIR_FLAGS_HIDDEN, &_printfilescallback));

	fprintf(stderr, "Dir list FILEIO_FLAGS_ANSI|LISTDIR_FLAGS_FILE|LISTDIR_FLAGS_DIRECTORY|LISTDIR_FLAGS_HIDDEN\n");
	assert(0 == listdir("", FILEIO_FLAGS_ANSI | FILEIO_FLAGS_TEST | LISTDIR_FLAGS_FILE | LISTDIR_FLAGS_DIRECTORY | LISTDIR_FLAGS_HIDDEN, &_printfilescallback));

	fprintf(stderr, "Dir list LISTDIR_FLAGS_FILE|LISTDIR_FLAGS_DIRECTORY|LISTDIR_FLAGS_HIDDEN|LISTDIR_FLAGS_NAMEONLY\n");
	assert(0 == listdir("", FILEIO_FLAGS_TEST | LISTDIR_FLAGS_FILE | LISTDIR_FLAGS_DIRECTORY | LISTDIR_FLAGS_HIDDEN | LISTDIR_FLAGS_NAMEONLY, &_printfilescallback));

	fprintf(stderr, " & & & & & & & & & & & & & & & & & & \n");

}
#endif

