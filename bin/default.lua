-- default.lua

_ENV.imgui = puss.require('puss_imgui')	-- MUST require before require other modules who use imgui

function __main__()
	local puss_app = puss.import('core.app')
	puss_app.init()
	while puss_app.update() do
		imgui.WaitEventsTimeout()
	end
	puss_app.uninit()
end

