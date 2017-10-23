-- default.lua

local function samples()
end

function __main__()
	print('usage: ./puss <script> [options...]')
	print()

	local function run(script)
		local cmd = string.format('%s/%s %s', puss._path, puss._self, script)
		print(cmd, os.execute(cmd))
	end

	local samples =
		{ 'samples/puss.lua'
		, 'samples/const.lua'
		, 'samples/glua.lua'
		, 'samples/gtk.lua'
		, 'samples/nuklear.lua'
		, 'source_editor/main.lua'
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

