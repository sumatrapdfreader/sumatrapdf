--oo/button: push-button control.
setfenv(1, require'winapi')
require'winapi.basebuttonclass'

Button = {
	__style_bitmask = bitmask{
		default = BS_DEFPUSHBUTTON,
	},
	__defaults = {
		text = '&OK',
		w = 100, h = 24,
		text_margin = {20,5}, --applied when autosize = true
	},
	__init_properties = {
	'text_margin', 'pushed', 'autosize'
	},
}

subclass(Button, BaseButton)

function Button:__before_create(info, args)
	Button.__index.__before_create(self, info, args)
	args.style = bit.bor(args.style, BS_PUSHBUTTON)
end

function Button:get_ideal_size(w,h) --fixing w or h work on vista+
	local size = SIZE()
	size.w = w or 0
	size.h = h or 0
	size = Button_GetIdealSize(self.hwnd, size)
	return {w = size.w, h = size.h}
end

function Button:__checksize()
	if self.autosize then
		local size = self.ideal_size
		self:resize(size.w, size.h)
	end
end

function Button:set_autosize(yes)
	self:__checksize()
end

function Button:set_text_margin(margin) --only works when autosize = true
	local rect = RECT()
	rect.x1 = margin.w or margin[1]
	rect.y1 = margin.h or margin[2]
	Button_SetTextMargin(self.hwnd, rect)
	self:__checksize()
end
function Button:get_text_margin()
	local rect = Button_GetTextMargin(self.hwnd)
	return {w = rect.x1, h = rect.y1}
end

function Button:set_pushed(pushed) Button_SetState(self.hwnd, pushed) end
function Button:get_pushed() return Button_GetState(self.hwnd) end


--showcase

if not ... then
require'winapi.showcase'
require'winapi.icon'
local window = ShowcaseWindow{w=300,h=300}
local b1 = Button{parent = window, default = true}
b1:focus()
function b1:on_click() print'b1 clicked' end
--b1:__inspect()

local b2 = Button{parent = window, y = 30, h = 40,
						double_clicks = true, valign = 'bottom', halign = 'right',
						image_list = {image_list = ShowcaseImageList(), align = 'center'}}
function b2:on_click() print'b2 clicked' end
function b2:on_focus() print'b2 focused' end
function b2:on_blur() print'b2 blured' end
function b2:on_double_click() print'b2 dbl-clicked' end
b2.pushed = true

b3 = Button{parent = window, y = 90, w = 100, h = 100, autosize = true, text_margin = {30,30}}
b3.icon = LoadIconFromInstance(IDI_INFORMATION)
b3:click()

b4 = Button{parent = window, y = 200, flat = true}
b5 = Button{parent = window, x = 110, h = 70, w = 120,
				text = 'I live in a society where pizza gets to my house before the police',
				word_wrap = true, autosize = false}
b5.word_wrap = true

MessageLoop()
end

