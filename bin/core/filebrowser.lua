-- filebrowser.lua

local docs = puss.import('core.docs')

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

_root_folders = _root_folders or {}
local root_folders = _root_folders

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
end

__exports.remove_folder = function(path)
	path = puss.filename_format(path, true)
	for i,v in ipairs(root_folders) do
		if path==v.path then
			table.remote(root_folders, i)
			break
		end
	end
end

__exports.remove_folders = function()
	root_folders = {}
	_root_folders = root_folders
end

local function fill_folder(dir, root)
	local index, dirs, files = {}, {}, {}
	dir.index, dir.dirs, dir.files = index, dirs, files

	root._list(dir.path, function(ok, fs, ds)
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
			table.insert(files, {name=v, parent=dir})
		end
	end)
end

local DIR_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
local FILE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen

local function show_folder(dir, root)
	if not dir.index then fill_folder(dir, root) end
	if dir.dirs then
		for _,v in ipairs(dir.dirs) do
			if imgui.TreeNodeEx(v.name, DIR_FLAGS, v.name) then
				show_folder(v, root)
				imgui.TreePop()
			end
			-- if imgui.IsItemClicked() then print('dir', v.path) end
		end
	end

	local file_clicked
	if dir.files then
		for _,v in ipairs(dir.files) do
			imgui.TreeNodeEx(v.name, FILE_FLAGS, v.name)
			if imgui.IsItemClicked() then file_clicked = v end
		end

		if file_clicked then
			-- print('file', file_clicked.parent.path..'/'..file_clicked.name)
			docs.open(file_clicked.parent.path..'/'..file_clicked.name)
		end
	end
end

__exports.update = function()
	imgui.Begin("FileBrowser")
	local remove_id
	for i,v in ipairs(root_folders) do
		local show, open = imgui.CollapsingHeader(v._label, true)
		if not open then remove_id = i end
		if show then show_folder(v, v) end
	end
	if remove_id then table.remove(root_folders, remove_id) end
	imgui.End()
end

