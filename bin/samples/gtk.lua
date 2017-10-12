puss.dofile('samples/utils.lua')

function __main__()
	print('* main', glua, GTK_MAJOR_VERSION)
	local run_sign = true
	local w = gtk_window_new()
	local b = gtk_button_new()
	b.label = 'test'
	w:add(b)
	b:signal_connect('clicked', print)
	w:signal_connect('delete-event', function(w)
		run_sign = false
		return true
	end)
	w:show_all()
	while run_sign do
		gtk_run_once()
	end
end

print('* outer', glua, GTK_MAJOR_VERSION)

if not glua then
	local glua = puss.require('puss_gtk')
	local glua_symbols = glua._symbols
	_ENV.glua = glua
	setmetatable(_ENV, {__index=glua_symbols})
	puss.dofile('samples/gtk.lua')	-- for use glib symbols & enums
end

