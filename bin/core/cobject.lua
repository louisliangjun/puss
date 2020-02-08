-- core.cobject

__exports.build_cobject = function(filename, name, id, fields, not_gen_field_id)
	local gen_field_enums = (not not_gen_field_id)
	local sname = name:gsub('(%l)(%u)', function(a,b) return a..'_'..b end):lower()
	local strfmt = string.format
	local ctypemap =
		{ ['bool'] = 'PussCBool'
		, ['int'] = 'PussCInt'
		, ['num'] = 'PussCNum'
		, ['lua'] = 'PussCLua'
		, ['ptr'] = 'PussCPtr'
		}

	local ctx = {}
	local write = function(s) ctx[#ctx+1] = s end
	write('// cobject '..name)
	write('// ')
	write(strfmt('#define PUSS_COBJECT_ID_%s'..' 0x%X', name:upper(), id))
	write(strfmt('#define %s_checkudata(L,a) (%s*)puss_cobject_checkudata((L),(a), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	write(strfmt('#define %s_testudata(L,a)  (%s*)puss_cobject_testudata((L),(a), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	write(strfmt('#define %s_check(stobj)    (%s*)puss_cobject_check((stobj), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	write(strfmt('#define %s_test(stobj)     (%s*)puss_cobject_test((stobj), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	if gen_field_enums then
		write(strfmt('#define %s_field(prop)     %s_ ## prop', sname, sname:upper()))
	else
		write(strfmt('#define %s_field(prop)     (lua_Integer)( (PussCValue*)(&(((const %s*)0)->prop)) - (((const %s*)0)->__parent__.values) )', sname, name, name))
	end
	write('')

	if gen_field_enums then
		for i,v in ipairs(fields) do
			-- local field_enum_name = (sname..'_'..v.name):upper()
			write('#define '..sname:upper()..'_'..v.name..' '..i)
		end
	end
	write('#define _'..sname:upper()..'_COUNT '..(#fields))
	write('')

	write('typedef struct _'..name..' {')
	write('	PussCObject	__parent__;')
	for i,v in ipairs(fields) do
		local ctp = ctypemap[v.type]
		if not ctp then error('not support type: '..tostring(v.type)) end
		local cdv = (v.type=='bool' and v.def) and 'TRUE' or v.def
		cdv = cdv and strfmt('	// default: %s', cdv) or ''
		write(strfmt('	%s	%s;%s', ctp, v.name, cdv))
	end
	write('} '..name..';')
	write('')

	for i,v in ipairs(fields) do
		local field = sname..'_field('..v.name..')'
		if v.type=='bool' or v.type=='int' or v.type=='num' then
			write('#define '..sname..'_set_'..v.name..'(stobj__,v__)     puss_cobject_set_'..v.type..'((stobj__), ('..field..'), (v__))')
		elseif v.type=='str' then
			write('#define '..sname..'_set_'..v.name..'(stobj__,s__,n__) puss_cobject_set_str((stobj__), ('..field..'), (s__), (n__))')
		elseif v.type=='lua' then
			write('#define '..sname..'_set_'..v.name..'(stobj__)         puss_cobject_stack_set((stobj__), ('..field..'))')
		end
	end
	write('')
	write('#define '..sname..'_set(stobj__, prop, v__)  '..sname..'_set_ ## prop((stobj__), (v__))')
	write('')

	ctx = table.concat(ctx, '\n')
	local f = io.open(filename, 'r')
	if f then
		local same = f:read(#ctx + 1)==ctx
		f:close()
		if same then return true end
	end
	f = io.open(filename, 'w')
	if not f then return false end
	f:write(ctx)
	f:close()
end
