-------------------------
-- Function and Bookmarks Tab for SideBar
-------------------------
local win_height = tonumber(props['position.height']) or 600
local panel_width = tonumber(props['sidebar.width']) or 326
local style = props['style.*.32']
local colorback = style:match('back:(#%x%x%x%x%x%x)')
local colorfore = style:match('fore:(#%x%x%x%x%x%x)') or '#000000'
local tab1 = gui.panel(panel_width)

local list_func_height = math.floor(win_height/3)
if list_func_height <= 0 then list_func_height = 200 end
local list_bookmarks = gui.list(true)
list_bookmarks:add_column("#", 24)
list_bookmarks:add_column("Закладки", panel_width)
tab1:add(list_bookmarks, "bottom", list_func_height)
if colorback then list_bookmarks:set_list_colour(colorfore,colorback) end

local list_func = gui.list(true)
list_func:add_column("Функции/Процедуры", 600)
tab1:client(list_func)
if colorback then list_func:set_list_colour(colorfore,colorback) end

----------------------------------------------------------

local function ShowCompactedLine(line_num)
	local function GetFoldLine(ln)
		while editor.FoldExpanded[ln] do ln = ln-1 end
		return ln
	end
	while not editor.LineVisible[line_num] do
		local x = GetFoldLine(line_num)
		editor:ToggleFold(x)
		line_num = x - 1
	end
end

----------------------------------------------------------
-- list_func   Functions/Procedures
----------------------------------------------------------
local table_functions = {}
-- 1 - function names
-- 2 - line number
-- 3 - function paramaters with parentheses
local _sort = 'order'
local _backjumppos -- store position if jumping

local Lang2lpeg = {}
do
	local P, V, Cg, Ct, Cc, S, R, C, Carg, Cf, Cb, Cp, Cmt = lpeg.P, lpeg.V, lpeg.Cg, lpeg.Ct, lpeg.Cc, lpeg.S, lpeg.R, lpeg.C, lpeg.Carg, lpeg.Cf, lpeg.Cb, lpeg.Cp, lpeg.Cmt

	--@todo: переписать с использованием lpeg.Cf
	local function AnyCase(str)
		local res = P'' --empty pattern to start with
		local ch, CH
		for i = 1, #str do
			ch = str:sub(i,i):lower()
			CH = ch:upper()
			res = res * S(CH..ch)
		end
		assert(res:match(str))
		return res
	end

	local PosToLine = function (pos) return editor:LineFromPosition(pos-1) end

--v------- common patterns -------v--
	-- basics
	local EOF = P(-1)
	local BOF = P(function(s,i) return (i==1) and 1 end)
	local NL = P"\n"+P"\r\n"+P"\r"-- + P"\f" -- pattern matching newline, platform-specific. \f = page break marker
	local AZ = R('AZ','az')+"_"
	local N = R'09'
	local ANY =  P(1)
	local ESCANY = P'\\'*ANY + ANY
	local SINGLESPACE = S'\n \t\r\f'
	local SPACE = SINGLESPACE^1

	-- simple tokens
	local IDENTIFIER = AZ * (AZ+N)^0 -- simple identifier, without separators

	local Str1 = P'"' * ( ESCANY - (S'"'+NL) )^0 * (P'"' + NL)--NL == error'unfinished string')
	local Str2 = P"'" * ( ESCANY - (S"'"+NL) )^0 * (P"'" + NL)--NL == error'unfinished string')
	local STRING = Str1 + Str2

	-- c-like-comments
	local line_comment = '//' * (ESCANY - NL)^0*NL
	local block_comment = '/*' * (ESCANY - P'*/')^0 * (P('*/') + EOF)
	local COMMENT = (line_comment + block_comment)^1

	local SC = SPACE + COMMENT
	local IGNORED = SPACE + COMMENT + STRING
	-- special captures
	local cp = Cp() -- pos capture, Carg(1) is the shift value, comes from start_code_pos
	local cl = cp/PosToLine -- line capture, uses editor:LineFromPosition
	local par = C(P"("*(1-P")")^0*P")") -- captures parameters in parentheses
