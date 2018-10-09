-- pages.lua

_pages = _pages or {}
_index = _index or setmetatable({}, {__mode='v'})
local pages = _pages
local index = _index
local selected_page_label = nil
local next_active_page_label = nil

local TABSBAR_FLAGS = ( ImGuiTabBarFlags_Reorderable
	| ImGuiTabBarFlags_AutoSelectNewTabs
	| ImGuiTabBarFlags_FittingPolicyScroll
	)

__exports.update = function(main_ui)
    imgui.BeginTabBar('PussMainTabsBar', TABSBAR_FLAGS)

	-- set active, must after draw tabs
	local active = next_active_page_label
	if active then
		next_active_page_label = nil
		local page = index[active]
		if page then imgui.SetTabItemSelected(active) end
	end

	-- draw tabs
	local selected
	for i, page in ipairs(pages) do
		local label = page.label
		page.was_open = page.open
		selected, page.open = imgui.BeginTabItem(label, page.open, page.unsaved and ImGuiTabItemFlags_UnsavedDocument or ImGuiTabItemFlags_None)
		if selected then
			local last = selected_page_label
			selected_page_label = label
			local draw = page.module.tabs_page_draw
			if draw then main_ui:protect_pcall(draw, page, last~=label) end
			imgui.EndTabItem()
		end
	end

	imgui.EndTabBar()

	-- close 
	for i=#pages,1,-1 do
		local page = pages[i]
		if not page.open then
			local close = page.module.tabs_page_close
			if close then main_ui:protect_pcall(close, page) end
		end
		if not page.was_open then
			local destroy = page.module.tabs_page_destroy
			if destroy then main_ui:protect_pcall(destroy, page) end
			index[page.label] = nil
			table.remove(pages, i)
		end
	end
end

__exports.create = function(label, module)
	local page = index[label]
	if page then return page end
	local page = {label=label, module=module, was_open=true, open=true}
	table.insert(pages, page)
	index[label] = page
	return page
end

__exports.active = function(label)
	next_active_page_label = label
end

__exports.lookup = function(label)
	return index[label]
end

__exports.save_all = function(check_only)
	if not check_only then
		for _, page in ipairs(pages) do
			local save = page.module.tabs_page_save
			if save then save(page) end
		end
	end

	local all_saved = true
	for _, page in ipairs(pages) do
		if page.unsaved then
			all_saved = false
			break
		end
	end
	return all_saved
end

