-- imgui_wrap.lua

local function parse_args(args)
	local r = {}
	args = args:match('^%s*%(%s*(.-)%s*%)%s*$') -- remove ()
	if args=='' then return r end	-- no args
	if args:match('%.%.%.') then return end -- not support ...
	args = args .. ','	-- append ','
	local noname_arg = 0
	local brackets = {}
	args = args:gsub('%b()', function(v)
		table.insert(brackets, v)
		return string.format('@%d', #brackets)
	end)
	for v in args:gmatch('%s*(.-)%s*,') do
		if v=='' then break end
		local arg, def = v:match('^(.-)%s*=%s*(.-)%s*$')
		arg = arg or v
		local type, name, arr = arg:match('^(.-)%s+([_%w]+)%s*(%b[])$')	-- fmt: type name[arr]
		if not type then	-- fmt: type (*func)(arg)
			local tp, ni, ai = arg:match('^(.-)@(%d+)%s*@(%d+)$')
			if tp then
				print(tp, ni, ai)
				local t = brackets[tonumber(ni)]
				type = tp .. ' ' .. t .. brackets[tonumber(ai)]
				name = t:match('[_%w]+')
			end
		end
		if not type then	-- fmt: type name
			type, name = arg:match('^(.-)%s+([_%w]+)$')
		end
		if not type then	-- fmt: type
			noname_arg = noname_arg + 1
			type, name = arg, string.format('__noname_param_%d', noname_arg)
		end
		def = def and def:gsub('@(%d+)', function(v) return brackets[tonumber(v)] or v end)
		table.insert(r, {type, name, arr, def})
	end
	return r
end

local ignore_apis =
	{ NewFrame = true
	, Render = true
	, Shutdown = true
	, EndFrame = true
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
		for ret, name, args in namespace_block:gmatch('static%s+inline%s+([_%w%s%*&]+[%*%&%s])([_%w]+)%s*(%b())%s*%b{}') do
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
	pasre_apis(apis, src)
	parse_enums(enums, src)

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

	generate_file(vlua.filename_format(out..'/'..'imgui_enums.inl'), function(writeln)
		local last_macro
		for _,v in ipairs(enums) do
			local enum, macro = table.unpack(v)
			if macro~=last_macro then
				if last_macro then writeln('#endif') end
				if macro then writeln(macro) end
				last_macro = macro
			end
			writeln('__REG_ENUM(', enum, ')')
		end
		if last_macro then writeln('#endif') end
		writeln()
	end)

	generate_file(vlua.filename_format(out..'/'..'imgui_lua.inl'), function(writeln)
		local function gen_param(tp, name, def)
			
		end

		local function gen_function(ret, name, args)
			writeln(string.format('  %s %s%s;', ret, name, args))
			args = parse_args(args)
			if not args then
				writeln('  // args not support')
			else
				for _, a in ipairs(args) do
					local tp, name, arr, def = table.unpack(a)
					writeln(string.format('  // type(%s) name(%s) arr(%s) def(%s)', tp, name, arr or '', def or ''))
				end
			end
		end

		writeln('// lua imgui wrappers')
		writeln()
		for _, v in ipairs(apis) do
			gen_function(table.unpack(v))
		end
		writeln()
	end)

end

