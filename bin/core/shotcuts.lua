-- shotcuts.lua

local keys = {}	-- keyname: code
do
	local function reg_range(from, to)
		local s = string.byte(from)
		local e = string.byte(to)
		assert( s <= e )
		for c=s,e do
			local k = string.char(c)
			keys[k] = c
		end
	end

	reg_range('0', '9')
	reg_range('A', 'Z')
	imgui.FetchExtraKeys(keys)
end

_shotcuts = _shotcuts or {}
local shotcuts = _shotcuts

local function shotcut_register(module, name, desc, key, ctrl, shift, alt, super)
	local code = keys[key]
	if not code then error('shotcut_register() not support key:'..tostring(key)) end

	local m = shotcuts[module]
	if not m then
		m = {}
		shotcuts[module] = m
	end

	m[name] = { module, name, desc or '', key, code, ctrl, shift, alt, super }
end

-- shotcuts register start
-- 
shotcut_register('app', 'reload', 'Reload scripts', 'F12', true, false, false, false)

shotcut_register('docs', 'save', 'Save file', 'S', true, false, false, false)
shotcut_register('docs', 'close', 'Close file', 'W', true, false, false, false)
shotcut_register('docs', 'find', 'Find in file', 'F', true, false, false, false)
shotcut_register('docs', 'jump', 'Jump in file', 'G', true, false, false, false)
shotcut_register('docs', 'replace', 'Replace in file', 'H', true, false, false, false)

-- 
-- shotcuts register end

local shotcut_sorted = {}
do
	local ms = {}
	for k in pairs(shotcuts) do table.insert(ms, k) end
	table.sort(ms)
	for _, module in ipairs(ms) do
		local m = shotcuts[module]
		local keys = {}
		for k in pairs(m) do table.insert(keys, k) end
		table.sort(keys)
		for _, name in ipairs(keys) do
			table.insert(shotcut_sorted, m[name])
		end
	end
end

local function shotcut_update()
	imgui.Text('module	name	desc	key	ctrl	shift	alt	super')
	for _, v in ipairs(shotcut_sorted) do
		imgui.Text(string.format('%s	%s	%s	%s	%s	%s	%s', v[1], v[2], v[3], v[4], v[6], v[7], v[8], v[9]))
	end
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(640, 480, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("Shotcut", show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	if res then shotcut_update() end
	imgui.End()
	return show
end

__exports.is_pressed = function(module, name)
	local m = shotcuts[module]
	if not m then return end
	local info = m[name]
	if not info then return end
	return imgui.IsShortcutPressed(table.unpack(info, 5))
end

