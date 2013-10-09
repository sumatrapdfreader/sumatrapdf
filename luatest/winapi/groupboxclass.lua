--oo/groupbox: groupbox control.
setfenv(1, require'winapi')
require'winapi.basebuttonclass'

GroupBox = subclass({
	__style_bitmask = bitmask{
		tabstop = WS_TABSTOP,
		align = {
			left = BS_LEFT,
			right = BS_RIGHT,
			center = BS_CENTER,
		},
		flat = BS_FLAT,
	},
	__defaults = {
		--transparent = true,
		--window properties
		text = 'Group',
		w = 200, h = 100,
	},
}, BaseButton)

function GroupBox:__before_create(info, args)
	GroupBox.__index.__before_create(self, info, args)
	--BS_NOTIFY gives us focus/blur events; WS_EX_TRANSPARENT avoids incompatibility with parent having WS_CLIPCHILDREN
	args.style = bit.bor(args.style, BS_GROUPBOX, BS_NOTIFY)
	args.style_ex = bit.bor(args.style_ex, WS_EX_TRANSPARENT)
end

--showcase
if not ... then
require'winapi.showcase'
local window = ShowcaseWindow{w=300,h=300}
local gb1 = GroupBox{parent = window, style = {align = 'right', flat = true}}
local gb2 = GroupBox{parent = window, style = {}, y = 100}
function gb1:on_focus() print'gb1 focused' end
function gb2:on_focus() print'gb2 focused' end
MessageLoop()
end
