--ffi/array: variable length array (VLA) wrapper/memoizer.
setfenv(1, require'winapi.namespace')
require'winapi.ffi'
require'winapi.types'

--we're changing the VLA initializer a bit: if we get a table as arg#1,
--we're creating a #t array initialized with the elements from the table.
--we're also returning the number of elements as the second argument since APIs usually need that.
arrays = setmetatable({}, {
	__index = function(t,k)
		local ctype = ffi.typeof(k..'[?]')
		local itemsize = ffi.sizeof(k)
		t[k] = function(t,...)
			local n
			if type(t) == 'table' then
				n = #t
				t = ctype(n, t)
			else
				n = t
				t = ctype(t,...)
			end
			return t, n
		end
		return t[k]
	end
})

if not ... then require'array_test' end
