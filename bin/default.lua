-- default.lua

function __main__()
	print('usage: ./puss <script> [options...]')
	print()

	local function run(script)
		local debug_mode = puss.debug and '--debug' or ''
		print('debug mode :', debug_mode)
		local cmd = string.format('%s/%s %s %s', puss._path, puss._self, script, debug_mode)
		print(cmd, os.execute(cmd))
	end

	local samples =
		{ 'samples/puss.lua'
		, 'samples/const.lua'
		, 'samples/imgui.lua'
		, 'samples/debug.lua'
		}

	while true do
		print('samples test:')
		for i, s in ipairs(samples) do
			print( string.format(' %d - %s', i, s) )
		end
		print(' other - quit')

		io.stdout:write('please select: ')
		io.stdout:flush()
		local s = samples[ math.tointeger(io.stdin:read('*l')) ]
		if not s then break end
		run(s)
	end
end

