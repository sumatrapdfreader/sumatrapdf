--oo/basewindow: base class for both overlapping windows and controls.
setfenv(1, require'winapi')
require'winapi.vobject'
require'winapi.handlelist'
require'winapi.window'
require'winapi.windowclasses'
require'winapi.wingdi'


Windows = class(HandleList) --track window objects by their hwnd

function Windows:get_active_window()
	return self:find(GetActiveWindow())
end

function Windows:window_at(p)
	return self:find(WindowFromPoint(p))
end

function Windows:map_point(to_window, ...) --x,y or point
	return MapWindowPoint(nil, to_window.hwnd, ...)
end

function Windows:map_rect(to_window, ...) --x1,y1,x2,y2 or rect
	return MapWindowRect(nil, to_window.hwnd, ...)
end

function Windows:get_cursor_pos(in_window)
	local p = GetCursorPos()
	return in_window and self:map_point(in_window, p) or p
end

Windows = Windows'hwnd' --singleton


MessageRouter = class(Object)

function MessageRouter:__init()
	local function dispatch(hwnd, WM, wParam, lParam)
		local window = Windows:find(hwnd)
		if window then
			return window:__handle_message(WM, wParam, lParam)
		end
		return DefWindowProc(hwnd, WM, wParam, lParam) --catch WM_CREATE etc.
	end
	self.proc = ffi.cast('WNDPROC', dispatch)
end

function MessageRouter:free()
	self.proc:free()
end

MessageRouter = MessageRouter() --singleton


BaseWindow = {
	__class_style_bitmask = bitmask{}, --reserved for windows who own their class
	__style_bitmask = bitmask{},
	__style_ex_bitmask = bitmask{},
	__defaults = {
		visible = true,
		enabled = true,
		x = 0,
		y = 0,
		min_w = 0,
		min_h = 0,
	},
	__init_properties = {},
	__wm_handler_names = index{
		--lifetime
		on_destroy = WM_DESTROY,
		on_destroyed = WM_NCDESTROY,
		--movement
		on_pos_changing = WM_WINDOWPOSCHANGING,
		on_pos_changed = WM_WINDOWPOSCHANGED,
		on_moving = WM_MOVING,
		on_resizing = WM_SIZING,
		on_moved = WM_MOVE,
		on_resized = WM_SIZE,
		on_focus = WM_SETFOCUS,
		on_blur = WM_KILLFOCUS,
		on_enable = WM_ENABLE,
		on_show = WM_SHOWWINDOW,
		--queries
		on_help = WM_HELP,
		on_set_cursor = WM_SETCURSOR,
		--mouse events
		on_mouse_move = WM_MOUSEMOVE,
		on_mouse_over = WM_MOUSEHOVER,
		on_mouse_leave = WM_MOUSELEAVE,
		on_lbutton_double_click = WM_LBUTTONDBLCLK,
		on_lbutton_down = WM_LBUTTONDOWN,
		on_lbutton_up = WM_LBUTTONUP,
		on_mbutton_double_click = WM_MBUTTONDBLCLK,
		on_mbutton_down = WM_MBUTTONDOWN,
		on_mbutton_up = WM_MBUTTONUP,
		on_rbutton_double_click = WM_RBUTTONDBLCLK,
		on_rbutton_down = WM_RBUTTONDOWN,
		on_rbutton_up = WM_RBUTTONUP,
		on_xbutton_double_click = WM_XBUTTONDBLCLK,
		on_xbutton_down = WM_XBUTTONDOWN,
		on_xbutton_up = WM_XBUTTONUP,
		on_mouse_wheel = WM_MOUSEWHEEL,
		on_mouse_hwheel = WM_MOUSEHWHEEL,
		--keyboard events
		on_key_down = WM_KEYDOWN,
		on_key_up = WM_KEYUP,
		on_syskey_down = WM_SYSKEYDOWN,
		on_syskey_up = WM_SYSKEYUP,
		on_key_down_char = WM_CHAR,
		on_syskey_down_char = WM_SYSCHAR,
		on_dead_key_up_char = WM_DEADCHAR,
		on_dead_syskey_down_char = WM_SYSDEADCHAR,
		--system events
		on_timer = WM_TIMER,
	},
	__wm_command_handler_names = {},
	__wm_notify_handler_names = {},
}

BaseWindow = subclass(BaseWindow, VObject)

