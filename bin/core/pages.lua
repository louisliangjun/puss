-- core.pages

local shotcuts = puss.import('core.shotcuts')

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

shotcuts.register('page/save_all', 'Save all pages', 'S', true, true, false, false)
shotcuts.register('page/close_all', 'Close all pages', 'W', true, true, false, false)

__exports.update = function()
    if not imgui.BeginTabBar('PussMainTabsBar', TABSBAR_FLAGS) then return end

	-- destroy closed tabs
	for i=#pages,1,-1 do
		local page = pages[i]
		if not page.was_open then
			local destroy = page.module.tabs_page_destroy
			if destroy then imgui.protect_pcall(destroy, page) end
			index[page.label] = nil
			if selected_page_label==page.label then selected_page_label = nil end
			imgui.SetTabItemClosed(page.label)
			table.remove(pages, i)
		end
	end

	-- set active
	local active = next_active_page_label
	if active then
		next_active_page_label = nil
		-- local page = index[active]
		-- if page then imgui.SetTabItemSelected(active) end
	end

	-- draw tabs
	local selected
	for i, page in ipairs(pages) do
		local label = page.label
		page.was_open = page.open
		local flags = ImGuiTabItemFlags_NoPushId
		if page.unsaved then flags = flags | ImGuiTabItemFlags_UnsavedDocument end
		if active and label==active then flags = flags | ImGuiTabItemFlags_SetSelected end
		selected, page.open = imgui.BeginTabItem(label, page.open, flags)
		if selected then
			local last = selected_page_label
			selected_page_label = label
			local draw = page.module.tabs_page_draw
			if draw then imgui.protect_pcall(draw, page, last~=label) end
			imgui.EndTabItem()
		end
	end

	imgui.EndTabBar()

	-- close tabs
	for i, page in ipairs(pages) do
		if not page.open then
			local close = page.module.tabs_page_close
			if close then imgui.protect_pcall(close, page) end
		end
	end

	if shotcuts.is_pressed('page/save_all') then save_all() end
	if shotcuts.is_pressed('page/close_all') then close_all() end
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

__exports.selected = function()
	return selected_page_label
end

__exports.lookup = function(label)
	return index[label]
end

__exports.save = function(check_only)
	local page = index[selected_page_label]
	if not page then return end
	local save = page.module.tabs_page_save
	if save then save(page) end
end

__exports.close = function()
	local page = index[selected_page_label]
	if not page then return end
	page.open = false
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

__exports.close_all = function()
	for i=#pages,1,-1 do
		local page = pages[i]
		if page.unsaved then
			local close = page.module.tabs_page_close
			if close then imgui.protect_pcall(close, page) end
			break
		end

		local destroy = page.module.tabs_page_destroy
		if destroy then imgui.protect_pcall(destroy, page) end
		index[page.label] = nil
		if selected_page_label==page.label then selected_page_label = nil end
		imgui.SetTabItemClosed(page.label)
		table.remove(pages, i)
	end
end
