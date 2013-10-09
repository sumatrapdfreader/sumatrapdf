--ffi/types: ctype wrapper/memoizer.
setfenv(1, require'winapi.namespace')
require'winapi.ffi'
require'winapi.util'

types = {}
setmetatable(types, types)

function types:__index(type_str)
	local ctype = ffi.typeof(type_str)
	self[type_str] = function(t,...)
		if ffi.istype(ctype, t) then return t end
		if t == nil then return ctype() end
		return ctype(t,...)
	end
	return self[type_str]
end

