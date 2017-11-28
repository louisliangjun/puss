// puss_module_socket.c

#ifdef _WIN32
	#include <winsock2.h>
	#include <windows.h>
	#include <mswsock.h>

	typedef int	socklen_t;
	#define get_last_error()	(int)WSAGetLastError()

	static int socket_set_nonblocking(SOCKET fd) {
		u_long flag = 1;
		return ioctlsocket(fd, FIONBIO, &flag);
	}

#else
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#include <fcntl.h>

	typedef int	SOCKET;
	#define INVALID_SOCKET		-1
	#define closesocket(fd)		close(fd)
	#define get_last_error()	errno

	static int socket_set_nonblocking(SOCKET fd) {
		int flags = fcntl(fd, F_GETFL, 0);
		if( flags < 0 )
			return -1;
		flags |= O_NONBLOCK;
		return fcntl(fd, F_SETFL, flags);
	}

#endif

#include <memory.h>
#include <stdlib.h>
#include <errno.h>

#include "puss_module.h"

#define socket_check_valid(fd)		((fd)!=INVALID_SOCKET)

static void socket_addr_build(lua_State* L, struct sockaddr* addr, const char* ip, unsigned port) {
	struct sockaddr_in* ipv4_addr = (struct sockaddr_in*)addr;
	uint32_t ip_addr;
	if( port > 0xFFFF )
		luaL_error(L, "bad port: %u", port);

	ip_addr = (ip==NULL) ? INADDR_ANY : inet_addr(ip);
	if( ip_addr==INADDR_NONE )
		luaL_error(L, "inet_addr(%s) failed", ip);

	memset( addr, 0, sizeof(struct sockaddr) );
	ipv4_addr->sin_family = AF_INET;
	ipv4_addr->sin_addr.s_addr = ip_addr;
	ipv4_addr->sin_port = htons( (uint16_t)port );
}

static void socket_addr_push(lua_State* L, const struct sockaddr* addr) {
	// IPv4
	if( addr->sa_family==AF_INET ) {
		struct sockaddr_in* ipv4 = (struct sockaddr_in*)addr;
		uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
		char saddr[32];
		sprintf(saddr, "%u.%u.%u.%u:%u", (ip>>24)&0xFF, (ip>>16)&0xFF, (ip>>8)&0xFF, ip&0xFF, ntohs(ipv4->sin_port));
		lua_pushstring(L, saddr);
	} else {
		lua_pushnil(L);
	}
}

typedef struct SocketUDP {
	SOCKET		fd;
	int			err;
	size_t		rlen;
	char		rbuf[16];	// keep 16 bytes
} SocketUDP;

#define PUSS_SOCKET_UDP_NAME	"[PussSocketUDP]"

static inline SocketUDP* lua_check_socket_udp(lua_State* L, int arg, int check_valid_fd) {
	SocketUDP* ud = (SocketUDP*)luaL_checkudata(L, arg, PUSS_SOCKET_UDP_NAME);
	if( check_valid_fd ) {
		luaL_argcheck(L, socket_check_valid(ud->fd), arg, "socket fd invalid");
	}
	return ud;
}

static int lua_socket_udp_close(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 0);
	if( !socket_check_valid(ud->fd) ) {
		closesocket(ud->fd);
		ud->fd = INVALID_SOCKET;
	}
	return 0;
}

static int lua_socket_udp_connect(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	const char* ip = luaL_checkstring(L, 2);
	unsigned port = (unsigned)luaL_checkinteger(L, 3);
	struct sockaddr addr;
	socket_addr_build(L, &addr, ip, port);
	lua_pushinteger(L, connect(ud->fd, &addr, sizeof(addr)));
	ud->err = get_last_error();
	return 1;
}

static int lua_socket_udp_send(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 2, &len);
	lua_pushinteger(L, send(ud->fd, msg, len, 0));
	ud->err = get_last_error();
	return 1;
}

