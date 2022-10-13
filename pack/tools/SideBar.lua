--[[--------------------------------------------------
SideBar.lua
Authors: Frank Wunderlich, mozers™, VladVRO, frs, BioInfo, Tymur Gubayev, ur4ltz, nicksabaka
Version 1.28.2
------------------------------------------------------
  Note: Require gui.dll
               lpeg.dll
              shell.dll
             COMMON.lua (function GetCurrentWord)
             eventmanager.lua (function AddEventHandler)

  Connection:
   In file SciTEStartup.lua add a line:
      dofile (props["SciteDefaultHome"].."\\tools\\SideBar.lua")

   Set in a file .properties:
    command.checked.17.*=$(sidebar.show)
    command.name.17.*=SideBar
    command.17.*=SideBar_ShowHide
    command.mode.17.*=subsystem:lua,savebefore:no

    # Set window(1) or pane(0) (default = 0)
    sidebar.win=1

    # Set show(1) or hide(0) to SciTE start
    sidebar.show=1

    # Set sidebar width and position
    sidebar.width=250 (default = 230)
    sidebar.position=left or right (default = right)

    # Set default settings for Functions/Procedures List
    sidebar.functions.flags=1
    sidebar.functions.params=1

    # Set abbrev preview 1-calltip, 0-annotation(default)
    sidebar.abbrev.calltip=1

    # Set annotation style
    style.*.255=fore:#808080,back:#FEFEFE
--]]--------------------------------------------------
require 'gui'
require 'lpeg'
require 'shell'

-- you can choose to make SideBar a stand-alone window
local win = tonumber(props['sidebar.win']) == 1

-- отображение флагов/параметров по умолчанию:
local _show_flags = tonumber(props['sidebar.functions.flags']) == 1
local _show_params = tonumber(props['sidebar.functions.params']) == 1

local tab_index = 0
local panel_width = tonumber(props['sidebar.width']) or 326
local win_height = tonumber(props['position.height']) or 600
local sidebar_position = (props['sidebar.position'] == 'left') and 'left' or 'right'
props['sidebar.position'] = sidebar_position

----------------------------------------------------------
-- Create panels
----------------------------------------------------------
local win_parent
if win then
	win_parent = gui.window "Side Bar"
	win_parent:size(panel_width + 24, 600)
	win_parent:on_close(function() props['sidebar.show']=0 end)
else
	win_parent = gui.panel(panel_width)
end
----------------------------------------------------------
local tabs = gui.tabbar(win_parent)
if shell.fileexists(props['sidebar.iconed.tabs']) then
	tabs:set_iconlib(props['sidebar.iconed.tabs'])
end

local tab_cnt = 0
while true do
	local tabs_path = props["SciteDefaultHome"].."\\tools\\SideBar_tab"..tab_cnt..".lua"
	local state, res = pcall(dofile, tabs_path)
	if not state or type(res)~='table' then break end
	tabs:add_tab(res.caption, res.wnd, res.icon_idx)
	win_parent:client(res.wnd)
	tab_cnt = tab_cnt+1
end
----------------------------------------------------------
-- Events
----------------------------------------------------------
tabs:on_select(function(ind)
	props['sidebar.active.tab'] = ind
	event('sb_tab_selected')(ind)
end)

--- Функции показывающие/прячущие боковую панель
function OnSwitchTab()
	local tab_idx = tonumber(props['sidebar.active.tab'])
	if tab_idx then
		tabs:select_tab(tab_idx)
	end
end

local SideBar_Show = win and function()
	win_parent:show()
	props['sidebar.show'] = 1
	OnSwitchTab()
end
or function()
	gui.set_panel(win_parent, sidebar_position)
	props['sidebar.show'] = 1
	OnSwitchTab()
end

--- Переключает отображение боковой панели
function SideBar_ShowHide()
	if tonumber(props['sidebar.show']) == 1 then
		props['sidebar.show'] = 0
		if win then win_parent:hide() else gui.set_panel() end
	else
		SideBar_Show()
	end
end

if not win then
	function MoveSideBarLeft()
		props['sidebar.position'] = 'left'
		gui.set_panel()
		gui.set_panel(win_parent, 'left')
		tabs:context_menu {
			'Панель справа|MoveSideBarRight'
		}
	end

	function MoveSideBarRight()
		props['sidebar.position'] = 'right' 
		gui.set_panel()
		gui.set_panel(win_parent, 'right')
		tabs:context_menu {
			'Панель слева|MoveSideBarLeft'
		}
	end
	
	props['sidebar.position'] = sidebar_position
	
	if sidebar_position == 'left' then
		tabs:context_menu {
			'Панель справа|MoveSideBarRight',
		}
	else
		tabs:context_menu {
			'Панель слева|MoveSideBarLeft',
		}
	end
end

-- now show SideBar:
if tonumber(props['sidebar.show'])==1 then
	AddEventHandler("OnInit", SideBar_Show, 'RunOnce')
end
