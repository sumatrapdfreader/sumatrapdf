--oo/control: base class for standard controls.
setfenv(1, require'winapi')
require'winapi.basewindowclass'
require'winapi.comctl'

Control = subclass({
	__defaults = {
		anchors = {
			left = true,
			top = true,
			right = false,
			bottom = false,
		},
	},
}, BaseWindow)

function Control:__before_create(info, args)
	Control.__index.__before_create(self, info, args)
	self.anchors = info.anchors
	--parent is either a window object or a handle. if it's a handle, self.parent will return nil.
	args.parent = info.parent and info.parent.hwnd or info.parent
	args.style = bit.bor(args.style, args.parent and WS_CHILD or WS_POPUP, WS_CLIPSIBLINGS, WS_CLIPCHILDREN)
end

function Control:__init(info)
	Control.__index.__init(self, info)
	--subclass the control to intercept the messages sent to it.
	self.__prev_proc = SetWindowLong(self.hwnd, GWL_WNDPROC, MessageRouter.proc)
end

function Control:get_info() --recreate the info table to serve for duplication of the window
	return update(Control.__index.get_info(self), {
		parent = self.parent,
	})
end

function Control:__default_proc(WM, wParam, lParam)
	return CallWindowProc(self.__prev_proc, self.hwnd, WM, wParam, lParam)
end

function Control:get_parent()
	return Windows:find(GetParent(self.hwnd))
end

function Control:set_parent(parent)
	local old_parent = self.parent
	if parent and not old_parent then --popup windows can become child windows
		SetWindowStyle(self.hwnd, setbits(GetWindowStyle(self.hwnd),
														bit.bor(WS_POPUP, WS_CHILD), WS_CHILD))
	end
	SetParent(self.hwnd, parent and parent.hwnd)
	pin(self, parent)
	if not parent and old_parent then --child windows can become popup windows (diff. from overlapped)
		SetWindowStyle(self.hwnd, setbits(GetWindowStyle(self.hwnd),
														bit.bor(WS_POPUP, WS_CHILD), WS_POPUP))
	end
	if (parent and not old_parent) or (not parent and old_parent) then
		SetWindowPos(self.hwnd, nil, 0, 0, 0, 0, SWP_FRAMECHANGED_ONLY)
	end
	--TODO: find out which of the windows is enough to send this message to.
	ChangeUIState(self.hwnd, UIS_INITIALIZE, bit.bor(UISF_HIDEACCEL, UISF_HIDEFOCUS))
	if old_parent then ChangeUIState(old_parent.hwnd, UIS_INITIALIZE, bit.bor(UISF_HIDEACCEL, UISF_HIDEFOCUS)) end
	if parent then ChangeUIState(parent.hwnd, UIS_INITIALIZE, bit.bor(UISF_HIDEACCEL, UISF_HIDEFOCUS)) end
end

--delphi-style anchors: resize when parent is resized based on anchors spec

local function anchor_dim(self, left, right, enlargement, x, w, adjustment)
	local xofs, wofs
	if right then
		if left then --left and right: resize
			wofs = enlargement + (adjustment or 0)
		else --only right: move
			xofs = enlargement
		end
	end --only left: do nothing; no left/no right: undefined, we choose to do nothing.
	x = xofs and x + xofs
	w = wofs and w + wofs
	return x,w
end

function Control:__parent_resizing(wp)
	Control.__index.__parent_resizing(self, wp)
	local pr, r = self.parent.screen_rect, self.rect
	local x, w = anchor_dim(self, self.anchors.left, self.anchors.right,
									pr.x1 + wp.w - pr.x2, r.x1, r.w, self.__anchor_w)
	local y, h = anchor_dim(self, self.anchors.top,  self.anchors.bottom,
									pr.y1 + wp.h - pr.y2, r.y1, r.h, self.__anchor_h)
	self:move(x, y, w, h)
	--real size might end up different than wanted size in which case we store the difference
	local rw, rh = self.w, self.h
	if w and rw ~= w then self.__anchor_w = w - rw else self.__anchor_w = nil end
	if h and rh ~= h then self.__anchor_h = h - rh else self.__anchor_h = nil end
end