--subclassing (generate vproperties for style bits)

function BaseWindow:__get_class_style_bit(k)
	return self.__class_style_bitmask:getbit(GetClassStyle(self.hwnd), k)
end

function BaseWindow:__get_style_bit(k)
	return self.__style_bitmask:getbit(GetWindowStyle(self.hwnd), k)
end

function BaseWindow:__get_style_ex_bit(k)
	return self.__style_ex_bitmask:getbit(GetWindowExStyle(self.hwnd), k)
end

function BaseWindow:__set_class_style_bit(k,v)
	SetClassStyle(self.hwnd, self.__class_style_bitmask:setbit(GetClassStyle(self.hwnd), k, v))
	SetWindowPos(self.hwnd, nil, 0, 0, 0, 0, SWP_FRAMECHANGED_ONLY)
end

function BaseWindow:__set_style_bit(k,v)
	SetWindowStyle(self.hwnd, self.__style_bitmask:setbit(GetWindowStyle(self.hwnd), k, v))
	SetWindowPos(self.hwnd, nil, 0, 0, 0, 0, SWP_FRAMECHANGED_ONLY)
end

function BaseWindow:__set_style_ex_bit(k,v)
	SetWindowExStyle(self.hwnd, self.__style_ex_bitmask:set(GetWindowExStyle(self.hwnd), style))
	SetWindowPos(self.hwnd, nil, 0, 0, 0, 0, SWP_FRAMECHANGED_ONLY)
end

function BaseWindow:__subclass(class)
	BaseWindow.__index.__subclass(self, class)
	--generate style vproperties from style bitmask definitions
	if rawget(class, '__class_style_bitmask') then
		class:__gen_vproperties(class.__class_style_bitmask.fields, class.__get_class_style_bit, class.__set_class_style_bit)
		update(class.__class_style_bitmask.fields, self.__class_style_bitmask.fields)
	end
	if rawget(class, '__style_bitmask') then
		class:__gen_vproperties(class.__style_bitmask.fields, class.__get_style_bit, class.__set_style_bit)
		update(class.__style_bitmask.fields, self.__style_bitmask.fields)
	end
	if rawget(class, '__style_ex_bitmask') then
		class:__gen_vproperties(class.__style_ex_bitmask.fields, class.__get_style_ex_bit, class.__set_style_ex_bit)
		update(class.__style_ex_bitmask.fields, self.__style_ex_bitmask.fields)
	end
	--inherit settings from the super class
	if rawget(class, '__defaults') then
		inherit(class.__defaults, self.__defaults)
	end
	if rawget(class, '__init_properties') then
		extend(class.__init_properties, self.__init_properties)
	end
	if rawget(class, '__wm_handler_names') then
		inherit(class.__wm_handler_names, self.__wm_handler_names)
	end
	if rawget(class, '__wm_command_handler_names') then
		inherit(class.__wm_command_handler_names, self.__wm_command_handler_names)
	end
	if rawget(class, '__wm_notify_handler_names') then
		inherit(class.__wm_notify_handler_names, self.__wm_notify_handler_names)
	end
end

--instantiating

function BaseWindow:__before_create(info, args)
	self.__state.min_w = info.min_w
	self.__state.min_h = info.min_h
	self.__state.max_w = info.max_w
	self.__state.max_h = info.max_h
	args.x = info.x
	args.y = info.y
	args.w = info.w
	args.h = info.h
	self:__adjust_wh(args) --adjust t.w,t.h with min/max_w/h
	args.style = self.__style_bitmask:set(args.style or 0, info)
	args.style_ex = self.__style_ex_bitmask:set(args.style_ex or 0, info)
	args.style = bit.bor(args.style, info.visible and WS_VISIBLE or 0,
													info.enabled and 0 or WS_DISABLED)
end

function BaseWindow:__after_create(info, args) end --stub

function BaseWindow:__init(info)
	BaseWindow.__index.__init(self)
	info = inherit(info or {}, self.__defaults)
	if not info.hwnd then
		self.__state = {}
		local args = {}
		self:__before_create(info, args)
		self.hwnd = CreateWindow(args)
		self.font = info.font or GetStockObject(DEFAULT_GUI_FONT)
		self:__after_create(info, args)
	else --we can also wrap an exisiting window given its handle
		self.hwnd = info.hwnd
	end
	Windows:add(self) --register the window so we can find it by hwnd
	if info.visible ~= self.visible then --WS_VISIBLE on the first created window is ignored in some cases
		self.visible = info.visible
	end
	--initialize properties that are extra to CreateWindow() in the prescribed order
	for _,name in ipairs(self.__init_properties) do
		if info[name] then self[name] = info[name] end
	end
