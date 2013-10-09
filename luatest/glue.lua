--glue library, see http://code.google.com/p/lua-files/wiki/glue

local glue = {}

local select, pairs, tonumber, tostring, unpack, xpcall, assert, getmetatable, setmetatable, type, pcall =
	   select, pairs, tonumber, tostring, unpack, xpcall, assert, getmetatable, setmetatable, type, pcall
local sort, format, byte, char, min, max =
	table.sort, string.format, string.byte, string.char, math.min, math.max

function glue.index(t)
	local dt={} for k,v in pairs(t) do dt[v]=k end
	return dt
end

function glue.keys(t, cmp)
	local dt={}
	for k in pairs(t) do
		dt[#dt+1]=k
	end
	if cmp == true then
		sort(dt)
	elseif cmp then
		sort(dt, cmp)
	end
	return dt
end

function glue.update(dt,...)
	for i=1,select('#',...) do
		local t=select(i,...)
		if t ~= nil then
			for k,v in pairs(t) do dt[k]=v end
		end
	end
	return dt
end

function glue.merge(dt,...)
	for i=1,select('#',...) do
		local t=select(i,...)
		if t ~= nil then
			for k,v in pairs(t) do
				if dt[k] == nil then dt[k]=v end
			end
		end
	end
	return dt
end

--TODO: document and test this if it's a keeper (used only for inspect functions)
local keys = glue.keys
function glue.sortedpairs(t, cmp)
	local kt = keys(t, cmp)
	local i = 0
	return function()
		i = i + 1
		return kt[i], t[kt[i]]
	end
end

function glue.extend(dt,...)
	for j=1,select('#',...) do
		local t=select(j,...)
		if t ~= nil then
			for i=1,#t do dt[#dt+1]=t[i] end
		end
	end
	return dt
end

function glue.append(dt,...)
	for i=1,select('#',...) do
		dt[#dt+1] = select(i,...)
	end
	return dt
end

local tinsert, tremove = table.insert, table.remove

--insert n elements at i, shifting elemens on the right of i (i inclusive) to the right.
local function insert(t, i, n)
	if n == 1 then --shift 1
		tinsert(t, i, t[i])
		return
	end
	for p = #t,i,-1 do --shift n
		t[p+n] = t[p]
	end
end

--remove n elements at i, shifting elements on the right of i (i inclusive) to the left.
local function remove(t, i, n)
	n = min(n, #t-i+1)
	if n == 1 then --shift 1
		tremove(t, i)
		return
	end
	for p=i+n,#t do --shift n
		t[p-n] = t[p]
	end
	for p=#t,#t-n+1,-1 do --clean tail
		t[p] = nil
	end
end

--shift all the elements to the right of i (i inclusive) to the left or further to the right.
function glue.shift(t, i, n)
	if n > 0 then
		insert(t, i, n)
	elseif n < 0 then
		remove(t, i, -n)
	end
	return t
end

glue.string = {}

local function iterate_once(s, s1)
	return s1 == nil and s or nil
end

function glue.string.gsplit(s, sep, start, plain)
	start = start or 1
	plain = plain or false
	local done = false
	local function pass(i, j, ...)
		if i then
			local seg = s:sub(start, i - 1)
			start = j + 1
			return seg, ...
		else
			done = true
			return s:sub(start)
		end
	end
	if not s:find(sep, start, plain) then
		return iterate_once, s
	end
	return function()
		if done then return end
		if sep == '' then done = true return s end
		return pass(s:find(sep, start, plain))
	end
end

function glue.string.trim(s)
	local from = s:match('^[%s]*()')
	return from > #s and '' or s:match('.*[^%s]', from)
end

local function format_ci_pat(c)
	return format('[%s%s]', c:lower(), c:upper())
end
function glue.string.escape(s, mode)
	if mode == '*i' then s = s:gsub('[%a]', format_ci_pat) end
	return (s:gsub('%%','%%%%'):gsub('%z','%%z')
				:gsub('([%^%$%(%)%.%[%]%*%+%-%?])', '%%%1'))
end

function glue.string.tohex(s, upper)
	if type(s) == 'number' then
		return format(upper and '%08.8X' or '%08.8x', s)
	end
	if upper then
		return (s:gsub('.', function(c)
		  return format('%02X', byte(c))
		end))
	else
		return (s:gsub('.', function(c)
		  return format('%02x', byte(c))
		end))
	end
end

function glue.string.fromhex(s)
	return (s:gsub('..', function(cc)
	  return char(tonumber(cc, 16))
	end))
end

glue.update(glue, glue.string)

local function select_at(i,...)
	return ...,select(i,...)
end
local function collect_at(i,f,s,v)
	local t = {}
	repeat
		v,t[#t+1] = select_at(i,f(s,v))
	until v == nil
	return t
end
local function collect_first(f,s,v)
	local t = {}
	repeat
		v = f(s,v); t[#t+1] = v
	until v == nil
	return t
end
function glue.collect(n,...)
	if type(n) == 'number' then
		return collect_at(n,...)
	else
		return collect_first(n,...)
	end
end

function glue.ipcall(f,s,v)
	local function pass(ok,v1,...)
		v = v1
		return v and ok,v,...
	end
	return function()
		return pass(pcall(f,v))
	end
end

function glue.pass(...) return ... end

function glue.inherit(t, parent)
	local meta = getmetatable(t)
	if meta then
		meta.__index = parent
	elseif parent ~= nil then
		setmetatable(t, {__index = parent})
	end
	return t
end

function glue.fileexists(name)
	local f = io.open(name, 'rb')
	if f then f:close() end
	return f ~= nil and name or nil
end

function glue.readfile(name, format)
	local f = assert(io.open(name, format=='t' and 'r' or 'rb'))
	local s = f:read'*a'
	f:close()
	return s
end

function glue.writefile(name, s, format)
	local f = assert(io.open(name, format=='t' and 'w' or 'wb'))
	f:write(s)
	f:close()
end

function glue.assert(v,err,...)
	if v then return v,err,... end
	err = err or 'assertion failed!'
	if select('#',...) > 0 then err = format(err,...) end
	error(err, 2)
end

function glue.unprotect(ok,result,...)
	if not ok then return nil,result,... end
	if result == nil then result = true end
	return result,...
end

local function pcall_error(e)
	return tostring(e) .. '\n' .. debug.traceback()
end
function glue.pcall(f, ...) --luajit and lua 5.2 only!
	return xpcall(f, pcall_error, ...)
end

local unprotect = glue.unprotect
function glue.fpcall(f,...) --bloated: 2 tables, 4 closures. can we reduce the overhead?
	local fint, errt = {}, {}
	local function finally(f) fint[#fint+1] = f end
	local function onerror(f) errt[#errt+1] = f end
	local function err(e)
		for i=#errt,1,-1 do errt[i]() end
		for i=#fint,1,-1 do fint[i]() end
		return tostring(e) .. '\n' .. debug.traceback()
	end
	local function pass(ok,...)
		if ok then
			for i=#fint,1,-1 do fint[i]() end
		end
		return unprotect(ok,...)
	end
	return pass(xpcall(f, err, finally, onerror, ...))
end

local fpcall = glue.fpcall
function glue.fcall(f,...)
	return assert(fpcall(f,...))
end

function glue.autoload(t, submodules)
	local mt = getmetatable(t) or {}
	assert(not mt.__index, '__index alread assigned')
	mt.__index = function(t, k)
		if submodules[k] then
			if type(submodules[k]) == 'string' then
				require(submodules[k])
			else
				submodules[k](k)
			end
		end
		return rawget(t, k)
	end
	return setmetatable(t, mt)
end


if not ... then require'glue_test' end

return glue

