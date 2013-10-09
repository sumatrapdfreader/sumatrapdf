--oo/vobject: object class with virtual properties.
--indexing attribute <name> returns the result of class:get_<name>().
--setting <value> on attribute <name> calls class:set_<name>(value).
--if there's a setter but no getter, <value> gets stored in table
--object.__state and class:set_<name>(value) is called.
--a property with a getter but no setter cannot be assigned to.
--a property with no getter and no setter is rawset.
setfenv(1, require'winapi')
require'winapi.object'

VObject = class(Object)

function VObject:__subclass(c) --class constructor
	VObject.__index.__subclass(self,c)
	c.__meta.__index = function(o,k)
		if c[k] ~= nil then return c[k] end --class property or method
		return c:__get_vproperty(o,k)
	end
	c.__meta.__newindex = function(o,k,v)
		c:__set_vproperty(o,k,v)
	end
end

function VObject:__get_vproperty(o,k)
	if type(k) == 'string' and self['get_'..k] then
		return self['get_'..k](o)
	elseif rawget(o, '__state') then
		return o.__state[k]
	end
end

function VObject:__set_vproperty(o,k,v)
	if type(k) == 'string' then
		if self['get_'..k] then
			if self['set_'..k] then --r/w property
				self['set_'..k](o,v)
			else --r/o property
				error(string.format('trying to set read only property "%s"', k), 2)
			end
		elseif self['set_'..k] then --stored property
			if not rawget(o, '__state') then rawset(o, '__state', {}) end
			o.__state[k] = v
			self['set_'..k](o,v)
		else
			rawset(o,k,v)
		end
	else
		rawset(o,k,v)
	end
end

function VObject:__gen_vproperties(names, getter, setter)
	for k in pairs(names) do
		if getter then
			self['get_'..k] = function(self) return getter(self, k) end
		end
		if setter then
			self['set_'..k] = function(self, v) return setter(self, k, v) end
		end
	end
end

--introspection

function VObject:get___class()
	return getmetatable(self).__class
end

function VObject:__vproperties() --returns {property = {get = source, set = source}}
	local t = {}
	for k,v,source in VObject.__index.__allpairs(self) do
		if type(k) == 'string' and k:find'^get_' or k:find'^set_' then
			local k,what = k:sub(5), k:sub(1,3)
			local t = t[k] or {}; t[k] = t[k] or t
			if what == 'get' and t.get == nil then t.get = source end
			if what == 'set' and t.set == nil then t.set = source end
		end
	end
	return pairs(t)
end


--showcase

if not ... then
local STATUS = 'OK'
local c = class(VObject)
function c:__init(...) print('init:', ...) end
function c:get_status() return STATUS end
function c:set_status(s) STATUS = s end
function c:set_text(s) end
local o = c('hi', 'there')
assert(o.status == 'OK') --virtual property get
o.status = 'EVEN BETTER' --virtual property set
assert(o.__state == nil) --no active properties yet
o.text = 'hello' --active property set
assert(o.text == 'hello') --active property get
assert(o.__state.text == o.text) --confirm the active property
assert(o.unknown == nil) --non-existent property
assert(o[1234] == nil) --non-existent property
assert(o[false] == nil) --non-existent property
--isinstance
assert(isinstance(o, VObject) == true)
assert(isinstance(o, c) == true)
assert(isinstance(o, o) == false)
--introspection
for k,v in o:__vproperties() do _G.print(k, v.get, v.set) end

end

