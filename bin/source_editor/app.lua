-- app.lua

local EDITOR = puss.import('editor')

local main_builder = nil
local main_window = nil

function on_main_window_delete(w, ...)
	print('on_main_window_delete signal handle!')
	gtk_main_quit()
end

function on_main_window_destroy(w, ...)
	print('on_main_window_destroy signal handle!')
	return true
end

local function main_window_open()
	assert( main_builder==nil, 'not support open twice!' )
	main_builder = gtk_builder_new()
	main_builder:add_from_file( string.format('%s/source_editor/app.glade', puss._path) )
	main_window = main_builder:get_object('main_window')
	-- puss_debug_panel_open()
	-- puss_modules_open()
	main_builder:connect_signals(_ENV)
	main_window:show_all()
end

local function on_app_activate(...)
	print('activate', ...)
	if not main_window then
		main_window_open()
		app:add_window(main_window)

		local ed = EDITOR.create(main_builder, 'noname', 'cpp')
	end

	main_window.window:raise()
end

local function open_from_gfile(gfile)
	local ed = EDITOR.create(main_builder, gfile:get_basename(), 'cpp')
	local cxt = g_file_get_content(gfile:get_path())
	ed:set_text(nil, cxt)
	return ed
end

local function on_app_open(app, files, nfiles, hint)
	print('open', app, files, nfiles, hint, #hint)
	if not main_window then
		main_window_open()
		app:add_window(main_window)
	end

	local t = glua.gobject_array_pointer_parse(files, nfiles)
	for i,v in ipairs(t) do
		-- print('open', app, files, nfiles, hint, #hint)
		open_from_gfile(v)
	end
end

_exports.main_builder = function()
	return main_builder
end

_exports.open_file = function(label, filename, line)
	local cxt = g_file_get_content(filename)
	local ed = EDITOR.create(main_builder, label, 'cpp')
	ed:set_text(nil, cxt)
	return ed
end

_exports.main = function()
	app = gtk_application_new('puss.org', G_APPLICATION_HANDLES_OPEN)
	app:set_default()
	app:signal_connect('activate', on_app_activate)
	app:signal_connect('open', on_app_open)

	local args = g_strv_new(puss._args)
	app:run(args:len(), args)
end

