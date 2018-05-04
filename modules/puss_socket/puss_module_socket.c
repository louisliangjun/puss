// puss_module_socket.c

#ifdef _WIN32
	#include <winsock2.h>
	#include <windows.h>
	#include <mswsock.h>

	typedef int	socklen_t;
	#define get_last_error()	(int)WSAGetLastError()

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

	typedef int	SOCKET;
	#define INVALID_SOCKET		-1
	#define closesocket(fd)		close(fd)
	#define get_last_error()	errno

	static int socket_set_nonblock(SOCKET fd, int nonblock) {
		int flags = fcntl(fd, F_GETFL, 0);
		if( flags < 0 )
			return -1;
		if( nonblock ) {
			flags |= O_NONBLOCK;
		} else {
			block = O_NONBLOCK;
			flags &= ~block;
		}
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
		sprintf(saddr, "%u.%u.%u.%u:%u", (ip>>24)&0xFF, (ip>>16)&0xFF, (ip>>8)&0xFF, ip&0xFF, ntohs(ipv4->sin_port));
		lua_pushstring(L, saddr);
	} else {
		lua_pushnil(L);
	}
}

typedef struct Socket {
	SOCKET		fd;
} Socket;

#define PUSS_SOCKET_NAME	"[PussSocket]"

static inline Socket* lua_check_socket(lua_State* L, int arg, int check_valid_fd) {
	Socket* ud = (Socket*)luaL_checkudata(L, arg, PUSS_SOCKET_NAME);
	if( check_valid_fd ) {
		luaL_argcheck(L, socket_check_valid(ud->fd), arg, "socket fd invalid");
	}
	return ud;
}

static int lua_socket_close(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 0);
	if( socket_check_valid(ud->fd) ) {
		closesocket(ud->fd);
		ud->fd = INVALID_SOCKET;
	}
	return 0;
}

static int lua_socket_bind(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	const char* ip = luaL_optstring(L, 2, "0.0.0.0");
	unsigned port = (unsigned)luaL_optinteger(L, 3, 0);
	struct sockaddr addr;
	socklen_t addr_len = sizeof(addr);
	int res;
	socket_addr_build(L, &addr, ip, port);
	res = bind(ud->fd, &addr, sizeof(addr));
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
	} else if( getsockname(ud->fd, &addr, &addr_len) < 0 ) {
		lua_pushnil(L);
	} else {
		socket_addr_push(L, &addr);
	}
	return 2;
}

static int lua_socket_listen(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	int backlog = (int)luaL_optinteger(L, 2, 5);
	struct sockaddr addr;
	socklen_t addr_len = sizeof(addr);
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
	struct sockaddr addr;
	socklen_t addr_len = sizeof(addr);
	SOCKET fd = accept(ud->fd, &addr, &addr_len);
	Socket* sock;
	if( !socket_check_valid(fd) ) {
		lua_pushnil(L);
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	
	sock = (Socket*)lua_newuserdata(L, sizeof(Socket));
	sock->fd = fd;
	luaL_setmetatable(L, PUSS_SOCKET_NAME);
	return 1;
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
	int res = send(ud->fd, msg, (int)len, 0);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	return 1;
}

static int lua_socket_recv(lua_State* L) {
	luaL_Buffer B;
	Socket* ud = lua_check_socket(L, 1, 1);
	int len = (int)luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
	int res;
	luaL_buffinitsize(L, &B, len);
	res = recv(ud->fd, B.b, len, 0);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
	} else {
		luaL_pushresultsize(&B, (size_t)res);
	}
	return 2;
}

