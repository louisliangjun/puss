-- core.filebrowser

local pages = puss.import('core.pages')
local docs = puss.import('core.docs')
local icons = puss.import('core.icons')
local thread = puss.import('core.thread')

--[[
file {
  name   : string
  parent : dir  -- parent dir
  icon   : TODO
}
dir {
  name   : string
  path   : string
  index  : map<name:file_or_dir>
  dirs   : array<dir>
  files  : array<file>
}
root_dir : dir {
  _label : string
}
--]]

_root_version = _root_version or 0
_root_folders = _root_folders or {}
local root_folders = _root_folders

local ftbuf = imgui.CreateByteArray(1024, 'lua c h inl cpp hpp cxx hxx')
local ptbuf = imgui.CreateByteArray(1024, puss._path)
local suffix_filter = {}

local function refresh_suffix_filter()
	suffix_filter = {}
	for suffix in ftbuf:str():gmatch('%S+') do suffix_filter[suffix]=true end
	thread.broadcast(nil, nil, 'set_suffix_filter', suffix_filter)
end

refresh_suffix_filter()

local current_selected_label
local current_expand_need
local current_expand_file

__exports.check_fetch_folders = function(ver)
	if ver==_root_version then return ver end

	local res = {}
	for i,v in ipairs(root_folders) do
		table.insert(res, v.path)
	end
	return _root_version, res
end

__exports.append_folder = function(path, async_list_dir)
	path = puss.filename_format(path, true)
	for i,v in ipairs(root_folders) do
		if path==v.path then return end
	end
	local name = path:match('.*/([^/]+)$') or path
	local dir =
		{ name = name
		, path = path
		, _list = async_list_dir
		, _label = name..'##'..path
		}
	table.insert(root_folders, dir)
	_root_version = _root_version + 1
end

__exports.remove_folder = function(path)
	path = puss.filename_format(path, true)
	for i,v in ipairs(root_folders) do
		if path==v.path then
			table.remote(root_folders, i)
			_root_version = _root_version + 1
			break
		end
	end
end

__exports.remove_folders = function()
	root_folders = {}
	_root_folders = root_folders
	_root_version = _root_version + 1
end

local function check_filter(filepath, filters)
	if not (filters and next(filters)) then return true end
	local suffix = filepath:match('.*%.(%w+)$')
	if suffix and filters[suffix] then return true end
end

local function fill_folder(dir, root, matched)
	local index, dirs, files = {}, {}, {}
	dir.index, dir.dirs, dir.files = index, dirs, files

	local exfile = current_expand_file
	root._list(dir.path, function(ok, fs, ds)
		if matched then current_expand_need = 'force' end
		if not ok then
			dir.index, dir.dirs, dir.files = nil, nil, nil
			return
		end
		table.sort(fs)
		table.sort(ds)
		for i,v in ipairs(ds) do
			table.insert(dirs, {name=v, path=dir.path..'/'..v})
		end
		for i,v in ipairs(fs) do
			if check_filter(v, suffix_filter) then
				table.insert(files, {name=v, parent=dir})
			end
		end
	end)
end

local DIR_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
local FILE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen

local function show_folder(dir, root, depth)
	if not dir.index then fill_folder(dir, root, depth) end
	if dir.dirs then
		for _,v in ipairs(dir.dirs) do
			local matched = (current_expand_file and depth and v.name==current_expand_file[depth])
			if matched and current_expand_need then
				-- print('   expand', v.name)
				imgui.SetNextTreeNodeOpen(true)
			end
			if imgui.TreeNodeEx(v.name, DIR_FLAGS, v.name) then
				show_folder(v, root, matched and (depth+1))
				imgui.TreePop()
			end
			-- if imgui.IsItemClicked() then print('dir', v.path) end
		end
	end

	local file_clicked
	if dir.files then
		for _,v in ipairs(dir.files) do
			local matched = (current_expand_file and depth and v.name==current_expand_file[depth])
			local flags = FILE_FLAGS
			if matched then flags = flags | ImGuiTreeNodeFlags_Selected end
			imgui.TreeNodeEx(v.name, flags, v.name)
			if imgui.IsItemClicked() then file_clicked = v end
			if matched and current_expand_need then
				-- print(matched, current_expand_need)
				current_expand_need = false
				if not imgui.IsItemVisible() then imgui.SetScrollHereY() end
			end
		end

		if file_clicked then
			-- print('file', file_clicked.parent.path..'/'..file_clicked.name)
			docs.open(file_clicked.parent.path..'/'..file_clicked.name)
		end
	end
