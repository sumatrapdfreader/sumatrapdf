require'setupluafiles'
local winapi = require'winapi'
require'mywin'
require'winapi.messageloop'

local c = winapi.MyWindow{title = 'Main',
	help_button = false, maximize_button = true, minimize_button = true,
	autoquit = true, w = 500, h = 300, visible = false}
c:show()

function c:WM_GETDLGCODE()
	return DLGC_WANTALLKEYS
end

function c:on_key_down(vk, flags)
	print('WM_KEYDOWN', vk, flags)
end

function c:on_paint(hdc)
	print(string.format("on_paint(hdc=%p)", hdc))
	local brWhite = winapi.GetStockObject(winapi.WHITE_BRUSH)
	local brBlack = winapi.GetStockObject(winapi.BLACK_BRUSH)
	--print(string.format("whiteBrush: %p", whiteBrush))
	--local prev1 = winapi.SelectObject(hdc, white)
	--local r = winapi.RECT(0, 0, 40, 40)
	local r = c:get_client_rect()
	print(r)
	winapi.FillRect(hdc, r, brWhite)
	local newDx = r.w / 2
	local newDy = r.h / 2
	local x = (r.w - newDx) / 2
	local y = (r.h - newDy) / 2
	r = winapi.RECT(x, y, x + newDx, y + newDy)
	winapi.FillRect(hdc, r, brBlack)
	--winapi.SelectObject(hdc, prev1)
	print(r)
end

winapi.MessageLoop()
