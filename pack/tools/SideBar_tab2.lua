-------------------------
-- ASCII Tab for SideBar
-------------------------
local panel_width = tonumber(props['sidebar.width']) or 326
local tab2 = gui.panel(panel_width)
local list_abbrev = gui.list(true)
list_abbrev:add_column("Сокращения", 60)
list_abbrev:add_column("Расширение", 600)
tab2:client(list_abbrev)
local style = props['style.*.32']
local colorback = style:match('back:(#%x%x%x%x%x%x)')
local colorfore = style:match('fore:(#%x%x%x%x%x%x)') or '#000000'
if colorback then list_abbrev:set_list_colour(colorfore,colorback) end
-- Переключатель способа предпросмотра аббревиатур: true = calltip, false = annotation
local Abbreviations_USECALLTIPS = tonumber(props['sidebar.abbrev.calltip']) == 1
-------------------------

function Abbreviations_ListFILL()
	list_abbrev:clear()
	local abbrev_filename = props['AbbrevPath']
	local abbr_table = ReadAbbrevFile(abbrev_filename)
	if not abbr_table then return end
	for i,v in ipairs(abbr_table) do
		list_abbrev:add_item({v.abbr, v.exp:gsub('\t','\\t'):gsub('%%%%','%%')}, v.exp)
	end
	list_abbrev:autosize(0)
end

local Abbreviations_HideExpansion = Abbreviations_USECALLTIPS and
function() editor:CallTipCancel() end or
function() editor:AnnotationClearAll() end

local scite_InsertAbbreviation = scite_InsertAbbreviation or scite.InsertAbbreviation
local function Abbreviations_InsertExpansion()
	local sel_item = list_abbrev:get_selected_item()
	if sel_item == -1 then return end
	local expansion = list_abbrev:get_item_data(sel_item)
	scite_InsertAbbreviation(expansion)
	gui.pass_focus() --don't need to call Abbreviations_HideExpansion(): on_focus will do
end

local function Abbreviations_ShowExpansion()
	local sel_item = list_abbrev:get_selected_item()
	if sel_item == -1 then return end
	local expansion = list_abbrev:get_item_data(sel_item)
	expansion = expansion:gsub('\\\\','\4'):gsub('\\r','\r'):gsub('(\\n','\n'):gsub('\\t','\t'):gsub('\4','\\'):gsub('%%%%','%%')
	local cp = editor:codepage()
	if cp ~= 65001 then expansion = expansion:from_utf8(cp) end

	local cur_pos = editor.CurrentPos
	editor:GotoPos(cur_pos-1)
	editor:GotoPos(cur_pos)
	if Abbreviations_USECALLTIPS then
		editor:CallTipCancel()
		editor:CallTipShow(cur_pos, expansion)
	else
		editor:AnnotationClearAll()
		editor.AnnotationVisible = ANNOTATION_BOXED
		local linenr = editor:LineFromPosition(cur_pos)
		editor.AnnotationStyle[linenr] = 255 -- номер стиля, в котором вы задали параметры для аннотаций
		editor.AnnotationText[linenr] = expansion:gsub('\t', '    ')
	end
end

list_abbrev:on_double_click( Abbreviations_InsertExpansion )

list_abbrev:on_select( Abbreviations_ShowExpansion )

list_abbrev:on_key(function(key)
	if key == 13 then -- Enter
		Abbreviations_InsertExpansion()
	elseif key == 27 then -- ESC
		Abbreviations_HideExpansion()
	end
end)

local function OnSwitch()
	if tab2:bounds() then -- visible Abbrev
		Abbreviations_ListFILL()
		gui.pass_focus()
	end
end

AddEventHandler("OnSwitchFile", OnSwitch)
AddEventHandler("OnOpen", OnSwitch)
AddEventHandler("OnSave", OnSwitch)
event('sb_tab_selected'):register(function(e, tab_id) if tab_id == 2 then OnSwitch() end end)
-------------------------
return {caption="Сокращения", wnd = tab2, icon_idx = 22}