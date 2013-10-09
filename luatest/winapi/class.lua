--oo/class: single inheritance object model.
--subclassing defined by super:__subclass(derived).
--introspection is in terms of class:__super().
setfenv(1, require'winapi')

function subclass(class, super)
	if super and super.__subclass then super:__subclass(class) end
	return class
end

function class(super)
	return subclass({}, super)
end

function isinstance(object, class) --defined in terms of object:__super()
	if type(object) ~= 'table' then return false end
	if not object.__super then return false end
	local super = object:__super()
	if super == nil then return false end
	if super == class then return true end
	return isinstance(super, class)
end
