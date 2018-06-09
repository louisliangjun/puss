-- filebrowser.lua

local app = puss.import('core.app')
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
  _key   : string -- key of path
}
--]]

_root_folders = _root_folders or {}
local root_folders = _root_folders

local gen_key = function(path) return path end
if puss._sep=='\\' then
	gen_key = function(path) return path:lower() end
end

__exports.append_folder = function(path)
	path = puss.filename_format(path, true)
	local skey = gen_key(path)
	for _,v in ipairs(root_folders) do
		if skey==v._key then return end
	end
	local name = path:match('.*/([^/]+)$') or path
	local dir =
		{ name = name
		, path = path
		, _label = name..'##'..skey
		, _key = skey
		}
	table.insert(root_folders, dir)
end

__exports.remove_folder = function(path)
end

local function fill_folder(dir)
	local index, dirs, files = {}, {}, {}
	dir.index, dir.dirs, dir.files = index, dirs, files

	local fs, ds = puss.file_list(dir.path)
	table.sort(fs)
	table.sort(ds)
	for i,v in ipairs(ds) do
		table.insert(dirs, {name=v, path=dir.path..'/'..v})
	end
	for i,v in ipairs(fs) do
		table.insert(files, {name=v, parent=dir})
	end
end

local DIR_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
local FILE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen

local function show_folder(dir)
	if not dir.index then fill_folder(dir) end

	for _,v in ipairs(dir.dirs) do
		if imgui.TreeNodeEx(v.name, DIR_FLAGS, v.name) then
			show_folder(v)
			imgui.TreePop()
		end
		-- if imgui.IsItemClicked() then print('dir', v.path) end
	end

	local file_clicked

	for _,v in ipairs(dir.files) do
		imgui.TreeNodeEx(v.name, FILE_FLAGS, v.name)
		if imgui.IsItemClicked() then file_clicked = v end
	end

	if file_clicked then
		-- print('file', file_clicked.parent.path..'/'..file_clicked.name)
		docs.open(file_clicked.parent.path..'/'..file_clicked.name)
	end
end

local function on_drop_files(files)
	for path in files:gmatch('(.-)\n') do
		local local_path = puss.utf8_to_local(path)
		local f = io.open(local_path, 'r')
		if not f then
			append_folder(path)
		else
			local ctx = f:read(64)
			if ctx==nil then
				append_folder(path)
			end
			f:close()
		end
	end
end

local function draw_blink_page()
	imgui.Text('drag folder here')
end

__exports.update = function()
	if #root_folders==0 then
		draw_blink_page()
	else
		local remove_id
		for i,v in ipairs(root_folders) do
			local show, open = imgui.CollapsingHeader(v._label, true)
			if not open then remove_id = i end
			if show then show_folder(v) end
		end
		if remove_id then table.remove(root_folders, remove_id) end
	end

	if imgui.IsWindowHovered() then
		local files = imgui.GetDropFiles()
		if files then on_drop_files(files) end
	end
end

