--core/debug: strict mode and some debug tools. entirely optional module.
setfenv(1, require'winapi.namespace')
require'glue'

--disable stdout buffering so we can print-debug stuff
io.stdout:setvbuf'no'

--set strict mode for the whole winapi namespace
local _G = _G
local declared = {}

function _M:__index(k)
	if declared[k] then return nil end
	if _G[k] ~= nil then return _G[k] end
	error(string.format('Undefined winapi global %s', k), 2)
end

function _M:__newindex(k,v)
	local w = debug.getinfo(2, 'S').what
	if w == 'main' or w == 'C' or declared[k] then
		declared[k] = true
		rawset(self, k, v)
	else
		error(string.format('Assignment to undeclared winapi global %s', k), 2)
	end
end

--utility to search the name of a constant in the winapi namespace
function findname(prefix, value)
	for k,v in pairs(_M) do
		if k:match('^'..prefix) and type(v) ~= 'cdata' and type(value) ~= 'cdata' and v == value then return k end
	end
	return tonumber(value) ~= nil and string.format('%x', value) or value
end

--print that can be used in expressions and recurses into tables
_print = print
local function __print(indent,...)
	if indent ~= '' then _print(indent,...) else _print(...) end
	for i=1,select('#',...) do
		local t = select(i,...)
		if type(t) == 'table' and (not getmetatable(t) or not getmetatable(t).__tostring) then
			for k,v in pairs(t) do __print(indent..'       ',k,v) end
		end
	end
	return ...
end

function print(...) return __print('',...) end

