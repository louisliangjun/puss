// puss_socket.c

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <windows.h>
	#include <mswsock.h>

	typedef int	socklen_t;
	#define get_last_error()		(int)WSAGetLastError()
	#define can_ignore_error(err)	((err)==WSAEWOULDBLOCK)

	static int socket_set_nonblock(SOCKET fd, int nonblock) {
		u_long flag = nonblock ? 1 : 0;
		return ioctlsocket(fd, FIONBIO, &flag);
	}
#else
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <fcntl.h>

	#include <sys/time.h>
	static int GetCurrentTime(void) {
		struct timeval tv;
		uint64_t ret;
		gettimeofday(&tv, 0);
		ret = tv.tv_sec;
		ret *= 1000;
		ret += (tv.tv_usec/1000);
		return (int)ret;
	}

	static void Sleep(unsigned ms) {
		usleep(ms*1000);
	}

	typedef int	SOCKET;
	#define INVALID_SOCKET		-1
	#define closesocket(fd)		close(fd)
	#define get_last_error()	errno
	#define can_ignore_error(err)	((err)==EAGAIN || (err)==EINTR)

	static int socket_set_nonblock(SOCKET fd, int nonblock) {
		int flags = fcntl(fd, F_GETFL, 0);
		if( flags < 0 )
			return -1;
		if( nonblock ) {
			flags |= O_NONBLOCK;
		} else {
			flags &= ~O_NONBLOCK;
		}
		return fcntl(fd, F_SETFL, flags);
	}
#endif

#include <memory.h>
#include <stdlib.h>
#include <errno.h>

#include "puss_plugin.h"

#define socket_check_valid(fd)		((fd)!=INVALID_SOCKET)

#ifdef _MSC_VER
	#pragma warning(disable: 4996)		// VC++ depart functions warning
#endif

static void socket_addr_build(lua_State* L, struct sockaddr* addr, const char* ip, unsigned port) {
	struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)addr;
	unsigned ip_addr;
	if( port > 0xFFFF )
		luaL_error(L, "bad port: %u", port);

	ip_addr = (ip==NULL) ? INADDR_ANY : inet_addr(ip);
	if( ip_addr==INADDR_NONE )
		luaL_error(L, "inet_addr(%s) failed", ip);

	memset( addr, 0, sizeof(struct sockaddr) );
	ipv4_addr->sin_family = AF_INET;
	ipv4_addr->sin_addr.s_addr = ip_addr;
	ipv4_addr->sin_port = htons( (unsigned short)port );
}

static void socket_addr_push(lua_State* L, const struct sockaddr* addr) {
	// IPv4
	if( addr->sa_family==AF_INET ) {
		struct sockaddr_in* ipv4 = (struct sockaddr_in*)addr;
		unsigned ip = ntohl(ipv4->sin_addr.s_addr);
		char saddr[32];
		sprintf(saddr, "%u.%u.%u.%u:%u", (ip>>24)&0xFF, (ip>>16)&0xFF, (ip>>8)&0xFF, ip&0xFF, (unsigned)ntohs(ipv4->sin_port));
		lua_pushstring(L, saddr);
	} else {
		lua_pushnil(L);
	}
}

typedef struct Socket {
	SOCKET			fd;
	struct sockaddr	addr;
	struct sockaddr	peer;
} Socket;

#define PUSS_SOCKET_NAME	"[PussSocket]"

static inline void socket_close(Socket* ud) {
	if( socket_check_valid(ud->fd) ) {
		closesocket(ud->fd);
		ud->fd = INVALID_SOCKET;
	}
}

static inline Socket* lua_check_socket(lua_State* L, int arg, int check_valid_fd) {
	Socket* ud = (Socket*)luaL_checkudata(L, arg, PUSS_SOCKET_NAME);
	if( check_valid_fd ) {
		luaL_argcheck(L, socket_check_valid(ud->fd), arg, "socket fd invalid");
	}
	return ud;
}

