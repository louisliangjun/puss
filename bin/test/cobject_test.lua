-- puss --console test/cobject_test.lua

local DemoObjectIDMask = 0x00000001 | 0x80000000 | 0x40000000

local fields =
	{ {name='a', type='int', deps={}}
	, {name='b', type='int', change_notify=true}
	, {name='c', type='int'}
	, {name='d', type='int'}
	, {name='e', type='int', deps={'a'}}
	, {name='f', type='int', deps={'b', 'e'}}
	, {name='g', type='int', deps={'f', 'k'}}
	, {name='h', type='int', deps={'c', 'd'}}
	, {name='i', type='int', deps={'d', 'j'}}
	, {name='j', type='int'}
	, {name='k', type='int', deps={'h', 'i'}}
	, {name='l', type='int', deps={}}
	, {name='m', type='int'}
	, {name='n', type='int', deps={'l', 'm'}}
	, {name='o', type='int', deps={}}
	}

function __main__()
	local puss_cobject = puss.import('core.cobject')

	puss_cobject.build_cobject(puss._path..'/test/cobject_demo.h', 'DemoObject', DemoObjectIDMask, fields)

	print('compile plugin ...')
	os.execute('cd /d '..puss._path..'/../ && tinycc\\win32\\tcc -shared -Iinclude -o bin\\plugins\\cobject_demo'..puss._plugin_suffix..' bin\\test\\cobject_demo.c 2>&1')

	local DemoObject = puss.cschema_create(DemoObjectIDMask, fields)
	print('create', DemoObject)
	puss.cschema_changed_reset(DemoObject, function(obj, field)
		print('changed notify', obj, field)
	end)

	puss.cschema_formular_reset(DemoObject, 1, function(obj, val)
		print('a changed formular')
		return val //  10
	end)

	print('load plguin ...')
	if puss.load_plugin_mpe then
		puss.load_plugin_mpe('cobject_demo')(DemoObject)
	else
		puss.load_plugin('cobject_demo')(DemoObject)
	end

	print('create object ...', DemoObject)
	_G.t1 = DemoObject()
	
	print('set a')
	t1[1] = 100
	print(table.unpack(t1, 1, 10))
	print('set b')
	t1[2] = 3
	print(table.unpack(t1, 1, 10))

	print('set b, e')
	t1(function(t) t[2]=4; t[5]=4; end)
	print(table.unpack(t1, 1, 10))

	puss.sleep(100000)
end

