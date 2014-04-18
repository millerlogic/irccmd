/*
  Copyright 2012-2014 Christopher E. Miller
  License: GPLv3, see LICENSE file.
*/

#define _WINSOCKAPI_

#ifdef _MSC_VER
	#pragma warning(disable:4267) /* conversion from 'x' to 'y', possible loss of data */
	#pragma warning(disable:4996) /* This function or variable may be unsafe. */
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if _DEBUG
#include <assert.h>
#endif

#include "frandom.h"
#include "fileio.h"
#include "utf8v.h"

#include <lauxlib.h>
#include <lualib.h>
#include <lualib.h>
#include <lua.h>


#if defined(WIN32) || defined(WIN64) || defined(WINNT)
#define _ON_WINDOWS_ 1
#endif

static FRandom frand;


#define LuaReturn(r, NumArgs) (-(NumArgs) + (r) - 1)


#ifndef LL_INLINE
	#ifdef _MSC_VER
		#define LL_INLINE __inline
	#else
		#define LL_INLINE inline
	#endif
#endif


static void _programError(const char *reason, int fatal)
{
	fprintf(stderr, "%sProgram Error: %s\n",
		fatal ? "Fatal " : "",
		reason
		);
	if(fatal)
	{
		exit(333);
	}
}


#ifdef _ON_WINDOWS_

#include <string.h>
#define stringicompare _stricmp
#define strtok_r strtok_s

#include <windows.h>
#define getPID (size_t)GetCurrentProcessId

#else

#include <strings.h>
#define stringicompare strcasecmp

#include <string.h>
/* char *strtok_r(char *restrict s, const char *restrict sep, char **restrict lasts); */

#include <unistd.h>
#define getPID (size_t)getpid

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#ifndef SD_RECEIVE
#define SD_RECEIVE 0
#define SD_SEND 1
#define SD_BOTH 2
#endif

#endif


