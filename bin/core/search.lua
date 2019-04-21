-- core.search

local shotcuts = puss.import('core.shotcuts')
local thread = puss.import('core.thread')
local docs = puss.import('core.docs')

local inbuf = imgui.CreateByteArray(1024)
local current_sel = 0
local current_key
local current_progress
local results_reset
local results = {}

shotcuts.register('search/next', 'Next search result', 'F4', false, false, false, false)

local function show_result(ps, pe)
	local highlight = imgui.GetColorU32(ImGuiCol_PlotHistogramHovered)
	for i=ps,pe do
		local v = results[i]
		if not v then break end
		local sel = (current_sel==i)
		local active = imgui.Selectable(v[1], sel)
		imgui.SameLine(nil, 10);	imgui.Text(v[2])
		imgui.SameLine(nil, 0);	imgui.PushStyleColor(ImGuiCol_Text, highlight);	imgui.Text(v[3]);	imgui.PopStyleColor()
		imgui.SameLine(nil, 0);	imgui.Text(v[4])
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
		if imgui.Button('SearchText:') then start_search(inbuf:str()) end
		imgui.SameLine()
		local reclaim_focus
		
		imgui.PushItemWidth(-1)
		if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
			start_search(inbuf:str())
			reclaim_focus = true
		end
		imgui.PopItemWidth()
		if reclaim_focus then imgui.SetKeyboardFocusHere(-1) end
	end

	imgui.BeginChild('##SearchResults')
	if results_reset then
		results_reset = false
		imgui.SetScrollY(0)
	end
	local line_height = imgui.GetTextLineHeightWithSpacing()
	local jump_next = shotcuts.is_pressed('search/next')
	if jump_next and current_sel <= #results then
		current_sel = current_sel + 1
		local v = results[current_sel]
		if v then
			local file, line = v[1]:match('^(.+):(%d+)$')
			-- print('open', file, line)
			docs.open(file, math.tointeger(line)-1, v[3])

			local h = imgui.GetWindowHeight()
			local pos = (current_sel - 1) * line_height
			local top = imgui.GetScrollY()
			if (pos < top) or (line_height >= h) then
				imgui.SetScrollY(pos)
			elseif (pos + imgui.GetTextLineHeight()) > (top + h) then
				imgui.SetScrollY(pos - h + math.max((h - line_height*2), (h/2)))
			end
		end
	end
	imgui.clipper_pcall(#results+1, imgui.GetTextLineHeightWithSpacing(), show_result)
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
	local offset = #results
	for i, v in ipairs(res) do
		local line, a, b, c = table.unpack(v)
		results[offset+i] = {string.format('%s:%s', filepath, line), a, b, c}
	end
end

__exports.start_search = function(text)
	current_sel = 0
	if text and #text > 0 then
		current_key, current_progress, results_reset, results = text, '', true, {}
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
