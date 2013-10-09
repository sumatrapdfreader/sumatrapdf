--oo/checkbox: checkbox control.
setfenv(1, require'winapi')
require'winapi.basebuttonclass'

CheckBox = subclass({
	__style_bitmask = bitmask{
		box_align = {
			left = 0,
			right = BS_LEFTTEXT,
		},
		pushlike = BS_PUSHLIKE,
		type = { --TODO: make two orthogonal properties out of these: autocheck and 3state or allow_grayed
			twostate = BS_CHECKBOX,
			threestate = BS_3STATE,
			twostate_autocheck = BS_AUTOCHECKBOX,
			threestate_autocheck = BS_AUTO3STATE,
		},
	},
	__defaults = {
		type = 'twostate_autocheck',
		text = 'Option',
		w = 100, h = 24,
		text_margin = {20,5},
	},
	__init_properties = {
		'checked'
	},
}, BaseButton)

local button_states = {
	[false] = BST_UNCHECKED,
	[true] = BST_CHECKED,
	indeterminate = BST_INDETERMINATE,
}
local button_state_names = index(button_states)
function CheckBox:set_checked(checked)
	Button_SetCheck(self.hwnd, button_states[checked])
end
function CheckBox:get_checked()
	return button_state_names[bit.band(Button_GetCheck(self.hwnd), 3)]
end

--showcase

if not ... then
require'winapi.showcase'
local window = ShowcaseWindow{w=300,h=200}
local cb1 = CheckBox{parent = window, w = 200, text = 'I am The Ocean',
							checked = 'indeterminate', image_list = {image_list = ShowcaseImageList()},
							type = 'threestate_autocheck', align = 'left', halign = 'center',
							box_align = 'right', flat = true}
function cb1:on_click() print'b1 clicked' end

local cb2 = CheckBox{parent = window, y = 30, type = 'threestate_autocheck', pushlike = true}

local cb3 = CheckBox{parent = window, y = 60, w = 150, h = 50,
							word_wrap = true, valign = 'top', double_clicks = true,
							text = "I'm a cheeeckbox and I'm ok. I sleep all night and I work all day."}
function cb3:on_double_click() print 'b3 dbl-clicked' end

MessageLoop()
end