/* Sockets: */
#ifdef _ON_WINDOWS_
#include <windows.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#define _INVALID_SOCKET INVALID_SOCKET
#define _SOCKET_ERROR SOCKET_ERROR
typedef SOCKET socket_t;
#define SOCKOPTVAL(val) (const char*)(val)
socket_t _stdinsock = _INVALID_SOCKET;
struct sockaddr_in _stdinsockaddr;
long _last_stdinsockaddr_addr = 0;
static void _fix_stdinsockaddr()
{
	char buf[64];
	union
	{
		long value;
		unsigned char bytes[4];
	}rnb;
	memset(&_stdinsockaddr, 0, sizeof(struct sockaddr_in));
	_stdinsockaddr.sin_family = AF_INET;
	/* _stdinsockaddr.sin_port = htons(56863); */
	_stdinsockaddr.sin_port = htons(56000 + getPID() % 1000);
	/* _stdinsockaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
	while(!_last_stdinsockaddr_addr)
		_last_stdinsockaddr_addr = frandom_raw(&frand);
	rnb.value = _last_stdinsockaddr_addr;
	sprintf(buf, "127.%u.%u.%u",
		(rnb.bytes[0] == 255) ? 0 : rnb.bytes[0],
		(rnb.bytes[1] == 255) ? 0 : rnb.bytes[1],
		(rnb.bytes[2] == 255) ? 2 : (rnb.bytes[2] == 0) ? 3 : rnb.bytes[2]
		);
	_stdinsockaddr.sin_addr.s_addr = inet_addr(buf);
}
HANDLE _stdinthread = NULL;
#define _lastSocketError WSAGetLastError()
static int _socketsStartup()
{
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2,0), &wsaData);
}
static int _socketsCleanup()
{
	if(_stdinsock != _INVALID_SOCKET)
	{
		closesocket(_stdinsock);
		_stdinsock = _INVALID_SOCKET;
	}
	if(_stdinthread)
	{
		CloseHandle(_stdinthread);
		_stdinthread = NULL;
	}
	return WSACleanup();
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#define _INVALID_SOCKET -1
#define _SOCKET_ERROR -1
typedef int socket_t;
#define SOCKOPTVAL(val) (val)
#define _lastSocketError errno
static int _socketsStartup()
{
	return 0;
}
static int _socketsCleanup()
{
	return 0;
}
#define closesocket close
#endif


static LL_INLINE unsigned long rrandom()
{
	return ((unsigned long)rand()        & 0x00000FFF)
		| (((unsigned long)rand() << 12) & 0x00FFF000)
		| (((unsigned long)rand() << 24) & 0xFF000000)
		;
}


/**	x = random([limit]) */
static int luafunc_random(lua_State *L)
{
	unsigned long rn = rrandom();
	if(lua_isnumber(L, 1))
	{
		unsigned long limit = (unsigned long)lua_tonumber(L, 1);
		if(limit > 0)
		{
			rn %= limit;
		}
		else
		{
			_programError("luafunc_random limit cannot be 0", 0);
			return 0;
		}
	}
	lua_pushnumber(L, rn);
	return 1; /* Number of return values. */
}


/**	lower is inclusive, upper is exclusive.
	x = frandom()
	x = frandom(upper)
	x = frandom(lower, upper)
*/
static int luafunc_frandom(lua_State *L)
{
	unsigned long rn;
	if(lua_isnumber(L, 1))
	{
		if(lua_isnumber(L, 2))
		{
			/* 2 args */
			unsigned long lower = (unsigned long)lua_tonumber(L, 1);
			unsigned long upper = (unsigned long)lua_tonumber(L, 2);
			if(lower >= upper)
			{
				_programError("luafunc_frandom upper must be greater than lower", 0);
				return 0;
			}
			rn = frandom_bounds(&frand, lower, upper);
		}
		else
		{
			/* 1 arg */
			unsigned long upper = (unsigned long)lua_tonumber(L, 1);
			rn = frandom(&frand);
			if(upper > 0)
			{
				rn %= upper;
			}
			else
			{
				_programError("luafunc_frandom upper cannot be 0", 0);
				return 0;
			}
		}
	}
	else
	{
		/* 0 args */
		rn = frandom(&frand);
	}
	lua_pushnumber(L, rn);
	return 1; /* Number of return values. */
}


/**	Note: milliseconds() is not very accurate and is subject to overflow! */
#ifdef _ON_WINDOWS_
#include <windows.h>
#define milliseconds GetTickCount
#else
#include <sys/time.h>
static LL_INLINE unsigned long milliseconds()
{
	struct timeval tv;
	if(-1 == gettimeofday(&tv, NULL))
		return 0;
	return (unsigned long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif


/**	Handles overflow as long as the times don't span longer than 49 days (32-bits). */
static unsigned long milliseconds_diff(unsigned long ms_old, unsigned long ms_new)
{
	return (ms_new - ms_old);
}


/**	x = milliseconds() */
static int luafunc_milliseconds(lua_State *L)
{
	lua_pushnumber(L, milliseconds());
	return 1; /* Number of return values. */
}


/**	x = milliseconds_diff(old, new) */
static int luafunc_milliseconds_diff(lua_State *L)
{
	unsigned long ms_old, ms_new;
	if(!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		return 0;
	ms_old = (unsigned long)lua_tonumber(L, 1);
	ms_new = (unsigned long)lua_tonumber(L, 2);
	lua_pushnumber(L, milliseconds_diff(ms_old, ms_new));
	return 1; /* Number of return values. */
}


static void addrhintsdefaults(struct addrinfo *paddrhints)
{
	memset(paddrhints, 0, sizeof(struct addrinfo));
	paddrhints->ai_socktype = SOCK_STREAM;
	paddrhints->ai_family = AF_INET;
}


static int tryluaaddresstype(lua_State *L, int luaIndex, struct addrinfo *paddrhints)
{
	if(lua_isnumber(L, luaIndex))
	{
		paddrhints->ai_socktype = lua_tointeger(L, luaIndex); /* Safe? */
	}
	else if(lua_isstring(L, luaIndex))
	{
		const char *typestr = lua_tostring(L, luaIndex);
		if(0 == stringicompare(typestr, "STREAM"))
		{
			paddrhints->ai_socktype = SOCK_STREAM;
		}
		else if(0 == stringicompare(typestr, "DGRAM"))
		{
			paddrhints->ai_socktype = SOCK_DGRAM;
		}
		else if(0 == stringicompare(typestr, "RAW"))
		{
			paddrhints->ai_socktype = SOCK_RAW;
		}
		else if(0 == stringicompare(typestr, "RDM"))
		{
			paddrhints->ai_socktype = SOCK_RDM;
		}
		else if(0 == stringicompare(typestr, "SEQPACKET"))
		{
			paddrhints->ai_socktype = SOCK_SEQPACKET;
		}
		else
		{
			lua_pushnil(L);
			lua_pushstring(L, "Unknown socket type");
			lua_pushnil(L);
			return 3; /* Number of return values. */
		}
	}
	return 0;
}


static int tryluaaddressfamily(lua_State *L, int luaIndex, struct addrinfo *paddrhints)
{
	if(lua_isnumber(L, luaIndex))
	{
		paddrhints->ai_family = lua_tointeger(L, luaIndex); /* Safe? */
	}
	else if(lua_isstring(L, luaIndex))
	{
		const char *familystr = lua_tostring(L, luaIndex);
		if(0 == stringicompare(familystr, "INET"))
		{
			paddrhints->ai_family = AF_INET;
		}
		else if(0 == stringicompare(familystr, "INET6"))
		{
			paddrhints->ai_family = AF_INET6;
		}
		else if(0 == stringicompare(familystr, "UNIX"))
		{
			paddrhints->ai_family = AF_UNIX;
		}
		else if(0 == stringicompare(familystr, "UNSPEC"))
		{
			paddrhints->ai_family = AF_UNSPEC;
		}
		else
		{
			lua_pushnil(L);
			lua_pushstring(L, "Unknown socket family");
			lua_pushnil(L);
			return 3; /* Number of return values. */
		}
	}
	return 0;
}


/**	socket = socket_connect(address, port [, type, family] [, socketCreatedFunc] )
	port can be a port number or service name.
	type can be nil, an integer, or one of the following strings:
		STREAM (default), DGRAM, RAW, RDM, SEQPACKET
	family can be nil, an integer or one of the following strings:
		INET (default), UNSPEC, UNIX, INET6
	socketCreatedFunc is called with a socket created, which can occur multiple times!
	socketCreatedFunc is optional and can be specified after or instead of [type,family].
*/
static int luafunc_socket_connect(lua_State *L)
{
	socket_t sock;
	struct addrinfo addrhints, *addr;
	const char *saddress, *sport;
	int socketCreatedFuncIndex = 0;
	char portbuf[16];
	int reuse = 1;

	addrhintsdefaults(&addrhints);

	if(lua_isstring(L, 1))
	{
		saddress = lua_tostring(L, 1);
	}
	else
	{
		lua_pushnil(L);
		lua_pushstring(L, "Invalid address specified");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}

	if(lua_isnumber(L, 2))
	{
		sprintf(portbuf, "%d", (int)lua_tointeger(L, 2));
		sport = portbuf;
	}
	else if(lua_isstring(L, 2))
	{
		sport = lua_tostring(L, 2);
	}
	else
	{
		lua_pushnil(L);
		lua_pushstring(L, "Invalid port specified");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}

	if(lua_isfunction(L, 3))
	{
		socketCreatedFuncIndex = 3;
	}
	else
	{
		int xaddrtype = tryluaaddresstype(L, 3, &addrhints);
		if(xaddrtype)
			return xaddrtype;

		if(lua_isfunction(L, 4))
		{
			socketCreatedFuncIndex = 4;
		}
		else
		{
			int xaddrfamily = tryluaaddressfamily(L, 4, &addrhints);
			if(xaddrfamily)
				return xaddrfamily;
		}

		if(!socketCreatedFuncIndex && lua_isfunction(L, 5))
		{
			socketCreatedFuncIndex = 5;
		}
	}

	do
	{
		int errv = 0;
		struct addrinfo *itaddr;
		sock = _INVALID_SOCKET;
		if(0 == getaddrinfo(saddress, sport, &addrhints, &addr))
		{
			for(itaddr = addr; itaddr; itaddr = itaddr->ai_next)
			{
				sock = socket(itaddr->ai_family, itaddr->ai_socktype, itaddr->ai_protocol);
				if(_INVALID_SOCKET == sock)
				{
					errv = _lastSocketError;
					continue;
				}

				if(socketCreatedFuncIndex)
				{
					lua_pushvalue(L, socketCreatedFuncIndex);
					lua_pushinteger(L, sock);
					if(lua_pcall(L, 1, 0, 0))
					{
						_programError("Error in irccmd.socket_connect callback socketCreatedFunc", 0);
						_programError(lua_tostring(L, -1), 0);
						lua_pop(L, 1);
					}
				}

#if defined(__APPLE__)
				setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&reuse, sizeof(int));
#endif

				if(_SOCKET_ERROR == connect(sock, itaddr->ai_addr, itaddr->ai_addrlen))
				{
					int xerr = _lastSocketError;
	#if _ON_WINDOWS_
					if(WSAEWOULDBLOCK == xerr)
						break; /* Connection pending... */
	#else
					if(EINPROGRESS == xerr)
						break; /* Connection pending... */
	#endif
					errv = xerr;
					closesocket(sock);
					sock = _INVALID_SOCKET;
					continue;
				}

				break; /* Connected. */
			}
			freeaddrinfo(addr);
		}

		if(_INVALID_SOCKET == sock)
		{
			lua_pushnil(L);
			lua_pushstring(L, "Unable to connect");
			if(errv)
				lua_pushinteger(L, errv);
			else
				lua_pushnil(L);
			return 3; /* Number of return values. */
		}

	}while(0);

	lua_pushinteger(L, sock);
	return 1; /* Number of return values. */
}


/**	true = socket_bind(socket, address, [port [, type, family]])
	Any parameter besides socket can be nil for default,
	however, one of address or port must be specified.
	Default for address and port are any address and random port.
	Future: deprecate [type,family], get it from the socket (getsockname)?
*/
static int luafunc_socket_bind(lua_State *L)
{
	socket_t sock;
	struct addrinfo addrhints, *addr;
	const char *saddress, *sport;
	char portbuf[16];

	if(!lua_isnumber(L, 1))
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Invalid arguments (socket_bind)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);

	addrhintsdefaults(&addrhints);

	if(lua_isstring(L, 2))
	{
		saddress = lua_tostring(L, 2);
	}
	else
	{
		saddress = NULL;
	}

	if(lua_isnumber(L, 3))
	{
		sprintf(portbuf, "%d", (int)lua_tointeger(L, 3));
		sport = portbuf;
	}
	else if(lua_isstring(L, 3))
	{
		sport = lua_tostring(L, 3);
	}
	else
	{
		sport = NULL;
	}

	if(!saddress && !sport)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Must specify address or port to bind");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}

	do
	{
		int xaddrtype = tryluaaddresstype(L, 4, &addrhints);
		if(xaddrtype)
			return xaddrtype;
	}while(0);

	do
	{
		int xaddrfamily = tryluaaddressfamily(L, 5, &addrhints);
		if(xaddrfamily)
			return xaddrfamily;
	}while(0);

	do
	{
		int x;
		if(0 == getaddrinfo(saddress, sport, &addrhints, &addr))
		{
			x = bind(sock, addr->ai_addr, addr->ai_addrlen);
			freeaddrinfo(addr);
			if(_SOCKET_ERROR == x)
			{
				lua_pushboolean(L, 0);
				lua_pushstring(L, "Unable to bind");
				lua_pushinteger(L, _lastSocketError);
				return 3; /* Number of return values. */
			}
		}
		else
		{
			lua_pushboolean(L, 0);
			lua_pushstring(L, "Unable to bind");
			lua_pushinteger(L, _lastSocketError);
			return 3; /* Number of return values. */
		}
	}while(0);

	lua_pushboolean(L, 1);
	return 1; /* Number of return values. */
}


/**	socket = socket_listen([address,] port, [backlog, [, type, family]])
	address can be nil or omitted to allow any address.
	backlog can be nil for irccmd's default backlog.
*/
static int luafunc_socket_listen(lua_State *L)
{
	socket_t sock;
	struct addrinfo addrhints, *addr;
	const char *saddress = NULL, *sport = NULL;
	char portbuf[16];
	int backlog = 10;
	int reuse = 1;
	int iarg = 1;

	addrhintsdefaults(&addrhints);
	addrhints.ai_flags |= AI_PASSIVE;

	if(lua_isstring(L, iarg))
	{
		saddress = lua_tostring(L, iarg++);
	}
	else if(lua_isnil(L, iarg))
	{
		iarg++;
	}

	if(lua_isnumber(L, iarg))
	{
		sprintf(portbuf, "%d", (int)lua_tointeger(L, iarg++));
		sport = portbuf;
	}
	else if(lua_isstring(L, iarg))
	{
		sport = lua_tostring(L, iarg++);
	}
	else
	{
		lua_pushnil(L);
		lua_pushstring(L, "Invalid port specified");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}

	if(lua_isnumber(L, iarg))
	{
		backlog = lua_tointeger(L, iarg++);
	}
	else
	{
		/* Backlog can be nil to skip. */
		iarg++;
	}

	do
	{
		int xaddrtype = tryluaaddresstype(L, iarg++, &addrhints);
		if(xaddrtype)
			return xaddrtype;
	}while(0);

	do
	{
		int xaddrfamily = tryluaaddressfamily(L, iarg++, &addrhints);
		if(xaddrfamily)
			return xaddrfamily;
	}while(0);

	do
	{
		int errv = 0;
		struct addrinfo *itaddr;
		sock = _INVALID_SOCKET;
		if(0 == getaddrinfo(saddress, sport, &addrhints, &addr))
		{
			for(itaddr = addr; itaddr; itaddr = itaddr->ai_next)
			{
				sock = socket(itaddr->ai_family, itaddr->ai_socktype, itaddr->ai_protocol);
				if(_INVALID_SOCKET == sock)
				{
					errv = _lastSocketError;
					/* continue; */
					break; /* No socket... */
				}

				setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SOCKOPTVAL(&reuse), sizeof(reuse));

				if(_SOCKET_ERROR == bind(sock, itaddr->ai_addr, itaddr->ai_addrlen))
				{
					errv = _lastSocketError;
					closesocket(sock);
					sock = _INVALID_SOCKET;
					/* continue; */
					break; /* Not bound... */
				}

				if(_SOCKET_ERROR == listen(sock, backlog))
				{
					errv = _lastSocketError;
					closesocket(sock);
					sock = _INVALID_SOCKET;
					/* continue; */
					break; /* Not listening... */
				}

				break; /* Listening. */
			}
			freeaddrinfo(addr);
		}

		if(_INVALID_SOCKET == sock)
		{
			lua_pushnil(L);
			lua_pushstring(L, "Unable to listen");
			if(errv)
				lua_pushinteger(L, errv);
			else
				lua_pushnil(L);
			return 3; /* Number of return values. */
		}

	}while(0);

	lua_pushinteger(L, sock);
	return 1; /* Number of return values. */
}


/**	(new_socket, address, port) = socket_accept(socket) */
static int luafunc_socket_accept(lua_State *L)
{
	socket_t sock;
	socket_t newsock;
	int addrlen = 0;
	union
	{
		struct sockaddr addr;
		char buf[1024 * 2];
	}addrbuf;
	char namebuf[64];
	char portbuf[16];

	if(!lua_isnumber(L, 1))
	{
		lua_pushnil(L);
		lua_pushstring(L, "Invalid arguments (socket_accept)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);

	addrlen = sizeof(addrbuf);
	newsock = accept(sock, &addrbuf.addr, &addrlen);
	if(_INVALID_SOCKET == newsock)
	{
		lua_pushnil(L);
		lua_pushstring(L, "Unable to accept");
		lua_pushinteger(L, _lastSocketError);
		return 3; /* Number of return values. */
	}
	if(addrlen > sizeof(addrbuf))
	{
		closesocket(newsock);
		lua_pushnil(L);
		lua_pushstring(L, "Address too large");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}

	lua_pushinteger(L, newsock);
	/*if(AF_INET == addrbuf.addr.sa_family)
	{
		lua_pushstring(L, inet_ntoa(((struct sockaddr_in*)&addrbuf.addr)->sin_addr));
		lua_pushinteger(L, ntohs(((struct sockaddr_in*)&addrbuf.addr)->sin_port));
	}
	else*/
	{
		if(getnameinfo(&addrbuf.addr, addrlen, namebuf, sizeof(namebuf),
			portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV))
				goto unk_addr;
		lua_pushstring(L, namebuf);
		if(portbuf[0] && (portbuf[0] >= '0' && portbuf[0] <= '9'))
		{
			lua_pushnumber(L, strtod(portbuf, NULL));
		}
		else
		{
			lua_pushnil(L); /* Unknown/unsupported port. */
		}
	}
	return 3; /* Number of return values. */
	unk_addr:
	lua_pushnil(L); /* Unknown/unsupported address. */
	lua_pushnil(L); /* Unknown/unsupported port. */
	return 3; /* Number of return values. */
}


/**	Release this socket, disconnecting if necessary. */
static int luafunc_socket_close(lua_State *L)
{
	if(lua_isnumber(L, 1))
	{
		closesocket(lua_tointeger(L, 1));
	}
	return 0; /* Number of return values. */
}


/**	true = socket_blocking(socket, bool) */
static int luafunc_socket_blocking(lua_State *L)
{
	socket_t sock;
	int byes;
	int x;
	if(!lua_isnumber(L, 1) || !lua_isboolean(L, 2))
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Invalid arguments (socket_blocking)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);
	byes = lua_toboolean(L, 2);
#ifdef _ON_WINDOWS_
	x = !byes;
	if(_SOCKET_ERROR == ioctlsocket(sock, FIONBIO, &x))
		goto err;
#else
	x = fcntl(sock, F_GETFL, 0);
	if(-1 == x)
		goto err;
	if(byes)
		x &= ~O_NONBLOCK;
	else
		x |= O_NONBLOCK;
	if(-1 == fcntl(sock, F_SETFL, x))
		goto err;
#endif
	lua_pushboolean(L, 1); /* Success. */
	return 1; /* Number of return values. */

err:
	lua_pushboolean(L, 0);
	lua_pushstring(L, "Unable to set socket blocking");
	lua_pushinteger(L, _lastSocketError);
	return 3; /* Number of return values. */
}


/**	true = socket_linger(socket, seconds)
	Set seconds to false to disable linger.
	Otherwise, seconds must be a number.
	Time is in whole seconds.
*/
static int luafunc_socket_linger(lua_State *L)
{
	socket_t sock;
	struct linger lr;

	if(!lua_isnumber(L, 1))
	{
		badargs:
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Invalid arguments (socket_linger)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);

	if(lua_isboolean(L, 2))
	{
		if(lua_toboolean(L, 2))
			goto badargs; /* True isn't valid. */
		lr.l_onoff = 0;
		lr.l_linger = 0;
	}
	else if(lua_isnumber(L, 2))
	{
		int secs = lua_tointeger(L, 2);
		if(secs < 0)
			goto badargs;
		if(secs > 65535)
			secs = 65535;
		lr.l_onoff = 1;
		lr.l_linger = secs;
	}
	else
	{
		goto badargs;
	}
	if(_SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_LINGER, SOCKOPTVAL(&lr), sizeof(lr)))
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Unable to set socket linger");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	lua_pushboolean(L, 1);
	return 1; /* Number of return values. */
}


#if 0
/**	true = socket_reuseaddr(socket, bool)
*/
static int luafunc_socket_reuseaddr(lua_State *L)
{
	socket_t sock;
	int on;

	if(!lua_isnumber(L, 1) || !lua_isboolean(L, 2))
	{
		badargs:
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Invalid arguments (socket_reuseaddr)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);

	on = lua_toboolean(L, 2);
	if(_SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SOCKOPTVAL(&on), sizeof(on)))
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Unable to set socket reuseaddr");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	lua_pushboolean(L, 1);
	return 1; /* Number of return values. */
}
#endif


