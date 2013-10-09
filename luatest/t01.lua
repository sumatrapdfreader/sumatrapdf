local winapi = require'winapi'
require'winapi.window'

local curr_depth = 0

local function spaces(depth)
	local s = ""
	-- TODO: why i=1 and not 0?
	for i=1, depth do
		s = s .. "  "
	end
	return s
end

local function printHwndRecur(idx, hwnd)
	local visible = winapi.IsWindowVisible(hwnd)
	if visible then
		local placement = winapi.GetWindowPlacement(hwnd)
		local txt = winapi.GetWindowText(hwnd)
		local r = placement.normalpos
		local dx = r.right - r.left
		local dy = r.bottom - r.top
		local s = spaces(curr_depth)
		print(string.format("%s%dx%d", s, dx, dy), txt)
		curr_depth = curr_depth + 1
		table.foreach(winapi.EnumChildWindows(hwnd), printHwndRecur)
		curr_depth = curr_depth - 1
	end
end

local function printAllWindows()
	curr_depth = 0
	table.foreach(winapi.EnumChildWindows(nil), printHwndRecur)
end

printAllWindows()
