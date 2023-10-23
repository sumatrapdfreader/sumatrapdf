// Read an image that was converted to RGB from CMYK without color management,
// and re-convert it using a better CMYK profile.

"use strict"

if (scriptArgs.length != 2) {
	print("usage: mutool run recolor-cmyk.js input.png output.png")
	quit()
}

var pixmap = new mupdf.Image(scriptArgs[0]).toPixmap()
mupdf.disableICC()
pixmap = pixmap.convertToColorSpace(mupdf.ColorSpace.DeviceCMYK)
mupdf.enableICC()
pixmap = pixmap.convertToColorSpace(mupdf.ColorSpace.DeviceRGB)
if (scriptArgs[1].match(/\.jpg/))
	pixmap.saveAsJPEG(scriptArgs[1], 100)
else
	pixmap.saveAsPNG(scriptArgs[1])
