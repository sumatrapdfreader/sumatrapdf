--oo/itemlist: class template for lists of structured items.
setfenv(1, require'winapi')
require'winapi.vobject'

ItemList = class(VObject)

function ItemList:__init(window, items) --stub
	self.window = window
	self.hwnd = window.hwnd
	self:add_items(items)
end

function ItemList:add_items(items)
	if not items then return end
	for i=1,#items do self:add(items[i]) end
end

function ItemList:add(i, item)
	if not item then i,item = nil,i end --i is optional
end

function ItemList:remove(i)
end

function ItemList:set(i, item)
end

function ItemList:get(i)
end

function ItemList:get_count()
end

local function remove_all(self)
	for i=self.count,1,-1 do
		self:remove(i)
	end
end
function ItemList:clear() --default impl. in case there's no API for it
	self.window:batch_update(remove_all, self)
end

function ItemList:get_info()
	local t = {}
	for i=1,self.count do
		t[i] = self:get(i)
	end
	return t
end

