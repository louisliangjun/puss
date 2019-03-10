-- core.miniline

local shotcuts = puss.import('core.shotcuts')
local filebrowser = puss.import('core.filebrowser')
local docs = puss.import('core.docs')
local thread = puss.import('core.thread')

local inbuf = imgui.CreateByteArray(512)
local open = false
local cursor = 1
local results = {}

local response_queue = puss.queue_create()

shotcuts.register('miniline/open', 'Open Miniline', 'P', true, false, false, false)

__exports.on_search_result = function(ok, key, res)
	if ok and key then results = res or {} end
end

local MINILINE_FLAGS = ( ImGuiWindowFlags_NoMove
	| ImGuiWindowFlags_NoDocking
	| ImGuiWindowFlags_NoTitleBar
	| ImGuiWindowFlags_NoResize
	| ImGuiWindowFlags_AlwaysAutoResize
	| ImGuiWindowFlags_NoSavedSettings
	| ImGuiWindowFlags_NoNav
	)

local check_refresh_index
do
	local last_index_ver

	check_refresh_index = function()
		local ver, folders = filebrowser.check_fetch_folders(last_index_ver)
		-- print(last_index_ver, ver)
		last_index_ver = ver
		if folders then
			thread.broadcast(nil, nil, 'search_indexes_rebuild', folders)
		end
	end
end

local function draw_miniline()
	if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_ESCAPE) then open = false end
	imgui.Text('file')
	imgui.SameLine()
	imgui.PushItemWidth(420)
	if imgui.InputText('##input', inbuf) then
		local str = inbuf:str()
		-- print('start search', str)
		thread.query('core.miniline', 'on_search_result', 'search_file', str)
	end
	imgui.SetItemDefaultFocus()
	imgui.PopItemWidth()

	if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_UP) then
		if cursor > 1 then cursor = cursor - 1 end
	end
	if imgui.IsShortcutPressed(PUSS_IMGUI_KEY_DOWN) then
		if cursor < #results then cursor = cursor + 1 end
	end

	local sel = imgui.IsShortcutPressed(PUSS_IMGUI_KEY_ENTER) and cursor
	for i=1,24 do
		local r = results[i]
		if not r then break end
		if i==cursor then
			imgui.PushStyleColor(ImGuiCol_Text,1,1,0,1)
		else
			imgui.PushStyleColor(ImGuiCol_Text,0.5,0.5,0.5,1)
		end
		if imgui.Selectable(r) then
			print(r, i, sel)
			sel = i
		end
		imgui.PopStyleColor()
	end

	if sel then
		local target = results[sel]
		open, cursor = false, 1
		inbuf:strcpy('')
		if target then docs.open(target) end
	end
end

__exports.update = function(show, x, y, w, h)
	if not open then
		if not shotcuts.is_pressed('miniline/open') then return end
		results = {}
	end

	check_refresh_index()

	imgui.SetNextWindowPos(x + (w * 0.5), y + 64, ImGuiCond_Always, 0.6, 0)
	show, open = imgui.Begin('##miniline', show, MINILINE_FLAGS)
	if show then
		if imgui.IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) then
			imgui.SetKeyboardFocusHere()
		else
			open = false
		end
		draw_miniline()
	end
    imgui.End()
    return show
end
