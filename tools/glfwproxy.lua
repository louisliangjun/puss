-- glfwproxy.lua

local function pasre_header(apis, enums, fname)
	local expose = nil

	local function parse_line(line)
		local enum = line:match('^%s*#%s*define%s+(GLFW_[_%w]+)%s+.+$')
		if enum then
			table.insert(enums, {expose, enum})
			return
		end

		local re_expose = line:match('^%s*(#%s*if%s+defined%s*%(%s*GLFW_EXPOSE_.*)$')
		if re_expose then
			expose = re_expose
			return
		end

		re_expose = line:match('^%s*(#%s*if%s+defined%s*%(%s*VK_.*)$')
		if re_expose then
			expose = re_expose
			return
		end

		local re_endif = line:match('^%s*#%s*endif.*$')
		if re_endif then
			expose = nil
			return
		end

		local ret, name, args = line:match('^%s*GLFWAPI%s+(.+)%s+([_%w]+)%s*%(%s*(.*)%s*%)%s*;%s*$')
		if ret then
			table.insert(apis, {expose, ret, name, args})
			return
		end
	end

	for line in io.lines(fname) do
		parse_line(line)
	end
end

function main()
	local apis = {}
	local enums = {}

	local root = vlua.match_arg('^%-path=(.+)$') or '.'

	pasre_header(apis, enums, vlua.filename_format(root..'/glfw3.h'))
	pasre_header(apis, enums, vlua.filename_format(root..'/glfw3native.h'))

	local function generate_file(filename, cb)
		local output_lines = {}

		local function writeln(...)
			local line = table.concat({...})
			table.insert(output_lines, line)
		end

		cb(writeln)

		local f = io.open(filename, 'w')
		f:write( table.concat(output_lines, '\n') )
		f:close()
	end

	local function generate_line(arr, writeln, cb)
		local last_expose
		for _,v in ipairs(arr) do
			local expose = v[1]
			if expose~=last_expose then
				if last_expose then writeln('#endif') end
				if expose then writeln(expose) end
				last_expose = expose
			end
			cb(writeln, table.unpack(v, 2))
		end
		if last_expose then writeln('#endif') end
	end

	generate_file(vlua.filename_format(root..'/'..'glfw3proxy.symbols'), function(writeln)
		generate_line(apis, writeln, function(writeln, ret, name, args)
			writeln('__GLFWPROXY_SYMBOL(', name, ')')
		end)
	end)

	generate_file(vlua.filename_format(root..'/'..'glfw3proxy.enums'), function(writeln)
		generate_line(enums, writeln, function(writeln, enum)
			writeln('__GLFWPROXY_ENUM(', enum, ')')
		end)
	end)

	generate_file(vlua.filename_format(root..'/'..'glfw3proxy.h'), function(writeln)
		writeln('// NOTICE : generate by glfwproxy.lua')
		writeln()
		writeln('#ifndef _GLFW_PROXY_H__')
		writeln('#define _GLFW_PROXY_H__')
		writeln()
		writeln('#include "glfw3.h"')
		writeln('#include "glfw3native.h"')
		writeln()
		writeln('PUSS_DECLS_BEGIN')
		writeln()
		writeln('typedef struct _GLFWProxy {')

		for _,v in ipairs(apis) do
			local expose, ret, name, args = table.unpack(v)
			if expose then
				writeln(expose)
				writeln('  ', string.format('%-24s(*%s)(%s);', ret, name, args))
				writeln('#else')
				writeln('  ', string.format('%-24s(*%s)(%s); /* keep pos */', 'void*', name, ''))
				writeln('#endif')
			else
				writeln(string.format('  %-24s(*%s)(%s);', ret, name, args))
			end
		end

		writeln('} GLFWProxy;')
		writeln()
		writeln('#ifndef _GLFWPROXY_NOT_USE_SYMBOL_MACROS')
		writeln('	#ifndef __glfw_proxy__')
		writeln('		#error "need define __glfw_proxy__(sym) first"')
		writeln('	#endif')
		generate_line(apis, writeln, function(writeln, ret, name, args)
			writeln('	#define ' .. name .. '	__glfw_proxy__(' .. name .. ')')
		end)
		writeln('#endif//_GLFWPROXY_NOT_USE_SYMBOL_MACROS')
		writeln()
		writeln('PUSS_DECLS_END')
		writeln()
		writeln('#endif//_GLFW_PROXY_H__')
		writeln()
	end)
