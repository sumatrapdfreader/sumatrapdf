setfenv(1, require'winapi')
require'winapi.windowclass'
require'winapi.color'
require'winapi.cursor'

--[[
MyWindow patches Window to add CS_HREDRAW and CS_VREDRAW by default and
calling Invalidate() when showing the window.

TODO: I'm thinking there is a shorter way than duplicating all of
__class_style_bitmask and __defaults
]]--

MyWindow = subclass({
	-- TODO: is there an easier way to add this?
	-- I only need to add hredraw and vredraw
	__class_style_bitmask = bitmask{ --only static, frame styles here
		noclose = CS_NOCLOSE, --disable close button and ALT+F4
		dropshadow = CS_DROPSHADOW, --only for non-movable windows
		own_dc = CS_OWNDC, --for opengl or other purposes
		hredraw = CS_HREDRAW,
		vredraw = CS_VREDRAW,
	},
	-- TODO: is there an easier way to add this?
	-- I only need to add hredraw and vredraw
	__defaults = {
		--class style bits
		noclose = false,
		dropshadow = false,
		own_dc = false,
		hredraw = true,
		vredraw = true,
		--window style bits
		border = true,
		titlebar = true,
		minimize_button = true,
		maximize_button = true,
		sizeable = true,
		sysmenu = true,
		vscroll = false,
		hscroll = false,
		dialog_frame = false,
		--window ex style bits
		help_button = false,
		tool_window = false,
		transparent = false,
		--class properties
		background = COLOR_WINDOW,
		cursor = LoadCursor(IDC_ARROW),
		--window properties
		title = 'Untitled',
		x = CW_USEDEFAULT,
		y = CW_USEDEFAULT,
		w = CW_USEDEFAULT,
		h = CW_USEDEFAULT,
		autoquit = false,
		state = nil,
		menu = nil,
	},
	}, Window)

function MyWindow:show()
	MyWindow.__index.show(self)
	-- Invalidate() is needed to trigger WM_PAINT when first shown the window
	self:invalidate()
end