end

function BaseWindow:get_info() --recreate the info table to serve for duplication of the window
	local t = {
		min_w = self.min_w,
		min_h = self.min_h,
		max_w = self.max_w,
		max_h = self.max_h,
		x = self.x,
		y = self.y,
		w = self.w,
		h = self.h,
		visible = self.visible,
		enabled = self.enabled,
		font = self.font,
	}
	self.__class_style_bitmask:get(GetClassStyle(self.hwnd), t)
	self.__style_bitmask:get(GetWindowStyle(self.hwnd), t)
	self.__style_ex_bitmask:get(GetWindowExStyle(self.hwnd), t)

	for k in pairs(t) do --remove defaults
		if self.__defaults[k] then t[k] = nil end
	end
	return t
end

--destroing

function BaseWindow:free()
	DestroyWindow(self.hwnd)
end

function BaseWindow:WM_NCDESTROY() --after children are destroyed
	Windows:remove(self)
	disown(self.hwnd) --prevent the __gc on hwnd calling DestroyWindow again
end

--class properties

function BaseWindow:get_background() return GetClassBackground(self.hwnd) end
function BaseWindow:set_background(bg) SetClassBackground(self.hwnd, bg) end

function BaseWindow:get_cursor() return GetClassCursor(self.hwnd) end
function BaseWindow:set_cursor(cursor) SetClassCursor(self.hwnd, cursor) end

function BaseWindow:get_icon() return GetClassIcon(self.hwnd) end
function BaseWindow:set_icon(icon) SetClassIcon(self.hwnd, icon) end

function BaseWindow:get_small_icon() GetClassSmallIcon(self.hwnd) end
function BaseWindow:set_small_icon(icon) SetClassSmallIcon(self.hwnd, icon) end

--properties

function BaseWindow:get_text() return GetWindowText(self.hwnd) end
function BaseWindow:set_text(text) SetWindowText(self.hwnd, text) end

function BaseWindow:set_font(font) SetWindowFont(self.hwnd, font) end
function BaseWindow:get_font() return GetWindowFont(self.hwnd) end

function BaseWindow:get_enabled() return IsWindowEnabled(self.hwnd) end
function BaseWindow:set_enabled(enabled) EnableWindow(self.hwnd, enabled) end
function BaseWindow:enable() self.enabled = true end
function BaseWindow:disable() self.enabled = false end

function BaseWindow:get_focused() return GetFocus() == self.hwnd end
function BaseWindow:focus() SetFocus(self.hwnd) end

function BaseWindow:next_child(last_child)
	if not last_child then return Windows:find(GetFirstChild(self.hwnd)) end
	return Windows:find(GetNextSibling(last_child.hwnd))
end
function BaseWindow:children() --returns a stateless iterator so get_children() wouldn't have worked
	return self.next_child, self
end

function BaseWindow:get_cursor_pos()
	return Windows:get_cursor_pos(self)
end

--properties/visibility

function BaseWindow:get_is_visible() --visible and all parents are visible too
	return IsWindowVisible(self.hwnd)
end

function BaseWindow:get_visible()
	return bit.band(GetWindowStyle(self.hwnd), WS_VISIBLE) == WS_VISIBLE
end

function BaseWindow:show()
	ShowWindow(self.hwnd, SW_SHOW)
	if not self.visible then --the first ever call to ShowWindow with SW_SHOWNORMAL is ignored in some cases
		ShowWindow(self.hwnd, SW_SHOW)
	end
	UpdateWindow(self.hwnd)
end

function BaseWindow:hide()
	ShowWindow(self.hwnd, SW_HIDE)
end

function BaseWindow:set_visible(visible)
	if visible then self:show() else self:hide() end
end

--message routing

function BaseWindow:__handle_message(WM, wParam, lParam)
	--look for a procedural-level handler self:WM_*()
	local handler = self[WM_NAMES[WM]]
	if handler then
		local ret = handler(self, DecodeMessage(WM, wParam, lParam))
		if ret ~= nil then return ret end
	end
	--look for a hi-level handler self:on_*()
	handler = self[self.__wm_handler_names[WM]]
	if handler then
		local ret = handler(self, DecodeMessage(WM, wParam, lParam))
		if ret ~= nil then return ret end
	end
	return self:__default_proc(WM, wParam, lParam)
