-- default.lua

local function samples()
	print('samples test:')
	print(' 1 - puss test')
	print(' 2 - const test')
	print(' 3 - glua test')
	print(' 4 - gtk test')
	io.stdout:write('please select: ')
	io.stdout:flush()
	local sel = math.tointeger(io.stdin:read('*l'))
	print('select:', sel)

	local function run(script)
		local cmd = string.format('%s/%s %s', puss._path, puss._self, script)
		print(cmd)
		os.execute(cmd)
	end

	if sel==1 then return run('samples/1_puss.lua') end
	if sel==2 then return run('samples/2_const.lua') end
	if sel==3 then return run('samples/3_glua.lua') end
	if sel==4 then return run('samples/4_gtk.lua') end
end

function __main__()
	print('usage: ./puss <script> [options...]')
	print()
	while true do
		samples()
	end
end

