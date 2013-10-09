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
	local dx = (r.right - r.left)
	local newDx = dx / 2
	local dy = (r.bottom - r.top)
	local newDy = dy / 2
	local x = (dx - newDx) / 2
	local y = (dy - newDy) / 2
	r = winapi.RECT(x, y, x + newDx, y + newDy)
	winapi.FillRect(hdc, r, brBlack)
	--winapi.SelectObject(hdc, prev1)
	print(r)
end

winapi.MessageLoop()
