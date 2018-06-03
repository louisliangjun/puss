-- filebrowser.lua

local app = puss.import('core.app')

--[[
file {
  name : string
  icon : TODO
}
dir {
  name   : string
  path   : string
  expend : bool
  index  : map<name:file_or_dir>
  dirs   : array<dir>
  files  : array<file>
}
--]]

_root_folders = _root_folders or {}
local root_folders = _root_folders

local gen_key = function(path) return path end
if puss._sep=='\\' then
	gen_key = function(path) return path:lower() end
end

__exports.add_folder = function(path)
	path = puss.filename_format(path, true)
	local skey = gen_key(path)
	for _,v in ipairs(root_folders) do
		local dkey = gen_key(v.path)
		if skey==dkey then return end
	end
	local dir =
		{ name = path:match('.*/([^/]+)$') or path
		, path = path
		, expend = false
		}
	table.insert(root_folders, dir)
end

__exports.update = function()
    if imgui.CollapsingHeader('folderA') then
    	imgui.Text('aaaaaa')
    end
    if imgui.CollapsingHeader('folderB') then
    	imgui.Text('bbbbbb')
    end
	
end

