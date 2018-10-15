-- luaproxy.lua

local function pasre_header(gen, fname)
	local s = ''
	for line in io.lines(fname) do
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

function main()
	local apis = {}

	local function gen_api(ret, name, args, anames, vaargs)
		-- NOTICE : not use LUA_COMPAT_MODULE
		if name=='luaL_pushmodule' then return end
		if name=='luaL_openlib' then return end
		table.insert(apis, {ret, name, args, anames, vaargs})
	end

	local root = vlua.match_arg('^%-path=(.+)$') or '.'
	local gen = vlua.match_arg('^%-gen=(.+)$') or '.'

	pasre_header(gen_api, vlua.filename_format(root..'/lua.h'))
	pasre_header(gen_api, vlua.filename_format(root..'/lauxlib.h'))
	pasre_header(gen_api, vlua.filename_format(root..'/lualib.h'))

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

	generate_file(vlua.filename_format(gen..'/'..'luaproxy.symbols'), function(writeln)
		for _,v in ipairs(apis) do
			writeln('__LUAPROXY_SYMBOL(', v[2], ')')
		end
	end)

	generate_file(vlua.filename_format(root..'/'..'luaproxy.h'), function(writeln)
		writeln('// NOTICE : generate by luaproxy.lua')
		writeln()
		writeln('#ifndef _LUA_PROXY_H__')
		writeln('#define _LUA_PROXY_H__')
		writeln()
		writeln('#ifdef __cplusplus')
		writeln('extern "C" {')
		writeln('#endif')
		writeln()
		writeln('#define __LUA_PROXY_FINISH_DUMMY__ "lua_proxy_dummy"')
		writeln()
		writeln('struct LuaProxy {')
		for _,v in ipairs(apis) do
			local ret, name, args, anames = table.unpack(v)
			writeln( string.format('  %-20s(*%s)(%s);', ret, name, args) )
		end
		writeln('')
		writeln('  const char* __lua_proxy_finish_dummy__;')
		writeln('};')
		writeln()
		writeln('#ifdef __cplusplus')
		writeln('}')
		writeln('#endif')
		writeln()
		writeln('#ifndef _LUAPROXY_NOT_USE_SYMBOL_MACROS')
		writeln('  #ifndef __lua_proxy_sym__')
		writeln('    #error "need define __lua_proxy_sym__(sym) first!"')
		writeln('  #endif')
		for _,v in ipairs(apis) do
			local name = v[2]
			writeln('  #define ' .. name .. '	__lua_proxy_sym__(' .. name .. ')')
		end
		writeln('#endif//_LUAPROXY_NOT_USE_SYMBOL_MACROS')
		writeln()
		writeln('#endif//_LUA_PROXY_H__')
		writeln()
	end)

	-- NOTICE : now not need use importlib
	--[[
	generate_file(vlua.filename_format(root..'/'..'luaproxy_importlib.inl'), function(writeln)
		writeln('static LuaProxy* __lua_proxy_imp__ = 0;')
		writeln('static void __lua_proxy_import__(LuaProxy* proxy) { __lua_proxy_imp__=proxy; }')
		writeln()
		writeln('int luaL_error(lua_State *L, const char *fmt, ...) {')
		writeln('  va_list argp;')
		writeln('  va_start(argp, fmt);')
		writeln('  luaL_where(L, 1);')
		writeln('  lua_pushvfstring(L, fmt, argp);')
		writeln('  va_end(argp);')
		writeln('  lua_concat(L, 2);')
		writeln('  return lua_error(L);')
		writeln('}')
		writeln()
		writeln('const char * lua_pushfstring(lua_State *L, const char *fmt, ...) {')
		writeln('  const char * res;')
		writeln('  va_list argp;')
		writeln('  va_start(argp, fmt);')
		writeln('  res = lua_pushvfstring(L,fmt,argp);')
		writeln('  va_end(argp);')
		writeln('  return res;')
		writeln('}')
		writeln()
		for _,v in ipairs(apis) do
			local ret, name, args, anames, vaargs = table.unpack(v)
			if not vaargs then
				writeln(ret .. ' ' .. name .. '(' .. args .. ') {')
					if ret~='void' then writeln(' return') end
					writeln(' __lua_proxy_imp__->' .. name .. '(' .. table.concat(anames, ',') .. '); ')
				writeln('}')
			end
		end
		writeln()
	end)
	--]]
end

