-------------------------
-- FileManager and Favorites Tab for SideBar
-------------------------
local panel_width = tonumber(props['sidebar.width']) or 326
local tab0 = gui.panel(panel_width)
local memo_path = gui.memo()
tab0:add(memo_path, "top", 22)
local style = props['style.*.32']
local colorback = style:match('back:(#%x%x%x%x%x%x)')
local colorfore = style:match('fore:(#%x%x%x%x%x%x)') or '#000000'
if colorback then memo_path:set_memo_colour('', colorback) end

local win_height = tonumber(props['position.height']) or 600
local list_dir_height = math.floor(win_height/3)
if list_dir_height <= 0 then list_dir_height = 200 end
local list_favorites = gui.list(true)
list_favorites:add_column("Избранное", panel_width)
tab0:add(list_favorites, "bottom", list_dir_height)
if colorback then list_favorites:set_list_colour(colorfore,colorback) end

local list_dir = gui.list(false, false) -- MultiColumn = false, SingleSelect = true
list_dir:set_theme(true)
tab0:client(list_dir)
if colorback then list_dir:set_list_colour(colorfore,colorback) end
----------------------------------------------------------
-- memo_path   Path and Mask
----------------------------------------------------------
local current_path = ''
local file_mask = '*.*'

local function FileMan_ShowPath()
	local rtf = [[{\rtf{\fonttbl{\f0\fcharset204 Helv;}}{\colortbl;\red0\green0\blue255;\red255\green0\blue0;}\f0\fs16]]
	local path = '\\cf1'..current_path:gsub('\\', '\\\\')
	local mask = '\\cf2'..file_mask..'}'
	memo_path:set_text(rtf..path..mask)
end

memo_path:on_key(function(key)
	if key == 13 then
		local new_path = memo_path:get_text()
		if new_path ~= '' then
			new_path = new_path:match('[^*]+')..'\\'
			local is_folder = gui.files(new_path..'*', true)
			if is_folder then
				current_path = new_path
			end
		end
		FileMan_ListFILL()
		return true
	end
end)

----------------------------------------------------------
-- Show Current Colour
----------------------------------------------------------
local function SetColour(colour)
	if colour:match('%x%x%x%x%x%x') then
		-- Set colour's value HEX
		memo_path:set_memo_colour("", "#"..colour)
	else
		-- Set default colour
		local def_bg = editor.StyleBack[32]
		local b = math.floor(def_bg / 65536)
		local g = math.floor((def_bg - b*65536) / 256)
		local r = def_bg - b*65536 - g*256
		local rgb_hex = string.format('#%2X%2X%2X', r, g, b)
		memo_path:set_memo_colour("", rgb_hex)
	end
end

props["dwell.period"] = 50
AddEventHandler("OnDwellStart", function(pos, cur_word)
	if pos ~= 0 then
		SetColour(cur_word)
	end
end)

local cur_word_old = ""
AddEventHandler("OnKey", function()
	if editor.Focus then
		local cur_word = GetCurrentWord()
		if cur_word ~= cur_word_old then
			SetColour(cur_word)
			cur_word_old = cur_word
		end
	end
end)

--[[function MemoCopy()
	editor:CopyText(memo_path:get_text())
end

memo_path:context_menu{
	'Копировать|MemoCopy',
}]]
----------------------------------------------------------
-- Common functions
----------------------------------------------------------
local function ReplaceWithoutCase(text, s_find, s_rep)
	s_find = string.gsub(s_find, '.', function(ch)
		if ch:match('%d') then return ch end -- avoid %d pattern
		local c,C = ch:lower(), ch:upper()
		return c~=C and '['..c..C..']' -- i.e. ch was a letter
				or '%'..ch -- not a letter, so escape it
	end)
	return string.gsub(text, s_find, s_rep)
end

----------------------------------------------------------
-- tab0:list_dir   File Manager
----------------------------------------------------------
function FileMan_ListFILL()
	if current_path == '' then return end
	local folders = gui.files(current_path..'*', true)
	if not folders then return end
	local table_folders = {}
	for i, d in ipairs(folders) do
		table_folders[i] = {'['..d..']', {d,'d'}}
	end
	table.sort(table_folders, function(a, b) return a[1]:lower() < b[1]:lower() end)
	local files = gui.files(current_path..file_mask)
	local table_files = {}
	if files then
		for i, filename in ipairs(files) do
			table_files[i] = {filename, {filename}}
		end
	end
	table.sort(table_files, function(a, b) return a[1]:lower() < b[1]:lower() end)

	list_dir:clear()
	list_dir:add_item ('[..]', {'..','d'})
	for i = 1, #table_folders do
		list_dir:add_item(table_folders[i][1], table_folders[i][2])
	end
	for i = 1, #table_files do
		list_dir:add_item(table_files[i][1], table_files[i][2])
	end
	list_dir:set_selected_item(0)
	FileMan_ShowPath()
	list_dir:autosize(0)
end

local function FileMan_GetSelectedItem(idx)
	if idx == nil then idx = list_dir:get_selected_item() end
	if idx == -1 then return '' end
	local data = list_dir:get_item_data(idx)
	local dir_or_file = data[1]
	local attr = data[2]
	return dir_or_file, attr
end

function FileMan_ChangeDir()
	local newPath = gui.select_dir_dlg('Выберите папку', current_path)
	if newPath == nil then return end
	if newPath:match('[\\/]$') then
		current_path = newPath
	else
		current_path = newPath..'\\'
	end
	FileMan_ListFILL()
end

function FileMan_MaskAllFiles()
	file_mask = '*.*'
	FileMan_ListFILL()
end

function FileMan_MaskOnlyCurrentExt()
	local filename, attr = FileMan_GetSelectedItem()
	if filename == '' then return end
	if attr == 'd' then return end
	file_mask = '*.'..filename:gsub('.+%.','')
	FileMan_ListFILL()
end

function FileMan_FileCopy()
	local filename = FileMan_GetSelectedItem()
	if filename == '' or filename == '..' then return end
	local path_destination = gui.select_dir_dlg("Копировать в...")
	if path_destination == nil then return end
	os_copy(current_path..filename, path_destination..'\\'..filename)
	FileMan_ListFILL()
end

function FileMan_FileMove()
	local filename = FileMan_GetSelectedItem()
	if filename == '' or filename == '..' then return end
	local path_destination = gui.select_dir_dlg("Переместить...")
	if path_destination == nil then return end
	os.rename(current_path..filename, path_destination..'\\'..filename)
	FileMan_ListFILL()
end

function FileMan_FileRename()
	local filename = FileMan_GetSelectedItem()
	if filename == '' or filename == '..' then return end
	local filename_new = shell.inputbox("Переименовать", "Новое имя файла:", filename, function(name) return not name:match('[\\/:|*?"<>]') end)
	if filename_new == nil then return end
	if filename_new ~= '' and filename_new ~= filename then
		os.rename(current_path..filename, current_path..filename_new)
		FileMan_ListFILL()
	end
end

function FileMan_FileDelete()
	local filename, attr = FileMan_GetSelectedItem()
	if filename == '' then return end
	if attr == 'd' then return end
	if shell.msgbox("Уверены, что хотите УДАЛИТЬ этот файл?\n"..filename, "DELETE", 4+256) == 6 then
	-- if gui.message("Are you sure you want to DELETE this file?\n"..filename, "query") then
		os.remove(current_path..filename)
		FileMan_ListFILL()
	end
end

local function FileMan_FileExecWithSciTE(cmd, mode)
	local p0 = props["command.0.*"]
	local p1 = props["command.mode.0.*"]
	local p2 = props["command.name.0.*"]
	props["command.name.0.*"] = 'tmp'
	props["command.0.*"] = cmd
	if mode == nil then mode = 'console' end
	props["command.mode.0.*"] = 'subsystem:'..mode..',savebefore:no'
	scite.MenuCommand(9000)
	props["command.0.*"] = p0
	props["command.mode.0.*"] = p1
	props["command.name.0.*"] = p2
end

function FileMan_FileExec(params)
	if params == nil then params = '' end
	local filename = FileMan_GetSelectedItem()
	if filename == '' then return end
	local file_ext = filename:match("[^.]+$")
	if file_ext == nil then return end
	file_ext = '%*%.'..string.lower(file_ext)
	
	local function CommandBuild(lng)
		local cmd = props['command.build.$(file.patterns.'..lng..')']
		cmd = cmd:gsub(props["FilePath"], current_path..filename)
		return cmd
	end
	-- Lua
	if string.match(props['file.patterns.lua'], file_ext) then
		dostring(params)
		dofile(current_path..filename)
	-- Batch
	elseif string.match(props['file.patterns.batch'], file_ext) then
		FileMan_FileExecWithSciTE(CommandBuild('batch'))
		return
	-- WSH
	elseif string.match(props['file.patterns.wscript']..props['file.patterns.wsh'], file_ext) then
		FileMan_FileExecWithSciTE(CommandBuild('wscript'))
	-- Other
	else
		local ret, descr = shell.exec(current_path..filename..params)
		if not ret then
			print (">Exec: "..filename)
			print ("Error: "..descr)
		end
	end
end

function FileMan_FileExecWithParams()
	if scite.ShowParametersDialog('Выполнение "'..FileMan_GetSelectedItem()..'". Установите папаметры:') then
		local params = ''
		for p = 1, 4 do
			local ps = props[tostring(p)]
			if ps ~= '' then
				params = params..' '..ps
			end
		end
		FileMan_FileExec(params)
	end
end

local function OpenFile(filename)
	if filename:match(".session$") ~= nil then
		filename = filename:gsub('\\','\\\\')
		scite.Perform ("loadsession:"..filename)
	else
		scite.Open(filename)
	end
	gui.pass_focus()
end

local function FileMan_OpenItem()
	local dir_or_file, attr = FileMan_GetSelectedItem()
	if dir_or_file == '' then return end
	if attr == 'd' then
		gui.chdir(dir_or_file)
		if dir_or_file == '..' then
			local new_path = current_path:gsub('(.*\\).*\\$', '%1')
			if not gui.files(new_path..'*',true) then return end
			current_path = new_path
		else
			current_path = current_path..dir_or_file..'\\'
		end
		FileMan_ListFILL()
	else
		OpenFile(current_path..dir_or_file)
	end
end

function FileMan_OpenSelectedItems()
	local si = list_dir:get_selected_items()
	for _,i in ipairs(si) do
		local dir_or_file, attr = FileMan_GetSelectedItem(i)
		if attr ~= 'd' then
			OpenFile(current_path..dir_or_file)
		end
	end
end

function FileMan_SelectCurrentFile()
	local path = props['FileDir']
	if path == '' then return end
	current_path = path:gsub('\\$','')..'\\'
	file_mask = '*.*'
	FileMan_ListFILL()
	local fn = props['FileNameExt']
	local cnt = list_dir:count()
	list_dir:set_selected_item(-1) -- clear selection
	for i=1,cnt-1 do
		if fn == list_dir:get_item_text(i) then list_dir:set_selected_item(i) return end
	end
end

function FileMan_Explore()
	local dir_or_file, attr = FileMan_GetSelectedItem()
	local cmd = ""
	if attr == 'd' then
		if dir_or_file==".." then dir_or_file = "" else cmd = "\\" end
	end
	shell.exec("explorer /select,"..current_path..dir_or_file..cmd)
end

list_dir:on_double_click(function()
	FileMan_OpenItem()
end)

list_dir:on_key(function(key)
	if key == 13 then -- Enter
		if list_dir:selected_count() > 1 then
			FileMan_OpenSelectedItems()
		else
			FileMan_OpenItem()
		end
	elseif key == 8 then -- BackSpace
		list_dir:set_selected_item(0)
		FileMan_OpenItem()
	elseif key == 46 then -- Delete
		FileMan_FileDelete()
	elseif key == 45 then -- Insert
		Favorites_AddFile()
	end
end)

list_dir:context_menu {
	'Выбрать папку...|FileMan_ChangeDir',
	'Показать все файлы|FileMan_MaskAllFiles',
	'C расширением выделенного файла|FileMan_MaskOnlyCurrentExt',
	'Показать в Проводнике|FileMan_Explore',
	'Перейти к текущему файлу|FileMan_SelectCurrentFile',
	'', -- separator
	'Открыть в SciTE|FileMan_OpenSelectedItems',
	'Выполнить|FileMan_FileExec',
	'Выполнить с параметрами|FileMan_FileExecWithParams',
	'', -- separator
	'Копировать в...|FileMan_FileCopy',
	'Переместить в...|FileMan_FileMove',
	'Переименовать|FileMan_FileRename',
	'Удалить файл\tDel|FileMan_FileDelete',
	'', -- separator
	'Добавить в Избранное\tIns|Favorites_AddFile',
}

----------------------------------------------------------
-- list_favorites   Favorites
----------------------------------------------------------
local favorites_filename = props['SciteUserHome']..'\\favorites.lst'
local list_fav_table = {}

local function Favorites_ListFILL()
	list_favorites:clear()
	table.sort(list_fav_table,
		function(a, b)
			local function IsSession(filepath)
				return filepath:gsub('^.*%.',''):upper() == 'SESSION'
			end
			local isAses = IsSession(a[1]:lower())
			local isBses = IsSession(b[1]:lower())
			if (isAses and isBses) or not (isAses or isBses) then
				return a[1]:lower() < b[1]:lower()
			else
				return isAses
			end
		end
	)
	for _, file in ipairs(list_fav_table) do
		list_favorites:add_item(file[1], file[2])
	end
end

local function Favorites_OpenList()
	local favorites_file = io.open(favorites_filename)
	if favorites_file then
		for fpath in favorites_file:lines() do
			if fpath ~= '' then
				fpath = ReplaceWithoutCase(fpath, '$(SciteDefaultHome)', props['SciteDefaultHome'])
				local fname = fpath:gsub('.+\\','')
				if fname == '' then fname = fpath:gsub('.+\\(.-)\\',' [%1]') end
				list_fav_table[#list_fav_table+1] = {fname, fpath}
			end
		end
		favorites_file:close()
	end
	Favorites_ListFILL()
end
Favorites_OpenList()

local function Favorites_SaveList()
	if pcall(io.output, favorites_filename) then
		for _, file in ipairs(list_fav_table) do
			io.write(ReplaceWithoutCase(file[2], props['SciteDefaultHome'], '$(SciteDefaultHome)')..'\n')
		end
		io.close()
	end
end

local function Favorites_ADD(fname, fpath)
	for _, t in ipairs(list_fav_table) do
		if t[1] == fname and t[2] == fpath then return end
	end
	list_fav_table[#list_fav_table+1] = {fname, fpath}
end

function Favorites_AddFile()
	local fname, attr = FileMan_GetSelectedItem()
	if fname == '' then return end
	local fpath = current_path..fname
	if attr == 'd' then
		fname = ' ['..fname..']'
		fpath = fpath:gsub('\\%.%.$', '')..'\\'
	end
	Favorites_ADD(fname, fpath)
	Favorites_ListFILL()
	Favorites_SaveList()
end

function Favorites_AddCurrentBuffer()
	Favorites_ADD(props['FileNameExt'], props['FilePath'])
	Favorites_ListFILL()
	Favorites_SaveList()
end

function Favorites_DeleteItem()
	local idx = list_favorites:get_selected_item()
	if idx == -1 then return end
	list_favorites:delete_item(idx)
	table.remove (list_fav_table, idx+1)
	Favorites_SaveList()
end

local function Favorites_OpenFile()
	local idx = list_favorites:get_selected_item()
	if idx == -1 then return end
	local fname = list_favorites:get_item_data(idx)
	if fname:match('\\$') then
		gui.chdir(fname)
		current_path = fname
		FileMan_ListFILL()
	else
		OpenFile(fname)
	end
end

local function Favorites_ShowFilePath()
	local sel_item = list_favorites:get_selected_item()
	if sel_item == -1 then return end
	local expansion = list_favorites:get_item_data(sel_item)
	editor:CallTipCancel()
	editor:CallTipShow(-2, expansion)
end

list_favorites:on_select(function()
	Favorites_ShowFilePath()
end)

list_favorites:on_double_click(function()
	Favorites_OpenFile()
end)

list_favorites:on_key(function(key)
	if key == 13 then -- Enter
		Favorites_OpenFile()
	elseif key == 46 then -- Delete
		Favorites_DeleteItem()
	end
end)

list_favorites:context_menu {
	'Добавить текущий файл|Favorites_AddCurrentBuffer',
	'Удалить\tDel|Favorites_DeleteItem',
}

local function OnSwitch()
	if tab0:bounds() then -- visible FileMan
		local path = props['FileDir']
		if path == '' then return end
		current_path = path:gsub('\\$','')..'\\'
		FileMan_ListFILL()
		gui.pass_focus()
	end
end

AddEventHandler("OnSwitchFile", OnSwitch)
AddEventHandler("OnOpen", OnSwitch)
AddEventHandler("OnSave", OnSwitch)
event('sb_tab_selected'):register(function(e, tab_id) if tab_id == 0 then OnSwitch() end end)
-------------------------
return {caption="Проводник", wnd = tab0, icon_idx = 57}