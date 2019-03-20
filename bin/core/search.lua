-- core.search

local thread = puss.import('core.thread')
local docs = puss.import('core.docs')

local inbuf = imgui.CreateByteArray(1024)
local ftbuf = imgui.CreateByteArray(1024, 'lua c h inl cpp hpp cxx hxx')
local current_sel
local current_key
local current_progress
local results = {}

local function start_search(text)
	local filter
	if text and #text > 0 then
		filter = {}
		for suffix in ftbuf:str():gmatch('%S+') do filter[suffix]=true end
		current_key, current_progress, results = text, '', {}
	else
		text = nil
	end
	thread.query(nil, nil, 'search_text', text, filter)
end

local function show_result(ps, pe)
	for i=ps,pe do
		local v = results[i]
		imgui.PushStyleColor(ImGuiCol_Text, 0.75, 0.75, 0, 1)
		local active = imgui.Selectable(v[1], current_sel==v)
		imgui.PopStyleColor()
		imgui.SameLine()
		imgui.Text(v[2])
		if active then
			current_sel = v
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
		imgui.Text('Search')
		imgui.SameLine()
		local reclaim_focus
		imgui.PushItemWidth(-1)
		if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
			start_search(inbuf:str())
			reclaim_focus = true
		end
		imgui.PopItemWidth()
		if reclaim_focus then imgui.SetKeyboardFocusHere(-1) end
		imgui.Text('Filter')
		imgui.SameLine()

		imgui.PushItemWidth(-1)
		imgui.InputText('##FilterText', ftbuf)
		imgui.PopItemWidth()
	end

	imgui.BeginChild('##SearchResults')
	imgui.clipper_pcall(#results, show_result)
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

__exports.update = function(show)
	local res
	imgui.SetNextWindowSize(640, 480, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("SearchResult", show)
	if res then show_search_ui() end
	imgui.End()
	return show
end
