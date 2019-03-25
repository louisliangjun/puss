-- core.icons

local ICONS_TEX, ICON_WIDTH, ICON_HEIGHT = nil, 32, 32
local ICONS =
	{ 'menu', 'trash', 'setting', 'refresh'
	, 'search', 'add', 'delete', 'file'
	, 'folder', 'folder-open', 'edit'
	}

pcall(function()
	for i,v in ipairs(ICONS) do ICONS[v] = i end
	local f = io.open(puss._path .. '/core/icons.png', 'rb')
	if not f then return end
	local ctx = f:read('*a')
	f:close()
	if not ctx then return end
	ICONS_TEX, ICON_WIDTH, ICON_HEIGHT = imgui.create_image(ctx)
	if __icons_tex then imgui.destroy_image(__icons_tex) end
	__icons_tex = ICONS_TEX
end)

local function image_button(label, icon, size, hint, tint_r, tint_g, tint_b, tint_a, frame_padding, bg_r, bg_g, bg_b, bg_a)
	local clicked
	if type(icon)=='string' then icon = ICONS[icon] end
	if icon and ICONS_TEX then
		local u = ICON_HEIGHT / ICON_WIDTH
		imgui.PushID(label)
		clicked = imgui.ImageButton(ICONS_TEX, size, size, u*(icon-1), 0, u*icon, 1, frame_padding, bg_r, bg_g, bg_b, bg_a, tint_r, tint_g, tint_b, tint_a)
		imgui.PopID()
	else
		clicked = imgui.Button(label)
	end
	if hint and imgui.IsItemHovered() then
		imgui.BeginTooltip()
		imgui.Text(hint)
		imgui.EndTooltip()
	end
	return clicked
end

__exports.button = image_button

