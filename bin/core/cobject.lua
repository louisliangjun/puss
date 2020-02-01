-- core.cobject

__exports.build_cobject = function(filename, name, id, fields)
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
	write(strfmt('#define PUSS_COBJECT_ID_%s'..' 0x%x', name:upper(), id))
	write(strfmt('#define %sCheck(L, idx)	(%s*)puss_cobject_check((L), (idx), PUSS_COBJECT_ID_%s)', name, name, name:upper()))
	write(strfmt('#define %sTest(L, idx)	(%s*)puss_cobject_test((L), (idx), PUSS_COBJECT_ID_%s)', name, name, name:upper()))
	write('')

	write(strfmt('enum %sFields', name))
	write('	{ _'..sname:upper()..'_NULL = 0')
	for i,v in ipairs(fields) do
		-- local field_enum_name = (sname..'_'..v.name):upper()
		write('	, '..(sname..'_'..v.name):upper()..' = '..i)
	end
	write('	, _'..sname:upper()..'_COUNT')
	write('	};')
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

	local f = io.open(filename, 'w')
	if not f then return false end
	f:write(table.concat(ctx, '\n'))
	f:close()
end