--^------- common patterns -------^--

	do --v------- asm -------v--
		-- redefine common patterns
		local SPACE = S' \t'^1

		local IGNORED = (ESCANY - NL)^0 * NL -- just skip line by line

		-- define local patterns
		local p = P"proc"
		local F = P"FRAME"
		-- create flags:
		F = Cg(F*Cc(true),'F')
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local par = C((ESCANY - NL)^0)
		local def1 = I*SPACE*(p+F)
		local def2 = p*SPACE*I*P','^-1
		local def = (SPACE+P'')*Ct((def1+def2)*(SPACE*par)^-1)*NL
		-- resulting pattern, which does the work
		local patt = (def + IGNORED + 1)^0 * EOF

		Lang2lpeg.Assembler = lpeg.Ct(patt)
	end --do --^------- ASM -------^--

	do --v------- Lua -------v--
		-- redefine common patterns
		local IDENTIFIER = IDENTIFIER*(P'.'*IDENTIFIER)^0*(P':'*IDENTIFIER)^-1
		-- LONG BRACKETS
		local long_brackets = #(P'[' * P'='^0 * P'[') *
			function (subject, i1)
				local level = _G.assert( subject:match('^%[(=*)%[', i1) )
				local _, i2 = subject:find(']'..level..']', i1, true)  -- true = plain "find substring"
				return (i2 and (i2+1)) or #subject+1--error('unfinished long brackets')
				-- ^ if unfinished long brackets then capture till EOF (at #subject+1)
		end
		local LUALONGSTR = long_brackets

		local multi  = P'--' * long_brackets
		local single = P'--' * (1 - NL)^0 * NL
		local COMMENT = multi + single
		local SC = SPACE + COMMENT

		local IGNORED = SPACE + COMMENT + STRING + LUALONGSTR

		-- define local patterns
		local f = P"function"
		local l = P"local"
		-- create flags
		l = Cg(l*SC^1*Cc(true),'l')^-1
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local funcdef1 = l*f*SC^1*I*SC^0*par -- usual function declaration
		local funcdef2 = l*I*SC^0*"="*SC^0*f*SC^0*par -- declaration through assignment
		local def = Ct(funcdef1 + funcdef2)
		-- resulting pattern, which does the work
		local patt = (def + IGNORED^1 + IDENTIFIER + 1)^0 * (EOF) --+ error'invalid character')

		Lang2lpeg.Lua = lpeg.Ct(patt)
	end --do --^------- Lua -------^--

	do --v----- Pascal ------v--
		-- redefine common patterns
		local IDENTIFIER = IDENTIFIER*(P'.'*IDENTIFIER)^0
		local STRING = P"'" *( ANY - (P"'"+NL) )^0 *(P"'"+NL) --NL == error'unfinished string')
		--^ there's no problem with pascal strings with double single quotes in the middle, like this:
		--  'first''second'
		--  in the loop, STRING just matches the 'first'-part, and then the 'second'.

		local multi1  = P'(*' *(1-P'*)')^0 * (P'*)' + EOF)--unfinished long comment
		local multi2  = P'{' *(1-P'}')^0 * (P'}' + EOF)--unfinished long comment
		local single = P'//' * (1 - NL)^0 * NL
		local COMMENT = multi1 + multi2 + single

		local SC = SPACE + COMMENT
		local IGNORED = SPACE + COMMENT + STRING

		-- define local patterns
		local f = AnyCase"function"
		local p = AnyCase"procedure"
		local c = AnyCase"constructor"
		local d = AnyCase"destructor"
		local restype = AZ^1
		-- create flags:
		-- f = Cg(f*Cc(true),'f')
		restype = Cg(C(restype),'')
		p = Cg(p*Cc(true),'p')
		c = Cg(c*Cc(true),'c')
		d = Cg(d*Cc(true),'d')
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local procdef = Ct((p+c+d)*SC^1*I*SC^0*par^-1)
		local funcdef = Ct(f*SC^1*I*SC^0*par^-1*SC^0*P':'*SC^0*restype*SC^0*P';')
		-- resulting pattern, which does the work
		local patt = (procdef + funcdef + IGNORED^1 + IDENTIFIER + 1)^0 * EOF

		Lang2lpeg.Pascal = lpeg.Ct(patt)
	end --^----- Pascal ------^--

	do --v----- C++ ------v--
		-- define local patterns
		local keywords = P'if'+P'else'+P'switch'+P'case'+P'while'+P'for'
		local nokeyword = -(keywords)
		local type = P"static "^-1*P"const "^-1*P"enum "^-1*P'*'^-1*IDENTIFIER*P'*'^-1
		local funcbody = P"{"*(ESCANY-P"}")^0*P"}"
		-- redefine common patterns
		local IDENTIFIER = P'*'^-1*P'~'^-1*IDENTIFIER
		IDENTIFIER = IDENTIFIER*(P"::"*IDENTIFIER)^-1
		-- create flags:
		type = Cg(type,'')
		-- create additional captures
		local I = nokeyword*C(IDENTIFIER)*cl
		-- definitions to capture:
		local funcdef = nokeyword*Ct((type*SC^1)^-1*I*SC^0*par*SC^0*(#funcbody))
		local classconstr = nokeyword*Ct((type*SC^1)^-1*I*SC^0*par*SC^0*P':'*SC^0*IDENTIFIER*SC^0*(P"("*(1-P")")^0*P")")*SC^0*(#funcbody)) -- this matches smthing like PrefDialog::PrefDialog(QWidget *parent, blabla) : QDialog(parent)
		-- resulting pattern, which does the work
		local patt = (classconstr + funcdef + IGNORED^1 + IDENTIFIER + ANY)^0 * EOF

		Lang2lpeg['C++'] = lpeg.Ct(patt)
	end --^----- C++ ------^--

	do --v----- JS ------v--
		-- redefine common patterns
		local NL = NL + P"\f"
		local regexstr = P'/' * (ESCANY - (P'/' + NL))^0*(P'/' * S('igm')^0 + NL)
		local STRING = STRING + regexstr
		local IGNORED = SPACE + COMMENT + STRING
		-- define local patterns
		local f = P"function"
		local m = P"method"
		local funcbody = P"{"*(ESCANY-P"}")^0*P"}"
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local funcdef =  Ct((f+m)*SC^1*I*SC^0*par*SC^0*(#funcbody))
		local eventdef = Ct(P"on"*SC^1*P'"'*I*P'"'*SC^0*(#funcbody))
		-- resulting pattern, which does the work
		local patt = (funcdef + eventdef + IGNORED^1 + IDENTIFIER + 1)^0 * EOF

		Lang2lpeg.JScript = lpeg.Ct(patt)
	end --^----- JS ------^--

	do --v----- VB ------v--
		-- redefine common patterns
		local SPACE = (S(" \t")+P"_"*S(" \t")^0*(P"\r\n"))^1
		local SC = SPACE
		local NL = (P"\r\n")^1*SC^0
		local STRING = P'"' * (ANY - (P'"' + P"\r\n"))^0*P'"'
		local COMMENT = (P"'" + P"REM ") * (ANY - P"\r\n")^0
		local IGNORED = SPACE + COMMENT + STRING
		local I = C(IDENTIFIER)*cl
		-- define local patterns
		local f = AnyCase"function"
		local p = AnyCase"property"
			local let = AnyCase"let"
			local get = AnyCase"get"
			local set = AnyCase"set"
		local s = AnyCase"sub"
		--local con=Cmt(AnyCase"const",(function(s,i) if _show_more then return i else return nil end end))
		--local dim=Cmt(AnyCase"dim",(function(s,i) if _show_more then return i else return nil end end))

		--local scr=P("<script>")
		--local stt=P("<stringtable>")

		local restype = (P"As"+P"as")*SPACE*Cg(C(AZ^1),'')
		let = Cg(let*Cc(true),'pl')
		get = Cg(get*Cc(true),'pg')
		set = Cg(set*Cc(true),'ps')
		p = NL*p*SC^1*(let+get+set)
		s = NL*Cg(s*Cc(true),'S')
		f = NL*Cg(f*Cc(true),'F')
		--dim = NL*Cg(dim*Cc(true),"D")
		--con = NL*Cg(con*Cc(true),"C")

		local e = NL*AnyCase"end"*SC^1*(AnyCase"sub"+AnyCase"function"+AnyCase"property")
		local body = (IGNORED^1 + IDENTIFIER + 1 - f - s - p - e)^0*e

		-- definitions to capture:
		f = f*SC^1*I*SC^0*par
		p = p*SC^1*I*SC^0*par
		s = s*SC^1*I*SC^0*par
		--con = con*SC^1*I
		--dim = dim*SC^1*I
		local def = Ct((f + s + p)*(SPACE*restype)^-1)*body --+ Ct(dim+con)
		-- resulting pattern, which does the work

		local patt = (def + IGNORED^1 + IDENTIFIER + (1-NL)^1 + NL)^0 * EOF

		Lang2lpeg.VisualBasic = lpeg.Ct(patt)
	end --^----- VB ------^--

	do --v------- Python -------v--
		-- redefine common patterns
		local SPACE = S' \t'^1
		local IGNORED = (ESCANY - NL)^0 * NL -- just skip line by line
		-- define local patterns
		local c = P"class"
		local d = P"def"
		-- create flags:
		c = Cg(c*Cc(true),'class')
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local def = (c+d)*SPACE*I
		def = (SPACE+P'')*Ct(def*SPACE^-1*par)*SPACE^-1*P':'
		-- resulting pattern, which does the work
		local patt = (def + IGNORED + 1)^0 * EOF

		Lang2lpeg.Python = lpeg.Ct(patt)
	end --do --^------- Python -------^--

	do --v----- Nemerle ------v--
		local IGNORED = SC
		local keywords = P'if'+P'else'+P'unless'+P'finally'+P'while'+P'for'+P'foreach'+P'try'+P'catch'+P'match'+P'when'+P'throw'+P'do'
		local nokeyword = -(keywords)
		local mod = P'public'+P'private'+P'static'+P'virtual'+P'def'+P'new'
		local funcbody = P"{"*(ESCANY-P"}")^0*P"}"

		local I = nokeyword*C(IDENTIFIER)*cl

		local typ = IDENTIFIER*(P'.'*IDENTIFIER)^0
		-- распознаем tuples в типе возвращаемом методом/функцией
		local tuple = typ*(SPACE^0*P'*'*SPACE^0*typ)^1
		local tot = tuple+typ
		-- распознаем коллекции/словари в возвращаемом типе
		local ar = typ*SPACE^0*P'['*SPACE^0*((tot*SPACE^0*P','*SPACE^0)^0*(tot*SPACE^0))^0*P']'
		local arr = typ*SPACE^0*P'['*SPACE^0*(((ar+tot)*SPACE^0*P','*SPACE^0)^0*((ar+tot)*SPACE^0))^0*P']'
		local type = arr+tot
		-- распознаем контракт метода
		local req = (P'requires'+P'ensures')*SPACE*(AZ+SPACE+R'09'+S'.,?!=></[]-+()*&^%$#@')^1

		-- методы/функции
		local method = nokeyword*Ct((mod*SPACE)^0*I*SPACE^0*par)*(SPACE^0*P':'*SPACE^0*type)^0*SC^0*req^0*(#funcbody)
		-- декларации методов интерфейсов
		local ifmethod = nokeyword*Ct((P'new'*SPACE)^0*I*SPACE^0*par)*SPACE^0*P':'*SPACE^0*type*SPACE^0*P';'

		local patt = (method + ifmethod + IGNORED^1 + IDENTIFIER + ANY)^0 * EOF

		Lang2lpeg['Nemerle'] = lpeg.Ct(patt)
	end --^----- Nemerle ------^--

	do --v------- nnCron -------v--
		-- redefine common patterns
		local IDENTIFIER = (ANY - SPACE)^1
		local SPACE = S' \t'^1
		local IGNORED = (ESCANY - NL)^0 * NL -- just skip line by line
		-- define local patterns
		local d = P":"
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local def = d*SPACE*I
		def = Ct(def*(SPACE*par)^-1)*IGNORED
		-- resulting pattern, which does the work
		local patt = (def + IGNORED + 1)^0 * EOF

		Lang2lpeg.nnCron = lpeg.Ct(patt)
	end --do --^------- nnCron -------^--

	do --v------- CSS -------v--
		-- helper
		local function clear_spaces(s)
			return s:gsub('%s+',' ')
		end
		-- redefine common patterns
		local IDENTIFIER = (ANY - SPACE)^1
		local NL = P"\r\n"
		local SPACE = S' \t'^1
		local IGNORED = (ANY - NL)^0 * NL -- just skip line by line
		local par = C(P"{"*(1-P"}")^0*P"}")/clear_spaces -- captures parameters in parentheses
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		-- definitions to capture:
		local def = Ct(I*SPACE*par)--*IGNORED
		-- resulting pattern, which does the work
		local patt = (def + IGNORED + 1)^0 * EOF

		Lang2lpeg.CSS = lpeg.Ct(patt)
	end --do --^------- CSS -------^--

	do --v----- * ------v--
		-- redefine common patterns
		local NL = P"\r\n"+P"\n"+P"\f"
		local SC = S" \t\160" -- без понятия что за символ с кодом 160, но он встречается в SciTEGlobal.properties непосредственно после [Warnings] 10 раз.
		local COMMENT = P'#'*(ANY - NL)^0*NL
		-- define local patterns
		local somedef = S'fFsS'*S'uU'*S'bBnN'*AZ^0 --пытаемся поймать что-нибудь, похожее на определение функции...
		local section = P'['*(ANY-P']')^1*P']'
		-- create flags
		local somedef = Cg(somedef, '')
		-- create additional captures
		local I = C(IDENTIFIER)*cl
		section = C(section)*cl
		local tillNL = C((ANY-NL)^0)
		-- definitions to capture:
		local def1 = Ct(somedef*SC^1*I*SC^0*(par+tillNL))
		local def2 = (NL+BOF)*Ct(section*SC^0*tillNL)*NL

		-- resulting pattern, which does the work
		local patt = (def2 + def1 + COMMENT + IDENTIFIER + 1)^0 * EOF
		-- local patt = (def2 + def1 + IDENTIFIER + 1)^0 * EOF -- чуть медленнее

		Lang2lpeg['*'] = lpeg.Ct(patt)
	end --^----- * ------^--

	do --v------- autohotkey -------v--
		-- redefine
		local NL = P'\n'+P'\r\n'
		-- local NL = S'\r\n'
		local ESCANY = P'`'*ANY + ANY
		
		-- helper
		local I = (ESCANY-S'(){},=:;\r\n')^1
		local LINE = (ESCANY-NL)^0
		local block_comment = '/*' * (ESCANY - P'*/')^0 * (P('*/') + EOF)
		local line_comment  = P';'*LINE*(NL + EOF)
		local COMMENT = line_comment + block_comment
		local BALANCED = P{ "{" * ((1 - S"{}") + V(1))^0 * "}" } -- capture balanced {}
		-- definitions to capture:
		local label     = C( I*P':'*#(1-S'=:'))*cl*LINE
		local keystroke = C( I*P'::' )*cl*LINE
		local hotstring = C( P'::'*I*P'::'*LINE )*cl
		local directive = C( P'#'*I )*cl*LINE
		local func      = C( I )*cl*par*(COMMENT+NL)^0*BALANCED
		local def = Ct( keystroke + label + hotstring + directive + func )
		-- resulting pattern, which does the work
		local patt = (SPACE^0*def + NL + COMMENT + LINE*NL)^0 * LINE*(EOF) --+ error'invalid character')

		Lang2lpeg.autohotkey = lpeg.Ct(patt)
	end --do --^------- autohotkey -------^--

	do --v----- SQL ------v--
		-- redefine common patterns
		--идентификатор может включать точку
		local IDENTIFIER = AZ * (AZ+N+P".")^0
		local STRING = (P'"' * (ANY - P'"')^0*P'"') + (P"'" * (ANY - P"'")^0*P"'")
		local COMMENT = ((P"--" * (ANY - NL)^0*NL) + block_comment)^1
		local SC = SPACE

		local cr = AnyCase"create"*SC^1
		local pr = AnyCase"proc"*AnyCase"edure"^0
		local vi = AnyCase"view"
		local tb = AnyCase"table"
		local tr = AnyCase"trigger"
		local IGNORED = SPACE + COMMENT + STRING
		-- create flags
		tr = Cg(cr*tr*SC^1*Cc(true),'tr')
		tb = Cg(cr*tb*SC^1*Cc(true),'tb')
		vi = Cg(cr*vi*SC^1*Cc(true),'vi')
		pr = Cg(cr*pr*SC^1*Cc(true),'pr')

		local I = C(IDENTIFIER)*cl
		--параметры процедур и вью - всё от имени до as
		local parpv = C((1-AnyCase"as")^0)*AnyCase"as"
		--параметры таблиц содержат комментарии и параметры
		local partb = C((P"("*(COMMENT + (1-S"()")+par)^1*P")"))
		-- -- definitions to capture:
		pr = pr*I*SC^0*parpv
		vi = vi*I*SC^0*parpv
		tb = tb*I*SC^0*partb
		tr = tr*I*SC^1*AnyCase"on"*SC^1*I --"параметр" триггера - идентификатор после I
		local def = Ct(( pr + vi + tb + tr))

		-- resulting pattern, which does the work
		local patt = (def + IGNORED^1 + IDENTIFIER + 1)^0 * EOF

		Lang2lpeg.SQL = lpeg.Ct(patt)
	end --^----- SQL ------^--

end

local Lang2CodeStart = {
	['Pascal']='^IMPLEMENTATION$',
}

local Lexer2Lang = {
	['asm']='Assembler',
	['cpp']='C++',
	['js']='JScript',
	['vb']='VisualBasic',
	['vbscript']='VisualBasic',
	['css']='CSS',
	['pascal']='Pascal',
	['python']='Python',
	['sql']='SQL',
	['lua']='Lua',
	['nncrontab']='nnCron',
}

local Ext2Lang = {}
do -- Fill_Ext2Lang
	local patterns = {
		[props['file.patterns.asm']]='Assembler',
		[props['file.patterns.cpp']]='C++',
		[props['file.patterns.wsh']]='JScript',
		[props['file.patterns.vb']]='VisualBasic',
		[props['file.patterns.wscript']]='VisualBasic',
		['*.css']='CSS',
		['*.sql']='SQL',
		[props['file.patterns.pascal']]='Pascal',
		[props['file.patterns.py']]='Python',
		[props['file.patterns.lua']]='Lua',
		[props['file.patterns.nemerle']]='Nemerle',
		[props['file.patterns.nncron']]='nnCron',
		['*.ahk']='autohotkey',
	}
	for i,v in pairs(patterns) do
		for ext in (i..';'):gmatch("%*%.([^;]+);") do
			Ext2Lang[ext] = v
		end
	end
end -- Fill_Ext2Lang

local function Functions_GetNames()
	table_functions = {}
	if editor.Length == 0 then return end

	local ext = props["FileExt"]:lower() -- a bit unsafe...
	local lang = Ext2Lang[ext]

	local start_code = Lang2CodeStart[lang]
	local lpegPattern = Lang2lpeg[lang]
	if not lpegPattern then
		lang = Lexer2Lang[props['Language']]
		start_code = Lang2CodeStart[lang]
		lpegPattern = Lang2lpeg[lang]
		if not lpegPattern then
			start_code = Lang2CodeStart['*']
			lpegPattern = Lang2lpeg['*']
		end
	end
	local textAll = editor:GetText()
	local start_code_pos = start_code and editor:findtext(start_code, SCFIND_REGEXP) or 0

	-- lpegPattern = nil
	table_functions = lpegPattern:match(textAll, start_code_pos+1) -- 2nd arg is the symbol index to start with
end

local function Functions_ListFILL()
	if props['sidebar.show']~='1' or props['sidebar.active.tab']~='1' then return end
	if _sort == 'order' then
		table.sort(table_functions, function(a, b) return a[2] < b[2] end)
	else
		table.sort(table_functions, function(a, b) return a[1]:lower() < b[1]:lower() end)
	end
	-- remove duplicates
	for i = #table_functions, 2, -1 do
		if table_functions[i][2] == table_functions[i-1][2] then
			table.remove (table_functions, i)
		end
	end
	list_func:clear()

	local function emptystr(...) return '' end
	local function GetParams (funcitem)
		return (funcitem[3] and ' '..funcitem[3]) or ''
	end
	local function GetFlags (funcitem)
		local res = ''
		local add = ''
		for flag,value in pairs(funcitem) do
			if type(flag)=='string' then
				if type(value)=='string' then	add = flag .. value
				elseif type(value)=='number' then add = flag..':'..value
				else add = flag end
				res = res .. '['.. add ..']'
			end
		end
		if res~='' then res = res .. ' ' end
		return res or ''
	end
	if not _show_params then GetParams = emptystr end
	if not _show_flags then GetFlags = emptystr end

	local function fixname (funcitem)
		return GetFlags(funcitem)..funcitem[1]..GetParams(funcitem)
	end
	for _, a in ipairs(table_functions) do
		local funcname = fixname(a)
		if tonumber(props["editor.unicode.mode"]) == IDM_ENCODING_DEFAULT then
			funcname = funcname:to_utf8(editor:codepage())
		end
		list_func:add_item(funcname, a[2])
	end
end

function Functions_SortByOrder()
	_sort = 'order'
	Functions_ListFILL()
end

function Functions_SortByName()
	_sort = 'name'
	Functions_ListFILL()
end

function Functions_ToggleParams ()
	_show_params = not _show_params
	Functions_ListFILL()
end

function Functions_ToggleFlags ()
	_show_flags = not _show_flags
	Functions_ListFILL()
end

local function Functions_GotoLine()
	local sel_item = list_func:get_selected_item()
	if sel_item == -1 then return end
	local pos = list_func:get_item_data(sel_item)
	if pos then
		ShowCompactedLine(pos)
		editor:GotoLine(pos)
		gui.pass_focus()
	end
end

list_func:on_double_click(function()
	Functions_GotoLine()
end)

list_func:on_key(function(key)
	if key == 13 then -- Enter
		Functions_GotoLine()
	end
end)

----------------------------------------------------------
-- list_bookmarks   Bookmarks
----------------------------------------------------------

local table_bookmarks = {}
local bm_file = props['SciteUserHome'].."\\bookmarks.lst"
local function GetBufferNumber()
	return tonumber(props['BufferNumber']) or 1
end

local function Bookmark_Add(line_number)
	local line_text = editor:GetLine(line_number)
	if line_text == nil then line_text = '' end
	line_text = line_text:gsub('^%s+', ''):gsub('%s+', ' ')
	if line_text == '' then
		line_text = ' - empty line - ('..(line_number+1)..')'
	end
	for _, a in ipairs(table_bookmarks) do
		if a.FilePath == props['FilePath'] and a.LineNumber == line_number then
		return end
	end
	local bmk = {}
	bmk.FilePath = props['FilePath']
	bmk.BufferNumber = GetBufferNumber()
	bmk.LineNumber = line_number
	if tonumber(props["editor.unicode.mode"]) == IDM_ENCODING_DEFAULT then
		line_text = line_text:to_utf8(editor:codepage())
	end
	bmk.LineText = line_text
	table_bookmarks[#table_bookmarks+1] = bmk
end

local function Bookmark_Delete(line_number)
	for i = #table_bookmarks, 1, -1 do
		if table_bookmarks[i].FilePath == props['FilePath'] then
			if line_number == nil then
				table.remove(table_bookmarks, i)
			elseif table_bookmarks[i].LineNumber == line_number then
				table.remove(table_bookmarks, i)
				break
			end
		end
	end
end

local function Bookmarks_ListFILL()
	if props['sidebar.show']~='1' or props['sidebar.active.tab']~='1' then return end
	table.sort(table_bookmarks, function(a, b)
									return a.BufferNumber < b.BufferNumber or
											a.BufferNumber == b.BufferNumber and
											a.LineNumber < b.LineNumber
								end)
	list_bookmarks:clear()
	for _, bmk in ipairs(table_bookmarks) do
		list_bookmarks:add_item({bmk.BufferNumber, bmk.LineText}, {bmk.FilePath, bmk.LineNumber})
	end
end

local function Bookmarks_RefreshTable()
	Bookmark_Delete()
	for i = 0, editor.LineCount do
		if editor:MarkerGet(i) == 2 then
			Bookmark_Add(i)
		end
	end
	Bookmarks_ListFILL()
end

local function Bookmarks_GotoLine()
	local sel_item = list_bookmarks:get_selected_item()
	if sel_item == -1 then return end
	local pos = list_bookmarks:get_item_data(sel_item)
	if pos then
		scite.Open(pos[1]) -- FilePath
		ShowCompactedLine(pos[2]) -- LineNumber
		editor:GotoLine(pos[2])
		gui.pass_focus()
	end
end

list_bookmarks:on_double_click( Bookmarks_GotoLine )

list_bookmarks:on_key(function(key)
	if key == 13 then -- Enter
		Bookmarks_GotoLine()
	end
end)

AddEventHandler("OnSendEditor", function(id_msg, wp, lp)
	if id_msg == SCI_MARKERADD then -- wp = line_number, lp = marker_type
		if lp == 1 then Bookmark_Add(wp) Bookmarks_ListFILL() end
	elseif id_msg == SCI_MARKERDELETE then -- wp = line_number, lp = marker_type
		if lp == 1 then Bookmark_Delete(wp) Bookmarks_ListFILL() end
	elseif id_msg == SCI_MARKERDELETEALL then -- wp = marker_type
		if wp == 1 then Bookmark_Delete() Bookmarks_ListFILL() end
	end
end)

AddEventHandler("OnTabMove", function(from, to)
--[[
123 from4 56 to7 89
123 to4 56 from7 89
]]
	if from < to then
		for _, bmk in ipairs(table_bookmarks) do
			if bmk.BufferNumber > from and bmk.BufferNumber <= to then
				bmk.BufferNumber = bmk.BufferNumber - 1  
			elseif bmk.BufferNumber == from then
				bmk.BufferNumber = to
			end
		end
	else
		for _, bmk in ipairs(table_bookmarks) do
			if bmk.BufferNumber == from then
				bmk.BufferNumber = to
			elseif bmk.BufferNumber >= to and bmk.BufferNumber < from then
				bmk.BufferNumber = bmk.BufferNumber + 1  
			end
		end
	end
	Bookmarks_ListFILL()
end)

AddEventHandler("OnClose", function(file)
	local bn = tonumber(props['BufferNumber'])
	if not bn then return end
--[[	for i = #table_bookmarks, 1, -1 do
		if table_bookmarks[i].FilePath == file then
			table.remove(table_bookmarks, i)
		end
		if table_bookmarks[i].BufferNumber<bn then table_bookmarks[i].BufferNumber = table_bookmarks[i].BufferNumber-1 end
	end]]
	Bookmarks_ListFILL()
end)

----------------------------------------------------------
-- Go to function definition
----------------------------------------------------------

-- По имени функции находим строку с ее объявлением (инфа берется из table_functions)
local function Func2Line(funcname)
	if not next(table_functions) then
		Functions_GetNames()
	end
	for i = 1, #table_functions do
		if funcname == table_functions[i][1] then
			return table_functions[i][2]
		end
	end
end

-- Переход на строку с объявлением функции
local function JumpToFuncDefinition()
	local funcname = GetCurrentWord()
	local line = Func2Line(funcname)
	if line then
		_backjumppos = editor.CurrentPos
		editor:GotoLine(line)
		return true -- обрываем дальнейшую обработку OnDoubleClick (выделение слова и пр.)
	end
end

local function JumpBack()
	if _backjumppos then
		editor:GotoPos(_backjumppos)
		_backjumppos = nil
	end
end

AddEventHandler("OnDoubleClick", function(shift, ctrl, alt)
-- AddEventHandler("OnClick", function(shift, ctrl, alt)
	if shift then
		return JumpToFuncDefinition()
	end
end)

AddEventHandler("OnKey", function(key, shift, ctrl, alt, char)
	if editor.Focus and ctrl then
		if key == 188 then -- '<'
			JumpBack()
		elseif key == 190 then -- '>'
			JumpToFuncDefinition()
		end
	end
end)
-------------------------
local line_count

local function OnSwitch()
	line_count = editor.LineCount
	if tab1:bounds() then -- visible Funk/Bmk
		Functions_GetNames()
		Functions_ListFILL()
		Bookmarks_ListFILL()
	end
	gui.pass_focus()
end

AddEventHandler("OnSwitchFile", OnSwitch)
AddEventHandler("OnOpen", OnSwitch)
AddEventHandler("OnSave", OnSwitch)
event('sb_tab_selected'):register(function(e, tab_id) if tab_id == 1 then OnSwitch() end end)
-- Обновление списков Functions и Bookmarks при изменении кол-ва строк в активном документе
AddEventHandler("OnInit", function() -- событие OnUpdateUI доступно только после инициализации OnInit
AddEventHandler("OnUpdateUI", function()
	if (editor.Focus and line_count) then
		local line_count_new = editor.LineCount
		local def_line_count = line_count_new - line_count
		if def_line_count ~= 0 then
			if tab1:bounds() then -- visible Funk/Bmk
				local cur_line = editor:LineFromPosition(editor.CurrentPos)
				for i = 1, #table_functions do
					local table_line = table_functions[i][2]
					if table_line > cur_line then
						table_functions[i][2] = table_line + def_line_count
					end
				end
				Functions_ListFILL()
				Bookmarks_RefreshTable()
			end
			line_count = line_count_new
		end
	end
end)
end, true)
-------------------------
list_func:context_menu {
	'Сортировать по порядку|Functions_SortByOrder',
	'Сортировать по имени|Functions_SortByName',
	'Показать/скрыть флаги|Functions_ToggleFlags',
	'Показать/скрыть параметры|Functions_ToggleParams',
}
-------------------------
return {caption="Функции", wnd = tab1, icon_idx = 48}