static int lua_socket_valid(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	lua_pushboolean(L, socket_check_valid(ud->fd));
	return 1;
}

static int lua_socket_addr(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	socket_addr_push(L, &(ud->addr));
	return 1;
}

static int lua_socket_peer(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	socket_addr_push(L, &(ud->peer));
	return 1;
}

static int _enum_get(lua_State* L, int arg, int val) {
	if( lua_type(L, arg)==LUA_TSTRING ) {
		lua_pushvalue(L, arg);
		if( lua_rawget(L, 1)==LUA_TNUMBER )
			val = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);
	}
	return val;
}

static int lua_socket_create(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	if( !lua_getmetatable(L, 1) )
		return luaL_error(L, "bad socket meta!");
	if( lua_getfield(L, -1, "__enums")!=LUA_TTABLE )
		return luaL_error(L, "bad socket enums table!");
	lua_replace(L, 1);
	lua_pop(L, 1);
	if( !socket_check_valid(ud->fd) ) {
		ud->fd = socket( _enum_get(L, 2, AF_INET), _enum_get(L, 3, SOCK_STREAM), _enum_get(L, 4, IPPROTO_TCP) );
	}
	lua_pushboolean(L, socket_check_valid(ud->fd));
	return 1;
}

static int lua_socket_close(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	socket_close(ud);
	return 0;
}

static int lua_socket_bind(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	const char* ip = luaL_optstring(L, 2, "0.0.0.0");
	unsigned port = (unsigned)luaL_optinteger(L, 3, 0);
	int reuse_addr = (int)lua_toboolean(L, 4);
	struct sockaddr* addr = &(ud->addr);
	socklen_t addr_len = sizeof(struct sockaddr);
	int res;
	socket_addr_build(L, addr, ip, port);
	if( reuse_addr ) {
		setsockopt(ud->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr));
	}
	res = bind(ud->fd, addr, addr_len);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
	} else {
		getsockname(ud->fd, addr, &addr_len);
		socket_addr_push(L, &(ud->addr));
	}
	return 2;
}

