--test/showcase: showcase window for the showcase part of modules.
setfenv(1, require'winapi')
require'winapi.windowclass'
require'winapi.messageloop'
require'winapi.comctl'
require'winapi.imagelistclass'

function ShowcaseWindow(info)
	return Window(update({}, {title = 'Showcase', autoquit = true}, info))
end

require'winapi.shellapi'
function ShowcaseImageList()
	return ImageList(ffi.cast('HIMAGELIST',
				SHGetFileInfo('c:\\', 0, 'SHGFI_ICON | SHGFI_SYSICONINDEX | SHGFI_SMALLICON')))
end


if not ... then
local window = ShowcaseWindow()
MessageLoop()
end
