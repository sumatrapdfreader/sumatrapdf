--class/window/waitemlist: accelerator item list.
setfenv(1, require'winapi')
require'winapi.itemlist'
require'winapi.accelerator'


local modifier_masks = {
	shift = FSHIFT,
	control = FCONTROL,
	ctrl = FCONTROL,
	alt = FALT,
}

local function parse_hotkey(s) --parse hotkeys like "shift + alt + F5" or "ctrl + C"; note: say "C" instead of "shift + c"
	local key, modifiers = nil, 0
	for k in s:gsplit'+' do
		k = k:trim()
		local m = modifier_masks[k:lower()]
		if m then
			modifiers = bit.bor(modifiers, m)
		elseif #k == 1 then
			key = k
		else
			key = flags('VK_'..k:upper())
		end
	end
	assert(key, 'invalid hotkey')
	return key, modifiers
end


WAItemList = class(ItemList)

function WAItemList:__init(window)
	WAItemList.__index.__init(self, window)
	self.__items = {}
end

function WAItemList:checkrange(i)
	assert(i >= 1 and i <= #self.__items + 1, 'index out of range')
end

function WAItemList:__changed()
	if self.haccel then
		DestroyAcceleratorTable(self.haccel)
		self.haccel = nil
	end
	local t = {}
	for i,v in ipairs(self.__items) do
		local key, modifiers = parse_hotkey(v.hotkey)
		t[i] = ACCEL{id = i, key = key, modifiers = modifiers}
	end
	self.haccel = CreateAcceleratorTable(t)
end

function WAItemList:add(i, item)
	if not item then i,item = nil,i end --i is optional
	if i then
		self:checkrange(i)
		table.insert(self.__items, i, item)
	else
		table.insert(self.__items, item)
	end
	self:__changed()
end

function WAItemList:add_items(items)
	if not items then return end
	for i=1,#items do
		table.insert(self.__items, items[i])
	end
	self:__changed()
end

function WAItemList:remove(i)
	self:checkrange(i)
	table.remove(self.__items, i)
	self:__changed()
end

function WAItemList:set(i, item)
	self:checkrange(i)
	self.__items[i] = item
	self:__changed()
end

function WAItemList:get(i)
	self:checkrange(i)
	return self.__items[i]
end

function WAItemList:get_count()
	return #self.__items
end

function WAItemList:clear()
	self.__items = {}
	self:__changed()
end

function WAItemList:WM_COMMAND(i)
	self:checkrange(i)
	self.__items[i].handler(self.window)
end

