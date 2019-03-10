-- core.samples

local sci = puss.import('core.sci')

local function gen_error_panel(err)
	return function()
		imgui.Text('please select samples or input code & press [F5]')
		if err then
			imgui.Text('load script error:')
			imgui.Text(err)
		end
	end
end

local sample_draw = gen_error_panel()

local samples = {}

local function append_sample(label, source)
	table.insert(samples, {label=label, source=source})
end

local function samples_update()
	local refresh, selected
	imgui.Columns(2)
	imgui.BeginChild('##Stage')
		imgui.protect_pcall(sample_draw)
	imgui.EndChild()
	imgui.NextColumn()
	imgui.BeginChild('##Select', nil, 150, false)
		local count = 8
		imgui.Columns( (#samples + count -1) // count )
		for i,v in ipairs(samples) do
			if imgui.Selectable(v.label) then selected = v end
			if i%count==0 then imgui.NextColumn() end
		end
		imgui.Columns(1)
	imgui.EndChild()
	refresh = imgui.Button('Press[F5] reload & preview ...')
	imgui.BeginChild('##Code', nil, nil, false, ImGuiWindowFlags_AlwaysHorizontalScrollbar)
		if not code_sv then code_sv, selected = sci.create('lua'), samples[1] end
		if selected then
			code_sv(function(sv)
				sv:SetText(selected.source)
				sv:EmptyUndoBuffer()
			end)
			refresh = true
		end
		code_sv()
		if imgui.IsWindowFocused() and imgui.IsKeyPressed(PUSS_IMGUI_KEY_F5) then refresh = true end
		if refresh then 
			local len, txt = code_sv:GetText(code_sv:GetTextLength())
			local env = setmetatable({}, {__index=_ENV})
			local f, err = load(txt, 'sample', 't', env)
			if f then
				f, err = imgui.protect_pcall(f)
				if f then
					f = env.draw
					if not f then err = 'function draw() MUST exist!!!' end
				end
			end
			sample_draw = f or gen_error_panel(err)
		end
	imgui.EndChild()
	imgui.Columns(1)
end

__exports.update = function(show)
	local res = false
	imgui.SetNextWindowSize(800, 600, ImGuiCond_FirstUseEver)
	res, show = imgui.Begin("SamplesWindow", show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
	if res then samples_update() end
	imgui.End()
	return show
end

-- samples
-- 
-- append_sample('SampleLabel', source=[[
-- imgui.Text('Sample source')
-- ]]})

append_sample('Imgui Samples Usage', [[
function draw()
	imgui.Text('dear imgui,')
	imgui.Separator()
	imgui.Text('Dear ImGui is a bloat-free graphical user interface library for C++.')
	imgui.Spacing()
	imgui.Text('Puss: Lua plugin library & interface.')
	imgui.Spacing()
	imgui.Text('PussImgui: Lua binding for imgui and implementations.')
	imgui.Spacing()
end
]])

append_sample('Texts', [[
function draw()
	imgui.Text('simple label string')

	imgui.TextColored(0.7, 0, 0, 1, 'RGBA: dark red color text')

	imgui.TextDisabled('disabled text')

	imgui.TextWrapped('===== wrapped lines begin =====\n 11111 22222 33333 44444 55555 66666 77777 88888 99999 aaaaa bbbbb ccccc ddddd eeeee fffff ggggg hhhhh iiiii jjjjj kkkkk lllll mmmmm nnnnn 11111 22222 33333 44444 55555 66666 77777 88888 99999 aaaaa bbbbb ccccc ddddd eeeee fffff ggggg hhhhh iiiii jjjjj kkkkk lllll mmmmm nnnnn\n==== wrapped lins end ====')

	imgui.LabelText('label', 'text')

	imgui.BulletText('bullet text')
end
]])

append_sample('Basic Buttons', [[
local click_count = 0

function draw()
	if imgui.Button('click me : ' .. click_count) then
		click_count = click_count + 1
	end

	if imgui.SmallButton('small') then
		click_count = click_count + 2
	end

	imgui.Text('InvisibleButton')
	if imgui.InvisibleButton('InvisibleButton', 200, 100) then
		click_count = click_count + 3
	end

	imgui.ArrowButton('left',  ImGuiDir_Left)
	imgui.ArrowButton('right', ImGuiDir_Right)
	imgui.ArrowButton('up',    ImGuiDir_Up)
	imgui.ArrowButton('down',  ImGuiDir_Down)
end
]])

append_sample('Check & Radio', [[
local clicked_count = 0

local checked = false
local flags = 0
local ra = 1
local rb = 2

function draw()
	local clicked
	imgui.Text( 'clicked: ' .. clicked_count )
	imgui.Text( 'checked:' .. tostring(checked) )

	clicked, checked = imgui.Checkbox('Checkbox', checked)

	imgui.Text( 'flags: ' .. flags )
	clicked, flags = imgui.CheckboxFlags('Flag1', flags, 1); imgui.SameLine()
	clicked, flags = imgui.CheckboxFlags('Flag2', flags, 2); imgui.SameLine()
	clicked, flags = imgui.CheckboxFlags('Flag4', flags, 4)

	if clicked then
		clicked_count = clicked_count + 1
	end

	if imgui.RadioButton('ra 1', ra==1) then ra=1 end; imgui.SameLine()
	if imgui.RadioButton('ra 2', ra==2) then ra=2 end; imgui.SameLine()
	if imgui.RadioButton('ra 3', ra==3) then ra=3 end

	clicked, rb = imgui.RadioButton('rb 1', rb, 1); imgui.SameLine()
	clicked, rb = imgui.RadioButton('rb 2', rb, 2); imgui.SameLine()
	clicked, rb = imgui.RadioButton('rb 3', rb, 3)
end
]])

append_sample('Drag Numbers', [[
local v1, v2, v3, v4 = 10, 20, 30, 40
local rs, re = 0, 100
local n1, n2 = 10, 20

function draw()
	local clicked
	imgui.Text(string.format('v1=%.1f v2=%.1f v3=%.1f v4=%.1f', v1, v2, v3, v4))

	clicked, v1 = imgui.DragFloat('DragFloat', v1, 10, 0, 1000)
	clicked, v1, v2 = imgui.DragFloat2('DragFloat2', v1, v2, 5, 0, 1000, '%.1f')
	clicked, v1, v2, v3 = imgui.DragFloat3('DragFloat3', v1, v2, v3, 1, 0, 1000, '%.1f')
	clicked, v1, v2, v3, v4 = imgui.DragFloat4('DragFloat4', v1, v2, v3, v4, 0.1, 0, 1000, '%.1f')

	imgui.Text(string.format('range [%.1f, %.1f]', rs, re))
	clicked, rs, re = imgui.DragFloatRange2('DragFloatRange2', rs, re, 1, 0, 100)

	clicked, n1, n2 = imgui.DragInt2('DragInt2', n1, n2, 5, 0, 1000, '%04d')
end
]])

append_sample('Slider Numbers', [[
local v1, v2, v3, v4 = 10, 20, 30, 40
local n1, n2 = 10, 20

function draw()
	local clicked
	imgui.Text(string.format('v1=%.1f v2=%.1f v3=%.1f v4=%.1f', v1, v2, v3, v4))

	clicked, v1 = imgui.SliderFloat('SliderFloat', v1, 0, 1000)
	clicked, v1, v2 = imgui.SliderFloat2('SliderFloat2', v1, v2, 0, 1000, '%.1f')
	clicked, v1, v2, v3 = imgui.SliderFloat3('SliderFloat3', v1, v2, v3, 0, 1000, '%.1f')
	clicked, v1, v2, v3, v4 = imgui.SliderFloat4('SliderFloat4', v1, v2, v3, v4, 0, 1000, '%.1f')

	clicked, n1, n2 = imgui.SliderInt2('SliderInt2', n1, n2, 0, 1000, '%04d')
	clicked, v1 = imgui.VSliderFloat('VSliderFloat', 40, 200, v1, 0, 1000)
	imgui.SameLine()
	clicked, n1 = imgui.VSliderInt('VSliderInt', 40, 200, n1, 0, 1000)
end
]])

append_sample('Input Numbers', [[
local v1, v2, v3, v4 = 10, 20, 30, 40
local n1, n2 = 10, 20

function draw()
	local res
	imgui.Text(string.format('v1=%.1f v2=%.1f v3=%.1f v4=%.1f', v1, v2, v3, v4))

	res, v1 = imgui.InputFloat('InputFloat', v1)
	res, v1, v2 = imgui.InputFloat2('InputFloat2 Readonly', v1, v2)
	res, v1, v2, v3 = imgui.InputFloat3('InputFloat3', v1, v2, v3)
	res, v1, v2, v3, v4 = imgui.InputFloat4('InputFloat4', v1, v2, v3, v4)

	res, n1, n2 = imgui.InputInt2('InputInt2', n1, n2)
end
]])

append_sample('Combo & Other ...', [[
local combo_value = 'preview value'

function draw()
	imgui.Text('combo:')
	if imgui.BeginCombo('label', combo_value, flags) then
		imgui.TextDisabled('some no Selectable Text')
		for i=1,6 do
			local v = 'value :' .. i
			if imgui.Selectable('value :' .. i) then combo_value=v end
		end
		imgui.Text('some no Selectable Text')
		imgui.EndCombo()
	end

	imgui.Text('progress:')
	imgui.ProgressBar(0.33, 300, 50, 'overlay')

	imgui.Text('bullet:')
	for i=1,6 do imgui.Bullet() end
end
]])

append_sample('Simple Layout', [[
function draw()
	imgui.Text('normal')
	imgui.TextColored(0, 0.8, 0, 1, 'NORMAL')
	imgui.Separator()

	imgui.Text('spacing')
	imgui.Spacing()
	imgui.Spacing()
	imgui.TextColored(0, 0.8, 0, 1, 'SPACING')
	imgui.Separator()

	imgui.Text('sameline')
	imgui.SameLine()
	imgui.TextColored(0, 0.8, 0, 1, 'SAMELINE')
	imgui.Separator()

	imgui.Text('undo sameline')
	imgui.SameLine()
	imgui.NewLine()
	imgui.TextColored(0, 0.8, 0, 1, 'UNDO SAMELINE')
	imgui.Separator()

	imgui.Text('dummy')
	imgui.Dummy(50, 50)
	imgui.TextColored(0, 0.8, 0, 1, 'DUMMY')
	imgui.Separator()
end
]])

append_sample('Indent Layout', [[
function draw()
	imgui.Text('Normal')
	imgui.Separator()

	imgui.Indent(50)
	imgui.Text('Indent 50')

	imgui.Unindent(20)
	imgui.Text('Unindent 20')
end
]])

append_sample('Group Layout', [[
function draw()
	imgui.Button('BeforeGroup')

	imgui.SameLine()
	imgui.BeginGroup()
		imgui.Button('InGroup1')
		imgui.Button('InGroup2')
		imgui.Button('InGroup3')
	imgui.EndGroup()

	imgui.SameLine()
	imgui.Button('AfterGroup')
end
]])

append_sample('Colunmns Layout', [[
function draw()
	local n = 3
	imgui.Columns(3)
	local n = imgui.GetColumnsCount()
	imgui.SetColumnOffset(1, 100)
	imgui.SetColumnWidth(1, 80)
	for row=1,5 do
		for i=1,n do
			local col = imgui.GetColumnIndex()
			imgui.Button(string.format('R%d:C%d', row, col))
			imgui.NextColumn()
		end
	end
	imgui.Columns(1)
end
]])

append_sample('Simple Plot', [[
local cur = 100
local arr = imgui.CreateFloatArray(256)

function draw()
	cur = cur + math.random(-3, 3)
	arr:push(cur)
	imgui.PlotLines('Demo1', arr, 256, 0, 'overlay1', nil, nil, 0, 100)
	imgui.PlotHistogram('Demo2', arr, 128, nil, 'overlay2', nil, nil, 0, 100)
end
]])

append_sample('List Clipper', [[
local function draw_step(display_start, display_end, label)
	for i = display_start, display_end do
		imgui.Text(string.format('%s:%s', label, i))
	end
end

function draw()
	imgui.clipper_pcall(1000000, imgui.GetTextLineHeightWithSpacing(), draw_step, 'demo')
end
]])

