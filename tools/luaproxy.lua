-- luaproxy.lua
-- 

local root = vlua.match_arg('^%-path=(.+)$') or '.'

local function pasre_header(gen, fname)
	local s = ''
	local fpath = vlua.filename_format(root..'/'..fname)
	for line in io.lines(fpath) do
		local decl
		if s:len() > 0 then
			line = line:match('^%s*(.-)%s*$')
			decl = s .. ' ' .. line
		else
			decl = line:match('^%s*LUA%w*_API%s+(.-)%s*$')
		end

		if decl then
			local ret, name, args = decl:match('^%s*(.-)%s*%(%s*(.-)%)%s*%((.-)%)%s*;')
			local vaargs = nil
			if ret then
				s = ''
				local anames = {}
				if args~='void' then
					local ass = args .. ','
					for av in ass:gmatch('%s*(.-)%s*,') do
						if av=='...' then
							vaargs = true
						else
							local aname = av:match('^.-([%w_]+)[%s%[%]]*$')
							if aname then table.insert(anames, aname) end
						end
					end
				end
				gen(ret, name, args, anames, vaargs)
			else
				s = decl	-- need more
			end
		else
			s = ''
		end
	end
end

local apis = {}

local function gen_luaproxy_h()
	local output = io.open(vlua.filename_format(root..'/'..'luaproxy.h'), 'w')
	output:write('// NOTICE : generate by luaproxy.lua\n\n')
	output:write('#ifndef _LUA_PROXY_INC_DECL_H__\n')
	output:write('#define _LUA_PROXY_INC_DECL_H__\n\n')
	output:write('typedef struct _LuaProxy {\n')
	output:write('	int     __lua_proxy_inited__;\n')
	for _,v in ipairs(apis) do
		local ret, name, args, anames = table.unpack(v)
		output:write( string.format('  %-16s(*_%s)(%s);\n', ret, name, args) )
	end
	output:write('} LuaProxy;\n\n')
	output:write('#endif//_LUA_PROXY_INC_DECL_H__\n')
	output:close()
end

local function gen_luaproxy_export()
	local output = io.open(vlua.filename_format(root..'/'..'luaproxy_export.inl'), 'w')
	output:write('// NOTICE : generate by luaproxy.lua\n\n')
	output:write('#include "luaproxy.h"\n')
	output:write([===[
static LuaProxy* __lua_proxy_export__(void) {
	static LuaProxy _proxy = {0};
	if( _proxy.__lua_proxy_inited__==0 ) {
		_proxy.__lua_proxy_inited__ = 1;

		#ifndef LUA_COMPAT_MODULE
			#define luaL_pushmodule	NULL
			#define luaL_openlib	NULL
		#endif
]===])

	for _,v in ipairs(apis) do
		local ret, name, args, anames = table.unpack(v)
		output:write('		_proxy._' .. name .. ' = ' .. name .. ';\n')
	end

	output:write([===[
	}
	return &_proxy;
}
]===])

	output:close()
end

local function gen_luaproxy_import()
	local output = io.open(vlua.filename_format(root..'/'..'luaproxy_import.inl'), 'w')
	output:write('// NOTICE : generate by luaproxy.lua\n\n')
	output:write('#include "luaproxy.h"\n')
	output:write('static LuaProxy* __lua_proxy__ = 0;\n')
	output:write('static void __lua_proxy_import__(LuaProxy* proxy) { __lua_proxy__=proxy; }\n\n')

	output:write([[
int luaL_error(lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  luaL_where(L, 1);
  lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_concat(L, 2);
  return lua_error(L);
}

const char * lua_pushfstring(lua_State *L, const char *fmt, ...) {
  const char * res;
  va_list argp;
  va_start(argp, fmt);
  res = lua_pushvfstring(L,fmt,argp);
  va_end(argp);
  return res;
}

]])

	for _,v in ipairs(apis) do
		local ret, name, args, anames, vaargs = table.unpack(v)
		if not vaargs then
			output:write(ret .. ' ' .. name .. '(' .. args .. ') {')
				if ret~='void' then output:write(' return') end
				output:write(' __lua_proxy__->_' .. name .. '(' .. table.concat(anames, ',') .. '); ')
			output:write('}\n')
		end
	end
	output:close()
end

function main()
	local function gen_api(ret, name, args, anames, vaargs)
		table.insert(apis, {ret, name, args, anames, vaargs})
	end

	pasre_header(gen_api, 'lua.h')
	pasre_header(gen_api, 'lauxlib.h')
	pasre_header(gen_api, 'lualib.h')

	gen_luaproxy_h()
	gen_luaproxy_export()
	gen_luaproxy_import()
end

