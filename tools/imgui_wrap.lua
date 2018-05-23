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

local function pasre_apis(apis, src)
	for namespace_block in src:gmatch('namespace%s+ImGui%s*(%b{})') do
		-- print('NAMESPACE BLOCK:', namespace_block)
		for ret, name, args in namespace_block:gmatch('IMGUI_API%s+([_%w%s%*&]-)%s*([_%w]+)%s*(%b()).-;') do
			-- print('API', ret, name, args)
			table.insert(apis, {ret, name, args})
		end
		for ret, name, args in namespace_block:gmatch('static%s+inline%s+([_%w%s%*&]-)%s*([_%w]+)%s*(%b())%s*%b{}') do
			-- print('API', ret, name, args)
			table.insert(apis, {ret, name, args})
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

local buffer_implements = [[
#define BYTE_ARRAY_NAME	"PussImguiByteArray"

typedef struct _ByteArrayLua {
	int				cap;
	unsigned char	buf[1];
} ByteArrayLua;

static int byte_array_tostring(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	lua_pushfstring(L, "Byte[%d] %p", ud->cap, ud);
	return 1;
}

static int byte_array_len(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	lua_pushinteger(L, ud->cap);
	return 1;
}

static int byte_array_sub(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	int from = (int)luaL_optinteger(L, 2, 1);
	int to = (int)luaL_optinteger(L, 3, -1);
	if( from < 0 ) { from += ud->cap; }
	from = (from < 1) ? 0 : (from - 1);
	if( to < 0 ) { to += ud->cap; }
	to = (to > ud->cap) ? ud->cap : to;
	if( from < to ) {
		lua_pushlstring(L, (const char*)(ud->buf + from), (to - from));
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int byte_array_reset(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	size_t len = 0;
	const char* buf = luaL_optlstring(L, 2, "", &len);
	if( len > (size_t)(ud->cap) ) { len = (size_t)(ud->cap); }
	memcpy(ud->buf, buf, len+1);
	return 0;
}

static int byte_array_strlen(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	lua_pushinteger(L, (lua_Integer)strlen((const char*)(ud->buf)));
	return 1;
}

static int byte_array_str(lua_State* L) {
	ByteArrayLua* ud = (ByteArrayLua*)luaL_checkudata(L, 1, BYTE_ARRAY_NAME);
	lua_pushstring(L, (const char*)(ud->buf));
	return 1;
}

static luaL_Reg byte_array_methods[] =
	{ {"__tostring", byte_array_tostring}
	, {"__len", byte_array_len}
	, {"sub", byte_array_sub}
	, {"reset", byte_array_reset}
	, {"strlen", byte_array_strlen}
	, {"str", byte_array_str}
	, {NULL, NULL}
	};

static int byte_array_create(lua_State* L) {
	int cap = (int)luaL_checkinteger(L, 1);
	size_t len = 0;
	const char* buf = luaL_optlstring(L, 2, "", &len);
	ByteArrayLua* ud;
	cap =  (cap < (int)len) ? (int)len : cap;
	ud = (ByteArrayLua*)lua_newuserdata(L, sizeof(ByteArrayLua) + cap);
	ud->cap = cap;
	memcpy(ud->buf, buf, len+1);
	if( luaL_newmetatable(L, BYTE_ARRAY_NAME) ) {
		luaL_setfuncs(L, byte_array_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

#define FLOAT_ARRAY_NAME	"PussImguiFloatArray"

typedef struct _FloatArrayLua {
	int		cap;
	int		len;
	float	buf[0];
} FloatArrayLua;

static int float_array_tostring(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	lua_pushfstring(L, "Float[%d/%d] %p", ud->len, ud->cap, ud);
	return 1;
}

static int float_array_len(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	lua_pushinteger(L, ud->len);
	return 1;
}

static int float_array_resize(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	int size = (int)luaL_checkinteger(L, 2);
	if( (size >= 0) && (size <= ud->cap) )
		ud->len = size;
	lua_pushinteger(L, ud->len);
	return 1;
}

static int float_array_get(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	int index = (int)luaL_checkinteger(L, 2);
	if( index > 0 ) {
		if( index <= ud->len ) {
			lua_pushnumber(L, ud->buf[index-1]);
			return 1;
		}
	} else if( index < 0 ) {
		if( (-index) <= ud->len ) {
			lua_pushnumber(L, ud->buf[ud->len+index]);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

static int float_array_set(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	int index = (int)luaL_checkinteger(L, 2);
	float value = (float)luaL_checknumber(L, 3);
	int ok = 0;
	if( index > 0 ) {
		if( index <= ud->cap ) {
			ud->buf[index-1] = value;
			ok = 1;
		}
	} else if( index < 0 ) {
		if( (-index) <= ud->cap ) {
			ud->buf[ud->cap+index] = value;
			ok = 1;
		}
	}
	lua_pushboolean(L, ok);
	return 1;
}

static int float_array_push(lua_State* L) {
	FloatArrayLua* ud = (FloatArrayLua*)luaL_checkudata(L, 1, FLOAT_ARRAY_NAME);
	float value = (float)luaL_checknumber(L, 2);
	lua_assert( ud->cap );
	if( ud->len < ud->cap ) {
		ud->buf[ud->len] = value;
		ud->len++;
	} else {
		float* a = ud->buf;
		int i = 0;
		for( i=1; i<ud->len; ++i ) {
			a[i-1] = a[i];
		}
		a[i] = value;
	}
	return 0;
}

static luaL_Reg float_array_methods[] =
	{ {"__tostring", float_array_tostring}
	, {"__len", float_array_len}
	, {"resize", float_array_resize}
	, {"get", float_array_get}
	, {"set", float_array_set}
	, {"push", float_array_push}
	, {NULL, NULL}
	};

static int float_array_create(lua_State* L) {
	int cap = (int)luaL_checkinteger(L, 1);
	size_t sz;
	FloatArrayLua* ud;
	luaL_argcheck(L, cap > 0, 1, "bad range");
	sz = sizeof(FloatArrayLua) + sizeof(float) * cap;
	ud = (FloatArrayLua*)lua_newuserdata(L, sz);
	memset(ud, 0, sz);
	ud->cap = cap;
	if( luaL_newmetatable(L, FLOAT_ARRAY_NAME) ) {
		luaL_setfuncs(L, float_array_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

]]

local ignore_apis =
	{ Shutdown = true
	, Render = true
	, SetAllocatorFunctions = true
	, MemAlloc = true
	, MemFree = true
	, NewFrame = true
	, EndFrame = true
	, PushFont = true
	, PopFont = true
	}

local implements = {}

implements.InputText = [[	// bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
	const char* label = luaL_checkstring(L, 1);
	ByteArrayLua* arr = (ByteArrayLua*)luaL_checkudata(L, 2, BYTE_ARRAY_NAME);
	ImGuiInputTextFlags flags = (ImGuiInputTextFlags)luaL_optinteger(L, 3, 0);
	bool changed = ImGui::InputText(label, (char*)arr->buf, (size_t)arr->cap, flags);
	lua_pushboolean(L, changed ? 1 : 0);
	return 1;]]

implements.InputTextMultiline = [[	// bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0,0), ImGuiInputTextFlags flags = 0, ImGuiTextEditCallback callback = NULL, void* user_data = NULL);
	const char* label = luaL_checkstring(L, 1);
	ByteArrayLua* arr = (ByteArrayLua*)luaL_checkudata(L, 2, BYTE_ARRAY_NAME);
	ImVec2 size( (float)luaL_optnumber(L, 3, 0.0), (float)luaL_optnumber(L, 4, 0.0) ); 
	ImGuiInputTextFlags flags = (ImGuiInputTextFlags)luaL_optinteger(L, 5, 0);
	bool changed = ImGui::InputTextMultiline(label, (char*)arr->buf, (size_t)arr->cap, size, flags);
	lua_pushboolean(L, changed ? 1 : 0);
	return 1;]]

implements.PlotLines = [[	// void PlotLines(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0,0), int stride = sizeof(float));
	int nargs = lua_gettop(L);
	const char* label = luaL_checkstring(L, 1);
	FloatArrayLua* arr = (FloatArrayLua*)luaL_checkudata(L, 2, FLOAT_ARRAY_NAME);
	const float* values = arr->buf;
	int values_count = (int)luaL_optinteger(L, 3, arr->len);
	int values_offset = (int)(luaL_optinteger(L, 4, 1) - 1);
	const char* overlay_text = luaL_optstring(L, 5, NULL);
	float scale_min = (float)((nargs<6) ? FLT_MAX : luaL_checknumber(L, 6));
	float scale_max = (float)((nargs<7) ? FLT_MAX : luaL_checknumber(L, 7));
	ImVec2 graph_size = ImVec2((float)luaL_optnumber(L, 8, 0.0), (float)luaL_optnumber(L, 9, 0.0));
	if( values_offset < 0 ) { values_offset = 0; }
	if( (values_offset + values_count) > arr->len ) { values_count = (arr->len - values_offset); }
	if( values_count < 0 ) { values_count = 0; }
	ImGui::PlotLines(label, values, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
	return 0;]]

implements.PlotHistogram = [[	// void PlotHistogram(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0,0), int stride = sizeof(float));
	int nargs = lua_gettop(L);
	const char* label = luaL_checkstring(L, 1);
	FloatArrayLua* arr = (FloatArrayLua*)luaL_checkudata(L, 2, FLOAT_ARRAY_NAME);
	const float* values = arr->buf;
	int values_count = (int)luaL_optinteger(L, 3, arr->len);
	int values_offset = (int)(luaL_optinteger(L, 4, 1) - 1);
	const char* overlay_text = luaL_optstring(L, 5, NULL);
	float scale_min = (float)((nargs<6) ? FLT_MAX : luaL_checknumber(L, 6));
	float scale_max = (float)((nargs<7) ? FLT_MAX : luaL_checknumber(L, 7));
	ImVec2 graph_size = ImVec2((float)luaL_optnumber(L, 8, 0.0), (float)luaL_optnumber(L, 9, 0.0));
	if( values_offset < 0 ) { values_offset = 0; }
	if( (values_offset + values_count) > arr->len ) { values_count = (arr->len - values_offset); }
	if( values_count < 0 ) { values_count = 0; }
	ImGui::PlotHistogram(label, values, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
	return 0;]]

implements.IsMousePosValid = [[	//	bool IsMousePosValid(const ImVec2* mouse_pos = NULL);
	ImVec2 pos;
	const ImVec2* mouse_pos = (lua_gettop(L) < 2) ? NULL : &pos;
	if( mouse_pos ) {
		pos.x = (float)luaL_checknumber(L, 1);
		pos.y = (float)luaL_checknumber(L, 2);
	}
	lua_pushboolean(L, ImGui::IsMousePosValid(mouse_pos) ? 1 : 0);
	return 1;]]

implements.ColorPicker4 = [[	//	bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0, const float* ref_col = NULL);
	const char* label = luaL_checkstring(L, 1);
	float col[4] = { (float)luaL_checknumber(L,2), (float)luaL_checknumber(L,3), (float)luaL_checknumber(L,4), (float)luaL_checknumber(L,5) };
	ImGuiColorEditFlags flags = (ImGuiColorEditFlags)luaL_optinteger(L, 6, 0);
	float _ref_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	const float* ref_col = (lua_gettop(L) > 6) ? _ref_col : NULL;
	if( ref_col ) {
		_ref_col[0] = (float)luaL_optnumber(L, 7, 0.0);
		_ref_col[1] = (float)luaL_optnumber(L, 8, 0.0);
		_ref_col[2] = (float)luaL_optnumber(L, 9, 0.0);
		_ref_col[3] = (float)luaL_optnumber(L, 10, 1.0);
	}
	lua_pushboolean(L, ImGui::ColorPicker4(label, col, flags, ref_col) ? 1 : 0);
	lua_pushnumber(L, col[0]);
	lua_pushnumber(L, col[1]);
	lua_pushnumber(L, col[2]);
	lua_pushnumber(L, col[3]);
	return 5;]]

implements.SetDragDropPayload = [[	//	bool SetDragDropPayload(const char* type, const void* data, size_t size, ImGuiCond cond = 0);
	const char* type = luaL_checkstring(L, 1);
	size_t size = 0;
	const char* data = luaL_checklstring(L, 2, &size);
	ImGuiCond cond = (ImGuiCond)luaL_optinteger(L, 3, 0);
	lua_pushboolean(L, ImGui::SetDragDropPayload(type, data, size, cond) ? 1 : 0);
	return 1;]]

implements.AcceptDragDropPayload = [[	//	const ImGuiPayload* AcceptDragDropPayload(const char* type, ImGuiDragDropFlags flags = 0);
	const char* type = luaL_optstring(L, 1, NULL);
	ImGuiDragDropFlags flags = (ImGuiCond)luaL_optinteger(L, 2, 0);
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type, flags);
	if( payload ) {
		lua_createtable(L, 0, 7);
		lua_pushlstring(L, (const char*)payload->Data, payload->DataSize);	lua_setfield(L, -2, "Data");
		lua_pushinteger(L, payload->SourceId);			lua_setfield(L, -2, "SourceId");
		lua_pushinteger(L, payload->SourceParentId);	lua_setfield(L, -2, "SourceParentId");
		lua_pushinteger(L, payload->DataFrameCount);	lua_setfield(L, -2, "DataFrameCount");
		lua_pushstring(L, payload->DataType);			lua_setfield(L, -2, "DataType");
		lua_pushboolean(L, payload->Preview);			lua_setfield(L, -2, "Preview");
		lua_pushboolean(L, payload->Delivery);			lua_setfield(L, -2, "Delivery");
	} else {
		lua_pushnil(L);
	}
	return 1;]]

function main()
	local out = vlua.match_arg('^%-out=(.+)$') or '.'
	local src = vlua.match_arg('^%-src=(.+)$') or './include'

	local apis = {}
	local enums = {}
	local inttypes = {}

	local function load_header(fname)
		local f = io.open(src..'/'..fname, 'r')
		local t = f:read('*a')
		f:close()

		t = t:gsub('/%*.-%*/', ' ')		-- remove comment /* */
		t = t:gsub('\r', '')			-- remove \r
		t = t:gsub('//.-\n', '\n')		-- remove line comment
		-- print(t)

		pasre_apis(apis, t)
		parse_enums(enums, t)
		parse_inttypes(inttypes, t)
	end

	load_header('imgui.h')
	load_header('imgui_tabs.h')

	local wraps, begins, ends = {}, {}, {}

	local function generate_file(filename, cb)
		local output_lines = {}
		local function concat(pos, ...)
			local ok, line = pcall(table.concat, {...})
			if not ok then
				print('concat(', ..., ')')
				error(line)
			end
			output_lines[pos] = line
			return pos
		end
		local mt = {}
		mt.__index = mt
		mt.__len = function(self) return #output_lines end
		mt.writeln = function(self, ...) return concat(#output_lines+1, ...) end
		mt.insert = function(self, ...) return table.insert(output_lines, ...) end
		mt.remove = function(self, ...) return table.remove(output_lines, ...) end
		mt.replace = function(self, pos, ...) return concat(pos, ...) end
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

		local function gen_ret_decl(ret)
			local atype, aname = ret, '__ret__'
			if atype=='void' then
				-- ignore
			elseif inttypes[atype] or atype=='float' or atype=='double' or atype=='bool' or atype:match(RE_UINT) or atype:match(RE_CSTR) or atype:match(RE_CVOID) then
				dst:writeln('	', atype, ' ', aname, ';')
			elseif atype=='ImVec2' or atype:match(RE_CIMVEC2) then
				dst:writeln('	ImVec2 ', aname, ';')
			elseif atype=='ImVec4' or atype:match(RE_CIMVEC4) then
				dst:writeln('	ImVec4 ', aname, ';')
			elseif atype=='ImTextureID' then
				dst:writeln('	ImTextureID ', aname, ';')
			else
				error(string.format('[NotSupport]	ret type(%s)', atype))
			end
		end

		local function gen_args_decl(args)
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
				elseif atype=='ImTextureID' then
					dst:writeln('	ImTextureID ', aname, ';')
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
		end

		local function gen_args_fetch(args)
			local iarg_use, narg_use, fmt = false, false, ''
			for i, a in ipairs(args) do
				local atype, aname, attr = a.atype, a.name or string.format('__arg_%d', i), a.attr
				if attr then
					if attr=='...' then
						fmt = fmt .. 'va'
					elseif attr:match('%[%d]') then
						local n = tonumber(attr:match('%[(%d)%]'))
						iarg_use = true
						if inttypes[atype] then
							fmt = fmt .. (inttypes[atype]=='int' and 'ai' or 'au')
							for i=1, n do dst:writeln('	', aname, '[',i-1,'] = ', '('..atype..')luaL_checkinteger(L, ++__iarg__);') end
						elseif atype=='float' or atype=='double' then
							fmt = fmt .. (atype=='float' and 'af' or 'ad')
							for i=1, n do dst:writeln('	', aname, '[',i-1,'] = ', '('..atype..')luaL_checknumber(L, ++__iarg__);') end
						end
					else
						error(string.format('[NotSupport]	type(%s)', atype))
					end
				elseif inttypes[atype] or atype:match(RE_UINT) then
					iarg_use, fmt = true, fmt .. (inttypes[atype]=='int' and 'i' or 'u')
					dst:writeln('	', aname, ' = ', a.def and '('..atype..')luaL_optinteger(L, ++__iarg__, '..a.def..')' or '('..atype..')luaL_checkinteger(L, ++__iarg__)', ';')
				elseif atype=='float' or atype=='double' then
					iarg_use, fmt = true, fmt .. (atype=='float' and 'f' or 'd')
					dst:writeln('	', aname, ' = ', a.def and '('..atype..')luaL_optnumber(L, ++__iarg__, '..a.def..')' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				elseif atype=='bool' then
					iarg_use, fmt = true, fmt .. 'b'
					if a.def then
						narg_use = true
						dst:writeln('	', aname, ' = (__iarg__ < __narg__) ? lua_toboolean(L, ++__iarg__)!=0 : ('..a.def..');')
					else
						dst:writeln('	', aname, ' = lua_toboolean(L, ++__iarg__)!=0;')
					end
				elseif atype:match(RE_PBOOL) then
					iarg_use, narg_use, fmt = true, true, fmt .. 'pb'
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? lua_toboolean(L, ++__iarg__)!=0 : false;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PINT) then
					iarg_use, narg_use, fmt = true, true, fmt .. 'pi'
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (int)luaL_checkinteger(L, ++__iarg__) : 0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PUINT) then
					iarg_use, narg_use, fmt = true, true, fmt .. 'pu'
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (unsigned int)luaL_checkinteger(L, ++__iarg__) : 0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PFLOAT) then
					iarg_use, narg_use, fmt = true, true, fmt .. 'pf'
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? (float)luaL_checknumber(L, ++__iarg__) : 0.0f;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_PDOUBLE) then
					iarg_use, narg_use, fmt = true, true, fmt .. 'pd'
					dst:writeln('	', '__value_', aname, ' = (__iarg__ < __narg__) ? luaL_checknumber(L, ++__iarg__) : 0.0;')
					dst:writeln('	', aname, ' = ', a.def and '(__iarg__ <= __narg__) ? &__value_'..aname..' : ('..a.def..')' or '&__value_'..aname, ';')
				elseif atype:match(RE_RFLOAT) then
					-- output only, ignore fetch
					fmt = fmt .. 'rf'
				elseif atype:match(RE_CSTR) then
					iarg_use, fmt = true, fmt .. 's'
					dst:writeln('	', aname, ' = ', a.def and 'luaL_optstring(L, ++__iarg__, '..a.def..')' or 'luaL_checkstring(L, ++__iarg__)', ';')
				elseif atype:match(RE_CVOID) then
					iarg_use, fmt = true, fmt .. 'pv'
					if a.def then
						narg_use = true
						dst:writeln('	', aname, ' = (__iarg__ < __narg__) ? lua_topointer(L, ++__iarg__) : ', a.def, ';')
					else
						dst:writeln('	', aname, ' = lua_topointer(L, ++__iarg__);')
					end
				elseif atype=='ImVec2' or atype:match(RE_RIMVEC2) or atype:match(RE_CIMVEC2) then
					iarg_use, fmt = true, fmt .. 'v2'
					if a.def then dst:writeln('	', aname, ' = ', a.def, ';') end
					dst:writeln('	', aname, '.x = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.x)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.y = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.y)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				elseif atype=='ImVec4' or atype:match(RE_RIMVEC4) or atype:match(RE_CIMVEC4) then
					iarg_use, fmt = true, fmt .. 'v4'
					if a.def then dst:writeln('	', aname, ' = ', a.def, ';') end
					dst:writeln('	', aname, '.x = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.x)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.y = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.y)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.z = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.z)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
					dst:writeln('	', aname, '.w = ', a.def and '(float)luaL_optnumber(L, ++__iarg__, '..aname..'.w)' or '(float)luaL_checknumber(L, ++__iarg__)', ';')
				elseif atype=='ImTextureID' then
					iarg_use, fmt = true, fmt .. 'pv'
					dst:writeln('	', aname, ' = ('..atype..')lua_topointer(L, ++__iarg__);')
				else
					error(string.format('[NotSupport]	arg type(%s)', atype))
				end
			end
			return iarg_use, narg_use, fmt
		end

		local function gen_api_invoke(ret, name, args)
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

		local function gen_ret_push(ret, args)
			local nret = 0
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
				elseif atype=='ImTextureID' then
					nret = nret + 1
					dst:writeln('	lua_pushlightuserdata(L, ', aname, ');')
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
				elseif atype=='ImTextureID' then
					nret = nret + 1
					dst:writeln('	lua_pushlightuserdata(L, ', aname, ');')
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
			return nret
		end

		local end_overrides =
			{ ['PopupModal'] = 'EndPopup'
			, ['PopupContextItem'] = 'EndPopup'
			, ['PopupContextWindow'] = 'EndPopup'
			, ['PopupContextVoid'] = 'EndPopup'
			}

		local stack_types = {}

		local function fetch_stack_type(tp)
			for i,k in ipairs(stack_types) do
				if k==tp then return i end
			end
			local n = #stack_types + 1
			stack_types[n] = tp
			return n
		end

		local function gen_begin_wraps(name, ret)
			local tp = name:match('^Begin(.*)$')
			if tp then
				tp = end_overrides[tp] or 'End'..tp
			elseif name=='TreeNode' or name=='TreePush' then
				tp = 'TreePop'
			elseif name=='PushStyleVar' then
				tp = 'PopStyleVar'
			end
			if not tp then return end
			if ret=='bool' then
				dst:writeln('	if(__ret__) { IMGUI_LUA_WRAP_STACK_BEGIN(', fetch_stack_type(tp), ') }')
			else
				dst:writeln('	IMGUI_LUA_WRAP_STACK_BEGIN(', fetch_stack_type(tp), ')')
			end
		end

		local function gen_end_wraps(name)
			local tp = name:match('^End(.*)$')
			if tp then
				tp = end_overrides[tp] or 'End'..tp
			elseif name=='TreePop' then
				tp = 'TreePop'
			elseif name=='PopStyleVar' then
				tp = 'PopStyleVar'
			end
			if not tp then return end
			dst:writeln('	IMGUI_LUA_WRAP_STACK_END(', fetch_stack_type(tp), ')')
		end

		local function gen_lua_wrap(ret, name, args, exist)
			local fname = 'wrap_' .. name
			if exist then dst:writeln('// [Override]') end
			local pos = dst:writeln(string.format('static int %s(lua_State* L) {', fname))
			local narg_pos = dst:writeln('	int __narg__ = lua_gettop(L);')
			local iarg_pos = dst:writeln('	int __iarg__ = 0;')
			gen_ret_decl(ret)
			gen_args_decl(args)
			local iarg_use, narg_use, suffix = gen_args_fetch(args)
			gen_end_wraps(name)
			gen_api_invoke(ret, name, args)
			gen_begin_wraps(name, ret)
			local nret = gen_ret_push(ret, args)
			if not iarg_use then dst:remove(iarg_pos) end
			if not narg_use then dst:remove(narg_pos) end
			dst:writeln('	return ', nret, ';')
			dst:writeln('}')
			if #suffix > 0 then
				fname = fname .. '_' .. suffix
				dst:replace(pos, string.format('static int %s(lua_State* L) {', fname))
			end
			return fname
		end

		local function gen_function(ret, name, args)
			dst:writeln()
			dst:writeln(string.format('// [Declare]  %s %s%s;', ret, name, args))
			args = parse_args(args)
			for _, a in ipairs(args) do
				dst:writeln(string.format('//   [Param] type(%s) name(%s) attr(%s) def(%s)', a.atype, a.name, a.attr or '', a.def or ''))
			end
			if implements[name] then
				dst:writeln(string.format('// [Implement]') )
			elseif ignore_apis[name] then
				dst:writeln(string.format('// [Ignore]') )
			else
				local exist = functions[name]
				local mark = #dst
				local ok, ret = pcall(gen_lua_wrap, ret, name, args, exist)
				if ok then
					if not exist then
						functions[name] = ret
						wraps[name] = ret
					end
				else
					if not exist then functions[name] = nil end
					dst:revert(mark)
					dst:writeln('// ', ret)
				end
			end
		end

		dst:writeln('// lua imgui wrappers')
		dst:writeln()
		dst:writeln('#ifndef IMGUI_LUA_WRAP_STACK_BEGIN')
		dst:writeln('	#define IMGUI_LUA_WRAP_STACK_BEGIN(tp)')
		dst:writeln('#endif//IMGUI_LUA_WRAP_STACK_BEGIN')
		dst:writeln('#ifndef IMGUI_LUA_WRAP_STACK_END')
		dst:writeln('	#define IMGUI_LUA_WRAP_STACK_END(tp)')
		dst:writeln('#endif//IMGUI_LUA_WRAP_STACK_END')
		dst:writeln()
		dst:insert(buffer_implements)

		for _, v in ipairs(apis) do
			gen_function(table.unpack(v))
		end

		do
			local t = {}
			for k in pairs(implements) do table.insert(t, k) end
			table.sort(t)
			for _, name in ipairs(t) do
				local impl = implements[name]
				dst:writeln(string.format('static int wrap_%s(lua_State* L) {', name))
				dst:insert(impl)
				dst:writeln('}')
				wraps[name] = 'wrap_'..name
				dst:writeln()
			end
		end

		local overrides = {}
		overrides.BeginChild = [[return lua_type(L, 1)==LUA_TSTRING ? wrap_BeginChild_sv2bi(L) : wrap_BeginChild_uv2bi(L);]]
		overrides.SetWindowPos = [[return lua_type(L, 1)==LUA_TSTRING ? wrap_SetWindowPos_sv2i(L) : wrap_SetWindowPos_v2i(L);]]
		overrides.SetWindowSize = [[return lua_type(L, 1)==LUA_TSTRING ? wrap_SetWindowSize_sv2i(L) : wrap_SetWindowSize_v2i(L);]]
		overrides.SetWindowCollapsed = [[return lua_type(L, 1)==LUA_TSTRING ? wrap_SetWindowCollapsed_sbi(L) : wrap_SetWindowCollapsed_bi(L);]]
		overrides.SetWindowFocus = [[return lua_gettop(L)==0 ? wrap_SetWindowFocus(L) : wrap_SetWindowFocus_s(L);]]
		overrides.PushStyleColor = [[return lua_gettop(L)<=2 ? wrap_PushStyleColor_iu(L) : wrap_PushStyleColor_iv4(L);]]
		overrides.PushStyleVar = [[return lua_gettop(L)<=2 ? wrap_PushStyleVar_if(L) : wrap_PushStyleVar_iv2(L);]]
		overrides.GetColorU32 = [[switch( lua_gettop(L) ) { case 1: return lua_isinteger(L, 1) ? wrap_GetColorU32_if(L) : wrap_GetColorU32_u(L); case 2: return wrap_GetColorU32_if(L); } return wrap_GetColorU32_v4(L);]]
		overrides.PushID = [[switch( lua_type(L, 1) ) { case LUA_TNUMBER: return wrap_PushID_i(L); case LUA_TUSERDATA: return wrap_PushID_pv(L); } return wrap_PushID_s(L); ]]
		overrides.GetID = [[return lua_type(L, 1)==LUA_TUSERDATA ? wrap_GetID_pv(L) : wrap_GetID_s(L);]]
		overrides.RadioButton = [[return lua_type(L, 2)==LUA_TBOOLEAN ? wrap_RadioButton_sb(L) : wrap_RadioButton_spii(L);]]
		overrides.TreeNode = [[return lua_gettop(L)==1 ? wrap_TreeNode_s(L) : (lua_type(L, 1)==LUA_TUSERDATA ? wrap_TreeNode_pvsva(L) : wrap_TreeNode_ssva(L));]]
		overrides.TreeNodeEx = [[return lua_gettop(L)<=2 ? wrap_TreeNodeEx_si(L) : (lua_type(L, 1)==LUA_TUSERDATA ? wrap_TreeNodeEx_pvisva(L) : wrap_TreeNodeEx_sisva(L));]]
		overrides.TreePush = [[return lua_type(L, 1)==LUA_TUSERDATA ? wrap_TreePush_pv(L) : wrap_TreePush_s(L);]]
		overrides.IsRectVisible = [[ return lua_gettop(L)<=2 ? wrap_IsRectVisible_v2(L) : wrap_IsRectVisible_v2v2(L);]]
		overrides.CollapsingHeader = [[return lua_type(L, 2)==LUA_TBOOLEAN ? wrap_CollapsingHeader_spbi(L) : wrap_CollapsingHeader_si(L);]]
		wraps.Selectable = wrap_Selectable_spbiv2
		wraps.ListBoxHeader = 'wrap_ListBoxHeader_sv2'
		wraps.ListBoxHeader2 = 'wrap_ListBoxHeader_sii'
		overrides.Value = [[return lua_type(L, 2)==LUA_TNUMBER ? (lua_isinteger(L, 2) ? wrap_Value_si(L) : wrap_Value_sfs(L)) : wrap_Value_sb(L);]]
		wraps.ValueUnsigned = 'wrap_Value_su'
		overrides.MenuItem = [[int selected=lua_toboolean(L,3); wrap_MenuItem_ssbb(L); lua_pushboolean(L, lua_toboolean(L,-1) ? !selected : selected); return 2;]]
		do
			local t = {}
			for k in pairs(overrides) do table.insert(t, k) end
			table.sort(t)
			for _, name in ipairs(t) do
				dst:writeln(string.format('static int override_%s(lua_State* L) { %s }', name, overrides[name]))
				wraps[name] = 'override_'..name
				dst:writeln()
			end
		end

		dst:writeln('#define IMGUI_LUA_WRAP_STACK_POP(tp) \\')
		dst:writeln('	switch(tp) { \\')
		for i, v in ipairs(stack_types) do
			dst:writeln('	case ', i, ':	ImGui::', v, '();	break; \\')
		end
		dst:writeln('	default:	break; \\')
		dst:writeln('	}')
		dst:writeln()
	end)

	generate_file(vlua.filename_format(out..'/'..'imgui_wraps.inl'), function(dst)
		local ws = {}
		for w in pairs(wraps) do table.insert(ws, w) end
		table.sort(ws)
		for _, w in ipairs(ws) do
			dst:writeln('__REG_WRAP(', w, ', ', wraps[w], ')')
		end
		dst:writeln()
	end)

end
