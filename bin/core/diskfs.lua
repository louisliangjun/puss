-- core.diskfs

local O_WRITE, O_READ = 'w', 'r'

__exports.filename_hash = function(filepath)
	return filepath
end

__exports.save = function(filepath, ctx)
	local f = io.open(puss.utf8_to_local(filepath), O_WRITE)
	if not f then return false end
	f:write(ctx)
	f:close()
	return true
end

__exports.load = function(filepath)
	local f = io.open(puss.utf8_to_local(filepath), O_READ)
	if not f then return end
	local ctx = f:read('*a')
	f:close()
	return ctx
end

__exports.list = function(dir)
	return puss.file_list(dir, true)	-- list file & convert name to utf8
end

__exports.exist = function(filepath)
	local pth = puss.utf8_to_local(filepath)
	local f = io.open(pth, 'r')
	if not f then return false end
	local ctx = f:read(64)
	f:close()
	return ctx and true
end

if os.getenv('OS')=='Windows_NT' then
	O_WRITE, O_READ = 'wb', 'rb'

	__exports.filename_hash = function(filepath)
		return filepath:lower()
	end
end
