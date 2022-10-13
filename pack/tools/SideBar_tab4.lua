-------------------------
-- Demo Tab for SideBar
-------------------------
local style = props['style.*.32']
local colorback = style:match('back:(#%x%x%x%x%x%x)')
local colorfore = style:match('fore:(#%x%x%x%x%x%x)') or '#000000'
local panel_width = tonumber(props['sidebar.width']) or 326
local tab4 = gui.panel(panel_width)
local label1
-------------------
local tree = gui.tree(true,false) -- haslines[true], editable[false]
if colorback then tree:set_tree_colour(colorfore,colorback) end
-- tab4:client(tree)
tab4:add(tree,'top',90)
--local itm1 = tree:add_item("item1", nil, 1) -- ("caption" [, parent_item = root][, icon_idx=-1][, sel_icon_idx=-1])
--[[tree:add_item("item2", itm1, 1)
tree:add_item("item3")
local itm4 = tree:add_item("item4")
local itm5 = tree:add_item("item5", itm4,-1)
local itm6 = tree:add_item("item6", itm5,0)
local itm7 = tree:add_item("item7", itm6,0)]]
-- local imgcnt = tree:set_iconlib([[toolbar\cool.dll]]) --path [, small_size = true] return count of loaded icons
-- local imgcnt = tree:set_iconlib([[%windir%\system32\shell32.dll]], false ) 
-- local imgcnt = tree:set_iconlib([[%SystemRoot%\system32\shell32.dll]])
--[[tree:add_item("qwerty", nil, 1)
tree:add_item("qwerty2", nil, 1)
tree:add_item("qwerty3", nil, 1)]]

local imgcnt = tree:set_iconlib([[toolbar\cool.dll]]) --path [, small_size = true] return count of loaded icons
local lib1_itm = tree:add_item("cool icons", nil, 16)
for icon_idx=0, imgcnt-1 do
	tree:add_item("item_"..icon_idx, lib1_itm, icon_idx, icon_idx, icon_idx*icon_idx )
end

-- tree:set_selected_item(itm5)
-- print("test:",tree:tree_item_get_text(itm5))
tree:on_select(function(sel_item)
	label1:set_text('select tree:'.. tree:tree_get_item_text(sel_item))
end)
tree:on_double_click(function(sel_item)
	label1:set_text('db click tree:'.. tostring(tree:tree_get_item_data(sel_item)))
end)

-- tree:set_tree_editable(true)
-- function tree_test_menu() label1:set_text('menu for tree clicked') end --ok
-- tree:context_menu{"test|tree_test_menu"} -- ok
-------------------
local listbox = gui.listbox(77)
tab4:add(listbox, "top", 100)
for i=1,15 do
	listbox:append("line"..i, i*i)
end

-- listbox:insert(idx, "caption"[,data]) -- ok
-- listbox:select(10) -- ok
listbox:remove(2) --ok
-- print("lb1:",listbox:count())
-- listbox:clear() --ok
-- print("lb2:",listbox:count()) -- ok
-- print("select=",listbox:select()) --ok
-- print("line_0_text=",listbox:get_line_text(4))
-- print("line_0_data=",listbox:get_line_data(4))
function list_test_menu() label1:set_text('menu for list clicked') end
listbox:context_menu{"test|list_test_menu"}

tab4:on_command(function(id, state)
	if id==77 then
		--[[
		1 - clicked
		2 - db clicked
		4 - focused
		5 - unfocused
		]]
		if state==2 then
		-- 	print("db clicked list:",listbox:get_line_text(listbox:select()))
			label1:set_text("list db clicked:"..listbox:get_line_text(listbox:select()))
		end
		if state==1 then
			label1:set_text("list clicked:"..listbox:get_line_data(listbox:select()))
		end
	end
end)
-------------------
local test_panel = gui.panel(panel_width)
tab4:add(test_panel, "top", 240)

local btn = gui.button("btn1",11)
test_panel:add(btn,"none")
btn:position(20,30)
btn:size(50,30)

local btn2 = gui.button("btn2",22, true)
test_panel:add(btn2,"none")
btn2:position(80,30)
btn2:size(50,30)

local chbox = gui.checkbox("check3",33, true) -- caption, id, auto
test_panel:add(chbox,"none")
chbox:position(150,35)

