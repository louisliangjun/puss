-- imgui_wrap.lua

local function parse_arg(arg, brackets)
	-- try fmt: type name[arr]
	local atype, name, attr = arg:match('^(.-)%s+([_%w]+)%s*(%b[])$')
	if atype then return atype, name, attr end

	-- try fmt: type (*func)(arg)
	local tp, ni, ai = arg:match('^(.-)@(%d+)%s*@(%d+)$')
	if tp then
		-- print(tp, ni, ai)
		local t = brackets[tonumber(ni)]
		return tp .. ' ' .. t .. brackets[tonumber(ai)], t:match('[_%w]+'), 'function'
	end

	-- try fmt: type name
	atype, name = arg:match('^(.-)%s+([_%w]+)$')
	if atype then return atype, name end

	-- try fmt: ...
	if arg:match('%.%.%.') then return nil, nil, '...' end

	-- try fmt: type
	return arg
end

local function parse_args(args)
	local r = {}
	args = args:match('^%s*%(%s*(.-)%s*%)%s*$') -- remove ()
	if args=='' then return r end	-- no args
	args = args .. ','	-- append ','
	local brackets = {}
	args = args:gsub('%b()', function(v)
		table.insert(brackets, v)
		return string.format('@%d', #brackets)
	end)
	for v in args:gmatch('%s*(.-)%s*,') do
		if v=='' then break end
		local arg, def = v:match('^(.-)%s*=%s*(.-)%s*$')
		local atype, name, attr = parse_arg(arg or v, brackets)
		def = def and def:gsub('@(%d+)', function(v) return brackets[tonumber(v)] or v end)
		table.insert(r, {atype=atype, name=name, attr=attr, def=def})
	end
	return r
end

local ignore_apis =
	{ NewFrame = true
	, Render = true
	, Shutdown = true
	, EndFrame = true
	, SetAllocatorFunctions = true
	, MemAlloc = true
	, MemFree = true
	}

local function pasre_apis(apis, src)
	for namespace_block in src:gmatch('namespace%s+ImGui%s*(%b{})') do
		-- print('NAMESPACE BLOCK:', namespace_block)
		for ret, name, args in namespace_block:gmatch('IMGUI_API%s+([_%w%s%*&]-)%s*([_%w]+)%s*(%b()).-;') do
			-- print('API', ret, name, args)
			if not ignore_apis[name] then
				table.insert(apis, {ret, name, args})
			end
		end
		for ret, name, args in namespace_block:gmatch('static%s+inline%s+([_%w%s%*&]-)%s*([_%w]+)%s*(%b())%s*%b{}') do
			-- print('API', ret, name, args)
			if not ignore_apis[name] then
				table.insert(apis, {ret, name, args})
			end
		end
	end
end

local function parse_enums(enums, src)
	for block in src:gmatch('enum%s+[_%w]*%s*(%b{})%s*;') do
		-- print('ENUMS BLOCK:', block)
		local macros = {}
		for macro, scope in block:gmatch('%s(#if%w+.-)\n(.-)#endif') do
			-- print('  MACRO', macro, scope)
			for line in scope:gmatch('%s*(.-)%s*[,\n]') do
				local enum = line:match('^%s*([_%w]+)%s*')
				-- print('  MACRO ENUM', enum, line)
				if enum then macros[enum] = macro end
			end
		end
		for line in block:gmatch('[%s{]*(.-)%s*[,}]') do
			local enum = line:match('^%s*([_%w]+)%s*')
			-- print('  ENUM', enum, line)
			if enum then table.insert(enums, {enum, macros[enum]}) end
		end
	end
end

local function parse_inttypes(types, src)
	types['ImU32'] = 'uint'
	types['ImGuiID'] = 'uint'
	types['ImWchar'] = 'uint'
	types['int'] = 'int'
	for enumtype in src:gmatch('typedef%s+int%s+(%w+)%s*;') do
		types[enumtype] = 'int'
	end
end

function main()
	local out = vlua.match_arg('^%-out=(.+)$') or '.'
	local src
	do
		local fname = vlua.match_arg('^%-src=(.+)$') or 'imgui.h'
		local f = io.open(fname, 'r')
		src = f:read('*a')
		f:close()
	end
	src = src:gsub('/%*.-%*/', ' ')		-- remove comment /* */
	src = src:gsub('\r', '')			-- remove \r
	src = src:gsub('//.-\n', '\n')		-- remove line comment
	-- print(src)

	local apis = {}
	local enums = {}
	local inttypes = {}
	local wraps = {}
	pasre_apis(apis, src)
	parse_enums(enums, src)
	parse_inttypes(inttypes, src)

	local function generate_file(filename, cb)
		local output_lines = {}

		local mt = {}
		mt.__index = mt
		mt.writeln = function(self, ...)
			local ok, line = pcall(table.concat, {...})
			if not ok then
				print('concat(', ..., ')')
				error(line)
			end
			local pos = #output_lines + 1
			output_lines[pos] = line
			return pos
		end
		mt.__len = function(self) return #output_lines end
		mt.remove = function(self, line) return table.remove(output_lines, line) end
		mt.revert = function(self, mark)
			if not mark then return #output_lines end
			while #output_lines > mark do
				table.remove(output_lines)
			end
		end

		cb(setmetatable({}, mt))

		local f = io.open(filename, 'w')
		f:write( table.concat(output_lines, '\n') )
		f:close()
	end

	generate_file(vlua.filename_format(out..'/'..'imgui_enums.inl'), function(dst)
		local last_macro
		for _,v in ipairs(enums) do
			local enum, macro = table.unpack(v)
			if macro~=last_macro then
				if last_macro then dst:writeln('#endif') end
				if macro then dst:writeln(macro) end
				last_macro = macro
			end
			dst:writeln('__REG_ENUM(', enum, ')')
		end
		if last_macro then dst:writeln('#endif') end
		dst:writeln()
	end)

	generate_file(vlua.filename_format(out..'/'..'imgui_lua.inl'), function(dst)
		local functions = {}

		local function gen_wrapname(name)
			local w = string.format('%s', name)
			local n = 0
			while functions[w] do
				n = n + 1
				w = string.format('%s_%d', name, n)
			end
			functions[w] = name
			return w
		end

		local RE_UINT = '^unsigned%s+int$'			-- unsigned int
		local RE_PBOOL = '^(bool)%s*%*$'			-- bool*
		local RE_PINT = '^(int)%s*%*$'				-- int*
		local RE_PUINT = '^(unsigned%s+int)%s*%*$'	-- unsigned int*
		local RE_PFLOAT = '^(float)%s*%*$'			-- float*
		local RE_RFLOAT = '^(float)%s*%&$'			-- float&
		local RE_PDOUBLE = '^(double)%s*%*$'		-- double*
		local RE_CSTR = '^const%s+char%s*%*$'		-- const char*
		local RE_CVOID = '^const%s+void%s*%*$'		-- const void*
		local RE_RIMVEC2 = '^ImVec2%s*%&$'			-- ImVec2&
		local RE_CIMVEC2 = '^const%s+ImVec2%s*%&$'	-- const ImVec2&
		local RE_RIMVEC4 = '^ImVec4%s*%&$'			-- ImVec4&
		local RE_CIMVEC4 = '^const%s+ImVec4%s*%&$'	-- const ImVec4&

		local function gen_lua_wrap(w, ret, name, args)
			dst:writeln(string.format('static int wrap_%s(lua_State* L) {', w))
			local narg_pos, narg_use = dst:writeln('	int __narg__ = lua_gettop(L);'), false
			local iarg_pos, iarg_use = dst:writeln('	int __iarg__ = 0;'), false

			-- ret decl
			do
				local atype, aname = ret, '__ret__'
				if atype=='void' then
					-- ignore
				elseif inttypes[atype] or atype=='float' or atype=='double' or atype=='bool' or atype:match(RE_UINT) or atype:match(RE_CSTR) or atype:match(RE_CVOID) then
					dst:writeln('	', atype, ' ', aname, ';')
				elseif atype=='ImVec2' or atype:match(RE_CIMVEC2) then
					dst:writeln('	ImVec2 ', aname, ';')
				elseif atype=='ImVec4' or atype:match(RE_CIMVEC4) then
					dst:writeln('	ImVec4 ', aname, ';')
				else
					error(string.format('[NotSupport]	ret type(%s)', atype))
				end
			end

			-- args decl
			for i, a in ipairs(args) do
				local atype, aname, attr = a.atype, a.name or string.format('__arg_%d', i), a.attr
				if attr then
					if attr=='function' then
						error(string.format('[NotSupportFunctionPointer]	arg type(%s)', atype))
					elseif attr=='...' then
						if args[i-1] and args[i-1].atype:match(RE_CSTR) then
							-- prepare use fmt only
						else
							error('[NotSupportVaArgs]	(...)')
						end
					elseif attr=='[]' then
						error(string.format('[NotSupportArray]	array arg type(%s)', atype))
					elseif attr:match('%[%d%]') then
						if inttypes[atype] or atype=='float' or atype=='double' then
							dst:writeln('	', atype, ' ', aname, attr, ';')
						else
							error(string.format('[NotSupportFixedArray]	fixed array arg type(%s)', atype))
						end
					else
						error(string.format('[NotSupportAttr]	attr(%s) arg type(%s)', attr, atype))
					end
				elseif not atype then
					error(string.format('[NotSupport]	arg ('..tostring(aname)..')'))
				elseif atype=='va_list' then
					error('[NotSupportVaList] va_list arg type')
				elseif inttypes[atype] or atype:match(RE_UINT) or atype=='float' or atype=='double' or atype=='bool' or atype:match(RE_CSTR) or atype:match(RE_CVOID) then
					dst:writeln('	', atype, ' ', aname, ';')
				elseif atype=='ImVec2' or atype:match(RE_RIMVEC2) or atype:match(RE_CIMVEC2) then
					dst:writeln('	ImVec2 ', aname, ';')
				elseif atype=='ImVec4' or atype:match(RE_RIMVEC4) or atype:match(RE_CIMVEC4) then
					dst:writeln('	ImVec4 ', aname, ';')
				elseif inttypes[atype] then
					dst:writeln('	', atype, ' ', aname, ';')
				elseif atype:match(RE_RFLOAT) then
					dst:writeln('	float ', aname, ' = 0.0f;')
				else
					local dt = atype:match(RE_PBOOL) or atype:match(RE_PINT) or atype:match(RE_PUINT) or atype:match(RE_PFLOAT) or atype:match(RE_PDOUBLE)
					if dt then
						dst:writeln('	', dt, ' __value_', aname, ';')
						dst:writeln('	', dt, '* ', aname, ';')
					else
						error(string.format('[NotSupport]	arg type(%s)', atype))
					end
				end
			end

			-- args fetch
			for i, a in ipairs(args) do
				local atype, aname, attr = a.atype, a.name or string.format('__arg_%d', i), a.attr
				if attr then
					if attr=='...' then
						-- not fetch
					elseif attr:match('%[%d]') then
						local n = tonumber(attr:match('%[(%d)%]'))
						iarg_use = true
						if inttypes[atype] then
							for i=1, n do dst:writeln('	', aname, '[',i-1,'] = ', '('..atype..')luaL_checkinteger(L, ++__iarg__);') end
						elseif atype=='float' or atype=='double' then
							for i=1, n do dst:writeln('	', aname, '[',i-1,'] = ', '('..atype..')luaL_checknumber(L, ++__iarg__);') end
						end
					else
						error(string.format('[NotSupport]	type(%s)', atype))
					end
				elseif inttypes[atype] or atype:match(RE_UINT) then
					iarg_use = true
					dst:writeln('	', aname, ' = ', a.def and '('..atype..')luaL_optinteger(L, ++__iarg__, '..a.def..')' or '('..atype..')luaL_checkinteger(L, ++__iarg__)', ';')
				elseif atype=='float' or atype=='double' then
					iarg_use = true
					dst:writeln('	', aname, ' = ', a.def and '('..atype..')luaL_optnumber(L, ++__iarg__, '..a.def..')' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				elseif atype=='bool' then
					iarg_use = true
					if a.def then
						narg_use = true
						dst:writeln('	', aname, ' = (__iarg__ < __narg__) ? lua_toboolean(L, ++__iarg__)!=0 : ('..a.def..');')
					else
						dst:writeln('	', aname, ' = lua_toboolean(L, ++__iarg__)!=0;')
					end
				elseif inttypes[atype] then
					iarg_use = true
					dst:writeln('	', aname, ' = ', a.def and '(int)luaL_optinteger(L, ++__iarg__, '..a.def..')' or '(int)luaL_checkinteger(L, ++__iarg__)', ';')
				elseif atype:match(RE_PBOOL) then
					iarg_use, narg_use = true, true
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? lua_toboolean(L, ++__iarg__)!=0 : false;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PINT) then
					iarg_use, narg_use = true, true
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (int)luaL_checkinteger(L, ++__iarg__) : 0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PUINT) then
					iarg_use, narg_use = true, true
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (unsigned int)luaL_checkinteger(L, ++__iarg__) : 0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PFLOAT) then
					iarg_use, narg_use = true, true
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (float)luaL_checknumber(L, ++__iarg__) : 0.0f;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PDOUBLE) then
					iarg_use, narg_use = true, true
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? luaL_checknumber(L, ++__iarg__) : 0.0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_RFLOAT) then
					-- output only, ignore fetch
				elseif atype:match(RE_CSTR) then
					iarg_use = true
					dst:writeln('	', aname, ' = ', a.def and 'luaL_optstring(L, ++__iarg__, '..a.def..')' or 'luaL_checkstring(L, ++__iarg__)', ';')
				elseif atype:match(RE_CVOID) then
					iarg_use = true
					if a.def then
						narg_use = true
						dst:writeln('	', aname, ' = (__iarg__ < __narg__) ? lua_topointer(L, ++__iarg__) : ', a.def, ';')
					else
						dst:writeln('	', aname, ' = lua_topointer(L, ++__iarg__);')
					end
				elseif atype=='ImVec2' or atype:match(RE_RIMVEC2) or atype:match(RE_CIMVEC2) then
					iarg_use = true
					if a.def then dst:writeln('	', aname, ' = ', a.def, ';') end
					dst:writeln('	', aname, '.x = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.x)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.y = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.y)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				elseif atype=='ImVec4' or atype:match(RE_RIMVEC4) or atype:match(RE_CIMVEC4) then
					iarg_use = true
					if a.def then dst:writeln('	', aname, ' = ', a.def, ';') end
					dst:writeln('	', aname, '.x = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.x)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.y = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.y)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.z = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.z)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.w = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.w)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				else
					error(string.format('[NotSupport]	arg type(%s)', atype))
				end
			end

			-- invoke
			do
				local prefix = ret=='void' and '	' or '	__ret__ = '
				local ts = { prefix, 'ImGui::', name, '(' }
				if #args > 0 then
					for i, a in ipairs(args) do
						if a.attr=='...' then
							assert( args[i-1].atype:match(RE_CSTR), 'bad logic' )
							table.insert(ts, #ts-1, '"%s",')
						else
							table.insert(ts, a.name or string.format('__arg_%d', i))
							table.insert(ts, ',')
						end
					end
					table.remove(ts)
				end
				table.insert(ts, ');')
				dst:writeln(table.concat(ts, ''))
			end

			local nret = 0

			-- push ret
			do
				local atype, aname = ret, '__ret__'
				if atype=='void' then
					-- ignore
				elseif inttypes[atype] or atype:match(RE_UINT) then
					nret = nret + 1
					dst:writeln('	lua_pushinteger(L, (lua_Integer)', aname, ');')
				elseif atype=='float' or atype=='double' or atype:match(RE_RFLOAT) then
					nret = nret + 1
					dst:writeln('	lua_pushnumber(L, ', aname, ');')
				elseif atype=='bool' then
					nret = nret + 1
					dst:writeln('	lua_pushboolean(L, ', aname, ' ? 1 : 0);')
				elseif atype:match(RE_CSTR) then
					nret = nret + 1
					dst:writeln('	lua_pushstring(L, ', aname, ');')
				elseif atype=='ImVec2' or atype:match(RE_RIMVEC2) or atype:match(RE_CIMVEC2) then
					nret = nret + 2
					dst:writeln('	lua_pushnumber(L, ', aname, '.x);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.y);')
				elseif atype=='ImVec4' or atype:match(RE_RIMVEC4) or atype:match(RE_CIMVEC4) then
					nret = nret + 4
					dst:writeln('	lua_pushnumber(L, ', aname, '.x);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.y);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.z);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.w);')
				else
					error(string.format('ret type(%s)', atype))
				end
			end

			-- put output args
			for i, a in ipairs(args) do
				local atype, aname, attr = a.atype, a.name or string.format('__arg_%d', i), a.attr
				if attr then
					if attr=='...' then
						-- no ret
					elseif attr:match('%[%d]') then
						local n = tonumber(attr:match('%[(%d)%]'))
						nret = nret + n
						if inttypes[atype] then
							for i=1, n do dst:writeln('	lua_pushinteger(L, ', aname, '[',i-1,']);') end
						elseif atype=='float' or atype=='double' then
							for i=1, n do dst:writeln('	lua_pushnumber(L, ', aname, '[',i-1,']);') end
						end
					else
						error(string.format('[NotSupport]	type(%s)', atype))
					end
				elseif not atype then
					error(string.format('not support ('..tostring(aname)..')'))
				elseif atype:match(RE_PBOOL) then
					nret = nret + 1
					dst:writeln('	if(', aname, ') lua_pushboolean(L, ', '(*', aname, ') ? 1 : 0); else lua_pushnil(L);')
				elseif atype:match(RE_PINT) or atype:match(RE_PUINT) then
					nret = nret + 1
					dst:writeln('	if(', aname, ') lua_pushinteger(L, ', '(lua_Integer)(*', aname, ')); else lua_pushnil(L);')
				elseif atype:match(RE_PFLOAT) or atype:match(RE_PDOUBLE) then
					nret = nret + 1
					dst:writeln('	if(', aname, ') lua_pushnumber(L, ', '(*', aname, ')); else lua_pushnil(L);')
				elseif atype:match(RE_RIMVEC2) then
					nret = nret + 2
					dst:writeln('	lua_pushnumber(L, ', aname, '.x);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.y);')
				elseif atype:match(RE_RIMVEC2) then
					nret = nret + 4
					dst:writeln('	lua_pushnumber(L, ', aname, '.x);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.y);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.z);')
					dst:writeln('	lua_pushnumber(L, ', aname, '.w);')
				end
			end
			dst:writeln('	return ', nret, ';')
			dst:writeln('}')

			if not iarg_use then dst:remove(iarg_pos) end
			if not narg_use then dst:remove(narg_pos) end
		end

		local function gen_function(ret, name, args)
			dst:writeln()
			dst:writeln(string.format('// [Declare]  %s %s%s;', ret, name, args))
			args = parse_args(args)
			for _, a in ipairs(args) do
				dst:writeln(string.format('//   [Param] type(%s) name(%s) attr(%s) def(%s)', a.atype, a.name, a.attr or '', a.def or ''))
			end
			local w = gen_wrapname(name)
			local mark = #dst
			local ok, err = pcall(gen_lua_wrap, w, ret, name, args)
			if ok then
				table.insert(wraps, w)
			else
				dst:revert(mark)
				dst:writeln('// ', err)
			end
		end

		dst:writeln('// lua imgui wrappers')
		dst:writeln()
		for _, v in ipairs(apis) do
			gen_function(table.unpack(v))
		end
		dst:writeln()
	end)

	generate_file(vlua.filename_format(out..'/'..'imgui_wraps.inl'), function(dst)
		for _, v in ipairs(wraps) do
			dst:writeln('__REG_WRAP(', v, ')')
		end
		dst:writeln()
	end)

end

