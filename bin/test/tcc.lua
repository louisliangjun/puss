local puss_tinycc = puss.load_plugin('puss_tinycc')

puss_tinycc.tcc_load_file = function(fname, flags)
	local f = io.open(fname, 'r') or io.open(puss._path..'/'..fname, 'r')
	if not f then return end
	local ctx = f:read('*a')
	f:close()
	return ctx
end

local function test_module(tcc)
	tcc:set_options('-Wall -Werror -g -nostdinc -nostdlib')
	tcc:set_output_type('memory')
	tcc:compile_string([[#include "core/c/header.h"

		static int foo(lua_State* L) {
			lua_Integer a = luaL_checkinteger(L, 1);
			lua_Integer b = luaL_checkinteger(L, 2);
			lua_Integer c = luaL_optinteger(L, 3, 1);
			lua_Integer r = (a + b) / c;
			lua_pushinteger(L, r);
			return 1;
		}

		int __module_init(lua_State* L) {
			lua_newtable(L);
			lua_pushcfunction(L, foo);
			lua_setfield(L, -2, "foo");
			return 1;
		}
	]])

	tcc:add_file('core/c/libtcc1.c')
	return tcc:relocate('__module_init')
end

function __main__()
	local lib = puss_tinycc.tcc_load_module(test_module)
	print('got it 111!')
	print(puss.trace_pcall(lib.foo, 1, 2))
	print('got it 222!')
	print(puss.trace_pcall(lib.foo, 1, 2, 0))
	print('got it 333!')
	print(puss.trace_pcall(lib.foo, 1, 2, 3))
	print('got it 444!')
	puss.sleep(5000)
end
