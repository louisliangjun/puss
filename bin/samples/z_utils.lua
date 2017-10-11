-- utils for samples

table.trace = function(t, name)
	local tname = '('..tostring(name or t)..')'
	print('<<<< table'..tname..' trace:', t)
	local keys = {}
	local table_insert = table.insert
	for k,v in pairs(t) do table_insert(keys, k) end
	table.sort(keys)
	for _, k in ipairs(keys) do print('', k, t[k]) end
	print('>>>> table'..tname..' keys:', #keys) 
end