static int lua_socket_udp_recv(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	int res = (int)recv(ud->fd, ud->rbuf, ud->rlen, 0);
	ud->err = get_last_error();
	lua_pushinteger(L, res);
	if( res > 0 ) {
		lua_pushlstring(L, ud->rbuf, (size_t)res);
	} else {
		lua_pushnil(L);
	}
	return 2;
}

static int lua_socket_udp_sendto(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	const char* ip = luaL_checkstring(L, 2);
	unsigned port = (unsigned)luaL_checkinteger(L, 3);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 4, &len);
	struct sockaddr addr;
	socket_addr_build(L, &addr, ip, port);
	lua_pushinteger(L, sendto(ud->fd, msg, (int)len, 0, &addr, sizeof(addr)));
	ud->err = get_last_error();
	return 1;
}

static int lua_socket_udp_recvfrom(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	int res = 0;
	struct sockaddr addr;
	socklen_t addr_len = sizeof(struct sockaddr);
	memset(&addr, 0, sizeof(addr));
	res = (int)recvfrom(ud->fd, ud->rbuf, ud->rlen, 0, &addr, &addr_len);
	ud->err = get_last_error();
	lua_pushinteger(L, res);
	if( res > 0 ) {
		lua_pushlstring(L, ud->rbuf, (size_t)res);
		socket_addr_push(L, &addr);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 3;
}

static int lua_socket_udp_errno(lua_State* L) {
	SocketUDP* ud = lua_check_socket_udp(L, 1, 1);
	lua_pushinteger(L, ud->err);
	return 1;
}

static const luaL_Reg socket_udp_methods[] =
	{ {"__gc",			lua_socket_udp_close}
	, {"close",			lua_socket_udp_close}
	, {"connect",		lua_socket_udp_connect}
	, {"send",			lua_socket_udp_send}
	, {"recv",			lua_socket_udp_recv}
	, {"sendto",		lua_socket_udp_sendto}
	, {"recvfrom",		lua_socket_udp_recvfrom}
	, {"errno",			lua_socket_udp_errno}
	, {NULL, NULL}
	};

static int lua_socket_udp_create(lua_State* L) {
	size_t rbuf_max = (size_t)luaL_checkinteger(L, 1);
	const char* ip = luaL_optstring(L, 2, "0.0.0.0");
	unsigned port = (unsigned)luaL_optinteger(L, 3, 0);
	struct sockaddr addr;
	socklen_t addr_len = sizeof(struct sockaddr);
	SOCKET fd = INVALID_SOCKET;
	SocketUDP* ud;

	socket_addr_build(L, &addr, ip, port);

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if( !socket_check_valid(fd) )
		return luaL_error(L, "socket create failed: invalied fd!");

	if( bind(fd, &addr, addr_len)!=0 ) {
		closesocket(fd);
		return luaL_error(L, "socket bind failed!");
	}

	if( socket_set_nonblocking(fd)!=0 ) {
		closesocket(fd);
		return luaL_error(L, "socket set nonblocking failed!");
	}

	ud = (SocketUDP*)lua_newuserdata(L, sizeof(SocketUDP) + rbuf_max);
	ud->fd = INVALID_SOCKET;
	ud->err = 0;
	ud->rlen = rbuf_max;
	if( luaL_newmetatable(L, PUSS_SOCKET_UDP_NAME) ) {
		luaL_setfuncs(L, socket_udp_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");	// metatable.__index = metatable
	}
	lua_setmetatable(L, -2);
	ud->fd = fd;
	getsockname(fd, &addr, &addr_len);
	socket_addr_push(L, &addr);
	return 2;
}

#define PUSS_SOCKET_LIB_NAME	"[PussSocketLib]"

static const luaL_Reg socket_lib_methods[] =
	{ {"socket_udp_create",	lua_socket_udp_create}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
#ifdef _WIN32
		WSADATA wsa_data;
		WSAStartup(MAKEWORD(2,0),&wsa_data);
		atexit(WSACleanup);
#endif
		__puss_iface__ = puss;
	}

	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_SOCKET_LIB_NAME)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		luaL_newlib(L, socket_lib_methods);
	}
	return 1;
}

