-- vmake.lua

if vlua.match_arg('^%-help$') then
	print('usage: vmake [target] [-options]')
	print('  vmake [target] -vmake-trace')
	print('  vmake -help')
	os.exit(0)
end

function trace()
	-- default not print
end

if vlua.match_arg('^%-vmake%-trace$') then
	trace = function(...)
		local t = {'>', ...}
		for i,v in ipairs(t) do t[i] = tostring(v) end
		print(table.concat(t, ' '))
	end
end

-- utils

function array_push(arr, ...)
	if type(arr)~='table' then arr = arr and {arr} or {} end

	local function _pack(t)
		if t==nil then
			-- ignore
		elseif type(t)=='table' then
			for i,v in ipairs(t) do _pack(v) end -- array
		else
			table.insert(arr, t)
		end
	end

	local n = select('#', ...)
	for i=1,n do
		_pack(select(i, ...))
	end
	return arr
end

-- array_pack('a', {'b', {'c'}}, 'd'}) ==> {'a','b','c','d'}
-- 
function array_pack(...)
	return array_push({}, ...)
end

-- args_concat('a', 'b', 'c') ==> 'a b c'
-- args_concat( {'a', 'b'}, 'c') ==> 'a b c'
-- args_concat('a', {'b'}, {'c'}) ==> 'a b c'
-- 
function args_concat(...)
	return table.concat(array_pack(...), ' ')
end

function array_convert(arr, convert)
	local outs = {}
	if type(arr)=='table' then
		for _, v in ipairs(arr) do
			local o = convert(v)
			if o then table.insert(outs, o) end
		end
	elseif arr then
		local o = convert(arr)
		if o then table.insert(outs, o) end
	end
	return outs
end

function table_convert(tbl, convert)
	local outs = {}
	for k, v in pairs(tbl) do
		local o = convert(k, v)
		if o then table.insert(outs, o) end
	end
	return outs
end

function path_concat(...)
	local pth = vlua.filename_format( table.concat(array_pack(...), '/') )
	return pth
end

function shell(...)
	local cmd = args_concat(...)
	local p = io.popen(cmd)
	local r = p:read('*a'):match('^%s*(.-)%s*$')
	p:close()
	return r
end

function shell_execute(...)
	local cmd = args_concat(...)
	print(cmd)
	if not os.execute(cmd) then error('shell_execute('..tostring(cmd)..')') end
end

function scan_files(path, matcher, no_path_prefix, no_loop)
	local last = path:sub(-1)
	if last~='/' and last~='\\' then path = path .. '/' end
	local outs = {}

	local function scan(base, pth)
		local fs, ds = vlua.file_list(base .. pth)
		for _,f in ipairs(fs) do
			if matcher(f) then
				table.insert(outs, pth .. f)
			end
		end
		if no_loop then return end
		for _,d in ipairs(ds) do scan(base, pth .. d .. '/') end
	end

	if no_path_prefix then
		scan(path, '')
	else
		scan('', path)
	end
	return outs
end

function scan_and_lookup_file(cflags, filename)
	local cflags = args_concat(cflags)
	for pth in cflags:gmatch('%-I([^ ]+)') do
		local f = path_concat(pth, filename)
		local exist, size, mtime = vlua.file_stat(f)
		if exist then return f, size, mtime end
	end
end

-- thread tasks

function fetch_includes_by_regex(src, include_paths)
	trace('fetch_includes_by_regex', src)
	local res = {}
	local function search_include_path(inc, paths)
		if paths==nil then return false end
		for _, pth in ipairs(paths) do
			local inc_file = path_concat(pth, inc)
			local exist, size, mtime = vlua.file_stat(inc_file)
			if exist then
				res[inc_file] = mtime
				return true
			end
		end
		return false
	end

	for line in io.lines(src) do
		local inc = line:match('^%s*#%s*include%s*"(.+)"') or line:match('^%s*#%s*include%s*<(.+)>')
		if inc then
			local pth = src:match('^(.*[\\/]).-$') or ''	-- first use src path
			if not search_include_path(inc, {pth}) then
				search_include_path(inc, include_paths)
			end
		end
	end

	return res
end

