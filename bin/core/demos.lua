-- demos.lua

local pages = puss.import('core.app')

function tabs_page_draw(page)
	imgui.Text(page.label)
	imgui.Text(string.format('DemoPage: %s %s', page, page.draw))
end

__exports.new_page = function()
	local label
	while true do
		last_index = (last_index or 0) + 1
		label = string.format('demo:%u', last_index)
		if not pages.lookup(label) then break end
	end
	return pages.create(label, _ENV)
end

