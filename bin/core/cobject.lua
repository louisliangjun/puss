-- core.cobject

__exports.build_cobject = function(filename, name, id, fields, not_gen_field_id)
	local gen_field_enums = (not not_gen_field_id)
	print('gen_field_enums', gen_field_enums)
	local sname = name:gsub('(%l)(%u)', function(a,b) return a..'_'..b end):lower()
	local strfmt = string.format
	local ctypemap =
		{ ['bool'] = 'PussCBool'
		, ['int'] = 'PussCInt'
		, ['num'] = 'PussCNum'
		, ['str'] = 'PussCStr'
		, ['lua'] = 'PussCLua'
		}

	local ctx = {}
	local write = function(s) ctx[#ctx+1] = s end
	write('// cobject '..name)
	write('// ')
	write(strfmt('#define PUSS_COBJECT_ID_%s'..' 0x%X', name:upper(), id))
	write(strfmt('#define %s_check(L, idx)  (%s*)puss_cobject_check((L), (idx), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	write(strfmt('#define %s_test(L, idx)   (%s*)puss_cobject_test((L), (idx), PUSS_COBJECT_ID_%s)', sname, name, name:upper()))
	if gen_field_enums then
		write(strfmt('#define %s_field(prop)    %s_ ## prop', sname, sname:upper()))
	else
		write(strfmt('#define %s_field(prop)    (lua_Integer)( (PussCValue*)(&(((const %s*)0)->prop)) - (((const %s*)0)->__parent__.values) )', sname, name, name))
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
		local cdv = (v.type=='bool' and v.def) and 'TRUE' or v.def
		cdv = cdv and strfmt('	// default: %s', cdv) or ''
		write(strfmt('	%s	%s;%s', ctp, v.name, cdv))
	end
	write('} '..name..';')
	write('')

	for i,v in ipairs(fields) do
		local field = sname..'_field('..v.name..')'
		if v.type=='bool' or v.type=='int' or v.type=='num' then
			write('#define '..sname..'_set_'..v.name..'(L, obj, val)  puss_cobject_set_'..v.type..'((L), &(obj->__parent__), ('..field..'), (val))')
		elseif v.type=='str' then
			write('#define '..sname..'_set_'..v.name..'(L, obj, str, len)  puss_cobject_set_str((L), &(obj->__parent__), ('..field..'), (str), (len))')
		elseif v.type=='lua' then
			write('#define '..sname..'_set_'..v.name..'(L, obj)  puss_cobject_stack_set((L), &(obj->__parent__), ('..field..'))')
		end
	end
	write('')
	write('#define '..sname..'_set(L, obj, prop, val)  '..sname..'_set_ ## prop((L), (obj), (val))')
	write('')

	local f = io.open(filename, 'w')
	if not f then return false end
	f:write(table.concat(ctx, '\n'))
	f:close()
end
