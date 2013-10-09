--ffi/util: wrapping and conversion functions.
setfenv(1, require'winapi.namespace')
require'winapi.ffi'
require'winapi.wintypes'

glue = require'glue'

--TODO: don't do this
local string = string
import(glue)
_M.string = string --put string module back
update(string, glue.string)


ffi.cdef[[
DWORD GetLastError(void);

void SetLastError(DWORD dwErrCode);

DWORD FormatMessageA(
			DWORD dwFlags,
	      LPCVOID lpSource,
			DWORD dwMessageId,
			DWORD dwLanguageId,
			LPSTR lpBuffer,
			DWORD nSize,
		   va_list *Arguments
	 );
]]

GetLastError = C.GetLastError
SetLastError = C.SetLastError

FORMAT_MESSAGE_FROM_SYSTEM     = 0x00001000

local function get_error_message(id)
	local bufsize = 2048
	local buf = ffi.new('char[?]', bufsize)
	local sz = C.FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nil, id, 0, buf, bufsize, nil)
	assert(sz ~= 0, 'error getting error message: %d', GetLastError())
	return ffi.string(buf, sz)
end

local NULL = ffi.new'void*'

function checkwith(valid)
	return function(ret)
		if type(ret) == 'cdata' and ret == NULL then ret = nil end --discard NULL pointers
		local valid, err = valid(ret)
		if not valid then
			local code = GetLastError()
			if code ~= 0 then
				err = get_error_message(code)
			end
			error(err,2)
		end
		return ret
	end
end

local function validz(ret) return ret == 0, 'zero expected, got non-zero' end
local function validnz(ret) return ret ~= 0, 'non-zero expected, got zero' end
local function validtrue(ret) return ret == 1, '1 (TRUE) expected, got 0 (FALSE)' end
local function validh(ret) return ret ~= nil, 'non NULL value expected, got NULL' end
local function validpoz(ret) return ret >= 0, 'positive number expected, got negative' end

checkz    = checkwith(validz)
checknz   = checkwith(validnz)
checktrue = checkwith(validtrue)
checkh    = checkwith(validh)
checkpoz  = checkwith(validpoz)

--special wrapper for functions for which a special return value could or could not indicate error.
local function callwith2(valid)
	return function(f,...)
		SetLastError(0)
		local ret = f(...)
		if type(ret) == 'cdata' and ret == NULL then ret = nil end --discard NULL pointers
		local valid_for_sure, err = valid(ret)
		if not valid_for_sure then --still possibly valid
			local code = GetLastError()
			if code ~= 0 then
				err = get_error_message(code)
				error(err,2)
			end
		end
		return ret
	end
end

callnz2 = callwith2(validnz)
callh2 = callwith2(validh)

function own(o, finalizer)
	return o and ffi.gc(o, finalizer)
end

function disown(o)
	return o and ffi.gc(o, nil)
end

function countfrom0(n) --adjust value from counting from 1 to counting from 0
	if n == nil then return -1 end
	if type(n) ~= 'number' then return n end
	return n-1
end

function countfrom1(n) --adjust value from counting from 0 to counting from 1
	if type(n) ~= 'number' then return n end
	if n < 0 then return nil end
	return n+1
end

function ptonumber(p) --turns a pointer into a number to make it indexable
	return p and tonumber(ffi.cast('ULONG', p))
end

function ptr(p) --just to discard NULL pointers; p must be a cdata or you get an error
	return p ~= NULL and p or nil
end

function constants(t) --import a table as module globals and return the reverse lookup table of it
	import(t); return index(t)
end

local band, bor, bnot, rshift = bit.band, bit.bor, bit.bnot, bit.rshift --cache

function flags(s) --accept and compute a 'FLAG1 | FLAG2' string; also nil turns 0
	if s == nil then return 0 end
	if type(s) ~= 'string' then return s end
	local x = 0
	for flag in (s..'|'):gmatch'([^|]+)|' do
		x = bor(x, _M[flag:trim()])
	end
	return x
end

function splitlong(n) --returns the low and the high word of a signed long (usually LPARAM or LRESULT)
	return band(n, 0xffff), rshift(n, 16)
end

function splitsigned(n) --good for coordinates
	local x, y = band(n, 0xffff), rshift(n, 16)
	if x >= 0x8000 then x = x-0xffff end
	if y >= 0x8000 then y = y-0xffff end
	return x, y
end

function getbit(from, mask)
	return band(from, mask) == mask
end

function setbit(over, mask, yes) --set a single bit of a value without affecting other bits
	return bor(yes and mask or 0, band(over, bnot(mask)))
end

function setbits(over, mask, bits) --set one or more bits of a value without affecting other bits
	return bor(bits, band(over, bnot(mask)))
end

--pin a resource to a target object so it is guaranteed not to get collected
--as long as the target is alive. more than one resource can be pinned to the same target.
local pins = setmetatable({}, {__mode = 'v'})
function pin(resource, target)
	pins[resource] = target
	return resource
end

function unpin(resource)
	pins[resource] = nil
end