end

local function check_expand_file()
	local selected = pages.selected()
	if current_selected_label == selected then return end
	current_selected_label = selected
	if not current_selected_label then return end
	-- print(selected)

	current_expand_file = nil
	local filepath = current_selected_label:match('.*###(.+)$')
	if not filepath then return end

	for i,v in ipairs(root_folders) do
		local path = v.path
		if filepath:sub(#path+1,#path+1)=='/' and filepath:sub(1, #path)==path then
			current_expand_file = {path}
			path = filepath:sub(#path+2)
			for s in path:gmatch('[^/]+') do
				table.insert(current_expand_file, s)
			end
		end
	end

	if current_expand_file then
		-- print(table.unpack(current_expand_file))
		current_expand_need = true
	end
end

local function refresh_folders()
	for i,v in ipairs(root_folders) do
		v.index, v.dirs, v.files = nil, nil, nil
	end
end

local open_setting = false
local open_add_dir = false

__exports.update = function(async_list_dir)
	if panel then
		if icons.button('Close', 'delete', 24, 'close panel', 0.6, 0, 0, 1) then
			local close_panel = panel
			panel = nil
			if close_panel=='Setting' then refresh_folders() end
		end
		imgui.SameLine(nil, 16)
	end
	if icons.button('Setting', 'setting', 24, 'close setting panel', open_setting and 0.4 or 1) then open_setting = not open_setting end
	imgui.SameLine()
	if icons.button('Add', 'add', 24, 'add new folder', open_add_dir and 0.4 or 1) then open_add_dir = not open_add_dir end
	imgui.SameLine()
	if icons.button('Refresh', 'refresh', 24, 'refresh all folders') then refresh_folders() end

	if open_setting then
		imgui.Separator()
		if imgui.Button('ResetFilter') then refresh_suffix_filter() end
		imgui.SameLine()
		imgui.PushItemWidth(-1)
		imgui.InputText('##FilterText', ftbuf)
		imgui.PopItemWidth()
	end

	if open_add_dir then
		imgui.Separator()
		if imgui.Button('Add Path') then append_folder(puss.filename_format(ptbuf:str(), true), async_list_dir) end
		imgui.SameLine()
		imgui.PushItemWidth(-1)
		imgui.InputText('##PathText', ptbuf)
		imgui.PopItemWidth()
	end

	local drop_folders_here = imgui.GetDropFiles(true)
	if drop_folders_here then
		for path in drop_folders_here:gmatch('(.-)\n') do
			local ok, st = puss.stat(path, true)
			if ok and st.dir then
				append_folder(puss.filename_format(path, true), async_list_dir)
			end
		end
	end

	imgui.Separator()
	imgui.BeginChild('##folders')
	current_expand_need = current_expand_need=='force'
	-- if current_expand_need then print(current_expand_need) end
	check_expand_file()

	local remove_id
	for i,v in ipairs(root_folders) do
		local matched = (current_expand_file and v.path==current_expand_file[1])
		if matched and current_expand_need then imgui.SetNextTreeNodeOpen(true) end
		local show, open = imgui.CollapsingHeader(v._label, true)
		if not open then remove_id = i end
		if show then
			imgui.PushID(v._label)
			show_folder(v, v, matched and 2)
			imgui.PopID()
		end
	end
	if remove_id then table.remove(root_folders, remove_id) end
	imgui.EndChild()
end
