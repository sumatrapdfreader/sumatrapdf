--oo/basebutton: base class for push-buttons, checkboxes, radio buttons.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.button'
require'winapi.imagelistclass'

BaseButton = subclass({
	__style_bitmask = bitmask{
		tabstop = WS_TABSTOP,
		halign = {
			left = BS_LEFT,
			right = BS_RIGHT,
			center = BS_CENTER,
		},
		valign = {
			top = BS_TOP,
			bottom = BS_BOTTOM,
			center = BS_VCENTER,
		},
		word_wrap = BS_MULTILINE,
		flat = BS_FLAT,
		double_clicks = BS_NOTIFY, --wait for a double click instead of clicking again (also for focus/blur events)
	},
	__defaults = {
		tabstop = true,
		__wm_command_handler_names = index{
			on_click = BN_CLICKED,
			on_double_click = BN_DOUBLECLICKED,
			on_focus = BN_SETFOCUS,
			on_blur = BN_KILLFOCUS,
		},
	},
	__init_properties = {
		'image_list', 'icon', 'bitmap'
	},
}, Control)

function BaseButton:__before_create(info, args)
	BaseButton.__index.__before_create(self, info, args)
	args.class = WC_BUTTON
	args.text = info.text
end

function BaseButton:__checksize() end --signal content change (for autosize feature of push-buttons)

function BaseButton:click() Button_Click(self.hwnd) end

local iml_align = {
	left = BUTTON_IMAGELIST_ALIGN_LEFT,
	right = BUTTON_IMAGELIST_ALIGN_RIGHT,
	top = BUTTON_IMAGELIST_ALIGN_TOP,
	bottom = BUTTON_IMAGELIST_ALIGN_BOTTOM,
	center = BUTTON_IMAGELIST_ALIGN_CENTER,
}
local iml_align_names = index(iml_align)
function BaseButton:set_image_list(iml)
	local imls = BUTTON_IMAGELIST()
	imls.imagelist = iml.image_list.himl
	imls.align = iml_align[iml.align]
	if iml.margin then imls.margin = iml.margin end
	Button_SetImageList(self.hwnd, imls)
	self:__checksize()
end
function BaseButton:get_image_list()
	local iml = Button_GetImageList(self.hwnd)
	return {
		image_list = ImageList(iml.imagelist),
		align = iml_align_names[iml.align],
		margin = iml.margin,
	}
end

function BaseButton:get_text() return GetWindowText(self.hwnd) end
function BaseButton:set_text(text) SetWindowText(self.hwnd, text) end

function BaseButton:set_icon(icon)
	SetWindowStyle(self.hwnd, setbit(GetWindowStyle(self.hwnd), BS_ICON, icon))
	Button_SetIcon(self.hwnd, icon)
	self:__checksize()
end
function BaseButton:get_icon() return Button_GetIcon(self.hwnd) end

function BaseButton:set_bitmap(bitmap)
	SetWindowStyle(self.hwnd, setbit(GetWindowStyle(self.hwnd), BS_BITMAP, bitmap))
	Button_SetBitmap(self.hwnd, bitmap)
	self:__checksize()
end
function BaseButton:get_bitmap() return Button_GetBitmap(self.hwnd) end

function BaseButton:set_text(text)
	BaseButton.__index.set_text(self, text)
	self:__checksize()
end