end

function BaseWindow:__default_proc(WM, wParam, lParam) --controls override this and call CallWindowProc instead
	return DefWindowProc(self.hwnd, WM, wParam, lParam)
end

--WM_COMMAND routing

function BaseWindow:WM_COMMAND(kind, id, command, hwnd)
	if kind == 'control' then
		local window = Windows:find(hwnd)
		if window then --some controls (eg. combobox) create their own child windows which we don't know about)
			local handler = window[window.__wm_command_handler_names[command]]
			if handler then return handler(window) end
		end
	elseif kind == 'menu' then
		--do nothing: our menu class has MNS_NOTIFYBYPOS so we get WM_MENUCOMMAND instead
	elseif kind == 'accelerator' then
		--do nothing: top-level windows handle accelerators
	end
end

--WM_NOTIFY routing

function BaseWindow:WM_NOTIFY(hwnd, code, ...)
	local window = Windows:find(hwnd)
	if window == nil then return end --TODO: find out which window is sending these notifications (listview's header maybe)
	local handler = window[WM_NOTIFY_NAMES[code]]
	--look for a procedural-level handler self:*N_*()
	if handler then
		local ret = handler(window, ...)
		if ret ~= nil then return ret end
	end
	--look for a hi-level handler self:on_*()
	handler = window[window.__wm_notify_handler_names[code]]
	if handler then
		local ret = handler(window, ...)
		if ret ~= nil then return ret end
	end
end

--WM_COMPAREITEM routing

function BaseWindow:WM_COMPAREITEM(hwnd, ci)
	print'WM_COMPAREITEM' --TODO: see this message
	local window = Windows:find(hwnd)
	if window and window.on_compare_items then
		return window:on_compare_items(ci.i1, ci.i2)
	end
end

--positioning

function BaseWindow:__parent_resizing(wp)
	if self.on_parent_resizing then
		self:on_parent_resizing(wp)
	end
end

function BaseWindow:__adjust_wh(t)
	t.w = math.max(t.w, self.min_w or t.w)
	t.h = math.max(t.h, self.min_h or t.h)
	t.w = math.min(t.w, self.max_w or t.w)
	t.h = math.min(t.h, self.max_h or t.h)
end

function BaseWindow:WM_WINDOWPOSCHANGING(wp) --adjust with min/max_w/h and resize children
	if bit.band(wp.flags, SWP_NOSIZE) == SWP_NOSIZE then return end
	self:__adjust_wh(wp)
	for child in self:children() do
		child:__parent_resizing(wp) --children can resize the parent by modifying wp
	end
	return 0
end

function BaseWindow:set_min_w()
	self.rect = self.rect --fake resize
end
BaseWindow.set_min_h = BaseWindow.set_min_w
BaseWindow.set_max_w = BaseWindow.set_min_w
BaseWindow.set_max_h = BaseWindow.set_min_w

function BaseWindow:get_screen_rect()
	return GetWindowRect(self.hwnd)
end

function BaseWindow:set_screen_rect(...)
	local r = RECT(...)
	MapWindowRect(nil, GetParent(self.hwnd), r)
	self:move(r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1)
end

function BaseWindow:get_rect(r)
	return MapWindowRect(nil, GetParent(self.hwnd), GetWindowRect(self.hwnd, r))
end

function BaseWindow:set_rect(...) --x1,y1,x2,y2 or rect
	local r = RECT(...)
	self:move(r.x1, r.y1, r.w, r.h)
end

function BaseWindow:get_client_rect(r)
	return GetClientRect(self.hwnd, r)
end

function BaseWindow:get_client_w()
	return GetClientRect(self.hwnd).x2
end

function BaseWindow:get_client_h()
	return GetClientRect(self.hwnd).y2
end

function BaseWindow:move(x,y,w,h) --use nil to assume current value
	if not (x or y or w or h) then return end
	local missing_xy = (x or y) and not (x and y)
	local missing_wh = (w or h) and not (w and h)
	if missing_xy or missing_wh then
		local r = self.rect
		if missing_xy then
			if not x then x = r.x1 end
			if not y then y = r.y1 end
		end
		if missing_wh then
			if not w then w = r.w end
			if not h then h = r.h end
		end
	end
	local flags = bit.bor(SWP_NOZORDER, SWP_NOOWNERZORDER, SWP_NOACTIVATE,
								x and 0 or SWP_NOMOVE,
								w and 0 or SWP_NOSIZE)
	SetWindowPos(self.hwnd, nil, x or 0, y or 0, w or 0, h or 0, flags)
end

function BaseWindow:resize(w,h)
	self:move(nil,nil,w,h)
end

function BaseWindow:get_x() return self.rect.x1 end
function BaseWindow:get_y() return self.rect.y1 end
function BaseWindow:get_w() return self.rect.w end
function BaseWindow:get_h() return self.rect.h end
function BaseWindow:set_x(x) self:move(x) end
function BaseWindow:set_y(y) self:move(nil,y) end
function BaseWindow:set_w(w) self:resize(w) end
function BaseWindow:set_h(h) self:resize(nil,h) end

function BaseWindow:child_at(...) --x,y or point
	return Windows:find(ChildWindowFromPoint(self.hwnd, ...))
end

function BaseWindow:real_child_at(...) --x,y or point
	return Windows:find(RealChildWindowFromPoint(self.hwnd, ...))
end

function BaseWindow:child_at_recursive(...) --x,y or point
	for w in self:children() do
		local child = w:child_at_recursive(...)
		if child then return child end
	end
	return self:child_at(...)
end

function BaseWindow:real_child_at_recursive(...) --x,y or point
	for w in self:children() do
		local child = w:real_child_at_recursive(...)
		if child then return child end
	end
	return self:real_child_at(...)
end

function BaseWindow:map_point(to_window, ...) --x,y or point
	return MapWindowPoint(self.hwnd, to_window and to_window.hwnd, ...)
end

function BaseWindow:map_rect(to_window, ...) --x1,y1,x2,y2 or rect
	return MapWindowRect(self.hwnd, to_window and to_window.hwnd, ...)
end

--positioning/z-order

function BaseWindow:bring_below(window)
	SetWindowPos(self.hwnd, window.window, 0, 0, 0, 0, SWP_ZORDER_CHANGED_ONLY)
end

function BaseWindow:bring_above(window)
	SetWindowPos(self.hwnd, GetPrevSibling(window.window) or HWND_TOP, 0, 0, 0, 0, SWP_ZORDER_CHANGED_ONLY)
end

function BaseWindow:bring_to_front()
	SetWindowPos(self.hwnd, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER_CHANGED_ONLY)
end

function BaseWindow:send_to_back()
	SetWindowPos(self.hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_ZORDER_CHANGED_ONLY)
end

--custom painting

function BaseWindow:set_updating(updating)
	if not self.visible then return end
	SetRedraw(self.hwnd, not updating)
end

function BaseWindow:batch_update(f,...) --can't change self.updating inside f
	if not self.visible or self.updating then
		f(...)
	end
	self.updating = true
	local ok,err = pcall(f,...)
	self.updating = nil
	self:redraw()
	assert(ok, err)
end

function BaseWindow:redraw()
	RedrawWindow(self.hwnd, nil, bit.bor(RDW_ERASE, RDW_FRAME, RDW_INVALIDATE, RDW_ALLCHILDREN))
end

function BaseWindow:invalidate()
	InvalidateRect(self.hwnd, nil, true)
end

function BaseWindow:WM_PAINT()
	if self.on_paint then
		self.__paintstruct = types.PAINTSTRUCT(self.__paintstruct)
		local hdc = BeginPaint(self.hwnd, self.__paintstruct)
		self:on_paint(hdc)
		EndPaint(self.hwnd, self.__paintstruct)
		return 0
	end
end

--drag/drop

function BaseWindow:dragging(...)
	return DragDetect(self.hwnd, POINT(...))
end

--timer setting & routing

function BaseWindow:settimer(timeout_ms, handler, id)
	id = SetTimer(self.hwnd, id or 1, timeout_ms)
	if not self.__timers then self.__timers = {} end
	self.__timers[id] = handler
	return id
end

function BaseWindow:stoptimer(id)
	KillTimer(self.hwnd, id or 1)
	self.__timers[id] = nil
end

function BaseWindow:WM_TIMER(id)
	local callback = self.__timers and self.__timers[id]
	if callback then callback(self, id) end
end

