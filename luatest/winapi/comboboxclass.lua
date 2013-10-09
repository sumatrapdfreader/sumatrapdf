--oo/combobox: standard combobox control based on ComboBoxEx32 control.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.comboboxex'

CBItemList = class(ItemList)

function CBItemList:add(i, item)
	if not item then i, item = nil, i end
	if type(item) == 'string' then
		local s = item
		item = COMBOBOXEXITEM()
		item.text = wcs(s)
	end
	item.i = i or 0
	item.selected_image = item.selected_image or item.image
	item.overlay_image = item.overlay_image or item.image
	ComboBoxEx_InsertItem(self.hwnd, item)
end

function CBItemList:remove(i)
	ComboBox_DeleteItem(self.hwnd, i)
end

function CBItemList:set(i, item)
	if type(item) == 'string' then
		item = COMBOBOXEXITEM()
		item.text = wcs(item)
	end
	item.i = i
	item.selected_image = item.selected_image or item.image
	item.overlay_image = item.overlay_image or item.image
	ComboBoxEx_SetItem(self.hwnd, item)
end

function CBItemList:get(i)
	self.__item = COMBOBOXEXITEM(self.__item)
	COMBOBOXEXITEM:clearmask(self.__item)
	self.__item.i = i
	ComboBoxEx_GetItem(self.hwnd, self.__item)
	return self.__item
end

function CBItemList:get_count()
	return ComboBox_GetCount(self.hwnd)
end

function CBItemList:select(i) ComboBox_SetCurSel(self.hwnd, i) end
function CBItemList:get_selected_index() return ComboBox_GetCurSel(self.hwnd) end
function CBItemList:get_selected() return self:get(self:get_selected_index()) end

--for ownerdraw lists only
function CBItemList:set_height(i, h) ComboBox_SetItemHeight(self.hwnd, i, h) end
function CBItemList:get_height(i) return ComboBox_GetItemHeight(self.hwnd, i) end


ComboBox = subclass({
	__style_bitmask = bitmask{
		tabstop = WS_TABSTOP,
		type = {
			simple = CBS_SIMPLE,
			dropdown = CBS_DROPDOWN,
			dropdownlist = CBS_DROPDOWNLIST,
		},
		autohscroll = CBS_AUTOHSCROLL,
		vscroll = CBS_DISABLENOSCROLL,
		fixedheight = CBS_NOINTEGRALHEIGHT,
		sort = CBS_SORT,
		case = {
			normal = 0,
			upper = CBS_UPPERCASE,
			lower = CBS_LOWERCASE,
		},
	},
	__style_ex_bitmask = bitmask{ --these don't work well with CBS_SIMPLE says MS!
		no_edit_image = CBES_EX_NOEDITIMAGE,
		no_edit_image2 = CBES_EX_NOEDITIMAGEINDENT,
		path_word_break = CBES_EX_PATHWORDBREAKPROC,
		no_size_limit = CBES_EX_NOSIZELIMIT,
		case_sensitive = CBES_EX_CASESENSITIVE,
	},
	__defaults = {
		tabstop = true,
		type = 'simple',
		autohscroll = true,
		case = 'normal',
		w = 100, h = 100,
	},
	__init_properties = {},
	__wm_command_handler_names = index{
		on_memory_error = CBN_ERRSPACE,
		on_selection_change = CBN_SELCHANGE,
		on_double_click = CBN_DBLCLK,
		on_focus = CBN_SETFOCUS,
		on_blur = CBN_KILLFOCUS,
		on_edit_change = CBN_EDITCHANGE,
		on_edit_update = CBN_EDITUPDATE,
		on_dropdown = CBN_DROPDOWN,
		on_closeup = CBN_CLOSEUP,
		on_select = CBN_SELENDOK,
		on_cancel = CBN_SELENDCANCEL,
	},
}, Control)

function ComboBox:__before_create(info, args)
	ComboBox.__index.__before_create(self, info, args)
	args.class = WC_COMBOBOXEX
	--args.style_ex = bit.bor(args.style_ex, WS_EX_COMPOSITED)
end

function ComboBox:__init(info)
	ComboBox.__index.__init(self, info)
	self.items = CBItemList(self)
end

function ComboBox:set_image_list(iml)
	ComboBoxEx_SetImageList(self.hwnd, iml.himl)
end
function ComboBox:get_image_list()
	local himl = ComboBoxEx_GetImageList(self.hwnd)
	return ImageLists:find(himl) or ImageList(himl)
end

function ComboBox:set_limit(limit) ComboBox_LimitText(self.hwnd, limit) end

function ComboBox:reset() ComboBox_Reset(self.hwnd) end

function ComboBox:set_item_height(height) ComboBox_SetItemHeight(self.hwnd, 'list', height) end
function ComboBox:get_item_height() ComboBox_GetItemHeight(self.hwnd, 'list') end

function ComboBox:set_edit_height(height) ComboBox_SetItemHeight(self.hwnd, 'edit', height) end
function ComboBox:get_edit_height() ComboBox_GetItemHeight(self.hwnd, 'edit') end

function ComboBox:set_edit_selection_indices(t) ComboBox_SetEditSel(self.hwnd, unpack(t,1,2)) end
function ComboBox:get_edit_selection_indices() return {ComboBox_GetEditSel(self.hwnd)} end

function ComboBox:set_dropped_down(show) ComboBox_ShowDropdown(self.hwnd, show) end
function ComboBox:get_dropped_down() return ComboBox_DroppedDown(self.hwnd) end

function ComboBox:set_dropped_width(w) ComboBox_SetDroppedWidth(self.hwnd, w) end
function ComboBox:get_dropped_width() return ComboBox_GetDroppedWidth(self.hwnd) end

--showcase

if not ... then
require'winapi.showcase'
local window = ShowcaseWindow{w=300,h=200}
local cb1 = ComboBox{parent = window, x = 10, y = 10, type = 'dropdownlist'}
cb1.image_list = ShowcaseImageList()
cb1.items:add{text = 'Option #1', image = 5}
cb1.items:add{text = 'Option #2'}
cb1.items:add{text = 'Option #3 (2nd option is gone)',
						image = 1, selected_image = 2, overlay_image = 3}
cb1.items:remove(2)
assert(cb1.items.count == 2)
print(cb1.items:get(2).text)
print(cb1.items:get(1).text)
cb1.selected_index = 1
cb1.dropped_width = 160
cb1.dropped_down = true
assert(cb1.dropped_down)
cb1.dropped_down = false

cb2 = ComboBox{parent = window, x = 10, y = 40, h = 80, type = 'dropdown'}
cb2.text = 'dude'
cb2.items:add'Option #1'
cb2.items:add'Option #2'

cb3 = ComboBox{parent = window, x = 120, y = 10, h = 80,
					type = 'simple', vscroll = false, autohscroll = true}
cb3.image_list = ShowcaseImageList()
cb3.items:add{text = 'Option #1', image = 1}
cb3.items:add{text = 'Option #2', image = 3, overlay_image = 4}
cb3.items:add{text = 'Option #3', image = 3, overlay_image = 4}
cb3.items:add{text = 'Option #4', image = 3, overlay_image = 4}
cb3.edit_height = 12
cb3.item_height = 14

MessageLoop()
end

