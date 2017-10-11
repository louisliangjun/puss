-- app.lua
-- 

local editor = puss.module_import('source_editor/editor.lua')

main_builder = nil
main_window = nil

local function puss_modules_open()
	puss.module_load('scene_editor/filebrowser.lua')
end

function on_main_window_delete(w, ...)
	print('on_main_window_delete signal handle!')
	gtk_main_quit()
end

function on_main_window_destroy(w, ...)
	print('on_main_window_destroy signal handle!')
	return true
end

function open_file(label, filename, line)
	local ed = editor.create(label)
	editor.set_language(ed, 'cpp')
	local cxt = g_file_get_content(filename)
	ed:set_text(nil, cxt)
	return ed
end

local function puss_main_window_open()
	assert( main_builder==nil, 'not support open twice!' )
	main_builder = gtk_builder_new()
	main_builder:add_from_file( string.format('%s/source_editor/app.glade', puss._path) )
	main_window = main_builder:get_object('main_window')
	-- puss_debug_panel_open()
	-- puss_modules_open()
	main_builder:connect_signals(_ENV)
	main_window:show_all()
end

local function puss_app_activate(...)
	print('activate', ...)
	if not main_window then
		puss_main_window_open()
		app:add_window(main_window)

		local ed = editor.create('noname')
		editor.set_language(ed, 'cpp')
	end

	main_window.window:raise()
end

local function puss_open_from_gfile(gfile)
	local ed = editor.create(gfile:get_basename())
	editor.set_language(ed, 'cpp')
	local cxt = g_file_get_content(gfile:get_path())
	ed:set_text(nil, cxt)
	return ed
end

local function puss_app_open(app, files, nfiles, hint)
	print('open', app, files, nfiles, hint, #hint)
	if not main_window then
		puss_main_window_open()
		app:add_window(main_window)
	end

	local t = glua.gobject_array_pointer_parse(files, nfiles)
	for i,v in ipairs(t) do
		-- print('open', app, files, nfiles, hint, #hint)
		puss_open_from_gfile(v)
	end
end

function main()
	app = gtk_application_new('puss.org', G_APPLICATION_HANDLES_OPEN)
	app:set_default()
	app:signal_connect('activate', puss_app_activate)
	app:signal_connect('open', puss_app_open)

	local args = g_strv_new(puss._args)
	app:run(args:len(), args)
end

