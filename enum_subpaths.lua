--[[
–рекурсивный обход папок в удалением по маске
]]
require "lfs"
local mask = "Intermediate"
function main(dir)
-- 	print(dir)
	if dir:find(mask) then
		print('remove dir:'.. dir)
		os.execute("rd /q /s \""..dir.."\\\"")
		return
	end
	local dirs = gui.files(dir.."\\*",true)
	for k, v in pairs(dirs) do
		main(dir.."\\"..v)
	end
end

local cur_path = debug.getinfo(1,"S").short_src
cur_path = cur_path:gsub("\\[^\\]+$","") -- cut filename
cur_path = cur_path:gsub("\\[^\\]+$","") -- cut subdir
main(cur_path)
mask=".vs"
main(cur_path)
os.execute("del /q /s *.bak *.log *.pdb *.lib *.bsc *.obj *.iobj *.ipdb *.tlog *.recipe *.idb *.ilk *.lastbuildstate *.exp")
