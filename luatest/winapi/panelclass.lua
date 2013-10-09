--oo/panel: custom frameless child window.
setfenv(1, require'winapi')
require'winapi.controlclass'
require'winapi.cursor'
require'winapi.color'
require'winapi.winbase' --GetCurrentThreadId

Panel = subclass({
	__class_style_bitmask = bitmask{ --only static, frame styles here
		dropshadow = CS_DROPSHADOW,
		own_dc = CS_OWNDC, --for opengl and cairo panels
	},
	__style_ex_bitmask = bitmask{
		transparent = WS_EX_TRANSPARENT,
	},
	__defaults = {
		--class style bits
		noclose = false,
		dropshadow = false,
		--other class properties
		--background = COLOR_WINDOW,
		cursor = LoadCursor(IDC_ARROW),
		--window properties
		w = 100, h = 100,
	},
}, Control)

local i = 0
local function gen_classname()
	i = i + 1
	return string.format('Panel_%d_%d', GetCurrentThreadId(), i)
end

function Panel:__before_create(info, args)
	Panel.__index.__before_create(self, info, args)
	self.__winclass = RegisterClass{
		name = gen_classname(),
		style = self.__class_style_bitmask:set(0, info),
		proc = MessageRouter.proc,
		cursor = info.cursor,
		background = info.background,
	}
	args.class = self.__winclass
end

Panel.__default_proc = BaseWindow.__default_proc

function Panel:WM_NCDESTROY()
	Panel.__index.WM_NCDESTROY(self)
	PostMessage(nil, WM_UNREGISTER_CLASS, self.__winclass)
end

