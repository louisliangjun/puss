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

local shotcut_columns = {'name', 'desc', 'key', 'code', 'ctrl', 'shift', 'alt', 'super'}
local n_shotcut_columns = #shotcut_columns

local function shotcut_draw_row(row)
	for i=1,n_shotcut_columns do
		imgui.Text(tostring(row[i]))
		imgui.NextColumn()
	end
end

local shotcut_sorted = nil

local function shotcut_update()
	if not shotcut_sorted then
		local keys = {}
		for k in pairs(shotcuts) do table.insert(keys, k) end
		table.sort(keys)
		for _, name in ipairs(keys) do
			table.insert(shotcut_sorted, shotcuts[name])
		end
	end

	imgui.Columns(n_shotcut_columns)
	imgui.Separator()
	shotcut_draw_row(shotcut_columns)
	imgui.Separator()
	for _, v in ipairs(shotcut_sorted) do
		shotcut_draw_row(v)
	end
	imgui.Columns(1)
	imgui.Separator()
end

__exports.register = function(name, desc, key, ctrl, shift, alt, super)
	local code = keys[key]
	if not code then error('shotcut_register() not support key:'..tostring(key)) end
	shotcuts[name] = { name, desc or '', key, code, ctrl, shift, alt, super }
	shotcut_sorted = nil
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(640, 480, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("Shotcut", show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	if res then shotcut_update() end
	imgui.End()
	return show
end

__exports.is_pressed = function(name)
	local info = shotcuts[name]
	if not info then return end
	return imgui.IsShortcutPressed(table.unpack(info, 4))
end

