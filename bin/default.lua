-- default.lua

_ENV.imgui = puss.require('puss_imgui')	-- MUST require before require other modules who use imgui

function __main__()
	local app_module_name = 'core.app'
	for i,v in ipairs(puss._args) do
		local m = v:match('^%-%-main%s*=%s*([%._%w]+)%s*$')
		if m then app_module_name = m end
	end

	local app = puss.import(app_module_name)
	app.init()
	while app.update() do
		imgui.WaitEventsTimeout()
	end
	app.uninit()
end

