-- puss --console test/cobject_test.lua

local COBJECT_SUPPORT_REF = 0x80000000
local COBJECT_SUPPORT_PROP = 0x40000000
local COBJECT_SUPPORT_SYNC = 0x20000000

local DemoObjectIDMask = 0x0001 | COBJECT_SUPPORT_REF | COBJECT_SUPPORT_PROP | COBJECT_SUPPORT_SYNC

local fields =
	{ {name='a', type='int', sync=0x01, deps={}}
	, {name='b', type='int', sync=0x01, change_notify=true}
	, {name='c', type='int', sync=0x01}
	, {name='d', type='int', sync=0x01}
	, {name='e', type='int', sync=0x11, deps={'a'}}
	, {name='f', type='int', sync=0x11, deps={'b', 'e'}}
	, {name='g', type='int', sync=0x13, deps={'f', 'k'}}
	, {name='h', type='int', sync=0x03, deps={'c', 'd'}}
	, {name='i', type='int', sync=0x03, deps={'d', 'j'}}
	, {name='j', type='int'}
	, {name='k', type='int', deps={'h', 'i'}}
	, {name='l', type='int', deps={}}
	, {name='m', type='int', sync=0x01}
	, {name='n', type='int', deps={'l', 'm'}}
	, {name='o', type='lua', sync=0x10, deps={}}
	}

local getmetatable = getmetatable
local trace = print

for i,v in ipairs(fields) do fields[v.name] = i end

local function perft(t, n, name)
	local m = 8
	for i=1,m do t[i] = i end
	local ts = puss.timestamp(true)
	for j=1,n or 10 do
		for i=1,m do t[i] = t[i] + 1 end
	end
	local te = puss.timestamp(true)
	trace(name, te-ts)
end

local function perf1(t, n, name)
	local m = 8
	for i=1,m do t:set(i, i) end
	local ts = puss.timestamp(true)
	for j=1,n or 10 do
		for i=1,m do t:set(i, t:get(i)+1) end
	end
	local te = puss.timestamp(true)
	trace(name, te-ts)
end

local function perf2(t, n, name)
	local m = 8
	for i=1,m do t:set(i, i) end
	local get, set = t.get, t.set
	local ts = puss.timestamp(true)
	for j=1,n or 10 do
		for i=1,m do set(t, i, get(t, i)+1) end
	end
	local te = puss.timestamp(true)
	trace(name, te-ts)
end

local function on_changed(obj, field)
	print('changed notify', obj, field)
end

local function trace_sync(t)
	local r = {}
	local n = t:__sync(r)
	for i=1,n*3,3 do
		trace('sync', string.format('%d %02x %s', r[i], r[i+1], r[i+2]))
	end
end

local function test()
	local puss_cobject = puss.import('core.cobject')

	puss_cobject.build_cobject(puss._path..'/test/cobject_demo.h', 'DemoObject', DemoObjectIDMask, fields, true)

	trace('compile plugin ...')
	local ok, err, code = os.execute('cd /d '..puss._path..'/../ && tinycc\\win32\\tcc -Werror -shared -Iinclude -o bin\\plugins\\cobject_demo'..puss._plugin_suffix..' bin\\test\\cobject_demo.c 2>&1')
	if not ok then return trace('compile plugin failed:', err, code) end

	local DemoObject = puss.cschema_create(DemoObjectIDMask, fields)
	trace('create', DemoObject)
	puss.cschema_monitor_reset(DemoObject, on_changed)
	puss.cschema_notify_mode_reset(DemoObject, 0)	-- 0-module first 1-property first

	puss.cschema_formular_reset(DemoObject, fields.a, function(obj, val) print('a changed cformular', obj, val); return val /  10.3; end)
	puss.cschema_formular_reset(DemoObject, fields.o, function(obj, val) print('o changed cformular', obj, val); return 'xxx'; end)

	trace('load plguin ...')
	local plugin
	if puss.load_plugin_mpe then
		plugin = puss.load_plugin_mpe('cobject_demo')
	else
		plugin = puss.load_plugin('cobject_demo')
	end
	plugin.reg(DemoObject, plugin)

	trace('create object ...', DemoObject)
	_G.t1 = DemoObject()
	local function tracet1()
		local tt = {}
		for i,v in ipairs(fields) do
			tt[#tt+1] = v.name
			tt[#tt+1] = ':'
			tt[#tt+1] = tostring(t1:get(i))
			tt[#tt+1] = '  '
		end
		trace(table.concat(tt))
	end

	trace('set a')
	t1:set(fields.a, 100)
	tracet1()
	trace('set b')
	t1:set(fields.b, 3)
	tracet1()
	trace_sync(t1)

	trace('set b, e')
	t1(function(t) t:set(fields.b, 4); t:set(fields.e, 4); end)
	tracet1()
	trace_sync(t1)

	trace('set o')
	t1(function(t) t:set(fields.o, 'aa') end)
	tracet1()
	trace_sync(t1)

	local function notify(obj, k)
		local cb = getmetatable(obj).on_changed
		if cb then cb(obj,k) end
	end

	local mt = {}
	mt.__index = mt
	mt.get = function(t,k) return t._v[k] end
	mt.set = function(t,k,v)
		local changed = (t._v[k] ~= v)
		t._v[k] = v
		if changed then notify(t,k) end
	end
	mt.on_changed = on_changed
	local t2 = setmetatable({_v={}},mt)

	_G.print = function() end
	local count = 1000000
	perft({}, count, 'table')
	perf1(t1, count, 'cobject')
	perf2(t1, count, 'cobj222')
	t1(function(t1) perf1(t1, count, 'cbatch') end)
	perf1(t2, count, 'lobject')
	do
		local ts = puss.timestamp(true)
		plugin.test(t1, count)
		local te = puss.timestamp(true)
		trace('tcc', te-ts)
	end
	do
		local ts = puss.timestamp(true)
		t1(plugin.test, count)
		local te = puss.timestamp(true)
		trace('tcbatch', te-ts)
	end
	_G.print = trace
	tracet1()
end

function __main__()
	print(pcall(test))
	puss.sleep(100000)
end

