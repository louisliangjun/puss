-- core.search

local thread = puss.import('core.thread')

local inbuf = imgui.CreateByteArray(4*1024)
local current_key = ''
local results = {}

local function show_result()
	imgui.PushItemWidth(-1)
	if imgui.InputText('##FindText', inbuf, ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue) then
		local text = inbuf:str()
		if #text==0 then return end
		current_key, results = text, {}
		thread.query(nil, nil, 'search_text', text)
	end
	imgui.PopItemWidth()

	imgui.Columns(3)
	for i, v in pairs(results) do
		imgui.Text(v[1])
		imgui.NextColumn()
		imgui.Text(v[2])
		imgui.NextColumn()
		if imgui.Selectable(v[3]) then
			print(table.unpack(v))
		end
		imgui.NextColumn()
	end
	imgui.Columns(1)
end

__exports.on_search_result = function(key, filepath, res)
	if current_key~=key then return end
	for i=1,#res,2 do
		table.insert(results, {filepath, res[i], res[i+1]})
	end
end

__exports.update = function(show)
	local res
	imgui.SetNextWindowSize(640, 480, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("SearchResult", show)
	if res then show_result() end
	imgui.End()
	return show
end