static int lua_socket_sendto(lua_State* L) {
	Socket* ud = lua_check_socket(L, 1, 1);
	const char* ip = luaL_checkstring(L, 2);
	unsigned port = (unsigned)luaL_checkinteger(L, 3);
	size_t len = 0;
	const char* msg = luaL_checklstring(L, 4, &len);
	struct sockaddr addr;
	int res;
	socket_addr_build(L, &addr, ip, port);
	res = sendto(ud->fd, msg, (int)len, 0, &addr, sizeof(addr));
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
	struct sockaddr addr;
	socklen_t addr_len = sizeof(struct sockaddr);
	luaL_Buffer B;
	memset(&addr, 0, sizeof(addr));
	luaL_buffinitsize(L, &B, len);
	res = recvfrom(ud->fd, B.b, len, 0, &addr, &addr_len);
	lua_pushinteger(L, res);
	if( res < 0 ) {
		lua_pushinteger(L, get_last_error());
		return 2;
	}
	luaL_pushresultsize(&B, (size_t)res);
	socket_addr_push(L, &addr);
	return 3;
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

static const luaL_Reg socket_methods[] =
	{ {"__gc",			lua_socket_close}
	, {"close",			lua_socket_close}
	, {"bind",			lua_socket_bind}
	, {"listen",		lua_socket_listen}
	, {"accept",		lua_socket_accept}
	, {"connect",		lua_socket_connect}
	, {"send",			lua_socket_send}
	, {"recv",			lua_socket_recv}
	, {"sendto",		lua_socket_sendto}
	, {"recvfrom",		lua_socket_recvfrom}
	, {"set_nonblock",	lua_socket_set_nonblock}
	, {NULL, NULL}
	};

static int lua_socket_create(lua_State* L) {
	int af = (int)luaL_optinteger(L, 1, AF_INET);
	int type = (int)luaL_optinteger(L, 2, SOCK_STREAM);
	int protocol = (int)luaL_optinteger(L, 3, IPPROTO_TCP);
	struct sockaddr addr;
	socklen_t addr_len = sizeof(struct sockaddr);
	Socket* ud = lua_newuserdata(L, sizeof(Socket));
	ud->fd = socket(af, type, protocol);
	luaL_setmetatable(L, PUSS_SOCKET_NAME);
	if( !socket_check_valid(ud->fd) ) {
		lua_pop(L, 1);
		lua_pushnil(L);
		lua_pushinteger(L, get_last_error());
	} else if( getsockname(ud->fd, &addr, &addr_len) < 0 ) {
		lua_pushnil(L);
	} else {
		socket_addr_push(L, &addr);
	}
	return 2;
}

#define PUSS_SOCKET_LIB_NAME	"[PussSocketLib]"

static const luaL_Reg socket_lib_methods[] =
	{ {"socket_create",	lua_socket_create}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

#ifdef _WIN32
	static void wsa_cleanup(void) {
		WSACleanup();
	}
#endif

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
#ifdef _WIN32
		WSADATA wsa_data;
		WSAStartup(MAKEWORD(2,0),&wsa_data);
		atexit(wsa_cleanup);
#endif
		__puss_iface__ = puss;
	}

	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_SOCKET_LIB_NAME)==LUA_TTABLE ) {
		return 1;
	}
	lua_pop(L, 1);

	puss_push_const_table(L);
	{
		lua_pushinteger(L, AF_INET);		lua_setfield(L, -2, "AF_INET");
		lua_pushinteger(L, AF_UNIX);		lua_setfield(L, -2, "AF_UNIX");
		lua_pushinteger(L, AF_INET6);		lua_setfield(L, -2, "AF_INET6");

		lua_pushinteger(L, PF_INET);		lua_setfield(L, -2, "PF_INET");
		lua_pushinteger(L, PF_UNIX);		lua_setfield(L, -2, "PF_UNIX");
		lua_pushinteger(L, PF_INET6);		lua_setfield(L, -2, "PF_INET6");

		lua_pushinteger(L, SOCK_STREAM);	lua_setfield(L, -2, "SOCK_STREAM");
		lua_pushinteger(L, SOCK_DGRAM);		lua_setfield(L, -2, "SOCK_DGRAM");
		lua_pushinteger(L, SOCK_RAW);		lua_setfield(L, -2, "SOCK_RAW");

		lua_pushinteger(L, IPPROTO_ICMP);	lua_setfield(L, -2, "IPPROTO_ICMP");
		lua_pushinteger(L, IPPROTO_IGMP);	lua_setfield(L, -2, "IPPROTO_IGMP");
		lua_pushinteger(L, IPPROTO_TCP);	lua_setfield(L, -2, "IPPROTO_TCP");
		lua_pushinteger(L, IPPROTO_UDP);	lua_setfield(L, -2, "IPPROTO_UDP");
	}
	lua_pop(L, 1);

	if( luaL_newmetatable(L, PUSS_SOCKET_NAME) ) {
		luaL_setfuncs(L, socket_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");	// metatable.__index = metatable
	}
	lua_pop(L, 1);

	luaL_newlib(L, socket_lib_methods);
	return 1;
}

