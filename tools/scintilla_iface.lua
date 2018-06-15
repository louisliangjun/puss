-- scintilla_face.lua modify from scintilla/scripts/Face.py - module for reading and parsing Scintilla.iface file
-- Released to the public domain.

local function sanitiseLine(line)
	line = line:gsub('^%s-(.-)%s*##.*$', '%1')
	if line~='' then return line end
end

local function decodeParam(param)
	local tp, nv = param:match('^(%w*)%s+(.-)%s*$')
	if tp==nil then return {'', '', ''} end
	local n,v = nv:match('^(.-)%s*=%s*(.-)%s*$')
	if n==nil then return {tp, nv, ''} end
	return {tp, n, v}
end

local ScintillaFunFilter = { fun=true, get=true, set=true }

local function ScintillaFaceReadFromFile(name)
	local self = { order={}, features={}, values={}, events={} }
	local currentCategory = ""
	local currentComment = {}
	local currentCommentFinished = false
	for line in  io.lines(name) do
		line = sanitiseLine(line)
		if line then
			if line:sub(1,1) == "#" then
				if line:sub(2,2) == " " then
					if currentCommentFinished then
						currentComment = {}
						currentCommentFinished = false
					end
					table.insert(currentComment, line:sub(3))
				end
			else
				currentCommentFinished = true
				local featureType, featureVal = line:match('^([%w_]+)%s+(.*)$')
				if ScintillaFunFilter[featureType] then
					local retType, name, value, param1, param2 = featureVal:match('^%s*([%w_]+)%s+([%w_]+)%s*=%s*(%d+)%s*%(%s*(.-)%s*,%s*(.-)%s*%)%s*$')
					if retType==nil then error('Failed to decode :' .. line) end
					local p1, p2 = decodeParam(param1), decodeParam(param2)
					if self.values[value] then error("Duplicate value " .. value .. " " .. name) end
					table.insert(self.order, name)
					local t = { FeatureType = featureType
						, ReturnType = retType
						, Value = value
						, Param1Type = p1[1], Param1Name = p1[2], Param1Value = p1[3]
						, Param2Type = p2[1], Param2Name = p2[2], Param2Value = p2[3]
						, Category = currentCategory
						, Comment = currentComment
						}
					self.features[name] = t
					self.values[value] = true

				elseif featureType == "evt" then
					local retType, name, value, params = featureVal:match('^%s*([%w_]+)%s+([%w_]+)%s*=%s*(%d+)%s*%(%s*(.-)%s*%)%s*$')
					if retType==nil then error('Failed to decode :' .. line) end
					if self.events[value] then error("Duplicate event " .. value .. " " .. name) end
					table.insert(self.order, name)
					local t = { FeatureType = featureType
						, ReturnType = retType
						, Value = value
						, Category = currentCategory
						, Comment = currentComment
						}
					self.features[name] = t
					self.events[value] = true

				elseif featureType == "cat" then
					currentCategory = featureVal

				elseif featureType == "val" then
					local name, value = featureVal:match('^%s*([%w_]+)%s*=%s*(.-)%s*$')
					if name==nil then error('Failed to decode :' .. line) end
					table.insert(self.order, name)
					local t = { FeatureType = featureType
						, Category = currentCategory
						, Comment = currentComment
						, Value = value
						}
					self.features[name] = t

				elseif featureType == "enu" then
					local name, value = featureVal:match('^%s*([%w_]+)%s*=%s*(.-)%s*$')
					if name==nil then error('Failed to decode :' .. line) end
					table.insert(self.order, name)
					local t = { FeatureType = featureType
						, Category = currentCategory
						, Value = value
						}
					self.features[name] = t

				elseif featureType == "lex" then
					local name, value, prefix = featureVal:match('^%s*([%w_]+)%s*=%s*([%w_]+)%s+(.-)%s*$')
					if name==nil then error('Failed to decode :' .. line) end
					table.insert(self.order, name)
					local t = { FeatureType = featureType
						, Category = currentCategory
						, Value = value
						, Prefix = prefix
						}
					self.features[name] = t
				end
			end
		end
	end

	return self
end

function main()
	local iface_filename = vlua.match_arg('^%-input=(.+)$')
	local output_filename = vlua.match_arg('^%-output=(.+)$')
	if not( iface_filename and output_filename ) then error('parse -input && -output failed!') end
	
	local iface = ScintillaFaceReadFromFile(iface_filename)

	-- fun/get/set
	local function rename(n) return n:gsub('[A-Z][a-z]', '_%1'):gsub('([a-z])([A-Z])', '%1_%2'):sub(2):lower() end
	-- print(rename('GetIMEText'))	==> get_ime_text

	local function iface_type(tp) return tp=='' and 'void' or tp end

	local strfmt = string.format

	local output = io.open(output_filename, 'w')
	output:write('// NOTICE : generate by scintilla_iface.lua\n\n')

	output:write('static IFaceDecl sci_functions[] =\n\t{ ')
	for _, name in ipairs(iface.order) do
		local feature = iface.features[name]
		if feature and ScintillaFunFilter[feature.FeatureType] then
			output:write( strfmt( '{ "%s", "%s", %s, IFaceType_%s, IFaceType_%s, IFaceType_%s }\n\t, '
						, name, rename(name), feature.Value
						, iface_type(feature.ReturnType)
						, iface_type(feature.Param1Type)
						, iface_type(feature.Param2Type)
						) )
		end
	end
	output:write('{ NULL, NULL, 0, IFaceType_void, IFaceType_void, IFaceType_void}\n\t};\n\n')

	-- val
	output:write('static IFaceVal sci_values[] =\n\t{ ')
	for _, name in ipairs(iface.order) do
		local feature = iface.features[name]
		if not feature then
			-- ignore
		elseif feature.FeatureType=='val' then
			output:write( strfmt( '{ "%s", %s }\n\t, ', name, name ) )
		elseif feature.FeatureType=='evt' then
			output:write( strfmt( '{ "SCN_%s", SCN_%s }\n\t, ', name:upper(), name:upper() ) )
		end
	end
	output:write('{ NULL, 0 }\n\t};\n\n')

	-- val
	output:write('static IFaceLex sci_lexers[] =\n\t{ ')
	for _, name in ipairs(iface.order) do
		local feature = iface.features[name]
		if feature and feature.FeatureType=='lex' then
			output:write( strfmt( '{ "%s", %s }\n\t, ', name, feature.Value) )
		end
	end
	output:write('{ NULL, 0 }\n\t};\n\n')

	output:close()
end