static int lua_socket_listen(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int backlog = (int)luaL_optinteger(L, 2, 5);
	int res = listen(ud->fd, backlog);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_accept(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int timeout = (int)luaL_optinteger(L, 2, 0);
	struct sockaddr* peer = &(ud->peer);
	socklen_t addr_len = sizeof(struct sockaddr);
	SOCKET fd = INVALID_SOCKET;
	int need_accept = 1;
	Socket* sock;
	if( timeout > 0 ) {
#ifdef _WIN32
		FD_SET rset;
		int total;
		struct timeval tv;
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000)*1000;
		FD_ZERO(&rset);
		FD_SET(ud->fd, &rset);
		total = select(0, &rset, NULL, NULL, &tv);
		need_accept = (total > 0);
#else
		int start = (int)GetCurrentTime();
		int now = start;
		for(;;) {
			fd = accept(ud->fd, peer, &addr_len);
			if( fd!=INVALID_SOCKET) {
				need_accept = 0;
				break;
			}
			now = (int)GetCurrentTime();
			if( (now-start) > timeout )
				break;
			Sleep(1);
		}
#endif
	}
	if( need_accept ) {
		fd = accept(ud->fd, peer, &addr_len);
	}
	if( !socket_check_valid(fd) ) {
		lua_pushnil(L);
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	
	sock = (Socket*)lua_newuserdata(L, sizeof(Socket));
	sock->fd = fd;
	sock->addr = ud->addr;
	sock->peer = ud->peer;
	luaL_setmetatable(L, PUSS_SOCKET_NAME);
	addr_len = sizeof(struct sockaddr);
	getsockname(sock->fd, &(sock->addr), &addr_len);
	socket_addr_push(L, peer);
	return 2;
}

static int lua_socket_connect(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	const char* ip = luaL_checkstring(L, 2);
	unsigned port = (unsigned)luaL_checkinteger(L, 3);
	struct sockaddr addr;
	int res;
	socket_addr_build(L, &addr, ip, port);
	res = connect(ud->fd, &addr, sizeof(addr));
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_send(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 2, &len);
	int offset = (int)luaL_optinteger(L, 3, 0);
	int res;
	if( (offset < 0) || (offset >= len) ) {
		return luaL_error(L, "offset(%d) out of range(%d)", offset, len);
	}
	res = send(ud->fd, msg+offset, (int)(len - offset), 0);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_recv(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int len = (int)luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
	int recv_full = (int)lua_toboolean(L, 3);
	luaL_Buffer B;
	int res;
	if( len <= 0 )
		return 0;
	luaL_buffinitsize(L, &B, len);
	if( recv_full ) {
		res = recv(ud->fd, B.b, len, MSG_PEEK);
		if( res > 0 ) {
			if( res < len )
				return 0;
			luaL_pushresultsize(&B, (size_t)res);
			while( len > 0 ) {
				int res = recv(ud->fd, B.b, len, 0);
				if( res > 0 ) {
					len -= res;
				} else if( res==0 ) {
					socket_close(ud);
					break;
				} else {
					res = get_last_error();
					if( !can_ignore_error(res) ) {
						socket_close(ud);
						break;
					}
				}
			}
			return 1;
		}
	} else {
		res = recv(ud->fd, B.b, len, 0);
		if( res > 0 ) {
			luaL_pushresultsize(&B, (size_t)res);
			return 1;
		}
	}
	if( res==0 ) {
		socket_close(ud);
	} else {
		res = get_last_error();
		if( !can_ignore_error(res) ) {
			socket_close(ud);
		}
	}
	lua_pushnil(L);
	lua_pushinteger(L, res);
	return 2;
}

static int lua_socket_sendto(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 2, &len);
	const char* ip = luaL_optstring(L, 3, NULL);
	unsigned port = (unsigned)luaL_optinteger(L, 4, 0);
	struct sockaddr* peer = &(ud->peer);
	int res;
	if( ip ) { socket_addr_build(L, peer, ip, port); }
	res = sendto(ud->fd, msg, (int)len, 0, peer, sizeof(struct sockaddr));
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_recvfrom(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int len = (int)luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
	int res = 0;
	struct sockaddr* peer = &(ud->peer);
	socklen_t addr_len = sizeof(struct sockaddr);
	luaL_Buffer B;
	memset(peer, 0, addr_len);
	luaL_buffinitsize(L, &B, len);
	res = recvfrom(ud->fd, B.b, len, 0, peer, &addr_len);
	if( res < 0 ) {
		lua_pushnil(L);
		lua_pushinteger(L, get_last_error());
	} else {
		luaL_pushresultsize(&B, (size_t)res);
		socket_addr_push(L, peer);
	}
	return 2;
}

static int lua_socket_set_timeout(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int recv_timeout_ms = (int)luaL_optinteger(L, 2, 0);
	int send_timeout_ms = (int)luaL_optinteger(L, 3, 0);
	int res = 0;
#ifdef _WIN32
	DWORD rtv = recv_timeout_ms;
	DWORD stv = send_timeout_ms;
#else
	struct timeval rtv;
	struct timeval stv;
	rtv.tv_sec = recv_timeout_ms / 1000; 
	rtv.tv_usec = (recv_timeout_ms % 1000) * 1000;
	stv.tv_sec = send_timeout_ms / 1000; 
	stv.tv_usec = (send_timeout_ms % 1000) * 1000;
#endif
	if( recv_timeout_ms && res==0 )	res = setsockopt(ud->fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&rtv, sizeof(rtv));
	if( send_timeout_ms && res==0 )	res = setsockopt(ud->fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&stv, sizeof(stv));
	lua_pushinteger(L, res);
	if( res ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_set_nonblock(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int nonblock = lua_toboolean(L, 2);
	int res = socket_set_nonblock(ud->fd, nonblock ? 1 : 0);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_set_broadcast(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	unsigned short port = (unsigned short)luaL_optinteger(L, 2, 0);
	const char* broadcast_ip = luaL_optstring(L, 3, NULL);
	struct sockaddr_in* peer = (struct sockaddr_in*)&(ud->peer);
	socklen_t addr_len = sizeof(struct sockaddr);
	int opt = -1;
	int res;
	if( port ) {
		memset(peer, 0, addr_len);
		peer->sin_family = AF_INET;
		peer->sin_addr.s_addr = broadcast_ip ? inet_addr(broadcast_ip) : htonl(INADDR_BROADCAST);
		peer->sin_port = htons(port);
	}
	res = setsockopt(ud->fd, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_utable(lua_State* L) {
	lua_check_socket(L, 1, 0);
	if( lua_getuservalue(L, 1)!=LUA_TTABLE ) {
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setuservalue(L, 1);
	}
	return 1;
}

static const luaL_Reg socket_methods[] =
	{ {"__index",		NULL}
	, {"__enums",		NULL}
	, {"__gc",			lua_socket_close}
	, {"valid",			lua_socket_valid}
	, {"addr",			lua_socket_addr}
	, {"peer",			lua_socket_peer}
	, {"create",		lua_socket_create}
	, {"close",			lua_socket_close}
	, {"bind",			lua_socket_bind}
	, {"listen",		lua_socket_listen}
	, {"accept",		lua_socket_accept}
	, {"connect",		lua_socket_connect}
	, {"send",			lua_socket_send}
	, {"recv",			lua_socket_recv}
	, {"sendto",		lua_socket_sendto}
	, {"recvfrom",		lua_socket_recvfrom}
	, {"set_timeout",	lua_socket_set_timeout}
	, {"set_nonblock",	lua_socket_set_nonblock}
	, {"set_broadcast",	lua_socket_set_broadcast}
	, {"utable",		lua_socket_utable}
	, {NULL, NULL}
	};

static int lua_socket_new(lua_State* L) {
	Socket* ud = lua_newuserdata(L, sizeof(Socket));
	ud->fd = INVALID_SOCKET;
	luaL_setmetatable(L, PUSS_SOCKET_NAME);
	return 1;
}

static const luaL_Reg socket_lib_methods[] =
	{ {"socket_new",	lua_socket_new}
	, {NULL, NULL}
	};

#ifdef _WIN32
	static int wsa_inited = 0;

	static void wsa_cleanup(void) {
		WSACleanup();
	}
#endif

void puss_socket_reg(lua_State* L) {
#ifdef _WIN32
	if( !wsa_inited ) {
		WSADATA wsa_data;
		WSAStartup(MAKEWORD(2,0),&wsa_data);
		atexit(wsa_cleanup);
		wsa_inited = 1;
	}
#endif

	if( luaL_newmetatable(L, PUSS_SOCKET_NAME) ) {
		luaL_setfuncs(L, socket_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");	// metatable.__index = metatable

		lua_newtable(L);
	#define _REG_ENUM(e)	lua_pushinteger(L, (e));	lua_setfield(L, -2, #e)
		_REG_ENUM(AF_INET);
		_REG_ENUM(AF_UNIX);
		_REG_ENUM(AF_INET6);
		_REG_ENUM(PF_INET);
		_REG_ENUM(PF_UNIX);
		_REG_ENUM(PF_INET6);
		_REG_ENUM(SOCK_STREAM);
		_REG_ENUM(SOCK_DGRAM);
		_REG_ENUM(SOCK_RAW);
		_REG_ENUM(IPPROTO_ICMP);
		_REG_ENUM(IPPROTO_IGMP);
		_REG_ENUM(IPPROTO_TCP);
		_REG_ENUM(IPPROTO_UDP);
	#undef _REG_ENUM
		lua_setfield(L, -2, "__enums");
	}
	lua_pop(L, 1);

	luaL_setfuncs(L, socket_lib_methods, 0);
}

