-- core.shotcuts

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
		shotcut_sorted = {}
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
	local sctrl, sshift, salt, ssuper = '', '', '', ''
	if ctrl then sctrl='Ctrl+' elseif ctrl==nil then sctrl='[Ctrl]' end
	if shift then sshift='Shift+' elseif shift==nil then sshift='[Shift]' end
	if alt then salt='Alt+' elseif alt==nil then salt='[Alt]' end
	if super then ssuper='Super+' elseif super==nil then ssuper='[Super]' end
	local skey = string.format('%s%s%s%s%s', sctrl, sshift, salt, ssuper, key)
	shotcuts[name] = { name, desc or '', skey, code, ctrl, shift, alt, super }
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

__exports.menu_item = function(name, ...)
	local info = shotcuts[name]
	return imgui.MenuItem(info[2], info[3], ...)
end
