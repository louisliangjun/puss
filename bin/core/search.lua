-- core.search

local shotcuts = puss.import('core.shotcuts')
local thread = puss.import('core.thread')
local docs = puss.import('core.docs')

local inbuf = imgui.CreateByteArray(1024)
local ftbuf = imgui.CreateByteArray(1024, 'lua c h inl cpp hpp cxx hxx')
local current_sel = 0
local current_key
local current_progress
local results = {}

shotcuts.register('search/next', 'Next search result', 'F4', false, false, false, false)

local function show_result(ps, pe)
	for i=ps,pe do
		local v = results[i]
		imgui.PushStyleColor(ImGuiCol_Text, 0.75, 0.75, 0, 1)
		local sel = (current_sel==i)
		local active = imgui.Selectable(v[1], sel)
		imgui.PopStyleColor()
		imgui.SameLine()
		imgui.Text(v[2])
		if active then
			current_sel = i
			local file, line = v[1]:match('^(.+):(%d+)$')
			-- print('open', file, line)
			docs.open(file, math.tointeger(line)-1)
		end
	end
end

local function show_search_ui()
	if current_key then
		if imgui.Button('Cancel') then start_search() end
		imgui.SameLine()
		imgui.PushItemWidth(-1)
		imgui.Text(current_progress)
		imgui.PopItemWidth()
	else
		if imgui.Button('SearchText:') then start_search(inbuf:str(), ftbuf:str()) end
		imgui.SameLine()
		local reclaim_focus
		
		imgui.PushItemWidth(-1)
		if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
			start_search(inbuf:str(), ftbuf:str())
			reclaim_focus = true
		end
		if reclaim_focus then imgui.SetKeyboardFocusHere(-1) end
		if imgui.Button('ResetFilter') then
			local filter = {}
			for suffix in ftbuf:str():gmatch('%S+') do filter[suffix]=true end
			thread.broadcast(nil, nil, 'set_suffix_filter', filter)
		end
		imgui.SameLine()
		imgui.InputText('##FilterText', ftbuf)
		imgui.PopItemWidth()
	end

	imgui.BeginChild('##SearchResults')
	local line_height = imgui.GetTextLineHeight()
	local jump_next = shotcuts.is_pressed('search/next')
	if jump_next and current_sel <= #results then
		current_sel = current_sel + 1
		local v = results[current_sel]
		if v then
			local file, line = v[1]:match('^(.+):(%d+)$')
			-- print('open', file, line)
			docs.open(file, math.tointeger(line)-1)

			local pos = (current_sel - 1) * line_height
			local top = imgui.GetScrollY()
			local h = imgui.GetWindowHeight()
			if pos <= top then
				imgui.SetScrollY(pos)
			elseif pos >= (top+h-5*line_height) then
				imgui.SetScrollY(pos+5*line_height-h)
			end
		end
	end
	imgui.clipper_pcall(#results, line_height, show_result)
	imgui.EndChild()
end

__exports.on_search_progress = function(key, index, total, filepath)
	if current_key~=key then return end
	if index < total then
		current_progress = string.format('[%s/%s] - %s', index, total, filepath)
	else
		current_key, current_progress = nil, nil
	end
end

__exports.on_search_result = function(key, filepath, res)
	if current_key~=key then return end
	for i=1,#res,2 do
		table.insert(results, {string.format('%s:%s', filepath, res[i]), res[i+1]})
	end
end

__exports.start_search = function(text)
	current_sel = 0
	if text and #text > 0 then
		current_key, current_progress, results = text, '', {}
	else
		text = nil
	end
	thread.query(nil, nil, 'search_text', text)
end

__exports.update = function(show)
	local res
	imgui.SetNextWindowSize(640, 480, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("SearchResult", show)
	if res then show_search_ui() end
	imgui.End()
	return show
end
