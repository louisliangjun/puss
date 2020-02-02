-- puss --console test/cobject_test.lua

local COBJECT_SUPPORT_REF = 0x80000000
local COBJECT_SUPPORT_PROP = 0x40000000
local COBJECT_SUPPORT_SYNC = 0x20000000

local DemoObjectIDMask = 0x00000001 | COBJECT_SUPPORT_REF | COBJECT_SUPPORT_PROP | COBJECT_SUPPORT_SYNC

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
	, {name='o', type='int', deps={}}
	}

local getmetatable = getmetatable

local trace = print

local function perf(t, n, name)
	local m = 8
	for i=1,m do t[i] = i end
	local ts = puss.timestamp(true)
	for j=1,n or 10 do
		for i=1,m do t[i] = t[i] + 1 end
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

	puss_cobject.build_cobject(puss._path..'/test/cobject_demo.h', 'DemoObject', DemoObjectIDMask, fields)

	trace('compile plugin ...')
	os.execute('cd /d '..puss._path..'/../ && tinycc\\win32\\tcc -shared -Iinclude -o bin\\plugins\\cobject_demo'..puss._plugin_suffix..' bin\\test\\cobject_demo.c 2>&1')

	local DemoObject = puss.cschema_create(DemoObjectIDMask, fields)
	trace('create', DemoObject)
	puss.cschema_changed_notify_handle_reset(DemoObject, on_changed)
	puss.cschema_changed_notify_mode_reset(DemoObject, 0)	-- 0-module first 1-property first

	puss.cschema_formular_reset(DemoObject, 1, function(obj, val)
		print('a changed formular')
		return val //  10
	end)

	trace('load plguin ...')
	if puss.load_plugin_mpe then
		puss.load_plugin_mpe('cobject_demo')(DemoObject)
	else
		puss.load_plugin('cobject_demo')(DemoObject)
	end

	trace('create object ...', DemoObject)
	_G.t1 = DemoObject()

	trace('set a')
	t1[1] = 100
	trace(table.unpack(t1, 1, 10))
	trace('set b')
	t1[2] = 3
	trace(table.unpack(t1, 1, 10))
	trace_sync(t1)

	trace('set b, e')
	t1(function(t) t[2]=4; t[5]=4; end)
	trace(table.unpack(t1, 1, 10))
	trace_sync(t1)

	local function notify(obj, k)
		local cb = getmetatable(obj).on_changed
		if cb then cb(obj,k) end
	end

	local mt = {}
	mt.__index = function(t,k) return t._v[k] end
	mt.__newindex = function(t,k,v)
		local changed = (t._v[k] ~= v)
		t._v[k] = v
		if changed then notify(t,k) end
	end
	mt.on_changed = on_changed
	local t2 = setmetatable({_v={}},mt)

	_G.print = function() end
	local count = 100000
	perf({}, count, 'table')
	perf(t1, count, 'cobject')
	perf(t2, count, 'lobject')
	_G.print = trace
end

function __main__()
	print(pcall(test))
	puss.sleep(100000)
end

