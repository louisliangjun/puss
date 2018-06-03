-- demos.lua

local app = puss.import('core.app')

function tabs_page_draw(page)
	imgui.Text(page.label)
	imgui.Text(string.format('DemoPage: %s %s', page, page.draw))
end

__exports.new_page = function()
	local label
	while true do
		last_index = (last_index or 0) + 1
		label = string.format('demo:%u', last_index)
		if not app.lookup_page(label) then break end
	end
	return app.create_page(label, _ENV)
end