local rbtn = gui.radiobutton("radio_1",44, true) -- caption, id, auto
test_panel:add(rbtn,"none")
rbtn:position(20,70)
rbtn:check(true)

local rbtn2 = gui.radiobutton("radio_2",55, true)
test_panel:add(rbtn2,"none")
rbtn2:position(100,70)

label1 = gui.label("StaticText",66)
test_panel:add(label1,"none")
label1:position(20,125)

--[[ toolbar style option
#define TBS_AUTOTICKS           0x0001
#define TBS_VERT                0x0002
#define TBS_HORZ                0x0000
#define TBS_TOP                 0x0004
#define TBS_BOTTOM              0x0000
#define TBS_LEFT                0x0004
#define TBS_RIGHT               0x0000
#define TBS_BOTH                0x0008
#define TBS_NOTICKS             0x0010
#define TBS_ENABLESELRANGE      0x0020
#define TBS_FIXEDLENGTH         0x0040
#define TBS_NOTHUMB             0x0080
#define TBS_TOOLTIPS            0x0100
#define TBS_REVERSED            0x0200
]]
local trbar1 = gui.trackbar(0x20 + 0x200, 77) -- style, id
test_panel:add(trbar1,"none")
trbar1:position(20,100)
trbar1:size(250, 20)
-- trbar1:size(20, 250) -- vert
-- trbar1:range(20,80)
--[[
-- style TBS_ENABLESELRANGE required
local selmin, selmax = trbar1:select() -- get selmin, selmax
trbar1:select(20,80) -- set selection
trbar1:sel_clear() -- clear selection
]]

test_panel:on_scroll( function(id, pos)
	if id==77 then
		label1:set_text("scroll to: " .. pos)
	end
end)

local cbbox = gui.combobox(88) -- id
test_panel:add(cbbox, "top")

for i=1,10 do
	cbbox:cb_append("text_"..i)
end

cbbox:cb_items_h(15) -- ok
cbbox:cb_setcursel(1) -- ok
-- print(cbbox:cb_getcursel(), cbbox:get_text())
cbbox:cb_setcursel(-1) -- clear selection
-- cbbox:cb_clear()

local btn3 = gui.button("Set Explorer", 133, true)
test_panel:add(btn3, "none")
btn3:position(20,150)
btn3:size(100,30)

local btn4 = gui.button("Set editable", 134, true)
test_panel:add(btn4, "none")
btn4:position(20,185)
btn4:size(100,30)

local tree_explorer = false
local tree_editable = false
test_panel:on_command(function(id, state)
-- 	print(string.format("command from [%d], state [%d]", id, state))
	if id==88 then -- combobox
		if state == 1 then
-- 			print(cbbox:get_text(), state)
			label1:set_text("cbox_text:"..cbbox:get_text())
		end
	end
	
	if id==44 then
		label1:set_text('radio_1_clicked')
	end
	
	if id==55 then
		label1:set_text('radio_2_clicked')
	end
	
	if id==11 then
		label1:set_text('btn_1_clicked')
		trbar1:select(30,60)
		--trbar1:set_pos(trbar1:get_pos()+5)
	-- 	print(">>",chbox:state(),chbox:check(),"<<")
	-- 	chbox:check( not chbox:check() )
		--rbtn:check( not rbtn:check() )
	end
	
	if id==22 then
		label1:set_text('btn_2_clicked')
		trbar1:sel_clear()
-- 		print(trbar1:select())
		--print('pos = ',trbar1:get_pos())
	end
	
	if id==33 then
		label1:set_text('checkbox:'..(chbox:check() and 'on' or 'off'))
	end
	
	if id==133 then
		tree_explorer = not tree_explorer
		tree:set_theme(tree_explorer)
		btn3:set_text(tree_explorer and "Set Normal" or "Set Explorer")
	end
	
	if id==134 then
		tree_editable = not tree_editable
		tree:set_tree_editable(tree_editable)
		btn4:set_text(tree_editable and "Set normal" or "Set editable")
	end
	
	return true
end)

function panel_test_menu() print('panel ok') end
test_panel:context_menu{"Скрыть панель|SideBar_ShowHide"}
-------------------------
return {caption="Test", wnd = tab4, icon_idx = 37}