--oo/toolbar: standard toolbar control.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.itemlist'
require'winapi.toolbar'

TBItemList = class(ItemList)

function TBItemList:add(i, item)
	if not item then i,item = nil,i end --i is optional
	if i then
		Toolbar_InsertButton(self.hwnd, i, item)
	else
		Toolbar_AddButton(self.hwnd, item)
	end
end

function TBItemList:remove(i)
	return Tooldbar_DeleteButton(self.hwnd, i)
end

function TBItemList:set(i, item)
	Toolbar_SetButtonInfo(self.hwnd, i, item)
end

function TBItemList:get(i)
	return Toolbar_GetButtonInfo(self.hwnd, i)
end

function TBItemList:get_count()
	return Toolbar_GetButtonCount(self.hwnd)
end

Toolbar = subclass({
	__style_bitmask = bitmask{
		customizable = CCS_ADJUSTABLE,
		tooltips = TBSTYLE_TOOLTIPS,
		multiline = TBSTYLE_WRAPABLE,
		alt_drag = TBSTYLE_ALTDRAG, --only if customizable
		flat = TBSTYLE_FLAT,
		list = TBSTYLE_LIST,  --not resettable
		custom_erase_background = TBSTYLE_CUSTOMERASE,
		is_drop_target = TBSTYLE_REGISTERDROP,
		transparent = TBSTYLE_TRANSPARENT, --not resettable
		no_divider = CCS_NODIVIDER,
		no_align = CCS_NOPARENTALIGN,
	},
	__style_ex_bitmask = bitmask{
		mixed_buttons = TBSTYLE_EX_MIXEDBUTTONS,
		hide_clipped_buttons = TBSTYLE_EX_HIDECLIPPEDBUTTONS,
		draw_drop_down_arrows = TBSTYLE_EX_DRAWDDARROWS, --not resettable
		double_buffer = TBSTYLE_EX_DOUBLEBUFFER,
	},
	__defaults = {
		w = 400, h = 48,
	},
	__init_properties = {
		'image_list',
	},
	__wm_command_handler_names = index{
	},
}, Control)

function Toolbar:__before_create(info, args)
	Toolbar.__index.__before_create(self, info, args)
	args.class = TOOLBARCLASSNAME
end

function Toolbar:__init(info)
	Comctl_SetVersion(self.hwnd, 6)
	Toolbar.__index.__init(self, info)
	self.items = TBItemList(self)
end

function Toolbar:set_image_list(iml)
	Toolbar_SetImageList(self.hwnd, iml.himl)
end

function Toolbar:get_image_list(iml)
	ImageLists:find(Toolbar_GetImageList(self.hwnd))
end

