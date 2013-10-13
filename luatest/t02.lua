require'setupluafiles'
local winapi = require'winapi'
require'winapi.showcase'
require'winapi.icon'
require'winapi.buttonclass'

local window = winapi.ShowcaseWindow{w=300,h=300}
local b1 = winapi.Button{parent = window, default = true}
b1:focus()
function b1:on_click() print'b1 clicked' end
--b1:__inspect()

local b2 = winapi.Button{parent = window, y = 30, h = 40,
						double_clicks = true, valign = 'bottom', halign = 'right',
						image_list = {image_list = winapi.ShowcaseImageList(), align = 'center'}}
function b2:on_click() print'b2 clicked' end
function b2:on_focus() print'b2 focused' end
function b2:on_blur() print'b2 blured' end
function b2:on_double_click() print'b2 dbl-clicked' end
b2.pushed = true

b3 = winapi.Button{parent = window, y = 90, w = 100, h = 100, autosize = true, text_margin = {30,30}}
--b3.icon = winapi.LoadIconFromInstance(IDI_INFORMATION)
b3:click()

b4 = winapi.Button{parent = window, y = 200, flat = true}
b5 = winapi.Button{parent = window, x = 110, h = 70, w = 120,
				text = 'I live in a society where pizza gets to my house before the police',
				word_wrap = true, autosize = false}
b5.word_wrap = true

winapi.MessageLoop()
