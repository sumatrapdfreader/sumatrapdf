require'setupluafiles'
local winapi = require'winapi'
require'winapi.window'

local curr_depth = 0

local function spaces(depth)
	local s = ""
	for i=1, depth do
		s = s .. "  "
	end
	return s
end

-- TODO: neither GetWindowParent() implementation does what I think it should
--local function GetWindowParent(hwnd) return ffi.cast('HWND', winapi.GetWindowLong(hwnd, winapi.GWL_HWNDPARENT)) end
local function GetWindowParent(hwnd) return winapi.GetOwner(hwnd) end
--local function GetWindowParent(hwnd) return winapi.GetParent(hwnd) end

-- return 0 for top-level window, number of parents otherwise
-- TODO: this doesn't do
local function windowNesting(hwnd)
	local n = 0
	hwnd = GetWindowParent(hwnd)
	while hwnd do
		n = n + 1
		hwnd = GetWindowParent(hwnd)
	end
	return n
end

local function printHwndInfo(hwnd)
	local visible = winapi.IsWindowVisible(hwnd)
	if not visible then return end

	local placement = winapi.GetWindowPlacement(hwnd)
	local txt = winapi.GetWindowText(hwnd)
	local r = placement.normalpos
	local dx = r.right - r.left
	local dy = r.bottom - r.top
	local n = windowNesting(hwnd)
	local s = spaces(n)
	print(string.format("%s%4dx%4d", s, dx, dy), txt)
end

local function printHwndInfo2(hwnd, indent)
	local visible = winapi.IsWindowVisible(hwnd)
	if not visible then return end

	local placement = winapi.GetWindowPlacement(hwnd)
	local txt = winapi.GetWindowText(hwnd)
	local r = placement.normalpos
	local dx = r.right - r.left
	local dy = r.bottom - r.top
	local n = windowNesting(hwnd)
	local s = spaces(indent)
	print(string.format("%s%4d x%5d", s, dx, dy), txt)
end

local function printChildHwndInfo(idx, hwnd)
	printHwndInfo(hwnd)
end

local function printWindows()
	local topLevelWindows = winapi.EnumChildWindows(nil)
	local function printTopLevelHwndInfo(idx, hwnd)
		printHwndInfo(hwnd)
		table.foreach(winapi.EnumChildWindows(hwnd), printChildHwndInfo)
	end
	table.foreach(topLevelWindows, printTopLevelHwndInfo)
end

local function printWindows2()
	local topLevelWindows = winapi.EnumChildWindows(nil)
	local function printTopLevel(idx, hwnd)
		printHwndInfo2(hwnd, 0)
		local indent = 1
	end
	table.foreach(topLevelWindows, printTopLevelHwndInfo)
end

local function printTopLevelWindows()
	local topLevelWindows = winapi.EnumChildWindows(nil)
	local function printTopLevel(idx, hwnd)
		printHwndInfo2(hwnd, 0)
		local indent = 1
	end
	table.foreach(topLevelWindows, printTopLevel)
end

printTopLevelWindows()
-- printWindows()
