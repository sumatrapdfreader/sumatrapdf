--oo/window: overlapping (aka top-level) windows.
setfenv(1, require'winapi')
require'winapi.basewindowclass'
require'winapi.menuclass'
require'winapi.windowclasses'
require'winapi.color'
require'winapi.cursor'
require'winapi.waitemlistclass'

Window = subclass({
	__class_style_bitmask = bitmask{ --only static, frame styles here
		noclose = CS_NOCLOSE, --disable close button and ALT+F4
		dropshadow = CS_DROPSHADOW, --only for non-movable windows
		own_dc = CS_OWNDC, --for opengl or other purposes
		hredraw = CS_HREDRAW,
		vredraw = CS_VREDRAW,
	},
	__style_bitmask = bitmask{ --only static, frame styles here
		border = WS_BORDER,
		titlebar = WS_CAPTION,
		minimize_button = WS_MINIMIZEBOX,
		maximize_button = WS_MAXIMIZEBOX,
		sizeable = WS_SIZEBOX,
		sysmenu = WS_SYSMENU, --not setting this hides all buttons
		vscroll = WS_VSCROLL,
		hscroll = WS_HSCROLL,
		dialog_frame = WS_DLGFRAME,
	},
	__style_ex_bitmask = bitmask{
		help_button = WS_EX_CONTEXTHELP, --only shown if both minimize and maximize buttons are hidden
		tool_window = WS_EX_TOOLWINDOW,
		transparent = WS_EX_TRANSPARENT,
	},
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
	__init_properties = {
		'state', 'menu',
	},
	__wm_handler_names = index{
		on_close = WM_CLOSE,
		on_quit = WM_QUIT,
		on_activate_app = WM_ACTIVATEAPP,
		on_query_open = WM_QUERYOPEN,
		--system changes
		on_query_end_session = WM_QUERYENDSESSION,
		on_end_session = WM_ENDSESSION,
		on_system_color_change = WM_SYSCOLORCHANGE,
		on_settings_change = WM_SETTINGCHANGE,
		on_device_mode_change = WM_DEVMODECHANGE,
		on_fonts_change = WM_FONTCHANGE,
		on_time_change = WM_TIMECHANGE,
		on_spooler_change = WM_SPOOLERSTATUS,
		on_input_language_change = WM_INPUTLANGCHANGE,
		on_user_change = WM_USERCHANGED,
		on_display_change = WM_DISPLAYCHANGE,
	},
}, BaseWindow)

--instantiating

local function name_generator(format)
	local n = 0
	return function()
		n = n + 1
		return string.format(format, n)
	end
end
local gen_classname = name_generator'Window%d'

function Window:__before_create_class(info, args)
	args.name = gen_classname()
	args.style = self.__class_style_bitmask:set(args.style or 0, info)
	args.proc = MessageRouter.proc
	args.icon = info.icon
	args.small_icon = info.small_icon
	args.cursor = info.cursor
	args.background = info.background
end

function Window:__before_create(info, args)
	Window.__index.__before_create(self, info, args)

	local class_args = {}
	self:__before_create_class(info, class_args)
	self.__winclass = RegisterClass(class_args)
	args.class = self.__winclass

	args.parent = info.owner and info.owner.hwnd
	args.text = info.title

	args.style = bit.bor(args.style, WS_OVERLAPPED, WS_CLIPCHILDREN, WS_CLIPSIBLINGS)
	args.style_ex = bit.bor(args.style_ex, WS_EX_CONTROLPARENT, --recurse when looking for the next control with WS_TABSTOP
									info.topmost and WS_EX_TOPMOST or 0)

	self.__state.maximized_pos = info.maximized_pos
	self.__state.maximized_size = info.maximized_size
	self.autoquit = info.autoquit --quit the app when the window closes

	if info.state then --we can't allow WS_VISIBLE, that will show the window in its normal state
		args.style = setbit(args.style, WS_VISIBLE, false)
		self.__show_state = info.state
	end
end

function Window:__init(info)
	Window.__index.__init(self, info)
	self.accelerators = WAItemList(self)
	if self.__show_state and info.visible then self:show() end
end

--destroying

function Window:close()
	CloseWindow(self.hwnd)
end

function Window:WM_NCDESTROY()
	Window.__index.WM_NCDESTROY(self)
	if self.menu then self.menu:free() end
	PostMessage(nil, WM_UNREGISTER_CLASS, self.__winclass)
	if self.autoquit then
		PostQuitMessage()
	end
end

function Window:WM_CTLCOLORSTATIC(wParam, lParam)
	 --TODO: fix group box
	 do return end
	 local hBackground = CreateSolidBrush(RGB(0, 0, 0))
	 local hdc = ffi.cast('HDC', wParam)
    SetBkMode(hdc, OPAQUE)
    SetTextColor(hdc, RGB(100, 100, 0))
	 return tonumber(hBackground)
