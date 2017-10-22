-- luaproxy.lua

local table_insert = table.insert
local table_concat = table.concat

local function pasre_header(apis, enums, fname)
	local expose = nil

	local function parse_line(line)
		local enum = line:match('^%s*#%s*define%s+(GLFW_[_%w]+)%s+.+$')
		if enum then
			table_insert(enums, {expose, enum})
			return
		end

		local re_expose = line:match('^%s*(#%s*if%s+defined%s*%(%s*GLFW_EXPOSE_.*)$')
		if re_expose then
			expose = re_expose
			return
		end

		local re_expose = line:match('^%s*(#%s*if%s+defined%s*%(%s*VK_.*)$')
		if re_expose then
			expose = re_expose
			return
		end

		local re_undef = line:match('^%s*#%s*undef.*$')
		if re_undef then
			expose = nil
			return
		end

		local ret, name, args = line:match('^%s*GLFWAPI%s+(%w+)%s+(%w+)%s*%(%s*(.*)%s*%)%s*;%s*$')
		if ret then
			table_insert(apis, {expose, ret, name, args})
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
			local line = table_concat({...})
			table_insert(output_lines, line)
		end

		cb(writeln)

		local f = io.open(filename, 'w')
		f:write( table_concat(output_lines, '\n') )
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
		generate_line(apis, writeln, function(writeln, ret, name, args)
			writeln( string.format('  %-24s(*%s)(%s);', ret, name, args) )
		end)
		writeln('} GLFWProxy;')
		writeln()
		writeln('#ifndef _GLFWPROXY_NOT_USE_SYMBOL_MACROS')
		writeln('	extern GLFWProxy* __glfw_proxy__;')
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