end

--[[
-----------------------------
-- puss glfw

vmake_target_add('puss_glfw_proxy', function(target)
	local src = path_concat('3rd', 'glfw-3.2.1', 'include', 'GLFW')
	local dst = path_concat(PUSS_INCLUDE_PATH, 'GLFW')
	local glfwproxy_lua = path_concat('tools', 'glfwproxy.lua')
	local deps =
		{ glfwproxy_lua
		, make_copy('glfw3.h', dst, src)
		, make_copy('glfw3native.h', dst, src)
		}
	if not( check_deps(path_concat(dst, 'glfw3proxy.h'), deps)
		and check_deps(path_concat(dst, 'glfw3proxy.symbols'), deps)
		and check_deps(path_concat(dst, 'glfw3proxy.enums'), deps)
		)
	then
		shell_execute(vlua.self, glfwproxy_lua, '-path="'..dst..'"')
	end
end)

vmake_target_add('puss_glfw', function(target)
	vmake('puss_core', 'puss_glfw_proxy')

	local srcpath = path_concat('modules', target)
	make_copy('puss_module_glfw.h', PUSS_INCLUDE_PATH, srcpath)

	-- TODO : now test
	local libs =
		{ path_concat('3rd', 'glfw-3.2.1', 'build', 'src', 'libglfw3.a')
		, '-lGL -lX11 -lXi -lXrandr -lXxf86vm -lXinerama -lXcursor -lrt -lm'
		}
	local objs = make_c2objs(srcpath, INCLUDES, '-fPIC', '-fvisibility=hidden')
	local output = path_concat('bin', 'modules', target..PUSS_MODULE_SUFFIX..'.so')
	return make_target(output, {vlua._script, objs}, CC, CFLAGS, '-shared', '-o', output, objs, libs)
end)

--]]

--[[
// puss_module_glfw.h

#ifndef _INC_PUSS_MODULE_GLFW_H_
#define _INC_PUSS_MODULE_GLFW_H_

#include "puss_module.h"
#include "GLFW/glfw3proxy.h"

PUSS_DECLS_BEGIN

typedef GLFWProxy	PussGLFWInterface;

PUSS_DECLS_END

#endif//_INC_PUSS_MODULE_GLFW_H_

--]]

--[[
// puss_module_glfw.c

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define _GLFWPROXY_NOT_USE_SYMBOL_MACROS
#include "puss_module_glfw.h"

static PussGLFWInterface glfw_iface;

static void register_enums(lua_State* L) {
	puss_push_const_table(L);

	#define __GLFWPROXY_ENUM(sym)		lua_pushinteger(L, sym); lua_setfield(L, -2, #sym);
	#include "GLFW/glfw3proxy.enums"
	#undef __GLFWPROXY_ENUM

	lua_pop(L, 1);
}

static luaL_Reg glfw_methods[] =
	{ {"", NULL}
	, {"", NULL}
	, {"", NULL}
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_MODULE_EXPORT int __puss_module_init__(lua_State* L, PussInterface* puss) {
	if( !__puss_iface__ ) {
		__puss_iface__ = puss;

		#define __GLFWPROXY_SYMBOL(sym)		glfw_iface.sym = sym;
		#include "GLFW/glfw3proxy.symbols"
		#undef __GLFWPROXY_SYMBOL
	}

	puss_interface_register(L, "PussGLFWInterface", &glfw_iface); // register C interface
	register_enums(L);

	luaL_newlib(L, glfw_methods);
	return 0;
}

--]]
