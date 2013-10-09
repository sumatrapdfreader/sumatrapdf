--oo/tabcontrol: standard tab control.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.itemlist'
require'winapi.tabcontrol'

TabItemList = class(ItemList)

function TabItemList:__init(tab)
	self.hwnd = tab.hwnd
end

function TabItemList:add(i, item)
	if not item then i, item = 0x7fffffff, i end
	if type(item) == 'string' then
		local s = item
		item = TCITEM()
		item.text = wcs(s)
	end
	TabCtrl_InsertItem(self.hwnd, i, item)
end

function TabItemList:del(i)
	TabCtrl_DeleteItem(self.hwnd, i)
end

function TabItemList:clear()
	TabCtrl_DeleteAllItems(self.hwnd)
end

function TabItemList:set(i, item)
	TabCtrl_SetItem(self.hwnd, i, item)
end

function TabItemList:get(i)
	self.__item = TCITEM:setmask(self.__item)
	TabCtrl_GetItem(self.hwnd, i, self.__item)
	return self.__item
end

function TabItemList:get_count()
	return TabCtrl_GetItemCount(self.hwnd)
end


TabControl = subclass({
	__style_bitmask = bitmask{
		tabstop = WS_TABSTOP,
	},
	__style_ex_bitmask = bitmask{ --these don't work well with CBS_SIMPLE says MS!

	},
	__defaults = {
		tabstop = true,
		w = 200, h = 100,
	},
	__init_properties = {},
	__wm_command_handler_names = index{

	}
}, Control)

function TabControl:__before_create(info, args)
	TabControl.__index.__before_create(self, info, args)
	args.class = WC_TABCONTROL
	--args.style_ex = bit.bor(args.style_ex, WS_EX_COMPOSITED)
end

function TabControl:__init(info)
	TabControl.__index.__init(self, info)
	self.items = TabItemList(self)
end

function TabControl:set_image_list(iml)
	TabCtrl_SetImageList(self.hwnd, iml.himl)
end

function TabControl:set_selected_index(i) TabCtrl_SetCurSel(self.hwnd, i) end
function TabControl:get_selected_index() return TabCtrl_GetCurSel(self.hwnd) end

--showcase
if not ... then
require'winapi.showcase'
local window = ShowcaseWindow{w=300,h=200}

t1 = TabControl{parent = window}
t1.image_list = ShowcaseImageList()
t1.items:add{text = 'tab#1', image = 1}
t1.items:add{text = 'tab#2', image = 5}
t1.selected_index = 2
assert(t1.selected_index == 2)

MessageLoop()
end
