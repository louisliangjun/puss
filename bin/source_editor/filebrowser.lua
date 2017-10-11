print('filebrowser')

local volume_monitor = g_volume_monitor_get()
local icon_theme = gtk_icon_theme_get_default()

local icon_size = 16
local folder_icon = gtk_icon_theme_load_icon(icon_theme, 'folder', icon_size,  0);
local file_icon = gtk_icon_theme_load_icon(icon_theme, 'text-x-generic', icon_size,  0);
print(volume_monitor, icon_theme, icon_size, folder_icon, file_iconm)

local file_tree_store

local function fill_subs(dir, parent_iter)
	local enumerator, err = g_file_enumerate_children(dir
					, 'standard::name,standard::type,standard::icon'
					, G_FILE_QUERY_INFO_NONE
					)
	if not enumerator then
		print(err)
		return
	end

	local dir_infos = {}
	local file_infos = {}
	while true do
		local fileinfo = g_file_enumerator_next_file(enumerator)
		if not fileinfo then break end

		local tname = g_file_info_get_name(fileinfo)
		if tname and tname:sub(1,1)~='.' then
			if g_file_info_get_file_type(fileinfo)==G_FILE_TYPE_DIRECTORY then
				table.insert(dir_infos, fileinfo)
			else
				table.insert(file_infos, fileinfo)
			end
		end
	end
	-- TODO : g_file_enumerator_close(enumerator, 0, 0);
	--dir_infos = g_list_sort(dir_infos, (GCompareFunc)fileinfo_sort_cmp);
	--file_infos = g_list_sort(file_infos, (GCompareFunc)fileinfo_sort_cmp);

	local iter = gtk_tree_iter_new()
	local subiter = gtk_tree_iter_new()

	for _, fileinfo in ipairs(dir_infos) do
		local name = g_file_info_get_name(fileinfo)
		local icon = g_file_info_get_icon(fileinfo)
		local info = icon and gtk_icon_theme_lookup_by_gicon(icon_theme, icon, icon_size, 0)
		local pbuf = info and gtk_icon_info_load_icon(info)
		local subfile = g_file_get_child(dir, name)

		gtk_tree_store_append(file_tree_store, iter, parent_iter);
		gtk_tree_store_set_value( file_tree_store, iter, 0, pbuf or folder_icon)
		gtk_tree_store_set_value( file_tree_store, iter, 1, name )
		gtk_tree_store_set_value( file_tree_store, iter, 2, subfile )

		if subfile then
			-- append [loading] node
			gtk_tree_store_append(file_tree_store, subiter, iter);
			gtk_tree_store_set_value( file_tree_store, subiter, 2, subfile )
		end
	end

	for _, fileinfo in ipairs(file_infos) do
		local name = g_file_info_get_name(fileinfo)
		local icon = g_file_info_get_icon(fileinfo)
		local info = icon and gtk_icon_theme_lookup_by_gicon(icon_theme, icon, icon_size, 0)
		local pbuf = info and gtk_icon_info_load_icon(info)

		gtk_tree_store_append(file_tree_store, iter, parent_iter);
		gtk_tree_store_set_value( file_tree_store, iter, 0, pbuf or file_icon)
		gtk_tree_store_set_value( file_tree_store, iter, 1, name )
	end
end

local function fill_root()
	local mounts = g_volume_monitor_get_mounts(volume_monitor)
	local iter = gtk_tree_iter_new()
	local subiter = gtk_tree_iter_new()
	for _, mnt in ipairs(mounts) do
		local icon, info, pbuf, name, file
		icon = g_mount_get_icon(mnt)
		info = icon and gtk_icon_theme_lookup_by_gicon(icon_theme, icon, icon_size, 0)
		pbuf = info and gtk_icon_info_load_icon(info)
		name = g_mount_get_name(mnt)
		file = g_mount_get_root(mnt)

		-- print(_, mnt, icon, info, pbuf, name, file)
		gtk_tree_store_prepend(file_tree_store, iter, 0)
		gtk_tree_store_set_value( file_tree_store, iter, 0, pbuf or folder_icon)
		gtk_tree_store_set_value( file_tree_store, iter, 1, name )
		gtk_tree_store_set_value( file_tree_store, iter, 2, file )

		-- append [loading] node
		if file then
			gtk_tree_store_prepend(file_tree_store, subiter, iter);
			gtk_tree_store_set_value( file_tree_store, subiter, 2, file )
		end
	end
end

local function ensure_fill_subs(parent_iter)
	local iter = gtk_tree_iter_new()

	-- check [loading] node
	if not gtk_tree_model_iter_nth_child(file_tree_store, iter, parent_iter, 0) then return end

	if gtk_tree_model_get_value(file_tree_store, iter, 1) then return end

	-- get dir GFile
	local dir = gtk_tree_model_get_value(file_tree_store, parent_iter, 2)
	if not dir then return end

	fill_subs(dir, parent_iter)

	-- remove [loading] node
	gtk_tree_store_remove(file_tree_store, iter)
end

-- fill_root()
local puss = puss_load_module('puss')

local builder = load_glade()
local panel = builder:get_object('filebrowser_panel')
file_tree_store = builder:get_object('file_tree_store')

local signal_handles =
	{ filebrowser_view_cb_keypress = function()
	  end
	, filebrowser_cb_row_activated = function(tree_view, path)
		local iter = gtk_tree_iter_new()
		if not gtk_tree_model_get_iter(file_tree_store, iter, path) then return end
		local name = gtk_tree_model_get_value(file_tree_store, iter, 1)
		local dir = gtk_tree_model_get_value(file_tree_store, iter, 2)
		if not dir then
			-- open file use puss
			local pth, filename
			local parent_iter = gtk_tree_iter_new()
			if gtk_tree_model_iter_parent(file_tree_store, parent_iter, iter) then
				dir = gtk_tree_model_get_value(file_tree_store, parent_iter, 2)
				if not dir then return end
				pth = g_file_get_path(dir)
			else
				--[=[
				#ifdef G_OS_WIN32
							return;
				#else
							pth = g_strdup("/");
				#endif
				--]=]
				return
			end

			local filename = string.format('%s/%s', pth, name)
			puss.open_file(name, filename)
		elseif gtk_tree_view_row_expanded(tree_view, path) then
			gtk_tree_view_collapse_row(tree_view, path)
		else
			gtk_tree_view_expand_to_path(tree_view, path)
		end
	  end
	, filebrowser_cb_row_collapsed = function()
	  end
	, filebrowser_cb_row_expanded = function(tree_view, iter, path)
		ensure_fill_subs(iter)
	  end
	, filebrowser_cb_refresh_button_clicked = function()
	  end
	}

builder:connect_signals(signal_handles)

fill_root()

local label = gtk_label_new()
label.label = 'FileBrowser'
label.visible = true

panel:show_all()

local container = puss.main_builder:get_object('left_panel')
container:append_page(panel, label)
