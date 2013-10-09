--oo/object: object class providing
setfenv(1, require'winapi')
require'winapi.class'

Object = class()

Object.__meta = {}

function Object:__subclass(c) --class constructor
	c.__meta = c.__meta or {}
	c.__meta.__index = c --dynamically inherit class fields
	c.__meta.__class = c --for introspection
	merge(c.__meta, self.__meta) --statically inherit instance metamethods
	c.__index = self --dynamically inherit super class fields
	c.__call = self.__call --statically inherit super class metamethods
	c.__gc = self.__gc
	setmetatable(c, c) --class metamethods are class methods
end

function Object:__init(...) end --object constructor, assumed empty

function Object:__call(...) --don't override this, override __init instead.
	local o = setmetatable({}, self.__meta)
	o:__init(...)
	return o
end

setmetatable(Object, Object)

--introspection

function Object:__super() --must work for both instances and classes
	return getmetatable(self).__class or self.__index
end

function Object:__supers() --returns iterator<class> with all super classes, bottom up
	return function(_,o)
		return o:__super()
	end,nil,self
end

function Object:__allpairs() --returns iterator<k,v,source>; iterates from bottom up
	local source = self
	local k,v
	return function()
		k,v = next(source,k)
		if k == nil then
			source = source:__super()
			if source == nil then return nil end
			k,v = next(source)
		end
		return k,v,source
	end
end

function Object:__pairs()
	local t = {}
	for k,v in self:__allpairs() do
		if t[k] == nil then t[k] = v end
	end
	return pairs(t)
end

function Object:__properties()
	local t = {} --{property_name = class_where_it_was_last_redefined}
	for k,_,source in self:__allpairs() do
		if t[k] == nil then t[k] = source end
	end
	return pairs(t)
end


--shwocase

if not ... then
--subclassing
local c = class(Object)
local init
function c:__init(...) init = true end --dummy constructor
local o = c('hi', 'there')
assert(init)
assert(o.unknown == nil) --non-existent property
--isinstance
assert(isinstance(o, c) == true)
assert(isinstance(o, Object) == true)
assert(isinstance(o, o) == false)
--introspection
for k,v,source in o:__allpairs() do _G.print(k,source) end
o.own_property = true
for k,v in o:__properties() do _G.print(k,v) end
for c in o:__supers() do _G.print(c, o:__super(), o:__super():__super()) end

end