function mkdir_by_targets(targets)
	local dirs = {}

	local function loop_mkdir(dir)
		local dir = dir:match('(.+)[\\/][^\\/]+$')
		if dir==nil or dir=='.' or dir=='..' then return end
		dirs[dir] = true
		loop_mkdir(dir)
	end

	if type(targets)=='table' then
		for _, f in ipairs(targets) do loop_mkdir(f) end
	else
		loop_mkdir(targets)
	end

	dirs = table_convert(dirs, function(k,v) return k end)
	table.sort(dirs, function(a,b) return #a<#b end)
	for _, dir in ipairs(dirs) do
		trace('mkdir', dir)
		vlua.file_mkdir(dir)
	end
end

-- deps format : 
--  { file1, file2 ... }
--  { file1 : mtime1, file2 : mtime2 }
--  { {file1:mtime1}, {file1,file2} }
-- 
function check_deps(target, deps)
	local target_mtime
	do
		local exist, size, mtime = vlua.file_stat(target)
		if not exist then return false end
		target_mtime = mtime
	end 

	local function do_check(dep)
		if type(dep)=='string' then
			local exist, size, mtime = vlua.file_stat(dep)
			if not exist then return false end
			if target_mtime < mtime then return false end
		elseif type(dep)=='table' then
			for k,v in pairs(dep) do
				if type(k)=='string' then
					if target_mtime < v then return false end
				else
					if not do_check(v) then return false end
				end
			end
		elseif dep then
			error('not support this deps format: ' .. tostring(dep))
		end
		return true
	end

	return do_check(deps)
end

function check_deps_execute(target, deps, ...)	-- ... is commands
	if check_deps(target, deps) then return true end
	mkdir_by_targets(target)
	local cmd = args_concat(...)
	print(cmd)
	return os.execute(cmd)
end

-- multi-thread support compile tasks

function compile_tasks_create(...)
	local srcs = array_pack(...)
	trace('compile_tasks_create:', srcs)
	return array_convert(srcs, function(src) return {obj=nil, src=src, deps={}} end)	-- deps={ filename : mtime }
end

function compile_tasks_fetch_deps(tasks, include_paths)
	assert( vlua.thread_pool, "MUST in main thread!" )
	local includes = {}	-- filename : incs_array
	local index = {}	-- filename : mtime

	local function parse_file(src, mtime)
		local old_mtime = index[src]
		if old_mtime==nil or old_mtime < mtime then
			index[src] = mtime
			vlua.thread_pool:run(src, 'fetch_includes_by_regex', src, include_paths)
		end
	end

	for _, t in ipairs(tasks) do
		if not t.src then
			print('task=', t)
			for k,v in pairs(t) do print(k,v) end
			error('comile bad <task>.src!')
		end
		local exist, size, mtime = vlua.file_stat(t.src)
		if not exist then error('file not found: ' .. t.src) end
		parse_file(t.src, mtime)
	end

	vlua.thread_pool:wait(function(src, ok, res)
		trace('fetch_includes_by_regex', src)
		if not res then
			print('!!!', src, ok, res)
			os.exit(1)
		end
		local incs = {}
		includes[src] = incs
		for inc, mtime in pairs(res) do
			trace('  add ', inc, mtime)
			table.insert(incs, inc)
			parse_file(inc, mtime)
		end
	end)

	local function parse_deps(deps, src)
		for _, inc in ipairs(includes[src]) do
			if deps[inc]==nil then
				deps[inc] = index[inc]
				parse_deps(deps, inc)
			end
		end
	end

	for _, t in ipairs(tasks) do
		t.deps[t.src] = index[t.src]
		parse_deps(t.deps, t.src)
	end
end

function compile_tasks_src2obj(tasks, objpath_build, parent_replace)
	for _, t in ipairs(tasks) do
		local src, is_abs_path = vlua.filename_format(t.src)
		local prefix, suffix = src:match('(.*)%.([^\\/]*)$')
		local obj_suffix = suffix and suffix:gsub('c', 'o'):gsub('C', 'O')
		local obj = (obj_suffix==suffix) and (src .. '.ooo') or (prefix .. '.' .. obj_suffix)
		if parent_replace then
			obj = obj:gsub('^(%.%.)[\\/]', parent_replace)
			obj = obj:gsub('[\\/](%.%.)[\\/]', parent_replace)
		end
		t.obj = objpath_build(obj, is_abs_path)
		trace('src2obj', src, t.obj)
	end
end

function compile_tasks_make_obj_dirs(tasks)
	local targets = array_convert(tasks, function(t) return t.obj end)
	mkdir_by_targets(targets)
end

function compile_tasks_build(tasks, command_build)	-- command_build(task) is commands
	assert( vlua.thread_pool, "MUST in main thread!" )
	for _, t in pairs(tasks) do
		local deps = next(t.deps) and t.deps or {t.src}
		trace('check_deps_execute:', command_build(t))
		vlua.thread_pool:run(t.src, 'check_deps_execute', t.obj, deps, command_build(t))
	end
	vlua.thread_pool:wait(function(src, ok, res, code)
		if ok then
			trace('build target(', src, ') succeed')
		else
			error( string.format('build target(%s) failed: %s %s!', src, res, code) )
		end
	end)
end

function make_objs(srcs, include_paths, objpath_build, command_build)
	local tasks = compile_tasks_create(srcs)
	compile_tasks_fetch_deps(tasks, include_paths)
	compile_tasks_src2obj(tasks, objpath_build, '')
	compile_tasks_make_obj_dirs(tasks)
	compile_tasks_build(tasks, command_build)
	return array_convert(tasks, function(t) return t.obj end)
end

function make_target(target, deps, ...)	-- ... is commands
	if check_deps_execute(target, deps, ...) then return target end
	error('make('..tostring(target)..'): failed!')
end

function make_copy(filename, target_path, source_path)
	local target = path_concat(target_path, filename)
	local source = path_concat(source_path, filename)
	if not check_deps(target, source) then
		vlua.file_copy(source, target)
	end
	return target
end