/**	bool = socket_shutdown(socket, how)
	how is one of the strings: RECEIVE, SEND, BOTH
*/
static int luafunc_socket_shutdown(lua_State *L)
{
	const char *sHow;
	int sock, how;
	if(!lua_isnumber(L, 1) || !lua_isstring(L, 2))
	{
badarg:
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Invalid arguments (socket_shutdown)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);
	sHow = lua_tostring(L, 2);
	if(0 == stringicompare("RECEIVE", sHow))
		how = SD_RECEIVE;
	else if(0 == stringicompare("SEND", sHow))
		how = SD_SEND;
	else if(0 == stringicompare("BOTH", sHow))
		how = SD_BOTH;
	else
		goto badarg;
	shutdown(sock, how);
	lua_pushboolean(L, 1);
	return 1; /* Number of return values. */
}


#ifdef SO_MAX_MSG_SIZE
#if SO_MAX_MSG_SIZE > 8192
#define SOCKET_DEFAULT_BUFFER_SIZE 8192
#else
#define SOCKET_DEFAULT_BUFFER_SIZE SO_MAX_MSG_SIZE
#endif
#else
#define SOCKET_DEFAULT_BUFFER_SIZE 8192
#endif


/**	bytes_sent = socket_send(socket, data [, flags])
	Returns nil on error; or number of bytes actually sent.
	data can contain embedded nul bytes.
	flags can be nil or one of the strings: NONE (default), OOB, DONTROUTE.
	NOSIGNAL (don't send SIGPIPE signal) is assumed.
*/
static int luafunc_socket_send(lua_State *L)
{
	int sock;
	const char *data;
	size_t dataLength = 0;
	int flags = 0;
	int sendlen = 0;
	int result;
	if(!lua_isnumber(L, 1) || !lua_isstring(L, 2))
	{
badarg:
		lua_pushnil(L);
		lua_pushstring(L, "Invalid arguments (socket_send)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);
	data = lua_tolstring(L, 2, &dataLength);
	if(lua_isstring(L, 3))
	{
		const char *sflags = lua_tostring(L, 3);
		if(0 == stringicompare("NONE", sflags))
			flags = 0;
		else if(0 == stringicompare("OOB", sflags))
			flags = MSG_OOB;
		/* else if(0 == stringicompare("PEEK", sflags))
			flags = MSG_PEEK; */
		else if(0 == stringicompare("DONTROUTE", sflags))
			flags = MSG_DONTROUTE;
		else
			goto badarg;
	}
#ifdef _ON_WINDOWS_
#elif !defined(__APPLE__)
	flags |= MSG_NOSIGNAL;
#endif
	if(dataLength > SOCKET_DEFAULT_BUFFER_SIZE)
		sendlen = SOCKET_DEFAULT_BUFFER_SIZE;
	else
		sendlen = (int)dataLength;
	result = send(sock, data, sendlen, flags);
	if(_SOCKET_ERROR == result)
	{
		lua_pushnil(L);
		lua_pushstring(L, "Unable to send");
		lua_pushinteger(L, _lastSocketError);
		return 3; /* Number of return values. */
	}
	lua_pushinteger(L, result);
	return 1; /* Number of return values. */
}


/**	data = socket_receive(socket [, flags [, maxBytes]])
	Returns nil on error; false on connection close, or data actually received.
	data can contain embedded nul bytes.
	flags can be nil or one of the strings: NONE (default), OOB, PEEK, DONTROUTE.
	NOSIGNAL (don't send SIGPIPE signal) is assumed.
	maxBytes can be specified to prevent reading more than this many bytes; must be > 0.
*/
static int luafunc_socket_receive(lua_State *L)
{
	int sock;
	char buf[SOCKET_DEFAULT_BUFFER_SIZE];
	int flags = 0;
	int maxBytes = sizeof(buf);
	int result;
	if(!lua_isnumber(L, 1))
	{
badarg:
		lua_pushnil(L);
		lua_pushstring(L, "Invalid arguments (socket_receive)");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	sock = lua_tointeger(L, 1);
	if(lua_isstring(L, 2))
	{
		const char *sflags = lua_tostring(L, 2);
		if(0 == stringicompare("NONE", sflags))
			flags = 0;
		else if(0 == stringicompare("OOB", sflags))
			flags = MSG_OOB;
		else if(0 == stringicompare("PEEK", sflags))
			flags = MSG_PEEK;
		else if(0 == stringicompare("DONTROUTE", sflags))
			flags = MSG_DONTROUTE;
		else
			goto badarg;
	}
#ifdef _ON_WINDOWS_
#elif !defined(__APPLE__)
	flags |= MSG_NOSIGNAL;
#endif
	if(lua_isnumber(L, 3))
	{
		maxBytes = lua_tointeger(L, 3);
		if(maxBytes <= 0)
			goto badarg;
		if(maxBytes > sizeof(buf))
			maxBytes = sizeof(buf);
	}
	result = recv(sock, buf, maxBytes, flags);
	if(_SOCKET_ERROR == result)
	{
		lua_pushnil(L);
		lua_pushstring(L, "Unable to receive");
		lua_pushinteger(L, _lastSocketError);
		return 3; /* Number of return values. */
	}
	if(!result)
	{
		lua_pushboolean(L, 0);
		lua_pushstring(L, "Connection closed");
		lua_pushnil(L);
		return 3; /* Number of return values. */
	}
	lua_pushlstring(L, buf, result);
	return 1; /* Number of return values. */
}


#ifdef _ON_WINDOWS_
static DWORD WINAPI _stdinthreadproc(LPVOID lpParameter)
{
#define STDIN_WCHAR_BUF_SIZE (1024 * 2)
#define STDIN_CCHAR_BUF_SIZE (1024 * 4)
	WCHAR wbuf[STDIN_WCHAR_BUF_SIZE];
	char cbuf[STDIN_CCHAR_BUF_SIZE];
	HANDLE hconsolein = GetStdHandle(STD_INPUT_HANDLE);
	UINT oldcp = GetConsoleCP();
	SetConsoleCP(CP_UTF8);
	(void)lpParameter;
	do
	{
		DWORD cmode;
		if(GetConsoleMode(hconsolein, &cmode))
		{
			SetConsoleMode(hconsolein, cmode | ENABLE_LINE_INPUT);
		}
	}
	while(0);
	while(1)
	{
		DWORD read;
		int x;
		if(!ReadConsoleW(hconsolein, wbuf, STDIN_WCHAR_BUF_SIZE, &read, NULL))
			break;
		cbuf[0] = 0;
		_widetoutf8(wbuf, read, cbuf, STDIN_CCHAR_BUF_SIZE);
		_fix_stdinsockaddr();
		x = bind(_stdinsock, (struct sockaddr*)&_stdinsockaddr, sizeof(struct sockaddr_in));
		_fix_stdinsockaddr();
		x = sendto(_stdinsock, cbuf, strlen(cbuf), 0, (struct sockaddr*)&_stdinsockaddr, sizeof(struct sockaddr_in));
	}
	SetConsoleCP(oldcp);
	CloseHandle(hconsolein);
	return 0;
}
#endif


int _stdinOpen = 1;


/**	result = socket_select(sockets [, microseconds])
	sockets: sockets to check for events; may be limited to 64 sockets.
	sockets is an array such that array[socket] = events_str
	events_str is a string combination of one or more of:
		r for read events.
		w for write events.
		e for error events.
	microseconds can be set to an integer wait timeout, or nil or -1 for maximum timeout.
	Returns array of sockets with events, "timeout" if the time elapsed, (nil,msg,code) on error.
	Key "stdin" with value "r" can be specified to wait for standard input.
		On "stdin" event, key "stdin" will have value "<line>"
*/
static int luafunc_socket_select(lua_State *L)
{
	int result;
	struct timeval tv;
	struct timeval *ptv = NULL;
	int n = 0;
#if _DEBUG
	int d_nreads = 0, d_nwrites = 0, d_nerrors = 0;
#endif
	fd_set reads, writes, errors;
	/* Windows has a problem with empty fd_set`s that aren't null. */
	fd_set *preads = NULL, *pwrites = NULL, *perrors = NULL;
	const char *want_stdin = NULL;
	socket_t stdinsock;
#ifdef _ON_WINDOWS_
	stdinsock = _stdinsock;
#else
	/* stdinsock = 0; */
	stdinsock = fileno(stdin);
#endif

	if(lua_isnumber(L, 2))
	{
		int microseconds = lua_tointeger(L, 2);
		if(microseconds >= 0)
		{
			tv.tv_sec = microseconds / 1000000;
			tv.tv_usec = microseconds % 1000000;
			ptv = &tv;
		}
	}

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	FD_ZERO(&errors);
	lua_pushnil(L); /* first key */
	while(lua_next(L, 1))
	{
		/* key at index -2 and value at index -1 */
		const char *es = lua_tostring(L ,-1);
		if(!lua_isnumber(L, -2) && lua_isstring(L, -2))
		{
			const char *kstr = lua_tostring(L, -2);
			if(_stdinOpen && 0 == stringicompare(kstr, "stdin") && strchr(es, 'r'))
			{
				want_stdin = kstr;
#if _DEBUG
				d_nreads++;
#endif
			}
		}
		else
		{
			socket_t sock = lua_tointeger(L, -2);
#ifdef _ON_WINDOWS_
			if(sock != _INVALID_SOCKET)
#else
			if(sock >= 0)
#endif
			{
				for(; *es; es++)
				{
					switch(*es)
					{
						case 'r':
							FD_SET(sock, &reads);
							preads = &reads;
#if _DEBUG
							d_nreads++;
#endif
							break;
						case 'w':
							FD_SET(sock, &writes);
							pwrites = &writes;
#if _DEBUG
							d_nwrites++;
#endif
							break;
						case 'e':
							FD_SET(sock, &errors);
							perrors = &errors;
#if _DEBUG
							d_nerrors++;
#endif
							break;
					}
				}
#ifdef _ON_WINDOWS_
#else
				if(sock > n)
					n = sock;
#endif
			}
		}
		lua_pop(L, 1); /* Remove value, keep key for next iteration. */
	}
#ifdef _ON_WINDOWS_
	if(want_stdin)
	{
		if(_stdinsock == _INVALID_SOCKET)
		{
			_stdinsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
		if(_stdinsock != _INVALID_SOCKET)
		{
			if(!_stdinthread)
			{
				_stdinthread = CreateThread(NULL, 0, &_stdinthreadproc, NULL, 0, NULL);
			}
			FD_SET(_stdinsock, &reads);
		}
	}
#else
	if(want_stdin)
	{
		FD_SET(stdinsock, &reads);
		if(stdinsock > n)
			n = stdinsock;
	}
#endif

	for(;;)
	{
		result = select(n + 1, preads, pwrites, perrors, ptv);
#ifdef _ON_WINDOWS_
		if(_SOCKET_ERROR == result && _lastSocketError == WSAEINTR)
			continue;
#else
		if(_SOCKET_ERROR == result && _lastSocketError == EINTR)
			continue;
#endif
		if(_SOCKET_ERROR == result)
		{
			lua_pushnil(L);
			lua_pushstring(L, "Socket select error");
			lua_pushinteger(L, _lastSocketError);
			return 3; /* Number of return values. */
		}
		break;
	}

#if _DEBUG
	d_nreads = 0, d_nwrites = 0, d_nerrors = 0;
#endif

	if(!result)
	{
		lua_pushstring(L, "timeout");
		return 1; /* Number of return values. */
	}

	lua_createtable(L, 0, result);
	lua_pushnil(L); /* first key */
	while(lua_next(L, 1))
	{
		/* key at index -2 and value at index -1 */
		socket_t sock = _INVALID_SOCKET;
		const char *es = lua_tostring(L, -1);
		char ebuf[4];
		int ebufindex = 0;
		if(!lua_isnumber(L, -2) && lua_isstring(L, -2))
		{
			const char *kstr = lua_tostring(L, -2);
			if(_stdinOpen && 0 == stringicompare(kstr, "stdin") && strchr(es, 'r'))
			{
				sock = stdinsock;
			}
			else
			{
				goto skip_current;
			}
		}
		else
		{
			sock = lua_tointeger(L, -2);
		}
		for(; *es; es++)
		{
			switch(*es)
			{
				case 'r':
					if(FD_ISSET(sock, &reads))
					{
						ebuf[ebufindex++] = 'r';
#if _DEBUG
						d_nreads++;
#endif
					}
					break;
				case 'w':
					if(FD_ISSET(sock, &writes))
					{
						ebuf[ebufindex++] = 'w';
#if _DEBUG
						d_nwrites++;
#endif
					}
					break;
				case 'e':
					if(FD_ISSET(sock, &errors))
					{
						ebuf[ebufindex++] = 'e';
#if _DEBUG
						d_nerrors++;
#endif
					}
					break;
			}
		}
		if(ebufindex)
		{
			ebuf[ebufindex] = '\0';
			if(want_stdin && sock == stdinsock)
			{
#ifdef _ON_WINDOWS_
				char cbuf[STDIN_CCHAR_BUF_SIZE];
				int fromlen = sizeof(_stdinsockaddr);
				int recvlen = 0;
				_fix_stdinsockaddr();
				if((recvlen = recvfrom(_stdinsock, cbuf, sizeof(cbuf), 0, (struct sockaddr*)&_stdinsockaddr, &fromlen)) < 1)
				{
					int stdinerr = _lastSocketError;
					_stdinOpen = 0;
					goto skip_current;
				}
				lua_pushstring(L, want_stdin); /* Index */
				lua_pushlstring(L, cbuf, recvlen); /* Value */
#else
				char cbuf[1024 * 4];
				if(!fgets(cbuf, sizeof(cbuf), stdin) || !cbuf[0])
				{
					_stdinOpen = 0;
					goto skip_current;
				}
				lua_pushstring(L, want_stdin); /* Index */
				lua_pushstring(L, cbuf); /* Value */
#endif
				want_stdin = NULL; /* Clear it, don't want to get it twice (like if user specifies fd 0). */
			}
			else
			{
				lua_pushinteger(L, sock); /* Index */
				lua_pushlstring(L, ebuf, ebufindex); /* Value */
			}
			lua_settable(L, -3 -2);
		}
		skip_current: ;
		lua_pop(L, 1); /* Remove value, keep key for next iteration. */
	}
	return 1; /* Number of return values. */
}


static void lua_print_args(lua_State *L, FILE *f)
{
	int iarg;
	for(iarg = 1; !lua_isnone(L, iarg); iarg++)
	{
		const char *s = lua_tostring(L, iarg); /* Convert to string, or return NULL. */
		if(s)
		{
			fileprint(f, s, 0);
		}
	}
}


/**	*/
static int luafunc_console_print(lua_State *L)
{
	lua_print_args(L, stdout);
	return 0; /* Number of return values. */
}


/**	*/
static int luafunc_console_print_err(lua_State *L)
{
	lua_print_args(L, stderr);
	return 0; /* Number of return values. */
}


/**	line = irc_input(line_data) */
static int luafunc_irc_input(lua_State *L)
{
	const unsigned char *us;
	size_t i, len;
	if(!lua_isstring(L, 1))
		return 0; /* Number of return values. */
	us = (const unsigned char*)lua_tolstring(L, 1, &len);
	for(i = 0; i < len; i++)
	{
		if(us[i] >= 0x80)
		{
			if(!isValidUTF8String(us, len))
			{
				char buf[1024 * 2]; /* IRC shouldn't need more than this. */
				size_t rl = latin1toUTF8(us, len, buf, sizeof(buf));
				if(rl >= sizeof(buf))
					rl = sizeof(buf);
				lua_pushlstring(L, buf, rl);
				return 1; /* Number of return values. */
			}
			break;
		}
	}
	lua_pushvalue(L, 1);
	return 1; /* Number of return values. */
}


/**	(prefix, cmd, params) = irc_input(line)
	params is a table; prefix may be nil if no prefix.
*/
static int luafunc_irc_parse(lua_State *L)
{
	const char *s, *s2;
	size_t i, x;
	if(!lua_isstring(L, 1))
		return 0; /* Number of return values. */
	s = lua_tostring(L, 1);

	/* prefix: */
	if(s[0] == ':')
	{
		s++;
		s2 = strchr(s, ' ');
		if(s2)
		{
			lua_pushlstring(L, s, s2 - s);
			s = s2 + 1;
		}
		else
		{
			x = strlen(s);
			lua_pushlstring(L, s, x);
			s += x;
		}
	}
	else
	{
		lua_pushnil(L);
	}

	/* cmd: */
	s2 = strchr(s, ' ');
	if(s2)
	{
		lua_pushlstring(L, s, s2 - s);
		s = s2 + 1;
	}
	else
	{
		x = strlen(s);
		if(x)
		{
			lua_pushlstring(L, s, x);
			s += x;
		}
		else
		{
			lua_pushnil(L);
		}
	}

	/* params: */
	lua_createtable(L, 0, 0);
	for(i = 0; s[0]; i++)
	{
		lua_pushinteger(L, 1 + i); /* Index */
		if(':' == s[0])
		{
			s++;
			x = strlen(s);
			lua_pushlstring(L, s, x);
			s += x;
		}
		else
		{
			s2 = strchr(s, ' ');
			if(s2)
			{
				lua_pushlstring(L, s, s2 - s);
				s = s2 + 1;
			}
			else
			{
				x = strlen(s);
				lua_pushlstring(L, s, x);
				s += x;
			}
		}
		lua_settable(L, -1 -2);
	}

	return 3; /* Number of return values. */
}


static LL_INLINE int tolower_ascii(char ch)
{
	return ((ch) >= 'A' && (ch) <= 'Z') ?  ('a' + ((ch) - 'A')) : (ch);
}

static LL_INLINE int tolower_rfc1459(char ch)
{
	/*  {}|~ are the lowercase of []\^  */
	return ((ch) >= 'A' && (ch) <= '^') ?  ('a' + ((ch) - 'A')) : (ch);
}

static LL_INLINE int tolower_strict_rfc1459(char ch)
{
	/*  {}| are the lowercase of []\  */
	return ((ch) >= 'A' && (ch) <= ']') ?  ('a' + ((ch) - 'A')) : (ch);
}


/**	*/
static int compare_ascii(const char *s1, size_t s1len, const char *s2, size_t s2len)
{
	size_t i;
	for(i = 0;; i++)
	{
		if(i == s1len)
		{
			if(i == s2len)
				break;
			return -1;
		}
		else if(i == s2len)
		{
			return +1;
		}
		if(tolower_ascii(s1[i]) != tolower_ascii(s2[i]))
			return s1[i] - s2[i];
	}
	return 0;
}

/**	*/
static int compare_rfc1459(const char *s1, size_t s1len, const char *s2, size_t s2len)
{
	size_t i;
	for(i = 0;; i++)
	{
		if(i == s1len)
		{
			if(i == s2len)
				break;
			return -1;
		}
		else if(i == s2len)
		{
			return +1;
		}
		if(tolower_rfc1459(s1[i]) != tolower_rfc1459(s2[i]))
			return s1[i] - s2[i];
	}
	return 0;
}

/**	*/
static int compare_strict_rfc1459(const char *s1, size_t s1len, const char *s2, size_t s2len)
{
	size_t i;
	for(i = 0;; i++)
	{
		if(i == s1len)
		{
			if(i == s2len)
				break;
			return -1;
		}
		else if(i == s2len)
		{
			return +1;
		}
		if(tolower_strict_rfc1459(s1[i]) != tolower_strict_rfc1459(s2[i]))
			return s1[i] - s2[i];
	}
	return 0;
}


#if _DEBUG
static void compare_Test()
{
	assert('a' == tolower_ascii('a'));
	assert('a' == tolower_ascii('A'));
	assert('f' == tolower_ascii('F'));
	assert('w' == tolower_ascii('w'));
	assert('w' == tolower_ascii('W'));
	assert('z' == tolower_ascii('Z'));
	assert('z' == tolower_ascii('z'));
	assert(0 == compare_ascii("hello", 5, "hello", 5));
	assert(0 == compare_ascii("Hello", 5, "hello", 5));
	assert(0 == compare_rfc1459("Hello", 5, "hello", 5));
	assert(0 == compare_rfc1459("H~l\\o", 5, "h^l|o", 5));
	assert(0 == compare_strict_rfc1459("Hello", 5, "hello", 5));
	assert(0 == compare_strict_rfc1459("Hel\\o", 5, "hel|o", 5));
}
#endif


/**	integer = compare_ascii(s1, s2) */
static int luafunc_compare_ascii(lua_State *L)
{

	const char *s1, *s2;
	size_t s1len, s2len;
	if(!lua_isstring(L, 1) || !lua_isstring(L, 2))
		return 0; /* Number of return values. */
	s1 = lua_tolstring(L, 1, &s1len);
	s2 = lua_tolstring(L, 2, &s2len);
	lua_pushinteger(L, compare_ascii(s1, s1len, s2, s2len));
	return 1; /* Number of return values. */
}


/**	integer = compare_rfc1459(s1, s2) */
static int luafunc_compare_rfc1459(lua_State *L)
{
	const char *s1, *s2;
	size_t s1len, s2len;
	if(!lua_isstring(L, 1) || !lua_isstring(L, 2))
		return 0; /* Number of return values. */
	s1 = lua_tolstring(L, 1, &s1len);
	s2 = lua_tolstring(L, 2, &s2len);
	lua_pushinteger(L, compare_rfc1459(s1, s1len, s2, s2len));
	return 1; /* Number of return values. */
}


/**	integer = compare_strict_rfc1459(s1, s2) */
static int luafunc_compare_strict_rfc1459(lua_State *L)
{
	const char *s1, *s2;
	size_t s1len, s2len;
	if(!lua_isstring(L, 1) || !lua_isstring(L, 2))
		return 0; /* Number of return values. */
	s1 = lua_tolstring(L, 1, &s1len);
	s2 = lua_tolstring(L, 2, &s2len);
	lua_pushinteger(L, compare_strict_rfc1459(s1, s1len, s2, s2len));
	return 1; /* Number of return values. */
}


lua_Alloc realLuaAllocFunc = NULL;
ptrdiff_t memLimit = 0;
ptrdiff_t memAllocCounter = 0;

static void *memLimitLuaAlloc(void *ud, void *ptr, size_t oldSize, size_t newSize)
{
	if(memLimit)
	{
		/* Using sign because this can legit end up negative (old memory realloc smaller). */
		memAllocCounter -= oldSize;
		memAllocCounter += newSize;
		if(newSize > oldSize)
		{
			if(memAllocCounter > memLimit)
			{
				memLimit = 0;
				return NULL;
			}
		}
	}
	return realLuaAllocFunc(ud, ptr, oldSize, newSize);
}


/**	(limit, counter) = memory_limit([limit] [, thread])
	If limit is set to 0 then there is no memory limit, the limit is removed.
	If limit is set > 0 then memory allocations are limited to this many bytes.
	If limit is omitted, just returns the current limit and counter.
	If a limit is set (limit>0) then the counter is reset to 0.
	counter is how many bytes have been allocated.
	Note: only works with one lua state, unless called with the same value on all states.
	When the limit is tripped, the lua allocator returns NULL and then the limit is removed.
	Note: consider emergency GC in lua 5.2.
	For strictness and/or accurate accounting, do a GC collection before setting the limit.
	To simply use the counter, set the limit very high, such as 0x7FFFFFFF.
	If thread is specified, the limit is set for the current state AND the thread.
*/
static int luafunc_memory_limit(lua_State *L)
{
	lua_State *lthread = lua_tothread(L, 2);
	if(lua_isnumber(L, 1))
	{
		lua_Integer lim = lua_tointeger(L, 1);
		if(0 == lim)
		{
			memLimit = 0;
		}
		else if(lim > 0)
		{
			void *ud;
			if(&memLimitLuaAlloc != lua_getallocf(L, &ud))
			{
				if(!realLuaAllocFunc)
				{
					realLuaAllocFunc = lua_getallocf(L, &ud);
					if(!realLuaAllocFunc)
					{
						_programError("luafunc_memory_limit: lua_getallocf returned NULL", 0);
						return 0;
					}
				}
				lua_setallocf(L, &memLimitLuaAlloc, ud);
				if(&memLimitLuaAlloc != lua_getallocf(L, NULL))
				{
					_programError("luafunc_memory_limit: lua_setallocf did not set the memory allocator", 0);
					return 0;
				}
			}
			if(lthread)
			{
				if(&memLimitLuaAlloc != lua_getallocf(lthread, &ud))
				{
					if(!realLuaAllocFunc)
					{
						_programError("luafunc_memory_limit: internal error, realLuaAllocFunc expected to be set", 0);
						return 0;
					}
					lua_setallocf(lthread, &memLimitLuaAlloc, ud);
					if(&memLimitLuaAlloc != lua_getallocf(lthread, NULL))
					{
						_programError("luafunc_memory_limit: lua_setallocf did not set the memory allocator", 0);
						return 0;
					}
				}
			}
			memLimit = lim;
			memAllocCounter = 0;
		}
	}
	lua_pushinteger(L, memLimit);
	lua_pushinteger(L, memAllocCounter);
	return 2; /* Number of return values. */
}


#ifdef HAS_UTF32toUTF8char
/**	string = UTF32toUTF8char(utf32number, ...)
	All the UTF32 codepoint numbers passed in compose the result string.
*/
static int luafunc_UTF32toUTF8char(lua_State *L)
{
	char _buf[32];
	char *buf = _buf;
	size_t bufmax = sizeof(_buf) / sizeof(_buf[0]);
	size_t bufpos = 0;
	int iarg;
	for(iarg = 1; !lua_isnone(L, iarg); iarg++)
	{
		if(lua_isnumber(L, iarg))
		{
			int n;
			unsigned long u8 = lua_tointeger(L, iarg);
			if(bufmax - bufpos < 4)
			{
				char *newbuf;
				bufmax *= 2;
				if(buf == _buf)
				{
					newbuf = malloc(bufmax);
					if(!newbuf)
					{
						return 0; /* Number of return values. */
					}
					memcpy(newbuf, buf, bufpos);
				}
				else
				{
					newbuf = realloc(buf, bufmax);
					if(!newbuf)
					{
						free(buf);
						return 0; /* Number of return values. */
					}
				}
				buf = newbuf;
			}
			n = UTF32toUTF8char(u8, buf + bufpos);
			bufpos += n;
		}
	}
	lua_pushlstring(L, buf, bufpos);
	if(buf != _buf)
	{
		free(buf);
	}
	return 1; /* Number of return values. */
}
#endif

static int luafunc_socket_startup(lua_State *L)
{
	if (_socketsStartup()) {
		return luaL_error(L, "unable to initialize sockets");
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int luafunc_socket_cleanup(lua_State *L)
{
	_socketsCleanup();
	return 0;
}


/*
	Usage: irccmd <address>[:<port>] [<nick> [<alt_nick>]]
	nick and alt_nick can contain replacements:
		%d for a random digit.
		%a for a random alpha char, %A for uppercase.
		%n for a random number between 10 and 999 (between 2 and 3 digits)
		%s for a random string of between 2 and 3 alpha chars, %S for uppercase.
	If no alt_nick supplied and nick contains no replacements, <nick>_%n is used.
	Other switches supported:
		-raw            write all received commands to stderr.
		-raw=<file>     where <file> is either stdout, stderr or a filename.
*/
int luaopen_irccmd(lua_State *L)
{
#if _DEBUG
	compare_Test();
	fileio_AllTests();
	fprintf(stderr, "Tests completed\n");
#endif

	frandom_init(&frand, rrandom());

	luaL_Reg array[] = {
		{ "random", &luafunc_random },
		{ "frandom", &luafunc_frandom },
		{ "milliseconds", &luafunc_milliseconds },
		{ "milliseconds_diff", &luafunc_milliseconds_diff },
		{ "console_print", &luafunc_console_print },
		{ "console_print_err", &luafunc_console_print_err },
		{ "irc_input", &luafunc_irc_input },
		{ "irc_parse", &luafunc_irc_parse },
		{ "compare_ascii", &luafunc_compare_ascii },
		{ "compare_rfc1459", &luafunc_compare_rfc1459 },
		{ "compare_strict_rfc1459", &luafunc_compare_rfc1459 },
		{ "socket_connect", &luafunc_socket_connect },
		{ "socket_bind", &luafunc_socket_bind },
		{ "socket_listen", &luafunc_socket_listen },
		{ "socket_accept", &luafunc_socket_accept },
		{ "socket_close", &luafunc_socket_close },
		{ "socket_blocking", &luafunc_socket_blocking },
		{ "socket_linger", &luafunc_socket_linger },
		/* { "socket_reuseaddr", &luafunc_socket_reuseaddr }, */
		{ "socket_shutdown", &luafunc_socket_shutdown },
		{ "socket_send", &luafunc_socket_send },
		{ "socket_receive", &luafunc_socket_receive },
		{ "socket_select", &luafunc_socket_select },
		{ "memory_limit", &luafunc_memory_limit },
#ifdef HAS_UTF32toUTF8char
		{ "UTF32toUTF8char", &luafunc_UTF32toUTF8char },
#endif
		{ "socket_startup", &luafunc_socket_startup },
		{ "socket_cleanup", &luafunc_socket_cleanup },
		{ NULL, NULL }
	};
	luaL_register(L, "internal", array);

	return 1;
}

