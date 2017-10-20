puss.dofile('samples/utils.lua')

function __main__()
	print('* main', glua, GTK_MAJOR_VERSION)

	local css_provider = gtk_css_provider_new()
	local ok, err = css_provider:load_from_data([[
		.TestClass {
			background: #ff0 ;
			background-color: #ff0 ;
		}

		* {
			background: #f0f ;
			background-color: #f0f ;
		}
	]], -1)
	if not ok then print(err:get_message()) end

	local run_sign = true
	local w = gtk_window_new()
	local b = gtk_button_new()
	b.label = 'test'
	b:get_style_context():add_class('TestClass')
	w:add(b)
	b:signal_connect('clicked', print)
	w:signal_connect('delete-event', function(w)
		run_sign = false
		return true
	end)
	w:show_all()

	local function apply_css(w)
		w:get_style_context():add_provider(css_provider, 1000)
		if not glua.gtype_check(w, 'GtkContainer') then return end
		gtk_container_forall(w, apply_css)
	end

	apply_css(w)

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

