--ffi/bitmask: bitmask encoding and decoding.
setfenv(1, require'winapi.namespace')
require'winapi.util'

local bitmask_class = {}
local bitmask_meta = {__index = bitmask_class}

function bitmask(fields)
	return setmetatable({fields = fields}, bitmask_meta)
end

function negate(mask)
	return {[true] = 0, [false] = mask}
end

function bitmask_class:compute_mask(t) --compute total mask for use with setbits()
	t = t or self.fields
	local v = 0
	for _, mask in pairs(t) do
		if type(mask) == 'table' then --choice mask
			v = bit.bor(v, self:compute_mask(mask))
		else
			v = bit.bor(v, mask)
		end
	end
	return v
end

local setbit, setbits = setbit, setbits

function bitmask_class:setbit(over, k, v)
	local mask = self.fields[k] --def: {name = mask | choicemask}; choicemask: {name = mask}
	assert(mask, 'unknown bitmask field "%s"', k)
	if type(mask) == 'table' then --choicemask
		over = setbits(over, self:compute_mask(mask), mask[v] or 0)
	else
		over = setbit(over, mask, v)
	end
	return over
end

function bitmask_class:set(over, t)
	if not t then return over end --no table is an empty table
	for k in pairs(self.fields) do
		if t[k] ~= nil then
			over = self:setbit(over, k, t[k])
		end
	end
	return over
end

function bitmask_class:getbit(from, k)
	local mask = self.fields[k]
	assert(mask, 'unknown bitmask field "%s"', k)
	if type(mask) == 'table' then --choicemask
		local default_choice
		for choice, choicemask in pairs(mask) do
			if choicemask == 0 then default_choice = choice end
			if bit.band(from, choicemask) ~= 0 then --return the first found choice
				return choice
			end
		end
		return default_choice
	end
	return bit.band(from, mask) == mask
end

function bitmask_class:get(from, into)
	local t = into or {}
	for k in pairs(self.fields) do
		t[k] = self:getbit(from, k)
	end
	return t
end

