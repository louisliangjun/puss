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
shotcut_register('app', 'reload', 'Reload scripts', 'F12', true)

shotcut_register('docs', 'save', 'Save file', 'S', true)

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
	for _, v in ipairs(shotcut_sorted) do
		imgui.Text( table.concat(v, '\t') )
	end
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(400, 300, ImGuiCond_FirstUseEver)
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

