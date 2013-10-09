--oo/imagelist: image list class.
setfenv(1, require'winapi')
require'winapi.itemlist'
require'winapi.handlelist'
require'winapi.imagelist'

ImageLists = HandleList'himl'

ImageList = subclass({
	__defaults = {
		w=32, h=32,
		initial_size = 0, --if > 0 you'll have to use set() instead of add()
		grow_size = 1, --if > 1 you'll have to use set() instead of add()
	},
	__flags_bitmask = bitmask{
		masked = ILC_MASK,
		colors = {
			default = ILC_COLOR,
			ddb = ILC_COLORDDB,
			['16bit'] = ILC_COLOR4,
			['256bit'] = ILC_COLOR8,
			['16bit'] = ILC_COLOR16,
			['24bit'] = ILC_COLOR24,
			['32bit'] = ILC_COLOR32,
		},
		mirror = {
			none = 0,
			all = ILC_MIRROR,
			item = ILC_PERITEMMIRROR,
		},
		preserve_size = ILC_ORIGINALSIZE, --vista+
	},
}, ItemList)

function ImageList:__init(t)
	if ffi.istype('HIMAGELIST', t) then
		self.himl = t
	else
		self.himl = ImageList_Create{
			w = t.w or t[1], h = t.h or t[2],
			flags = self.__flags_bitmask:set(bit.bor(ILC_COLOR32, ILC_MASK), t),
			initial_size = t.initial_size or self.__defaults.initial_size,
			grow_size = t.grow_size or self.__defaults.grow_size,
		}
		if t.bk_color then self.bk_color = t.bk_color end
	end
	ImageLists:add(self)
end
function ImageList:destroy()
	ImageList_Destroy(self.himl)
	ImageLists:remove(self)
	self.himl = nil
end

function ImageList:set(i,t)
	if t.icon then
		ImageList_ReplaceIcon(self.himl, i, t.icon)
	elseif t.transparent_color then	--there's no ImageList_ReplaceMasked
		ImageList_AddMasked(self.himl, t.image, t.transparent_color)
		ImageList_Copy(self.himl, i, self.count, ILCF_MOVE)
		ImageList_Remove(self.himl, self.count)
	else
		ImageList_Replace(self.himl, i, t.image, t.mask_image)
	end
end

function ImageList:move(to, from)
	if to == from then return end
	if to < from then
		for k=from,to+1,-1 do
			ImageList_Copy(self.himl, k, k-1, ILCF_SWAP)
		end
	else
		for k=to,from-1 do
			ImageList_Copy(self.himl, k, k+1, ILCF_SWAP)
		end
	end
end

function ImageList:add(i,t) --mask is an image or a color number
	if not t then i,t = nil,i end
	if t.icon then
		ImageList_AddIcon(self.himl, t.icon)
	elseif t.transparent_color then
		ImageList_AddMasked(self.himl, t.image, t.transparent_color)
	else
		ImageList_Add(self.himl, t.image, t.mask_image)
	end
	if i then self:move(self.count, i) end
end

function ImageList:remove(i)
	return ImageList_Remove(self.himl, i)
end

function ImageList:get(i, ILD)
	return {icon = ImageList_GetIcon(self.himl, i, ILD)}
end

function ImageList:clear() return
	ImageList_RemoveAll(self.himl)
end

function ImageList:get_count()
	return ImageList_GetImageCount(self.himl)
end

function ImageList:set_count(count)
	return ImageList_SetImageCount(self.himl, count)
end

function ImageList:get_bk_color() return ImageList_GetBkColor(self.himl) end
function ImageList:set_bk_color(color) ImageList_SetBkColor(self.himl, color) end

function ImageList:set_overlay(i, ioverlay)
	return ImageList_SetOverlayImage(self.himl, i, ioverlay)
end

function ImageList:get_size()
	local w,h = ImageList_GetIconSize(self.himl)
	return {w=w,h=h}
end
function ImageList:set_size(size)
	ImageList_SetIconSize(self.himl, size.w, size.h)
end

function ImageList:draw(i, dc, x, y, IDL)
	ImageList_Draw(self.himl, i, dc, x, y, IDL)
end


--showcase

if not ... then
require'winapi.showcase'
require'winapi.icon'
require'winapi.resource'
local window = ShowcaseWindow{w=300,h=200}
local m = ImageList{w=32,h=32,colors='32bit'}
--m.bk_color = 234234
m:add{icon = LoadIconFromInstance(IDI_WARNING)}
m:add{icon = LoadIconFromInstance(IDI_INFORMATION)}
m:add(2, {icon = LoadIconFromInstance(IDI_ERROR)})
m:set(3, {icon = LoadIconFromInstance(IDI_ERROR)})
m.count = m.count + 1
print(m.count)
print(m.size)
function window:WM_PAINT()
	local p = ffi.new'PAINTSTRUCT'
	local hdc = C.BeginPaint(window.hwnd, p)
	for i=1,m.count do
		m:draw(i, hdc, 50+i*34, 50, ILD_NORMAL)
	end
	C.EndPaint(window.hwnd, p)
end
window:invalidate()
MessageLoop()
m:destroy()
end
