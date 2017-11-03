-- nuklearproxy.lua

local table_insert = table.insert
local table_concat = table.concat

local function pasre_header(apis, enums, lines)
	local inc = nil
	local function parse_line(line)
		local re_inc = line:match('^%s*(#%s*ifdef%s+NK_INCLUDE_.*)$')
		if re_inc then
			inc = re_inc
			return
		end

		local re_endif = line:match('^%s*#%s*endif.*$')
		if re_endif then
			inc = nil
			return
		end

		local ret, name, args = line:match('^%s*NK_API%s+(.+)%s+([_%w]+)%s*%(%s*(.*)%s*%)%s*;%s*$')
		if ret then
			table_insert(apis, {inc, ret, name, args})
			return
		end
	end

	for _, line in ipairs(lines) do
		parse_line(line)
	end
end

function main()
	local out = vlua.match_arg('^%-out=(.+)$') or '.'
	local lines = {}
	do
		local fname = vlua.match_arg('^%-src=(.+)$') or 'nuklear.h'
		for line in io.lines(fname) do
			-- #endif /* NK_NUKLEAR_H_ */
			table_insert(lines, line)
			if line:match('^%s*#%s*endif%s+/%*%s*NK_NUKLEAR_H_%s*%*/') then
				table_insert(lines, '')
				break
			end
		end
	end

	local apis = {}
	local enums = {}

	pasre_header(apis, enums, lines)

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
		local last_inc
		for _,v in ipairs(arr) do
			local inc = v[1]
			if inc~=last_inc then
				if last_inc then writeln('#endif') end
				if inc then writeln(inc) end
				last_inc = inc
			end
			cb(writeln, table.unpack(v, 2))
		end
		if last_inc then writeln('#endif') end
	end

	do
		local f = io.open(vlua.filename_format(out..'/'..'nuklear.h'), 'w')
		f:write( table_concat(lines, '\n') )
		f:close()
	end

	generate_file(vlua.filename_format(out..'/'..'nuklearproxy.symbols'), function(writeln)
		generate_line(apis, writeln, function(writeln, ret, name, args)
			writeln('__NUKLEARPROXY_SYMBOL(', name, ')	// ', ret, ' ', args)
		end)
	end)
end