end

--onwership

function Window:get_owner() --note there's no set_owner: owner can't be changed
	return Windows:find(GetOwner(self.hwnd))
end

--positioning

function Window:WM_GETMINMAXINFO(info)
	if self.maximized_pos then info.maximized_pos = self.maximized_pos end
	if self.maximized_size then info.maximized_size = self.maximized_size end
	return 0
end

--window properties

Window.get_title = BaseWindow.get_text
Window.set_title = BaseWindow.set_text

function Window:get_active() return GetActiveWindow() == self.hwnd end
function Window:activate() SetActiveWindow(self.hwnd) end

local window_state_names = { --GetWindowPlacement distills states to these 3
	[SW_SHOWNORMAL] = 'normal',
	[SW_SHOWMAXIMIZED] = 'maximized',
	[SW_SHOWMINIMIZED] = 'minimized',
}
function Window:get_state()
	local wpl = GetWindowPlacement(self.hwnd)
	return window_state_names[wpl.command]
end

function Window:minimize() ShowWindow(self.hwnd, SW_MINIMIZE) end
function Window:maximize() ShowWindow(self.hwnd, SW_SHOWMAXIMIZED) end
function Window:restore() ShowWindow(self.hwnd, SW_RESTORE) end

local function set_state(self, st)
	if st == 'maximized' then self:maximize() end
	if st == 'minimized' then self:minimize() end
	if st == 'normal' then self:restore() end
end

function Window:set_state(st)
	if not self.visible then
		self.__show_state = st --set a promise to set this state on the next show()
	else
		set_state(self, st)
	end
end

function Window:show()
	if self.__show_state then
		set_state(self, self.__show_state)
		self.__show_state = nil
	else
		Window.__index.show(self)
	end
end

function Window:set_maximized_pos()
	self.rect = self.rect --trigger a resize
end
Window.set_maximized_size = Window.set_maximized_pos

function Window:get_topmost()
	return bit.band(GetWindowExStyle(self.hwnd), WS_EX_TOPMOST) == WS_EX_TOPMOST
end

function Window:set_topmost(topmost)
	SetWindowPos(self.hwnd, topmost and HWND_TOPMOST or HWND_NOTOPMOST,
						0, 0, 0, 0, bit.bor(SWP_NOSIZE, SWP_NOMOVE, SWP_NOACTIVATE))
end

--menus

function Window:get_menu()
	return Menus:find(GetMenu(self.hwnd))
end

function Window:set_menu(menu)
	if self.menu then self.menu:__set_window(nil) end
	SetMenu(self.hwnd, menu and menu.hmenu)
	if menu then menu:__set_window(self) end
end

function Window:WM_MENUCOMMAND(menu, i)
	menu = Menus:find(menu)
	if menu.WM_MENUCOMMAND then menu:WM_MENUCOMMAND(i) end
end

--accelerators

function Window:WM_COMMAND(kind, id, ...)
	if kind == 'accelerator' then
		self.accelerators:WM_COMMAND(id) --route message to individual accelerators
	end
	Window.__index.WM_COMMAND(self, kind, id, ...)
end

--events

function Window:WM_ACTIVATE(WA, minimized, other_hwnd)
	if WA == WA_ACTIVE or WA == WA_CLICKACTIVE then
		if self.on_activate then self:on_activate(Windows:find(other_hwnd)) end
	elseif WA == WA_INACTIVE then
		if self.on_deactivate then self:on_deactivate(Windows:find(other_hwnd)) end
	end
end

--showcase

if not ... then
require'winapi.messageloop'
require'winapi.icon'
require'winapi.font'

local c = Window{title = 'Main',
	help_button = true, maximize_button = false, minimize_button = false,
	autoquit = true, w = 500, h = 300, visible = false}
c:show()

--c:restore()
--c:maximize()
c.cursor = LoadCursor(IDC_HAND)
c.icon = LoadIconFromInstance(IDI_INFORMATION)
--[[
print(c.visible, c.state)
c:maximize()
print(c.visible, c.state)
c:minimize()
print(c.visible, c.state)
c:show()
print(c.visible, c.state)
c:restore()
print(c.visible, c.state)
]]

--[[
c3 = Window{topmost = true, title='Topmost', h=200,w=200}

local c2 = Window{title = 'Owned by Main', dialog_frame = true, w = 500, h = 300, visible = true, owner = c}
--c2.min_size = {h=100, h=100}
c2.max_size = {w=300, h=300}
]]

function c:WM_GETDLGCODE()
	return DLGC_WANTALLKEYS
end

function c:on_key_down(vk, flags)
	print('WM_KEYDOWN', vk, flags)
end

MessageLoop()

end

