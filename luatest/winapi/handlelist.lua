--oo/handlelist: track objects by their corresponding 32bit pointer handle
setfenv(1, require'winapi')
require'winapi.vobject'

HandleList = class(VObject)

function HandleList:__init(handle_property)
	self.handle_property = handle_property
	self.items = {} --{ptonumber(handler) = object}; object is also pinned (table is not weak)
end

function HandleList:add(obj) self.items[ptonumber(obj[self.handle_property])] = obj end
function HandleList:remove(obj) self.items[ptonumber(obj[self.handle_property])] = nil end
function HandleList:find(handle) return self.items[ptonumber(handle)] end

