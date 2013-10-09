--core/namespace: create and return the only namespace for the entire API
local _M = {__index = _G}
setmetatable(_M, _M)
_M._M = _M

setfenv(1, _M)

--utility to import the contents of a table into the global winapi namespace
--because when strict mode is enabled we can't do glue.update(_M, t)
function import(globals)
	for k,v in pairs(globals) do
		rawset(_M, k, v)
	end
	return globals
end

return _M
