-- ffi = require'ffi'
-- require'winapi.windowclass'
-- require'winapi.messageloop'
-- require'winapi.icon'
-- require'winapi.font'
local winapi = require'winapi'
require'winapi.messageloop'
require'winapi.vkcodes'
require'winapi.keyboard'

local c = winapi.Window{title = 'Main',
	help_button = false, maximize_button = true, minimize_button = true,
	autoquit = true, w = 500, h = 300, visible = false}
c:show()

function c:WM_GETDLGCODE()
	return DLGC_WANTALLKEYS
end

function c:on_key_down(vk, flags)
	print('WM_KEYDOWN', vk, flags)
end

-- TODO: why is it only called on resize (when getting bigger)? Is it window flags?
-- TODO: why doesn't it paint the black rectangle?
function c:on_paint(hdc)
	print(string.format("on_paint(hdc=%p)", hdc))
	local whiteBrush = winapi.GetStockObject(winapi.BLACK_BRUSH)
	--print(string.format("whiteBrush: %p", whiteBrush))
	--local prev1 = winapi.SelectObject(hdc, white)
	--local r = winapi.RECT(0, 0, 40, 40)
	local r = c:get_client_rect()
	winapi.FillRect(hdc, r, whiteBrush)
	--winapi.FillRect(hdc, {0, 0, 10, 10}, whiteBrush)
	--winapi.SelectObject(hdc, prev1)
	print(r)
end

winapi.MessageLoop()
