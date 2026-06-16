# JavaScript API reference for sumatrapdf-tool run

> The command-line tools are provided by `sumatrapdf-tool`, which is installed next to `SumatraPDF.exe`. They only work after SumatraPDF has been installed.

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

API reference for [sumatrapdf-tool run](Tool-run.md). The scripting engine is the same as [`mutool run`](https://mupdf.readthedocs.io/en/latest/tools/mutool-run.html); this page is adapted from the [MuPDF JavaScript reference](https://mupdf.readthedocs.io/en/latest/reference/javascript/index.html).

See also [JavaScript examples](Tool-run-javascript-examples.md).

## Contents

- [Introduction](#introduction)
- [Functions](#functions)
- [Types](#types)
  - [Archive](#archive)
  - [Buffer](#buffer)
  - [Color](#color)
  - [ColorSpace](#colorspace)
  - [DOM](#dom)
  - [DefaultColorSpaces](#defaultcolorspaces)
  - [Device](#device)
  - [DisplayList](#displaylist)
  - [DisplayListDevice](#displaylistdevice)
  - [Document](#document)
  - [DocumentWriter](#documentwriter)
  - [DrawDevice](#drawdevice)
  - [Font](#font)
  - [Image](#image)
  - [Link](#link)
  - [LinkDestination](#linkdestination)
  - [Matrix](#matrix)
  - [MultiArchive](#multiarchive)
  - [OutlineItem](#outlineitem)
  - [OutlineIterator](#outlineiterator)
  - [Page](#page)
  - [Path](#path)
  - [PathWalker](#pathwalker)
  - [Pixmap](#pixmap)
  - [Point](#point)
  - [Quad](#quad)
  - [Rect](#rect)
  - [Shade](#shade)
  - [Story](#story)
  - [StrokeState](#strokestate)
  - [StructuredText](#structuredtext)
  - [StructuredTextWalker](#structuredtextwalker)
  - [Text](#text)
  - [TextWalker](#textwalker)
  - [TreeArchive](#treearchive)
  - [PDFAnnotation](#pdfannotation)
  - [PDFDocument](#pdfdocument)
  - [PDFFilespecParams](#pdffilespecparams)
  - [PDFGraftMap](#pdfgraftmap)
  - [PDFObject](#pdfobject)
  - [PDFPage](#pdfpage)
  - [PDFProcessor](#pdfprocessor)
  - [PDFWidget](#pdfwidget)

## Introduction

This is the JavaScript API available through `sumatrapdf-tool run`. The
scripting engine is the same as `mutool run`.

## Functions

Most functionality is provided by member functions of class objects.
There are just a couple of top level configuration functions listed
here.

#### mupdf.installLoadFontFunction(callback)

Install a handler to load system (or missing) fonts.

The callback function will be called with four arguments:

```
callback(fontName, scriptName, isBold, isItalic)
```

The callback should return either a `Font` object for the requested
font, or `null` if an exact match cannot be found (so that the font
loading machinery can keep looking through the chain of fallback
fonts).

#### mupdf.enableICC()

Enable ICC-profiles based operation.

#### mupdf.disableICC()

Disable ICC-profiles based operation.

#### mupdf.emptyStore()

Empty all cached entries from the store.

#### mupdf.shrinkStore(percent)

Remove cached entries from the store until it holds less
data than the specified threshold.

If the store was initialized with a limit, the threshold is a
percentage of the limit. If the store is unlimited in size, the
threshold is a percentage of what the store currently holds.

Returns a boolean indicating whether or not the store was able to be
shrunk to below the threshold.

- **number percent**: 
- **Returns:** boolean

#### mupdf.setUserCSS(stylesheet, useDocumentStyles)

Set a style sheet to apply to all reflowable documents.

- **string stylesheet**: The CSS text to use.
- **boolean useDocumentStyles**: 
Whether to respect the document's own style sheet.

## Types

### Common

### Archive

#### Constructors

#### Archive(path)

Create a new archive based either on a tar- or zip-file or the contents of a directory.

- **string path**: Path string to the archive file or directory.

```
var archive1 = new mupdf.Archive("example1.zip")
var archive2 = new mupdf.Archive("example2.tar")
var archive3 = new mupdf.Archive("images/")
```

#### Archive(buffer)

Create a new archive based either on a tar- or zip-file contained in a buffer.

- **Buffer | ArrayBuffer | Uint8Array | string buffer**: Buffer containing a archive.

```
var archive1 = new mupdf.Archive(fs.readFileSync("example1.zip"))
var archive2 = new mupdf.Archive(fs.readFileSync("example1.tar"))
```

#### Instance methods

#### Archive.prototype.getFormat()

Returns a string describing the archive format.

- **Returns:** string

```
var archive = new mupdf.Archive("example1.zip")
print(archive.getFormat())
```

#### Archive.prototype.countEntries()

Returns the number of entries in the archive.

- **Returns:** number

```
var archive = new mupdf.Archive("example1.zip")
var numEntries = archive.countEntries()
```

#### Archive.prototype.listEntry(idx)

Returns the name of entry number idx in the archive, or null if idx is
out of range.

- **number idx**: Entry index.

- **Returns:** string | null

```
var archive = new mupdf.Archive("example1.zip")
var entry = archive.listEntry(0)
```

#### Archive.prototype.hasEntry(name)

Returns true if an entry of the given name exists in the archive.

- **string name**: Entry name to look for.

- **Returns:** boolean

```
var archive = new mupdf.Archive("example1.zip")
var hasEntry = archive.hasEntry("file1.txt")
```

#### Archive.prototype.readEntry(name)

Returns the contents of the entry of the given name.

- **string name**: Name of entry to look for.

- **Returns:** `Buffer`

```
var archive = new mupdf.Archive("example1.zip")
var contents = archive.readEntry("file1.txt")
```

#### Examples

```
		var archive = new mupdf.Archive("example1.zip")
		var n = archive.countEntries()
		for (var i = 0; i < n; ++i) {
			var entry = archive.listEntry(i)
			var contents = archive.readEntry(entry)
			console.log("entry", entry, contents.length)
		}
```

### Buffer

Buffer objects are used for working with binary data, e.g. returned from
`asJPEG`. They can be used much like arrays, but are much more
efficient since they only store bytes.

#### Constructors

#### Buffer(data)

Creates a new Buffer initialized with contents from data.

- **Buffer | ArrayBuffer | Uint8Array | string | null data**: Contents used to populate buffer, or leave it empty if `null`.

```
var buffer = new mupdf.Buffer("hello world")
```

#### Instance properties

#### Buffer.prototype.length

The number of bytes in this buffer (read-only).

- **Throws:** TypeError on attempted writes.

- **Returns:** number

#### Buffer.prototype.[n]

Get or set the byte at index `n`.

- **Throws:** RangeError on out of bounds accesses.

```
var byte = buffer[0]
```

#### Instance methods

#### Buffer.prototype.asString()

Returns the contents of this buffer as a string.

- **Returns:** string

```
var str = buffer.asString()
```

#### Buffer.prototype.getLength()

Returns the number of bytes in this buffer.

- **Returns:** number

```
var length = buffer.getLength()
```

#### Buffer.prototype.readByte(at)

Returns the byte at the specified index `at` if within `0 <= at < getLength()`. Otherwise returns `undefined`.

- **number at**: Index to read byte at.

- **Returns:** number

```
buffer.readByte(0)
```

#### Buffer.prototype.slice(start, end)

Create a new buffer containing a (subset of) the data in this buffer.
Start and end are offsets from the beginning of this buffer, and if negative from the end of this buffer.
If `start` points to the end of this buffer, or if `end` point to at or before `start`, then an empty buffer will be returned.

- **number start**: Start index.
- **number end**: End index (optional).

- **Returns:** `Buffer`

```
var buffer = new mupdf.Buffer()
buffer.write("hello world") // buffer contains "hello world"
var newBuffer = buffer.slice(1, -1) // newBuffer contains "ello worl"
```

#### Buffer.prototype.write(str)

Append the string as UTF-8 to the end of this buffer.

- **string str**: String to append.

```
buffer.write("hello world")
```

#### Buffer.prototype.writeBuffer(data)

Append the contents of the `data` buffer to the end of this buffer.

- **Buffer | ArrayBuffer | Uint8Array | string data**: Data buffer to append.

```
buffer.writeBuffer(anotherBuffer)
```

#### Buffer.prototype.writeByte(byte)

Append a single byte to the end of this buffer.
Only the least significant 8 bits of the value are appended.

- **number byte**: The byte value to append.

```
buffer.writeByte(0x2a)
```

#### Buffer.prototype.writeLine(str)

Append string to the end of this buffer ending with a newline.

- **string str**: String to append.

```
buffer.writeLine("a line")
```

#### Buffer.prototype.writeRune(c)

Encode a unicode character as UTF-8 and append to the end of
the buffer.

- **number c**: The character unicode codepoint.

```js
buffer.writeRune(0x4f60) // To append U+4f60, 你
buffer.writeRune(0x597d) // To append U+597d, 好
buffer.writeRune(0xff01) // To append U+ff01, ！
```

#### Buffer.prototype.save(filename)

Write the contents of the buffer to a file.

- **string filename**: Filename to save to.

```js
buffer.save("buffer.dat")
```

### Color

Colors are specified as arrays with the appropriate number of components for
the associated `ColorSpace`. Each component value is stored as floating
point value between 0 and 1, where 0 means no colorant and 1
means maximum colorant.

For example:

- In the `ColorSpace.DeviceCMYK` color space the components are [Cyan, Magenta, Yellow, Black]. A full intensity magenta color would therefore be [0, 1, 0, 0].
- In the `ColorSpace.DeviceRGB` color space the components are [Red, Green, Blue]. A full intensity green color would therefore be [0, 1, 0].
- In the `ColorSpace.DeviceGray` color space the components are [Black]. A full intensity black color would therefore be [0].

In TypeScript this is defined as follows:

```
	type Color = [number] | [number, number, number] | [number, number, number, number]
```

### ColorSpace

Defines what color system is used for an object, and which colorants exist.

For a `Pixmap` the interface `Pixmap.prototype.getColorSpace()` returns its colorspace,
which defines what colorants make up the pixel colors in the Pixmap and
how those colorants are stored. If that colorspace is e.g.,
`ColorSpace.DeviceRGB`, then the colorants **red**, **green** and
**blue** are combined to make up the color of each pixel.

#### Constructors

#### ColorSpace(from, name)

Create a new ColorSpace from an ICC profile.

- **Buffer | ArrayBuffer | Uint8Array | string from**: A buffer containing an ICC profile.
- **string name**: A user descriptive name for the new colorspace.

```
var icc_colorspace = new mupdf.ColorSpace(fs.readFileSync("SWOP.icc"), "SWOP")
```

#### Static properties

#### ColorSpace.DeviceGray

The default Grayscale colorspace

#### ColorSpace.DeviceRGB

The default RGB colorspace

#### ColorSpace.DeviceBGR

The default RGB colorspace, but with components in reverse order

#### ColorSpace.DeviceCMYK

The default CMYK colorspace

#### ColorSpace.Lab

The default Lab colorspace

#### Instance methods

#### ColorSpace.prototype.getName()

Return the name of this colorspace.

#### ColorSpace.prototype.getNumberOfComponents()

A Grayscale colorspace has one component, RGB has 3, CMYK has 4, and DeviceN may have any number of components.

- **Returns:** number of components

```
var cs = mupdf.ColorSpace.DeviceRGB
var num = cs.getNumberOfComponents() // 3
```

#### ColorSpace.prototype.getType()

Returns a string indicating the type of this colorspace, one of:

- **Returns:** `"None" | "Gray" | "RGB" | "BGR" | "CMYK" | "Lab" | "Indexed" | "Separation"`

#### ColorSpace.prototype.isCMYK()

Returns `true` if the object is a CMYK color space.

- **Returns:** boolean

```
var bool = colorSpace.isCMYK()
```

#### ColorSpace.prototype.isDeviceN()

Returns `true` if the object is a Device N color space.

- **Returns:** boolean

```
var bool = colorSpace.isDeviceN()
```

#### ColorSpace.prototype.isGray()

Returns `true` if the object is a gray color space.

- **Returns:** boolean

```
var bool = colorSpace.isGray()
```

#### ColorSpace.prototype.isIndexed()

Returns `true` if the object is an Indexed color space.

- **Returns:** boolean

```
var bool = colorSpace.isIndexed()
```

#### ColorSpace.prototype.isLab()

Returns `true` if the object is a Lab color space.

- **Returns:** boolean

```
var bool = colorSpace.isLab()
```

#### ColorSpace.prototype.isRGB()

Returns `true` if the object is an RGB color space.

- **Returns:** boolean

```
var bool = colorSpace.isRGB()
```

#### ColorSpace.prototype.isSubtractive()

Returns `true` if the object is a subtractive color space.

- **Returns:** boolean

```
var bool = colorSpace.isSubtractive()
```

#### ColorSpace.prototype.toString()

Return name of this colorspace.

- **Returns:** string

```
var cs = mupdf.ColorSpace.DeviceRGB
var name = cs.toString() // "[ColorSpace DeviceRGB]"
```

### DOM

This represents an HTML or an DOM node. It is a helper class intended to
access the DOM (Document Object Model) content of a `Story`
object.

#### Constructors

#### DOM

*(not constructible with `new`)*

Instances of this class are returned by `Story.prototype.document()`.

#### Instance methods

#### DOM.prototype.body()

Return a DOM for the body element.

- **Returns:** `DOM`

```
var result = xml.body()
```

#### DOM.prototype.documentElement()

Return a DOM for the top level element.

- **Returns:** `DOM`

```
var result = xml.documentElement()
```

#### DOM.prototype.createElement(tag)

Create an element with the given tag type, but do not link it into
the `DOM` yet.

- **string tag**: Tag name to use for element.

- **Returns:** `DOM`

```
var result = xml.createElement("div")
```

#### DOM.prototype.createTextNode(text)

Create a text node with the given text contents, but do not link
it into the `DOM` yet.

- **string text**: Text contents to put into element.

- **Returns:** `DOM`

```
var result = xml.createElement("Hello world!")
```

#### DOM.prototype.find(tag, attribute, value)

Find the first element matching the tag, attribute and value. Set
either of those to `null` to match anything.

- **string tag**: The tag of the element to look for.
- **string attribute**: The attribute of the element to look for.
- **string value**: The value of the attribute of the element to look for.

- **Returns:** `DOM` | null

```
var result = xml.find("tag", "attribute", "value")
```

#### DOM.prototype.findNext(tag, attribute, value)

Find the next element matching the tag, attribute and value. Set either
of those to `null` to match anything.

- **string tag**: The tag of the element to look for.
- **string attribute**: The attribute of the element to look for.
- **string value**: The value of the attribute of the element to look for.

- **Returns:** `DOM` | null

```
var result = xml.findNext("tag", "attribute", "value")
```

#### DOM.prototype.appendChild(dom, childDom)

Insert an element as the last child of a parent, unlinking the child
from its current position if required.

- **DOM dom**: The DOM that will be parent.
- **DOM childDom**: The DOM that will be a child.

```
xml.appendChild(dom, childDom)
```

#### DOM.prototype.insertBefore(dom, elementDom)

Insert an element before this element, unlinking the new element
from its current position if required.

- **DOM dom**: The reference DOM.
- **DOM elementDom**: The DOM that will be inserted before.

```
xml.insertBefore(dom, elementDom)
```

#### DOM.prototype.insertAfter(dom, elementDom)

Insert an element after this element, unlinking the new element
from its current position if required.

- **DOM dom**: The reference DOM.
- **DOM elementDom**: The DOM that will be inserted after.

```
xml.insertAfter(dom, elementDom)
```

#### DOM.prototype.remove()

Remove this element from the `DOM`. The element can be
added back elsewhere if required.

```
var result = xml.remove()
```

#### DOM.prototype.clone()

Clone this element (and its children). The clone is not yet linked
into the `DOM`.

- **Returns:** `DOM`

```
var result = xml.clone()
```

#### DOM.prototype.firstChild()

Return the first child of the element as a `DOM`, or
`null` if no child exist.

- **Returns:** `DOM` | null

```
var result = xml.firstChild()
```

#### DOM.prototype.parent()

Return the parent of the element as a `DOM`, or `null`
if no parent exists.

- **Returns:** `DOM` | null

```
var result = xml.parent()
```

#### DOM.prototype.next()

Return the next element as a `DOM`, or null if no such
element exists.

- **Returns:** `DOM` | null

```
var result = xml.next()
```

#### DOM.prototype.previous()

Return the previous element as a `DOM`, or null if no such
element exists.

- **Returns:** `DOM` | null

```
var result = xml.previous()
```

#### DOM.prototype.addAttribute(attribute, value)

Add attribute with the given value, returns the updated element as
a DOM.

- **string attribute**: Desired attribute name.
- **string value**: Desired attribute value.

- **Returns:** `DOM`

```
var result = xml.addAttribute("attribute", "value")
```

#### DOM.prototype.removeAttribute(attribute)

Remove the specified attribute from the element, returns the
updated element as a DOM.

- **string attribute**: The name of the attribute to remove.

- **Returns:** `DOM`

```
xml.removeAttribute("attribute")
```

#### DOM.prototype.getAttribute(attribute)

Return the element's attribute value as a string, or null if no
such attribute exists.

- **string attribute**: The name of the attribute to look up.

- **Returns:** string | null

```
var result = xml.attribute("attribute")
```

#### DOM.prototype.getAttributes()

Returns a dictionary object with properties and their values
corresponding to the element's attributes and their values.

- **Returns:** Object

```
var dict = xml.getAttributes()
```

#### DOM.prototype.getText()

Returns the text contents of the node.

- **Returns:** string | null

```
var text = xml.getText()
```

#### DOM.prototype.getTag()

Returns the tag name for the node.

- **Returns:** string | null

```
var text = xml.getTag()
```

### DefaultColorSpaces

An object given to a `Device` with the `Device.prototype.setDefaultColorSpaces()` call so it
can obtain the current default `ColorSpace` objects.

#### Constructors

#### DefaultColorSpaces()

#### Instance methods

#### DefaultColorSpaces.prototype.getDefaultGray()

Get the default gray colorspace.

- **Returns:** `ColorSpace`

#### DefaultColorSpaces.prototype.getDefaultRGB()

Get the default RGB colorspace.

- **Returns:** `ColorSpace`

#### DefaultColorSpaces.prototype.getDefaultCMYK()

Get the default CMYK colorspace.

- **Returns:** `ColorSpace`

#### DefaultColorSpaces.prototype.getOutputIntent()

Get the output intent.

- **Returns:** `ColorSpace`

#### DefaultColorSpaces.prototype.setDefaultGray(colorspace)

- **ColorSpace colorspace**: The new default gray colorspace.

#### DefaultColorSpaces.prototype.setDefaultRGB(colorspace)

- **ColorSpace colorspace**: The new default RGB  colorspace.

#### DefaultColorSpaces.prototype.setDefaultCMYK(colorspace)

- **ColorSpace colorspace**: The new default CMYK colorspace.

#### DefaultColorSpaces.prototype.setOutputIntent(colorspace)

- **ColorSpace colorspace**: The new default output intent.

### Device

All built-in devices have the methods listed below. Any function that
accepts a device will also accept a Javascript object with the same
methods. Any missing methods are simply ignored, so you only need to
create methods for the device calls you care about.

Many of the methods take graphics objects as arguments: `Path`,
`Text`, `Image` and `Shade`.

Colors are specified as arrays with the appropriate number of components
for the color space.

The methods that clip graphics, e.g. `Device.prototype.clipPath()`,
`Device.prototype.clipStrokePath()`, `Device.prototype.clipText()`, etc., must be balanced with
a corresponding `Device.prototype.popClip()`.

#### Constructors

#### Device(callbacks)

Create a Device which calls back to Javascript.

The callback object may provide functions matching the methods
on the Device class. Any device calls that don't have a corresponding
function will simply be ignored.

- **callbacks**: object containing callback functions

You can create other devices with `DrawDevice` and `DisplayListDevice`.

#### Constants

:term:`Blend mode` constants for use with `Device.prototype.beginGroup`:

#### Device.BLEND_NORMAL

#### Device.BLEND_MULTIPLY

#### Device.BLEND_SCREEN

#### Device.BLEND_OVERLAY

#### Device.BLEND_DARKEN

#### Device.BLEND_LIGHTEN

#### Device.BLEND_COLOR_DODGE

#### Device.BLEND_COLOR_BURN

#### Device.BLEND_HARD_LIGHT

#### Device.BLEND_SOFT_LIGHT

#### Device.BLEND_DIFFERENCE

#### Device.BLEND_EXCLUSION

#### Device.BLEND_HUE

#### Device.BLEND_SATURATION

#### Device.BLEND_COLOR

#### Device.BLEND_LUMINOSITY

#### Instance methods

#### Device.prototype.close()

Tell this device that we are done, and flush any pending output.

Before closing, ensure that there have been as many calls to
`popClip()` as there have been to the clipping functions:
`clipPath()`, `clipStrokePath()`, `clipText()`, etc.

```
device.close()
```

Line art
^^^^^^^^

#### Device.prototype.fillPath(path, evenOdd, ctm, colorspace, color, alpha)

Fill a path.

- **Path path**: Path object.
- **boolean evenOdd: Use :term:`even-odd rule` or :term**: `non-zero winding number rule` to fill the path.
- **Matrix ctm**: The transform to apply.
- **ColorSpace colorspace**: The colorspace of the color to fill with.
- **Color color**: The color to fill the path with.
- **number alpha: The :term**: `opacity`.

```
device.fillPath(path, false, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1, 0, 0], true)
```

#### Device.prototype.strokePath(path, stroke, ctm, colorspace, color, alpha)

Stroke a path.

- **Path path**: Path object.
- **StrokeState stroke**: Stroke state.
- **Matrix ctm**: The transform to apply.
- **ColorSpace colorspace**: Colorspace.
- **Color color**: The color to stroke the path with.
- **number alpha: The :term**: `opacity`.

```
device.strokePath(path,
{dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
mupdf.Matrix.identity,
mupdf.ColorSpace.DeviceRGB,
[0, 1, 0],
0.5
)
```

#### Device.prototype.clipPath(path, evenOdd, ctm)

Clip a path.

- **Path path**: Path object.
- **boolean evenOdd: Use :term:`even-odd rule` or :term**: `non-zero winding number rule` to fill the path.
- **Matrix ctm**: The transform to apply.

```
device.clipPath(path, true, mupdf.Matrix.identity)
```

#### Device.prototype.clipStrokePath(path, stroke, ctm)

Clip & stroke a path.

- **Path path**: Path object.
- **StrokeState stroke**: Stroke state.
- **Matrix ctm**: The transform to apply.

```
device.clipStrokePath(path, true, mupdf.Matrix.identity)
```

Text
^^^^

#### Device.prototype.fillText(text, ctm, colorspace, color, alpha)

Fill a text object.

- **Text text**: Text object.
- **Matrix ctm**: The transform to apply.
- **ColorSpace colorspace**: Colorspace
- **Color color**: The color used to fill the text.
- **number alpha: The :term**: `opacity`.

```
device.fillText(text, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1, 0, 0], 1)
```

#### Device.prototype.strokeText(text, stroke, ctm, colorspace, color, alpha)

Stroke a text object.

- **Text text**: Text object.
- **StrokeState stroke**: Stroke state.
- **Matrix ctm**: The transform to apply.
- **ColorSpace colorspace**: Colorspace
- **Color color**: The color used to stroke the text.
- **number alpha: The :term**: `opacity`.

```
device.strokeText(text,
{ dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
mupdf.Matrix.identity,
mupdf.ColorSpace.DeviceRGB,
[1, 0, 0],
1
)
```

#### Device.prototype.clipText(text, ctm)

Clip a text object.

- **Text text**: Text object.
- **Matrix ctm**: The transform to apply.

```
device.clipText(text, mupdf.Matrix.identity)
```

#### Device.prototype.clipStrokeText(text, stroke, ctm)

Clip & stroke a text object.

- **Text text**: Text object.
- **StrokeState stroke**: stroke state.
- **Matrix ctm**: The transform to apply.

```
device.clipStrokeText(text,
{ dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
mupdf.Matrix.identity
)
```

#### Device.prototype.ignoreText(text, ctm)

Invisible text that can be searched but should not be visible, such as for overlaying a scanned OCR image.

- **Text text**: Text object.
- **Matrix ctm**: The transform to apply.

```
device.ignoreText(text, mupdf.Matrix.identity)
```

Shadings
^^^^^^^^

#### Device.prototype.fillShade(shade, ctm, alpha)

Fill a shading, also known as a gradient.

- **Shade shade**: The gradient.
- **Matrix ctm**: The transform to apply.
- **number alpha: The :term**: `opacity`.

```
device.fillShade(shade, mupdf.Matrix.identity, true, { overPrinting: true })
```

Images
^^^^^^

#### Device.prototype.fillImage(image, ctm, alpha)

Draw an image. An image always fills a unit rectangle [0, 0, 1, 1], so must be transformed to be placed and drawn at the appropriate size.

- **Image image**: Image object.
- **Matrix ctm**: The transform to apply.
- **number alpha: The :term**: `opacity`.

```
device.fillImage(image, mupdf.Matrix.identity, false, { overPrinting: true })
```

#### Device.prototype.fillImageMask(image, ctm, colorspace, color, alpha)

An image mask is an image without color. Fill with the color where the image is opaque.

- **Image image**: Image object.
- **Matrix ctm**: The transform to apply.
- **ColorSpace colorspace**: Colorspace
- **Color color**: The color to be used.
- **number alpha: The :term**: `opacity`.

```
device.fillImageMask(image, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [0, 1, 0], true)
```

#### Device.prototype.clipImageMask(image, ctm)

Clip graphics using the image to mask the areas to be drawn.

- **Image image**: Image object.
- **Matrix ctm**: The transform to apply.

```
device.clipImageMask(image, mupdf.Matrix.identity)
```

Clipping and masking
^^^^^^^^^^^^^^^^^^^^

#### Device.prototype.popClip()

Pop the clip mask installed by the last clipping operation.

```
device.popClip()
```

#### Device.prototype.beginMask(area, luminosity, colorspace, color)

Create a soft mask. Any drawing commands between `beginMask` and `endMask` are grouped and used as a clip mask.

- **Rect area**: Mask area.
- **boolean luminosity**: If luminosity is `true`, the mask is derived from the luminosity (grayscale value) of the graphics drawn; otherwise the color is ignored completely and the mask is derived from the alpha of the group.
- **ColorSpace colorspace**: Colorspace
- **Color color**: The color to be used.

```
device.beginMask([0, 0, 100, 100], true, mupdf.ColorSpace.DeviceRGB, [1, 0, 1])
```

#### Device.prototype.endMask()

Ends the mask.

```
device.endMask()
```

Groups and transparency
^^^^^^^^^^^^^^^^^^^^^^^

#### Device.prototype.beginGroup(area, colorspace, isolated, knockout, blendmode, alpha)

Begin a transparency blending group. See :term:`knockout and isolation`
and :term:`blend mode` in the glossary for a cursory overview of the
concepts.

- **Rect area**: The blend area.
- **ColorSpace colorspace**: Colorspace
- **boolean isolated**: Whether the group is isolated.
- **boolean knockout**: Whether the group is knockout.
- **string blendmode**: The blend mode used when compositing this group with its backdrop.
- **number alpha: The :term**: `opacity`.

The blendmode is one of these string values or the corresponding enum constants:
Normal, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Hue, Saturation, Color, Luminosity.

You can also use the `Device.BLEND_NORMAL` constant:

```
device.beginGroup(
[0, 0, 100, 100],
mupdf.ColorSpace.DeviceRGB,
true,
true,
"Multiply",
0.5
)
```

#### Device.prototype.endGroup()

Ends the blending group.

```
device.endGroup()
```

Tiling
^^^^^^

#### Device.prototype.beginTile(area, view, xstep, ystep, ctm, id, doc_id)

Draw a tiling pattern. Any drawing commands between `beginTile` and `endTile` are grouped and then repeated across the whole page. Apply a clip mask to restrict the pattern to the desired shape.

- **Rect area**: Area
- **Rect view**: View
- **number xstep**: x step.
- **number ystep**: y step.
- **Matrix ctm**: The transform to apply.
- **number id**: 
- **number doc_id**: 
The purpose of id/doc_id is to allow for efficient caching of
rendered tiles. If id is 0, then no caching is performed. If
it is non-zero, then id/doc_id are assumed to uniquely
identify this tile.

```
device.beginTile([0, 0, 100, 100], [100, 100, 200, 200], 10, 10, mupdf.Matrix.identity, 0)
```

#### Device.prototype.endTile()

Ends the tiling pattern.

```
device.endTile()
```

Render flags
^^^^^^^^^^^^

#### Device.prototype.renderFlags(set, clear)

The specified rendering flags are set, and some others are cleared.

Both set and clear are arrays where each element one of these flag names:

- "mask"
- "color"
- "uncacheable"
- "fillcolor-undefined"
- "strokecolor-undefined"
- "startcap-undefined"
- "dashcap-undefined"
- "endcap-undefined"
- "linejoin-undefined"
- "miterlimit-undefined"
- "linewidth-undefined"
- "bbox-defined"
- "gridfit-as-tiled"

- **Array of string set**: Rendering flags to set.
- **Array of string clear**: Rendering flags to clear.

```
device.renderFlags(["mask", "startcap-undefined"], [])
```

Device colorspaces
^^^^^^^^^^^^^^^^^^

#### Device.prototype.setDefaultColorSpaces(defaultCS)

Change the set of default device colorspaces to one given.

- **DefaultColorSpaces defaultCS**: The new set of default colorspaces.

```
var defaultCS = new DefaultColorSpaces()
defaultCS.setDefaultRGB(defaultCS.getDefaultGray())
device.setDefaultColorSpaces(new DefaultColorSpaces())
```

Layers
^^^^^^

#### Device.prototype.beginLayer(name)

Begin a marked-content layer with the given name.

- **string name**: Name of this marked-content layer.

```
device.beginLayer("my tag")
```

#### Device.prototype.endLayer()

End a marked-content layer.

```
device.endLayer()
```

Structures
^^^^^^^^^^

#### Device.prototype.beginStructure(structure, raw, index)

Begin a :term:`standard structure type` with the raw tag name and a unique identifier.

- **string structure**: One of the pre-defined structure types in PDF.
- **string raw**: The tag name.
- **number index**: A unique identifier.

```
device.beginStructure("Document", "my_tag_name", 123)
```

#### Device.prototype.endStructure()

End a standard structure element.

```
device.endStructure()
```

Metatext
^^^^^^^^

#### Device.prototype.beginMetatext(meta, text)

Begin meta text where meta is either of:

- "ActualText"
- "Alt"
- "Abbreviation"
- "Title"

- **string meta**: The meta text type
- **string text**: The text value.

```
device.beginMetatext("Title", "My title")
```

#### Device.prototype.endMetatext()

End meta text information.

```
device.endMetatext()
```

### DisplayList

A display list is a sequence of device calls that can be replayed multiple
times. This is useful e.g. when you need to render a page at multiple
resolutions, or when you want to both render a page and later search for
text in it. Using a display list to do this, improves performance because
it avoids repeatedly reinterpreting the document from file

To populate a display list use the `DisplayListDevice`.

```
	var list = new mupdf.DisplayList([0, 0, 595, 842])
	var listDevice = new mupdf.DisplayListDevice(list)
	page.run(listDevice, mupdf.Matrix.identity)
```

	var pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0, 0, 595, 842], false)
	var drawDevice = new mupdf.DrawDevice(mupdf.Matrix.identity, pixmap)
	list.run(drawDevice, mupdf.Matrix.identity)

	var searchHits = list.search("hello world")

#### Constructors

#### DisplayList(mediabox)

Create an empty display list. The mediabox rectangle should be the
bounds of the page.

- **Rect mediabox**: The size of the page.

```
var displayList = new mupdf.DisplayList([0, 0, 595, 842])
```

#### Instance methods

#### DisplayList.prototype.run(device, matrix)

Play back this display lists sequence of device calls to the given device.

- **Device device**: The device to replay the device calls to.
- **Matrix matrix**: Transformation matrix to apply to coordinates in all device calls.

```
displayList.run(device, mupdf.Matrix.identity)
```

#### DisplayList.prototype.getBounds()

Return a bounding rectangle that encompasses all the contents of the display list.

- **Returns:** `Rect`

```
var bounds = displayList.getBounds()
```

#### DisplayList.prototype.toPixmap(matrix, colorspace, alpha)

Render a display list to a `Pixmap`.

- **Matrix matrix**: Transformation matrix.
- **ColorSpace colorspace**: The desired colorspace of the returned pixmap.
- **boolean alpha**: Whether the returned pixmap has transparency or not. If the pixmap handles transparency, it starts out transparent (otherwise it is filled white), before the contents of the display list are rendered onto the pixmap.

- **Returns:** `Pixmap`

```
var pixmap = displayList.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, false)
```

#### DisplayList.prototype.toStructuredText(options)

Extract the text on the page into a `StructuredText` object.

- **string options**: 
See :doc:`/reference/common/stext-options`.

- **Returns:** `StructuredText`

```
var sText = displayList.toStructuredText("preserve-whitespace")
```

#### DisplayList.prototype.search(needle, max_hits)

Search the display list text for all instances of the text value
`needle`, and return an array of search hits. Each search hit is an
array of `Quad`, each corresponding to a single character in the search
hit.

- **string needle**: The text to search for.
- **number options**: Optional options for the search. A logical or of options such as `StructuredText.SEARCH_EXACT`.

- **Returns:** Array of Array of `Quad`

```
var results = displayList.search("my search phrase")
```

#### DisplayList.protoype.decodeBarcode(subarea, rotate)

Decodes a barcode detected in the display list, and returns an
object with properties for barcode type and contents.

- **Rect subarea**: Only detect barcode within subarea. Defaults to the entire area returned by `DisplayList.prototype.getBounds()`.
- **number rotate**: Degrees of rotation to rotate display list before detecting barcode. Defaults to 0.

- **Returns:** Object with barcode information.

```js
var barcodeInfo = displayList.decodeBarcode([0, 0, 100, 100 ], 0)
```

### DisplayListDevice

#### Constructors

#### DisplayListDevice(displayList)

Create a device for recording onto a display list.

- **DisplayList displayList**: the display list to populate.

```
var myDisplayList = new mupdf.DisplayList([0, 0, 100, 100])
var displayListDevice = new mupdf.DisplayListDevice(myDisplayList)
```

### Document

MuPDF can open many document types such as PDF, XPS, CBZ, EPUB, FictionBook 2
and a handful of image formats.

#### Document

*(not constructible with `new`)*

#### Constants

Permission flags for `Document.prototype.hasPermission`:

#### Document.PERMISSION_PRINT

`"print"` -- Print the document.

#### Document.PERMISSION_EDIT

`"edit"` -- Modify the contents of the document by operations other than those controlled by the other flags: (annotate, form, assemble).

#### Document.PERMISSION_COPY

`"edit"` -- Copy or otherwise extract text from the document.

#### Document.PERMISSION_ANNOTATE

`"annotate"` -- Add or modify annotations.

#### Document.PERMISSION_FORM

`"form"` -- Fill in existing form fields.

#### Document.PERMISSION_ACCESSIBILITY

`"accessibility"` --
Copy or otherwise extract text from the document in support of accessibility.

#### Document.PERMISSION_ASSEMBLE

`"assemble"` -- Insert, rotate, or delete pages and create bookmarks.

#### Document.PERMISSION_PRINT_HQ

`"print-hq"` -- Print the document to a representation from which a faithful digital
copy of the PDF content could be generated.

#### Static methods

#### 

Document.openDocument(filename)
Document.openDocument(filename, dir)
Document.openDocument(filename, accelerator, dir)
Document.openDocument(buffer, magic)
Document.openDocument(buffer, magic, acceleratorbuffer)
Document.openDocument(buffer, magic, acceleratorbuffer, dir)

Open the named or given document.

- **string filename**: File name to open.
- **Buffer | ArrayBuffer | Uint8Array | string buffer**: Buffer containing a document file.
- **string magic: An optional :term**: `MIME-type` or file extension. Defaults to "application/pdf".
- **string accelerator**: File name of accelerator file.
- **Buffer | ArrayBuffer | Uint8Array | string acceleratorbuffer**: Buffer containing an accelerator file.
- **Archive dir**: An archive from which to load resources for rendering.

- **Returns:** Document

```
var document1 = mupdf.Document.openDocument("my_pdf.pdf", "application/pdf")
var document2 = mupdf.Document.openDocument("my_pdf.pdf", dir)
var document3 = mupdf.Document.openDocument("my_pdf.pdf", acceleratorfile, dir)
var document4 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), "application/pdf")
var document5 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), acceleratorbuffer, "application/pdf")
var document6 = mupdf.Document.openDocument(fs.readFileSync("my_pdf.pdf"), acceleratorbuffer, dir, "application/pdf")
```

#### 

Document.recognize(magic)
Document.recognizeContent(filename)
Document.recognizeContent(buffer, magic)
Document.recognizeContent(buffer, dir, magic)

Check if MuPDF can open a document with the provided magic, or with the contents in the given file/buffer.

- **string magic: An optional :term**: `MIME-type` or file extension. Defaults to "application/pdf".
- **string filename**: File name to check contents of.
- **Buffer | ArrayBuffer | Uint8Array | string buffer**: Buffer containing a PDF file.
- **Archive dir**: An archive from which to load resources for rendering.

- **Returns:** boolean

```
var recognized1 = mupdf.Document.recognize("application/pdf")
var recognized2 = mupdf.Document.recognizeContent("my_pdf.pdf")
var recognized3 = mupdf.Document.recognizeContent(buffer, "application/pdf")
var recognized4 = mupdf.Document.recognizeContent(buffer, dir, "application/pdf")
```

#### Instance methods

#### Document.prototype.needsPassword()

Returns `true` if a password is required to open a password protected PDF.

- **Returns:** boolean

```
var needsPassword = document.needsPassword()
```

#### Document.prototype.authenticatePassword(password)

Returns a bitfield value against the password authentication result.

- **string password**: The password to attempt authentication with.

- **Returns:** number

| **Bitfield value** | **Description** |
| --- | --- |
| 0 | Failed |
| 1 | No password needed |
| 2 | Is User password and is okay |
| 4 | Is Owner password and is okay |
| 6 | Is both User & Owner password and is okay |

```
var auth = document.authenticatePassword("abracadabra")
```

#### Document.prototype.hasPermission(permission)

Check if a user is allowed permission to perform certain operations on the document.

- **"print" | "edit" | "copy" | "annotate" | "form" | "accessibility" | "assemble" | "print-hq" permission**: 

See `Document.PERMISSION_PRINT`, etc.

- **Returns:** boolean

```
var canEdit1 = document.hasPermission("edit")
var canEdit2 = document.hasPermission(Document.PERMISSION_EDIT)
```

#### Document.prototype.getMetaData(key)

Return various meta data information. The common keys are: format, encryption, info:ModDate, and info:Title. Returns `undefined` if the meta data does not exist.

- **string key**: What metadata type to return.

- **Returns:** string | null

```
var format = document.getMetaData("format")
var modificationDate = doc.getMetaData("info:ModDate")
var author = doc.getMetaData("info:Author")
```

#### Document.prototype.setMetaData(key, value)

Set document meta data information field to a new value.

- **string key**: Metadata key to set.
- **string value**: New value to set for the given key.

```
document.setMetaData("info:Author", "My Name")
```

#### Document.prototype.isReflowable()

Returns true if the document is reflowable, such as EPUB, FB2 or XHTML.

- **Returns:** boolean

```
var isReflowable = document.isReflowable()
```

#### Document.prototype.layout(pageWidth, pageHeight, fontSize)

Layout a reflowable document (EPUB, FictionBook2, HTML or XHTML) to fit
the specified page and font sizes.

- **number pageWidth**: Desired page width.
- **number pageHeight**: Desired page height.
- **number fontSize**: Desire font size.

```
document.layout(300, 300, 16)
```

#### Document.prototype.countPages()

Count the number of pages in the document. This may change if you call
the layout function with different parameters.

- **Returns:** number

```
var numPages = document.countPages()
```

#### Document.prototype.loadPage(number)

Returns a `Page` object for the given page number.

For documents where `Document.prototype.isPDF()` returns true,
the returned `Page` is of the subclass `PDFPage`.

- **number number**: Number of page to load, 0 means the first page in the document.

- **Returns:** `Page` | `PDFPage`.

```
var page = document.loadPage(0) // loads the 1st page of the document
```

#### Document.prototype.loadOutline()

Returns an array with the outline (also known as table of contents or
bookmarks). In the array is an object for each heading with the
property 'title', and a property 'page' containing the page number. If
the object has a 'down' property, it contains an array with all the
sub-headings for that entry.

- **Returns:** Array of `OutlineItem` (nested).

```
var outline = document.loadOutline()
```

#### Document.prototype.outlineIterator()

Returns an `OutlineIterator` for the document outline.

- **Returns:** `OutlineIterator`

```
var obj = document.outlineIterator()
```

#### Document.prototype.resolveLink(link)

Resolve a document internal link URI to a page index.

- **Link | string link**: A link or a link URI string to resolve.

- **Returns:** number

```
var pageNumber = document.resolveLink(my_link)
```

#### Document.prototype.resolveLinkDestination(link)

Resolve a document internal link URI to a link destination.

- **Link | string link**: A link or a link URI string to resolve.

- **Returns:** `LinkDestination`

```
var linkDestination = document.resolveLinkDestination(linkuri)
```

#### Document.prototype.isPDF()

Returns `true` if the document is a `PDFDocument`.

- **Returns:** boolean

```
var isPDF = document.isPDF()
```

#### Document.prototype.asPDF()

Returns a PDF version of the document (if possible).
PDF documents return themselves.
Documents that have an underlying PDF representation return that.
Other document types return null.

- **Returns:** `PDFDocument` | null

```
var doc = mupdf.Document.openDocument(filename)
var pdf = doc.asPDF()
if (pdf) {
// the document has a native PDF representation
} else {
// it does not have a native PDF representation
}
```

#### Document.prototype.formatLinkURI(linkDestination)

Format a document internal link destination object to a URI string suitable for `Page.prototype.createLink()`.

- **LinkDestination linkDestination**: The link destination object to format.

- **Returns:** string

```
var uri = document.formatLinkURI({
chapter: 0,
page: 42,
type: "FitV",
x: 0,
y: 0,
width: 100,
height: 50,
zoom: 1
})
page.createLink([0, 0, 100, 100], uri)
```

### DocumentWriter

DocumentWriter objects are used to create new documents in several formats.

#### Constructors

#### 

DocumentWriter(buffer, format, options)
DocumentWriter(filename, format, options)

Create a new document writer to create a document with the specified format and output options. If `format` is `null` it is inferred from the filename. The `options` argument is a comma separated list of flags and key-value pairs.

For supported output `format` values and `options` see
:doc:`/reference/common/document-writer-options`.

- **Buffer buffer**: The buffer to output to.
- **Buffer filename**: The file name to output to.
- **string format**: The file format.
- **string options**: The options as key-value pairs.

```
var writer = new mupdf.DocumentWriter(buffer, "PDF", "")
```

#### Instance methods

#### DocumentWriter.prototype.beginPage(mediabox)

Begin rendering a new page. Returns a `Device` that can be used to render the page graphics.

- **Rect mediaBox**: The page size.

- **Returns:** `Device`

```
var device = writer.beginPage([0, 0, 100, 100])
```

#### DocumentWriter.prototype.endPage()

Finish the page rendering.

```
writer.endPage()
```

#### DocumentWriter.prototype.close()

Finish the document and flush any pending output. It is a
requirement to make this call to ensure that the output file is
complete.

```
writer.close()
```

### DrawDevice

The DrawDevice can be used to render to a `Pixmap`; either by running a Page
using `Page.prototype.run()` with it or by calling the device methods directly.

#### Constructors

#### DrawDevice(matrix, pixmap)

Create a device for drawing into a `Pixmap`. The `Pixmap` bounds used should match the transformed page bounds, or you can adjust them to only draw a part of the page.

- **Matrix matrix**: The matrix to apply.
- **Pixmap pixmap**: The pixmap that will be drawn to.

```
var drawDevice = new mupdf.DrawDevice(mupdf.Matrix.identity, pixmap)
```

### Font

Font objects can be created from TrueType, OpenType,
Type1 or CFF fonts. In PDF there are also special
Type3 fonts.

#### Constructors

#### 

Font(name, data, index)
Font(name, filename, index)

Create a new font. Either from a buffer, a file name, or a
built-in font name.

The built-in standard PDF fonts are:

- `"Times-Roman"`
- `"Times-Italic"`
- `"Times-Bold"`
- `"Times-BoldItalic"`
- `"Helvetica"`
- `"Helvetica-Oblique"`
- `"Helvetica-Bold"`
- `"Helvetica-BoldOblique"`
- `"Courier"`
- `"Courier-Oblique"`
- `"Courier-Bold"`
- `"Courier-BoldOblique"`
- `"Symbol"`
- `"ZapfDingbats"`

The built-in CJK fonts are referenced by language code:
`"zh-Hant"`, `"zh-Hans"`, `"ja"`, `"ko"`.

- **string name**: Font name.
- **Buffer | ArrayBuffer | Uint8Array data**: The font data if loaded from a buffer.
- **string filename**: The name of the font file to load.
- **number index**: Subfont index (only used for TTC fonts).

```
var font1 = new mupdf.Font("Times-Roman")
var font2 = new mupdf.Font("ko")
```

                var font3 = new mupdf.Font("Comic Sans", "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf")
                var font4 = new mupdf.Font("Comic Sans", "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf", 1)
		var font5 = new mupdf.Font("Freight Sans", fs.readFileSync("DroidSansFallbackFull.ttf"))

#### Constants

Encoding constants for `PDFDocument.prototype.addSimpleFont`:

#### Font.SIMPLE_ENCODING_LATIN

WinAnsiEncoding (a.k.a. CP-1252)

#### Font.SIMPLE_ENCODING_GREEK

ISO-8859-7

#### Font.SIMPLE_ENCODING_CYRILLIC

KOI8-U

#### Instance methods

#### Font.prototype.getName()

Get the font name.

- **Returns:** string

```
var name = font.getName()
```

#### Font.prototype.encodeCharacter(unicode)

Get the glyph index for a unicode character. Glyph `0` is returned if the font does not have a glyph for the character.

- **number | string unicode**: The unicode character, or the first unicode character of a string.

- **Returns:** number.

```
var index = font.encodeCharacter(0x42)
```

#### Font.prototype.advanceGlyph(glyph , wmode)

Return advance width for a glyph in either horizontal or vertical writing mode.

- **number glyph**: The glyph as unicode character.
- **number wmode**: `0` for horizontal writing, and `1` for vertical writing, defaults to horizontal.

- **Returns:** number

```
var width = font.advanceGlyph(0x42)
```

#### Font.prototype.isBold()

Returns `true` if font is bold.

- **Returns:** boolean

```
var isBold = font.isBold()
```

#### Font.prototype.isItalic()

Returns `true` if font is italic.

- **Returns:** boolean

```
var isItalic = font.isItalic()
```

#### Font.prototype.isMono()

Returns `true` if font is monospaced.

- **Returns:** boolean

```
var isMono = font.isMono()
```

#### Font.prototype.isSerif()

Returns `true` if font is serif.

- **Returns:** boolean

```
var isSerif = font.isSerif()
```

### Image

#### Constructors

#### 

Image(pixmap, mask)
Image(data, mask)
Image(filename, mask)

Create an Image and populate it with image data decoded either from a pixmap,
a data buffer, or a file.

- **Pixmap pixmap**: A pixmap with the image data.
- **Buffer | ArrayBuffer | Uint8Array data**: Image data.
- **string filename**: Image file to load.
- **Image mask**: An optional image mask.

```
var image1 = new mupdf.Image(pixmap, imagemask)
var image2 = new mupdf.Image(buffer, imagemask)
var image3 = new mupdf.Image("logo.png", imagemask)
```

#### Instance methods

#### Image.prototype.getWidth()

Get the image width in pixels.

- **Returns:** number

```
var width = image.getWidth()
```

#### Image.prototype.getHeight()

Get the image height in pixels.

- **Returns:** number

```
var height = image.getHeight()
```

#### Image.prototype.getXResolution()

Returns the x resolution for the `Image` in dots per inch.

- **Returns:** number

```
var xRes = image.getXResolution()
```

#### Image.prototype.getYResolution()

Returns the y resolution for the `Image` in dots per inch.

- **Returns:** number

```
var yRes = image.getYResolution()
```

#### Image.prototype.getColorSpace()

Returns the `ColorSpace` for the `Image`. Returns null if the image has
no colors (for example if it is an opacity mask with only an alpha
channel).

- **Returns:** `ColorSpace` | null

```
var cs = image.getColorSpace()
```

#### Image.prototype.getNumberOfComponents()

Number of colors; plus one if an alpha channel is present.

- **Returns:** number

```
var num = image.getNumberOfComponents()
```

#### Image.prototype.getBitsPerComponent()

Returns the number of bits per component.

- **Returns:** number

```
var bits = image.getBitsPerComponent()
```

#### Image.prototype.getImageMask()

Returns `true` if this image is an image mask.

- **Returns:** boolean

```
var hasMask = image.getImageMask()
```

#### Image.prototype.getMask()

Get another `Image` used as a mask for this one.

- **Returns:** `Image` | null

```
var mask = image.getMask()
```

#### Image.prototype.getInterpolate()

Returns whether interpolation was used during decoding.

- **Returns:** boolean

```js
var interpolate = image.getInterpolate()
```

#### Image.prototype.getColorKey()

Returns an array with `2 * getNumberOfComponents()` integers
for an image with color key masking, or `null` if masking is
not used. Each pair of integers define an interval, and
component values within those intervals are not painted.

- **Returns:** Array of number | null

```js
var result = image.getColorKey()
```

#### Image.prototype.getDecode()

Returns an array with `2 * getNumberOfComponents()` numbers
for an image with color mapping, or `null` if mapping is not
used. Each pair of numbers define the lower and upper values to
which the component values are mapped linearly.

- **Returns:** Array of number | null

```js
var arr = image.getDecode()
```

#### Image.prototype.getOrientation()

Returns the orientation of the image.

- **Returns:** number

```js
var orientation = image.getOrientation()
```

#### Image.prototype.setOrientation(orientation)

Set the image orientation to the given orientation.

- **number orientation**: 
Orientation value described in this table:

======= ===========
Value	Description
======= ===========
0	Undefined
1	0 degree ccw rotation. (Exif = 1)
2	90 degree ccw rotation. (Exit = 8)
3	180 degree ccw rotation. (Exif = 3)
4	270 degree ccw rotation. (Exif = 6)
5	flip on X. (Exif = 2)
6	flip on X, then rotate ccw by 90 degrees. (Exif = 5)
7	flip on X, then rotate ccw by 180 degrees. (Exif = 4)
4	flip on X, then rotate ccw by 270 degrees. (Exif = 7)
======= ===========

#### Image.prototype.toPixmap(scaledWidth, scaledHeight)

Create a `Pixmap` from this image. The `scaledWidth` and
`scaledHeight` arguments are optional, but may be used to decode a
down-scaled `Pixmap`.

- **number scaledWidth**: 
- **number scaledHeight**: 

- **Returns:** `Pixmap`

```js
var pixmap = image.toPixmap()
var scaledPixmap = image.toPixmap(100, 100)
```

### Link

Link objects contain information about page links, such as their bounding
box and their destination URI.

To determine whether a link is document-internal or an external link to a web page, see `Link.prototype.isExternal()`.
Finally, to resolve a document-internal link see `Document.prototype.resolveLinkDestination()`.

#### Constructors

#### Link

*(not constructible with `new`)*

To obtain the links on a page see `Page.prototype.getLinks()`.

To create a new link on a page see `Page.prototype.createLink()`.

#### Instance methods

#### Link.prototype.getBounds()

Returns a rectangle describing the link's location on the page.

- **Returns:** `Rect`

```
var rect = link.getBounds()
```

#### Link.prototype.setBounds(rect)

Sets the bounds for the link's location on the page.

- **Rect rect**: Desired bounds for link.

```js
link.setBounds([0, 0, 100, 100])
```

#### Link.prototype.getURI()

Returns a string URI describing the link's destination. If
`isExternal()` returns `true`, this is a URI suitable for a
browser, if it returns `false` pass it to `Document.prototype.resolveLink` to access
to the destination page in the document.

- **Returns:** string

```
var uri = link.getURI()
```

#### Link.prototype.setURI(uri)

Sets this link's destination to the given URI. To create links to other
pages within the document, see `Document.prototype.formatLinkURI()`.

- **string uri**: An URI describing the desired link destination.

#### Link.prototype.isExternal()

Returns a boolean indicating if the link is external or not. URIs whose
link URI has a valid scheme followed by colon, e.g.
`https://example.com` and `mailto:test@example.com`, are defined to
be external.

- **Returns:** boolean

```
var isExternal = link.isExternal()
```

### LinkDestination

A link destination points to a location within a document and how a document viewer should show that destination.

#### LinkDestination(chapter, page, type, x, y, width, height)

- **number chapter**: 
- **number page**: 
- **"Fit" | "FitB" | "FitH" | "FitBH" | "FitV" | "FitBV" | "FitR" | "XYZ" type**: 

#### Constants

The possible type values:

#### LinkDestination.FIT

Display the page with contents zoomed to make the entire page visible.

#### LinkDestination.FIT_H

Scroll to the top coordinate and zoom to make the page width visible.

#### LinkDestination.FIT_V

Scroll to the left coordinate and zoom to make the page height visible.

#### LinkDestination.FIT_B

Zoom to fit the page bounding box.

#### LinkDestination.FIT_BH

Zoom to fit the page bounding box width.

#### LinkDestination.FIT_BV

Zoom to fit the page bounding box height.

#### LinkDestination.FIT_R

Scroll and zoom to make the specified rectangle visible.

#### LinkDestination.XYZ

Display with coordinates at the top left zoomed in to the specified magnification factor.

#### Instance properties

#### LinkDestination.prototype.chapter

    The chapter within the document.

#### LinkDestination.prototype.page

    The page within the document.

#### LinkDestination.prototype.type

    Either "Fit", "FitB", "FitH", "FitBH", "FitV", "FitBV", "FitR" or "XYZ".

    The type controls which of the x, y, width, height, and zoom values are used.

#### LinkDestination.prototype.x

    The left coordinate. Used for "FitV", "FitBV", "FitR", and "XYZ".

#### LinkDestination.prototype.y

    The top coordinate. Used for "FitH", "FitBH", "FitR", and "XYZ".

#### LinkDestination.prototype.width

    The width of the zoomed in region. Used for "FitR".

#### LinkDestination.prototype.height

    The height of the zoomed in region. Used for "FitR".

#### LinkDestination.prototype.zoom

    The zoom factor. Used for "XYZ".

### Matrix

A Matrix is used to compute transformations of points and graphics
objects.

3-by-3 matrices of this form can perform 2-dimensional transformations
such as translations, rotations, scaling, and skewing:

```
	/ a b 0 \
	| c d 0 |
	\ e f 1 /
```

Because of the fixes values in the right-most column, such a matrix is
represented in Javascript as an array of six numbers:

```
	[a, b, c, d, e, f]
```

In TypeScript the Matrix type is defined as follows:

```
	type Matrix = [number, number, number, number, number, number]
```

#### Constructors

#### Matrix

*(interface type)*

Matrices are not represented by a class; they are just plain arrays of six numbers.

#### Static properties

#### Matrix.identity

The identity matrix, short hand for `[1, 0, 0, 1, 0, 0]`.

```
var m = mupdf.Matrix.identity
```

#### Static methods

#### Matrix.scale(sx, sy)

Returns a scaling matrix, short hand for `[sx, 0, 0, sy, 0, 0]`.

- **number sx**: X scale as a floating point number.
- **number sy**: Y scale as a floating point number.

- **Returns:** `Matrix`

```
var m = mupdf.Matrix.scale(2, 2)
```

#### Matrix.translate(tx, ty)

Return a translation matrix, short hand for `[1, 0, 0, 1, tx, ty]`.

- **number tx**: X translation as a floating point number.
- **number ty**: Y translation as a floating point number.

- **Returns:** `Matrix`

```
var m = mupdf.Matrix.translate(2, 2)
```

#### Matrix.rotate(theta)

Return a rotation matrix, short hand for
`[cos(theta), sin(theta), -sin(theta), cos(theta), 0, 0]`.

- **number theta**: Rotation in degrees, positive for CW and negative for CCW.

- **Returns:** `Matrix`

```
var m = mupdf.Matrix.rotate(90)
```

#### Matrix.concat(a, b)

Concatenate matrices `a` and `b`. Bear in mind that matrix
multiplication is not commutative.

- **Matrix a**: Left side matrix.
- **Matrix b**: Right side matrix.

- **Returns:** `Matrix`

```
var m = mupdf.Matrix.concat([1, 1, 1, 1, 1, 1], [2, 2, 2, 2, 2, 2])
// expected result [4, 4, 4, 4, 6, 6]
```

#### Matrix.invert(matrix)

Inverts the supplied matrix and returns the result.

- **Matrix matrix**: Matrix to invert.

- **Returns:** `Matrix`

```
var m = mupdf.Matrix.invert([1, 0.5, 1, 1, 1, 1])
```

### MultiArchive

#### Constructors

#### MultiArchive()

Create a new empty multi archive.

```
var multiArchive = new mupdf.MultiArchive()
```

#### Instance methods

#### MultiArchive.prototype.mountArchive(subArchive, path)

Add an archive to the set of archives handled by a multi archive.
If `path` is `null`, the `subArchive` contents appear at the
top-level, otherwise they will appear prefixed by the string
`path`.

- **Archive subArchive**: An archive that will be a child archive of this one.
- **string path**: The path at which the archive will be inserted.

In the following example `example1.zip` contains `file1.txt` and
`example2.zip` contains `file2.txt`. The MultiArchive now lets you
access both `file1.txt` and `subpath/file2.txt`:

```
var archive = new mupdf.MultiArchive()
archive.mountArchive(new mupdf.Archive("example1.zip"), null)
archive.mountArchive(new mupdf.Archive("example2.zip"), "subpath")
console.log(archive.hasEntry("file1.txt"))
console.log(archive.hasEntry("subpath/file2.txt"))
```

### OutlineItem

Outline items are returned from the `Document.prototype.loadOutline` method and
represent a table of contents entry.

They are also used with the `OutlineIterator` interface.

#### Constructors

#### OutlineItem

*(interface type)*

Outline items are passed around as plain objects.

```js
	interface OutlineItem {
		title: string | undefined,
		uri: string | undefined,
		open: boolean,
		down?: OutlineItem[],
		page?: number,
	}
```

### OutlineIterator

An outline iterator can be used to walk over all the items in an Outline and
query their properties. To be able to insert items at the end of a list of
sibling items, it can also walk one item past the end of the list. To get an
instance of `OutlineIterator` use `Document.prototype.outlineIterator()`.

In the context of a PDF file, the document's Outline is also known as Table of
Contents or Bookmarks.

#### Constructors

#### OutlineIterator

*(not constructible with `new`)*

OutlineIterator instances are returned by `Document.prototype.outlineIterator()`.

#### Constants

Navigation return codes:

#### OutlineIterator.ITERATOR_DID_NOT_MOVE

Movement was not possible.

#### OutlineIterator.ITERATOR_AT_ITEM

New position has a valid item.

#### OutlineIterator.ITERATOR_AT_EMPTY

New position has no item, but one can be inserted here.

Style bit flags:

#### OutlineIterator.FLAG_BOLD

Bit is set outline item style is bold.

#### OutlineIterator.FLAG_ITALIC

Bit is set if outline item style is italic.

#### Instance methods

#### OutlineIterator.prototype.item()

Return a `OutlineItem` or `null` if out of range.

- **Returns:** `OutlineItem` | null

```
var obj = outlineIterator.item()
```

#### OutlineIterator.prototype.next()

Move the iterator position to "next". The return value is negative if
this movement is not possible, `0` if the new position has a valid
item, or `1` if the new position has no item but one can be inserted
here.

- **Returns:** number

```
var result = outlineIterator.next()
```

#### OutlineIterator.prototype.prev()

Move the iterator position to "previous". The return value is negative
if this movement is not possible, `0` if the new position has a valid
item, or `1` if the new position has no item but one can be inserted
here.

- **Returns:** number

```
var result = outlineIterator.prev()
```

#### OutlineIterator.prototype.up()

Move the iterator position "up". The result value is negative if this
movement is not possible, `0` if the new position has a valid item,
or `1` if the new position has no item but one can be inserted here.

- **Returns:** number

```
var result = outlineIterator.up()
```

#### OutlineIterator.prototype.down()

Move the iterator position "down". The return value is negative if this
movement is not possible, `0` if the new position has a valid item,
or `1` if the new position has no item but one can be inserted here.

- **Returns:** number

```
var result = outlineIterator.down()
```

#### OutlineIterator.prototype.insert(item)

Insert item before the current item. The position does not change. The
return value is `0` if the current position has a valid item, or
`1` if the position has no valid item.

- **OutlineItem item**: the item to insert

- **Returns:** number

```
var result = outlineIterator.insert(item)
```

#### OutlineIterator.prototype.delete()

Delete the current item. This implicitly moves to the next item. The
return value is `0` if the new position has a valid item, or `1` if
the position contains no valid item, but one may be inserted at this
position.

- **Returns:** number

```
outlineIterator.delete()
```

#### OutlineIterator.prototype.update(item)

Updates the current item properties with values from the supplied item's properties.

- **OutlineItem item**: An item populated with the properties that should be stored.

```
outlineIterator.update(item)
```

### Page

A Page object is a page that has been loaded from a `Document`.

Page objects that belong to a `PDFDocument`, also provide
the interface described in `PDFPage`.

#### Constructors

#### Page

*(not constructible with `new`)*

Page instances are returned by `Document.prototype.loadPage()`.

#### Constants

The :term:`page box` types:

#### Page.MEDIA_BOX

#### Page.CROP_BOX

#### Page.BLEED_BOX

#### Page.TRIM_BOX

#### Page.ART_BOX

#### Instance methods

#### Page.prototype.getBounds(box)

Returns a rectangle describing the page dimensions.

- **string box: Which :term**: `page box` to query.

- **Returns:** `Rect`

```
var rect = page.getBounds()
```

#### Page.prototype.run(device, transform)

Calls device functions for all the contents on the page, using the
specified transform.

The device can be one of the built-in devices (`DrawDevice` and `DisplayListDevice`)
or a Javascript `Device`.

The matrix transforms coordinates from user space to device space.

- **Device device**: The device object.
- **Matrix matrix**: The transformation matrix.

```
page.run(dev, mupdf.Matrix.identity)
```

#### Page.prototype.runPageContents(device, transform)

This is the same as the `Page.prototype.run()` method above but it only
runs the page itself and omits annotations and widgets.

- **Device device**: The device object.
- **Matrix matrix**: The transformation matrix.

```
page.runPageContents(dev, mupdf.Matrix.identity)
```

#### Page.prototype.runPageAnnots(device, transform)

This is the same as the `Page.prototype.run()` method above but it only
runs the page annotations.

- **Device device**: The device object.
- **Matrix matrix**: The transformation matrix.

```
page.runPageAnnots(dev, mupdf.Matrix.identity)
```

#### Page.prototype.runPageWidgets(device, transform)

This is the same as the `Page.prototype.run()` method above but it only
runs the page widgets.

- **Device device**: The device object.
- **Matrix matrix**: The transformation matrix.

```
page.runPageWidgets(dev, mupdf.Matrix.identity)
```

#### Page.prototype.toPixmap(matrix, colorspace, alpha, showExtras)

Render the page into a `Pixmap` using the specified transform
matrix and colorspace. If `alpha` is `true`, the page will be drawn
on a transparent background, otherwise white. If `showExtras` is
`true` then the operation will include any page annotations and/or
widgets.

- **Matrix matrix**: The transformation matrix.
- **ColorSpace colorspace**: The desired colorspace of the returned pixmap.
- **boolean alpha**: Whether the resulting pixmap should have an alpha component. Defaults to `true`.
- **boolean showExtras**: Whether to render annotations and widgets. Defaults to `true`.

- **Returns:** `Pixmap`

```
var pixmap = page.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true, true)
```

#### Page.prototype.toDisplayList(showExtras)

Record the contents on the page into a `DisplayList`. If
`showExtras` is `true` then the operation will include all
annotations and/or widgets on the page.

- **boolean showExtras**: Whether to render annotations and widgets. Defaults to `true`.

- **Returns:** `DisplayList`

```
var displayList = page.toDisplayList(true)
```

#### Page.prototype.toStructuredText(options)

Extract the text on the page into a `StructuredText` object.

- **string options**: 
See :doc:`/reference/common/stext-options`.

- **Returns:** `StructuredText`

```
var sText = page.toStructuredText("preserve-whitespace")
```

#### Page.prototype.search(needle, maxHits)

Search the page text for all instances of the `needle` value,
and return an array of search hits.

Each search hit is an array of `Quad`, each corresponding
to a character in the search hit.

- **string needle**: The text to search for.
- **number options**: Optional options for the search. A logical or of options such as `StructuredText.SEARCH_EXACT`.

- **Returns:** Array of Array of `Quad`

```
var results = page.search("my search phrase")
```

#### Page.prototype.getLinks()

Return an array of all the links on the page. If there are no
links then an empty array is returned.

Each link is an object with a 'bounds' property, and either a
'page' or 'uri' property, depending on whether it's an internal or
external link.

- **Returns:** Array of `Link`

```
var links = page.getLinks()
var link = links[0]
var linkDestination = doc.resolveLink(link)
```

#### Page.prototype.createLink(rect, uri)

Create a new link with the supplied metrics for the page, linking to the destination URI string.

To create links to other pages within the document see the `Document.prototype.formatLinkURI` method.

- **Rect rect**: Rectangle specifying the active area on the page the link should cover.
- **string destinationUri**: A URI string describing the desired link destination.

- **Returns:** `Link`.

```
// create a link to an external URL
var link = page.createLink([0, 0, 100, 50], "https://example.com")
```

// create a link to another page in the document
var link = page.createLink([0, 100, 100, 150], "#page=1&view=FitV,0")

#### Page.prototype.deleteLink(link)

Delete the link from the page.

- **Link link**: The link to remove.

```
page.deleteLink(link_obj)
```

#### Page.prototype.getLabel()

Returns the page number as a string using the numbering scheme of the document.

- **Returns:** string

```
var label = page.getLabel()
```

#### Page.prototype.isPDF()

Returns `true` if the page is from a PDF document.

- **Returns:** boolean

```
var isPDF = page.isPDF()
```

#### Page.prototype.decodeBarcode(subarea, rotate)

Decodes a barcode detected on the page, and returns an object with
properties for barcode type and contents.

- **Rect subarea**: Only detect barcode within subarea. Defaults to the entire page.
- **number rotate**: Degrees of rotation to rotate page before detecting barcode. Defaults to 0.

- **Returns:** Object with barcode information.

```js
var info = page.decodeBarcode(page.getBounds(), 0)
```

### Path

A path object represents vector graphics, as if drawn by a pen. They can
be used for stroking, filling or as a clip mask. The Path only determines
the shape, not properties such as color, line width or line joins, etc.

A path contains a sequence of zero or more subpaths, each subpath consists
of a sequence of zero or more basic operations which begins by a move to
point and ends with a close:

* move to point, append by calling `Path.prototype.moveTo()`
* close subpath, append by calling `Path.prototype.closePath()`
* draw a straight line, append by calling `Path.prototype.lineTo()`
* draw a cubic Bézier curve, append by calling either of `Path.prototype.curveTo()`,
  `Path.prototype.curveToV()`, or `Path.prototype.curveToY()`
* draw a rectangle, append by calling `Path.prototype.rect()`, this is a subpath on
  its own and is equivalent to a move to a position, followed by drawing three
  lines and closing the subpath.

After a subpath is closed, the only operation that may be appended is a
move to point, in order to start a new subpath. This is a requirement to
ensure that an unclosed path always has a current point from which to
start basic drawing operations.

Once all desired operations have been appended to a Path, its bounds can
be determined, and it can be transformed. Finally, it's possible to
iterate over the basic path operations via `Path.prototype.walk()`.

#### Constructors

#### Path()

Create a new empty path.

```
var path = new mupdf.Path()
```

#### Instance methods

#### Path.prototype.closePath()

Append operation closing the current subpath by drawing a straight
line to the point given in the last `moveTo()`. This also invalidates
the current point, but a new one may be set by calling `moveTo()`.

```
path.closePath()
```

#### Path.prototype.curveTo(x1, y1, x2, y2, x3, y3)

Append operation drawing a cubic Bézier curve from the current point
to (x3, y3) using (x1, y1) and (x2, y2) as control points.

- **number x1**: X1 coordinate.
- **number y1**: Y1 coordinate.
- **number x2**: X2 coordinate.
- **number y2**: Y2 coordinate.
- **number x3**: X3 coordinate.
- **number y3**: Y3 coordinate.

```
path.curveTo(0, 0, 10, 10, 100, 100)
```

#### Path.prototype.curveToV(cx, cy, ex, ey)

Append operation drawing a cubic Bézier curve from the current point
to (ex, ey) using the current point and (cx, cy) as control points.
Will be converted to `Path.prototype.curveTo()` when appended to this Path.

- **number cx**: CX coordinate.
- **number cy**: CY coordinate.
- **number ex**: EX coordinate.
- **number ey**: EY coordinate.

```
path.curveToV(0, 0, 100, 100)
```

#### Path.prototype.curveToY(cx, cy, ex, ey)

Append operation drawing a cubic Bézier curve from the current point
to (ex, ey) using the (cx, cy) and (ex, ey) as control points. Will be
converted to `Path.prototype.curveTo()` when appended to this Path.

- **number cx**: CX coordinate.
- **number cy**: CY coordinate.
- **number ex**: EX coordinate.
- **number ey**: EY coordinate.

```
path.curveToY(0, 0, 100, 100)
```

#### Path.prototype.getBounds(strokeState, transform)

Return a bounding rectangle for the path.

Since the path does not describe properties such as line width, line
joins, etc., the caller must provide a `strokeState`, containing
those properties, to determine the bounds of path if it is stroked.

If no `strokeState` is provided, this call determines the bounds of
the path if it is filled.

`transform` is applied to the points in the path while computing the
bounds, but it is not applied to the points as stored in path, i.e.
the Path is not altered.

- **StrokeState | null stroke**: The stroking properties to use.
- **Matrix transform**: A transform matrix applied to all points in path.

- **Returns:** `Rect`

```
var rect = path.getBounds(strokeState, mupdf.Matrix.identity)
```

#### Path.prototype.lineTo(x, y)

Append operation drawing a straight line from the current point to the
given point.

- **number x**: X coordinate.
- **number y**: Y coordinate.

```
path.lineTo(20, 20)
```

#### Path.prototype.moveTo(x, y)

Append operation of lifting and moving the pen to the given point.
This begins a new subpath and sets the current point.

- **number x**: X coordinate.
- **number y**: Y coordinate.

```
path.moveTo(10, 10)
```

#### Path.prototype.rect(x1, y1, x2, y2)

Shorthand for sequence:

```
moveTo(x1, y1)
lineTo(x2, y1)
lineTo(x2, y2)
lineto(x1, y2)
closePath()
```

- **number x1**: X1 coordinate.
- **number y1**: Y1 coordinate.
- **number x2**: X2 coordinate.
- **number y2**: Y2 coordinate.

```
path.rect(0, 0, 100, 100)
```

#### Path.prototype.transform(matrix)

Transform the path by applying the given transformation matrix.

This is done by transforming each point in all of the paths' basic
drawing operations.

- **Matrix matrix**: Transformation matrix to apply.

```
path.transform(mupdf.Matrix.scale(2, 2))
```

#### Path.prototype.walk(walker)

Iterate over all the basic drawing operations in this Path, calling
a corresponding callback in the walker object passing the coordinates
stored with the drawing operation.

- **PathWalker walker**: Object with callback functions.

```
function print(...args) {
console.log(args.join(" "))
}
```

var pathPrinter = {
moveTo: function (x, y) { print("moveTo", x, y) },
lineTo: function (x, y) { print("lineTo", x, y) },
curveTo: function (x1, y1, x2, y2, x3, y3) { print("curveTo", x1, y1, x2, y2, x3, y3) },
closePath: function () { print("closePath") },
}

var traceDevice = {
fillPath: function (path, evenOdd, ctm, colorSpace, color, alpha) {
print("fillPath", evenOdd, ctm, colorSpace, color, alpha)
path.walk(pathPrinter)
},
clipPath: function (path, evenOdd, ctm) {
print("clipPath", evenOdd, ctm)
path.walk(pathPrinter)
},
strokePath: function (path, stroke, ctm, colorSpace, color, alpha) {
print("strokePath", JSON.stringify(stroke), ctm, colorSpace, color, alpha)
path.walk(pathPrinter)
},
clipStrokePath: function (path, stroke, ctm) {
print("clipStrokePath", JSON.stringify(stroke), ctm)
path.walk(pathPrinter)
}
}

var doc = mupdf.Document.openDocument(fs.readFileSync("test.pdf"), "application/pdf")
var page = doc.loadPage(0)
var device = new mupdf.Device(traceDevice)
page.run(device, mupdf.Matrix.identity)

### PathWalker

#### Constructors

#### PathWalker

*(interface type)*

An object implementing this interface of optional callback functions
can be used to get calls whenever `Path.prototype.walk()` iterates over a
basic drawing operation corresponding to that of the function name.

#### closePath()

Called when `Path.prototype.walk()` encounters a close subpath operation.

#### curveTo(x1, y1, x2, y2, x3, y3)

Called when `Path.prototype.walk()` encounters an operation drawing a Bézier
curve from the current point to (x3, y3) using (x1, y1) and (x2, y2)
as control points.

- **number x1**: X1 coordinate.
- **number y1**: Y1 coordinate.
- **number x2**: X2 coordinate.
- **number y2**: Y2 coordinate.
- **number x3**: X3 coordinate.
- **number y3**: Y3 coordinate.

#### lineTo(x, y)

Called when `Path.prototype.walk()` encounters an operation drawing a straight
line from the current point to the given point.

- **number x**: X coordinate.
- **number y**: Y coordinate.

#### moveTo(x, y)

Called when `Path.prototype.walk()` encounters an operation moving the pen to
the given point, beginning a new subpath and sets the current point.

- **number x**: X coordinate.
- **number y**: Y coordinate.

### Pixmap

A Pixmap object contains a color raster image (short for pixel map).
The components in a pixel in the Pixmap are all byte values,
with the transparency as the last component.

A Pixmap also has a location (x, y) in addition to its size;
so that they can easily be used to represent tiles of a page.

#### Constructors

#### 

Pixmap(colorspace, bbox, alpha)
Pixmap(pixmap, mask)

Create a new empty Pixmap whose pixel data is **not**
initialized. Alternatively create a new Pixmap based on an
existing Pixmap without alpha and combine it with a single
component soft mask of the same dimensions.

- **ColorSpace | null colorspace**: The desired colorspace for the new pixmap. `null` implies a single component alpha pixmap.
- **Rect bbox**: The desired dimensions of the new pixmap.
- **boolean alpha**: Whether the new pixmap should have an alpha component.
- **Pixmap pixmap**: The original pixmap without alpha.
- **Pixmap mask**: Soft mask used as alpha in the combined pixmap.

```
var pixmap1 = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0, 0, 100, 100], true)
var pixmap2 = new mupdf.Pixmap(
new mupdf.Image("photo.png").toPixmap(),
new mupdf.Image("softmask.png").toPixmap()
)
```

#### Instance methods

#### Pixmap.prototype.clear(value)

Clear the pixels to the specified value. Pass 255 for white, 0 for black, or omit for transparent.

- **number value**: The value to use for clearing.

```
pixmap.clear(255)
```

#### Pixmap.prototype.getBounds()

Return the pixmap bounds.

- **Returns:** `Rect`

```
var rect = pixmap.getBounds()
```

#### Pixmap.prototype.getWidth()

Get the width of the pixmap.

- **Returns:** number

```
var w = pixmap.getWidth()
```

#### Pixmap.prototype.getHeight()

Get the height of the pixmap.

- **Returns:** number

```
var h = pixmap.getHeight()
```

#### Pixmap.prototype.getNumberOfComponents()

Number of colors; plus one if an alpha channel is present.

- **Returns:** number

```
var num = pixmap.getNumberOfComponents()
```

#### Pixmap.prototype.getAlpha()

Returns whether an alpha channel is present.

- **Returns:** boolean

```
var alpha = pixmap.getAlpha()
```

#### Pixmap.prototype.getStride()

Number of bytes per row.

- **Returns:** number

```
var stride = pixmap.getStride()
```

#### Pixmap.prototype.getColorSpace()

Returns the colorspace of this pixmap. Returns null if the pixmap has
no colors (for example if it is an opacity mask with only an alpha
channel).

- **Returns:** `ColorSpace` | null

```
var cs = pixmap.getColorSpace()
```

#### Pixmap.prototype.setResolution(x, y)

Set horizontal and vertical resolution.

- **number x**: Horizontal resolution in dots per inch.
- **number y**: Vertical resolution in dots per inch.

```
pixmap.setResolution(300, 300)
```

#### Pixmap.prototype.getX()

Returns the x coordinate of the pixmap.

```js
var x = pixmap.getX()
```

#### Pixmap.prototype.getY()

Returns the y coordinate of the pixmap.

- **Returns:** number

```js
var y = pixmap.getY()
```

#### Pixmap.prototype.getXResolution()

Returns the horizontal resolution in dots per inch for this pixmap.

- **Returns:** number

```
var xRes = pixmap.getXResolution()
```

#### Pixmap.prototype.getYResolution()

Returns the vertical resolution in dots per inch for this pixmap.

- **Returns:** number

```
var yRes = pixmap.getYResolution()
```

#### Pixmap.prototype.invert()

Invert all pixels. All components are processed, except alpha which is unchanged.

```
pixmap.invert()
```

#### Pixmap.prototype.invertLuminance()

Transform all pixels so that luminance of each pixel is inverted,
and the chrominance remains as unchanged as possible.
All components are processed, except alpha which is unchanged.

```
pixmap.invertLuminance()
```

#### Pixmap.prototype.gamma(p)

Apply gamma correction to this pixmap. All components are processed,
except alpha which is unchanged.

Values `>= 0.1 & < 1` darkens the pixmap, `> 1 & < 10` lightens the pixmap.

- **number p**: Desired gamma level.

```
pixmap.gamma(3.5)
```

#### Pixmap.prototype.tint(black, white)

Tint all pixels in RGB, BGR or Gray pixmaps.
Map black and white respectively to the given hex RGB values.

- **Color | number black**: Map black to this color.
- **Color | number white**: Map white to this color.

```
pixmap.tint(0xffff00, 0xffff00)
```

#### Pixmap.prototype.warp(points, width, height)

Return a warped subsection of this pixmap, where the corner of
the input quadrilateral will be "warped" to become the four corner
points of the returned pixmap defined by the requested dimensions.

- **Quad points**: The corners of a convex quadrilateral within the `Pixmap` to be warped.
- **number width**: Width of resulting pixmap.
- **number height**: Height of resulting pixmap.

- **Returns:** `Pixmap`

```
var warpedPixmap = pixmap.warp([[0, 0], [100, 100], [130, 170], [150, 200]], 200, 200)
```

#### Pixmap.prototype.autowarp(points)

Same as `Pixmap.prototype.warp()` except that width and height
are automatically determined.

- **Quad points**: The corners of a convex quadrilateral within the `Pixmap` to be warped.

- **Returns:** `Pixmap`

```js
var warpedPixmap = pixmap.autowarp([0,0,100,0,0,100,100,100])
```

#### Pixmap.prototype.convertToColorSpace(colorspace, keepAlpha)

Convert pixmap into a new pixmap of a desired colorspace.
A proofing colorspace, a set of default colorspaces and color
parameters used during conversion may be specified.
Finally a boolean indicates if alpha should be preserved
(default is to not preserve alpha).

- **ColorSpace colorspace**: The desired colorspace.
- **boolean keepAlpha**: Whether to keep the alpha component.

- **Returns:** `Pixmap`

#### Pixmap.prototype.getPixels()

Returns an array of pixels for this pixmap.

- **Returns:** Array of number

```
var pixels = pixmap.getPixels()
```

#### Pixmap.prototype.asPNG()

Returns a buffer of this pixmap as a PNG.

- **Returns:** `Buffer`

```
var buffer = pixmap.asPNG()
```

#### Pixmap.prototype.asPSD()

Returns a buffer of this pixmap as a PSD.

- **Returns:** `Buffer`

```
var buffer = pixmap.asPSD()
```

#### Pixmap.prototype.asPAM()

Returns a buffer of this pixmap as a PAM.

- **Returns:** `Buffer`

```
var buffer = pixmap.asPAM()
```

#### Pixmap.prototype.asJPEG(quality, invert_cmyk)

Returns a buffer of this pixmap as a JPEG.
Note, if this pixmap has an alpha channel then an exception will be thrown.

- **number quality**: Desired compression quality, between `0` and `100`.
- **boolean invert_cmyk: How to handle polarity in :term**: `CMYK JPEG` images.

- **Returns:** `Buffer`

```
var buffer = pixmap.asJPEG(80, false)
```

#### Pixmap.prototype.decodeBarcode(rotate)

Decodes a barcode detected in the pixmap, and returns an object with
properties for barcode type and contents.

- **number rotate**: Degrees of rotation to rotate pixmap before detecting barcode. Defaults to 0.

- **Returns:** Object with barcode information.

```js
var barcodeInfo = displayList.decodeBarcode([0, 0, 100, 100 ], 0)
```

#### Pixmap.prototype.encodeBarcode(barcodeType, contents, size, errorCorrectionLevel, quietZones, humanReadableText)

Encodes a barcode into a pixmap. The supported types of barcode is either one of:

| Matrix | Linear Product | Linear Industrial |  |  |  |
| --- | --- | --- | --- | --- | --- |
| String | Name | String | Name | String | Name |
| `qrcode` | QR Code | `upca` | UPC-A | `code39` | Code 39 |
| `microqrcode` | Micro QR Code | `upce` | UPC-E | `code93` | Code 93 |
| `rmqrcode` | rMQR Code | `ean8` | EAN-8 | `code128` | Code 128 |
| `aztec` | Aztec | `ean13` | EAN-13 | `codabar` | Codabar |
| `datamatrix` | DataMatrix | `databar` | DataBar | `databarexpanded` | DataBar Expanded |
| `pdf417` | PDF417 | `databarlimited` | DataBar Limited | `dxfilmedge` | DX Film Edge |
| `maxicode` | MaxiCode | \ | `itf` | ITF |  |

- **string barcodeType**: The desired barcode type.
- **string contents**: The textual content to encode into the barcode.
- **number size**: The size of the barcode in pixels.
- **number errorCorrectionLevel**: The error correction level (0-8).
- **boolean quietZones**: Whether to add an empty margin around the barcode.
- **boolean humanReadableText**: Whether to add human-readable text. Some barcodes, e.g. EAN-13, can have the barcode contents printed in human-readable text next to the barcode.

- **Returns:** `Pixmap`

```js
var pix = Pixmap.encodeBarcode("qrcode", "Hello world!", 100, 2, true, false)
```

#### Pixmap.prototype.getSample(x, y, index)

Get the value of component `index` at position x, y (relative to
the image origin: 0, 0 is the top left pixel).

- **number x**: X coordinate.
- **number y**: Y coordinate.
- **number index**: Component index. i.e. For CMYK ColorSpaces 0 = Cyan, 3 = Black, for RGB 0 = Red, 2 == Blue etc.

- **Throws:** RangeError if x, y, or index are out of range.

- **Returns:** number

```js
// Get green component of pixel at 10, 10
var sample = rgbpixmap.getSample(10, 10, 1)
```

#### Pixmap.prototype.saveAsPNG(filename)

Save this Pixmap as a PNG. Only works for gray and RGB images.

- **string filename**: Desired name of image file.

```js
pixmap.saveAsPNG("filename.png")
```

#### Pixmap.prototype.saveAsJPEG(filename, quality)

Save this Pixmap as a JPEG file. Only works for gray, RGB and CMYK images.

- **string filename**: Desired name of image file.
- **number quality**: Desired quality between 0 and 100. Defaults to 90.

```js
pixmap.saveAsJPEG("filename.jpg", 80)
```

#### Pixmap.prototype.saveAsPAM(filename)

Save this Pixmap as a PAM file.

- **string filename**: Desired name of image file.

```js
pixmap.saveAsPAM("filename.pam")
```

#### Pixmap.prototype.saveAsPNM(filename)

Save this Pixmap as a PNM file. Only works for gray and RGB images without alpha.

- **string filename**: Desired name of image file.

```js
pixmap.saveAsPNM("filename.pnm")
```

#### Pixmap.prototype.saveAsPBM(filename)

Save this Pixmap as a PBM file. Only works for alpha only, gray and CMYK images without alpha.

- **string filename**: Desired name of image file.

```js
pixmap.saveAsPBM("filename.pbm")
```

#### Pixmap.prototype.saveAsPKM(filename)

Save this Pixmap as a PKM file. Only works for alpha only, gray and CMYK images without alpha.

- **string filename**: Desired name of image file.

```js
pixmap.saveAsPKM("filename.pkm")
```

#### Pixmap.prototype.saveAsJPX(filename, quality)

Save this Pixmap as a JPX file.

- **string filename**: Desired name of image file.
- **number quality**: Desired quality between 0 and 100. Defaults to 90.

```js
pixmap.saveAsJPX("filename.jpx", 90)
```

#### Pixmap.prototype.detectDocument(points)

Detect a "document" in a `Pixmap`. Only a grayscale `Pixmap`
without alpha is supported, anything else will cause an exception
to be thrown.

Returns null if no document was detected.

- **Returns:** `Quad` | null

```js
var documentLocation = pixmap.detectDocument([0,0,100,0,100,100,0,100])
```

#### Pixmap.prototype.detectSkew()

Returns the angle of skew detected from `Pixmap`.
Note, if the `Pixmap` is not Greyscale with no alpha then an exception will be thrown.

- **Returns:** number

```js
var angle = pixmap.detectSkew()
```

#### Pixmap.prototype.deskew(angle, border)

Returns a new `Pixmap` being the deskewed version of the supplied `Pixmap`.
Note, if a `Pixmap` is supplied that is not RGB or Greyscale, or has alpha then an exception will be thrown.

- **number angle**: The angle to deskew.
- **string border**: "increase" increases the size of the pixmap so no pixels are lost. "maintain" maintains the size of the pixmap. "decrease" decreases the size of the page so no new pixels are shown.
- **Returns:** `Pixmap`

```js
var deskewed = pixmap.deskew(angle, 0)
```

#### Pixmap.prototype.computeMD5()

Returns the MD5 digest of the pixmap pixel data.
The digest is returned as a string of 16 hex digits.

- **Returns:** string

```js
var md5 = pixmap.computeMD5()
```

### Point

A Point describes a point in space. It is represented by an array with two coordinates.

#### Constructors

#### Point

*(interface type)*

Points are not represented by a class; they are just plain arrays of two numbers.

#### Static methods

#### Point.transform(point, matrix)

Transforms the supplied point by the given transformation matrix.

- **Point point**: Point to transform.
- **Matrix matrix**: Matrix describing transformation to perform.

- **Returns:** `Point`

```
var m = mupdf.Point.transform([100, 100], [1, 0.5, 1, 1, 1, 1])
```

### Quad

An object representing a quadrilateral or :term:`QuadPoint`.

In TypeScript the Quad type is defined as follows:

```
	type Quad = [ number, number, number, number number, number, number, number ]
```

#### Constructors

#### Quad

*(interface type)*

Quads are not represented by a class; they are just plain arrays of eight numbers.

#### Static properties

#### Quad.empty

A quad whose coordinates are such that it is categorized as empty.

#### Quad.invalid

A quad whose coordinates are such that it is categorized as invalid.

#### Quad.infinite

A quad whose coordinates are such that it is categorized as infinite.

#### Static methods

#### Quad.isEmpty(quad)

Returns a boolean indicating if the quad is empty or not.

- **Quad quad**: Rectangle to evaluate.

- **Returns:** boolean

```
var isEmpty = mupdf.Quad.isEmpty([0, 0, 0, 0]) // true
var isEmpty = mupdf.Quad.isEmpty([0, 0, 100, 100]) // false
```

#### Quad.isValid(quad)

Returns a boolean indicating if the quad is valid or not.

- **Quad quad**: Rectangle to evaluate.

- **Returns:** boolean

```
var isValid = mupdf.Quad.isValid([0, 0, 100, 100]) // true
var isValid = mupdf.Quad.isValid([0, 0, -100, 100]) // false
```

#### Quad.isInfinite(quad)

Returns a boolean indicating if the quad is infinite or not.

- **Quad quad**: Rectangle to evaluate.

- **Returns:** boolean

```
var isInfinite = mupdf.Quad.isInfinite([0x80000000, 0x80000000, 0x7fffff80, 0x7fffff80]) //true
var isInfinite = mupdf.Quad.isInfinite([0, 0, 100, 100]) // false
```

#### Quad.transform(quad, matrix)

Transforms the supplied quad by the given transformation matrix.

Transforming an invalid, empty or infinite quad results in the
supplied quad being returned without change.

- **Quad quad**: Quad to transform.
- **Matrix matrix**: Matrix describing transformation to perform.

- **Returns:** `Quad`

```
var m = mupdf.Quad.transform([0, 0, 100, 100], [1, 0.5, 1, 1, 1, 1])
```

#### Quad.isPointInside(quad, point)

Return whether the point is inside the quad.

:returns boolean

```
var inside = mupdf.Rect.isPointInside([0, 0, 100, 100], [50, 50])
```

#### Quad.quadFromRect(rect)

Create a Quad that maps exactly to the coordinate of rectangle.

- **Rect rect**: 

- **Returns:** `Quad`

### Rect

A Rect describes an axis-aligned rectangle by specifying the coordinates
of its upper left and lower right corners. In Javascript they are
represented by a 4-element array: `[minX, minY, maxX, maxY]`

The relationship between the `minX`, `minY`, `maxX` and `maxY`
values can be set so that a rectangle is categorized as invalid, empty or
infinite. This matters when passing such rectangles to
`Rect.transform()`. There are convenience functions to obtain such
rectangles: `Rect.empty`, `Rect.invalid` and
`Rect.infinite`.

In TypeScript the Rect type is defined as follows:

```
	type Rect = [number, number, number, number]
```

#### Constructors

#### Rect

*(interface type)*

Rects are not represented by a class; they are just plain arrays of four numbers.

#### Static properties

#### Rect.empty

A rectangle whose coordinates are such that it is categorized as empty.

#### Rect.invalid

A rectangle whose coordinates are such that it is categorized as invalid.

#### Rect.infinite

A rectangle whose coordinates are such that it is categorized as infinite.

#### Static methods

#### Rect.isEmpty(rect)

Returns whether the rectangle is empty or not.

- **Rect rect**: Rectangle to evaluate.

- **Returns:** boolean

```
var isEmpty = mupdf.Rect.isEmpty([0, 0, 0, 0]) // true
var isEmpty = mupdf.Rect.isEmpty([0, 0, 100, 100]) // false
```

#### Rect.isValid(rect)

Returns whether the rectangle is valid or not.

- **Rect rect**: Rectangle to evaluate.

- **Returns:** boolean

```
var isValid = mupdf.Rect.isValid([0, 0, 100, 100]) // true
var isValid = mupdf.Rect.isValid([0, 0, -100, 100]) // false
```

#### Rect.isInfinite(rect)

Returns whether the rectangle is infinite or not.

- **Rect rect**: Rectangle to evaluate.

- **Returns:** boolean

```
var isInfinite = mupdf.Rect.isInfinite([0x80000000, 0x80000000, 0x7fffff80, 0x7fffff80]) //true
var isInfinite = mupdf.Rect.isInfinite([0, 0, 100, 100]) // false
```

#### Rect.transform(rect, matrix)

Transforms the supplied rectangle by the given transformation matrix.

Transforming an invalid, empty or infinite rectangle results in the
supplied rectangle being returned without change.

- **Rect rect**: Rectangle to transform.
- **Matrix matrix**: Matrix describing transformation to perform.

- **Returns:** `Rect`

```
var m = mupdf.Rect.transform([0, 0, 100, 100], [1, 0.5, 1, 1, 1, 1])
```

#### Rect.isPointInside(rect, point)

Return whether the point is inside the rectangle.

:returns boolean

```
var inside = mupdf.Rect.isPointInside([0, 0, 100, 100], [50, 50])
```

#### Rect.rectFromQuad(quad)

Create a Rect that encompasses the entire quad.

- **Quad quad**: 

- **Returns:** `Rect`

### Shade

A Shade object is used to define shadings.

#### Shade

*(not constructible with `new`)*

#### Instance methods

#### Shade.prototype.getBounds()

Returns a rectangle containing the dimensions of the shading
contents.

- **Returns:** `Rect`

```
var bounds = shade.getBounds()
```

### Story

#### Constructors

#### Story(contents, userCSS, em, archive)

Create a new story with the given contents, formatted according to the
provided user-defined CSS and em size, and an archive to lookup images,
etc.

- **string contents**: HTML source code. If omitted, a basic minimum is generated.
- **string userCSS**: CSS source code. Defaults to the empty string.
- **number em**: The default text font size. Default to 12.
- **Archive archive**: An archive from which to load resources for rendering. Currently supported resource types are images and text fonts. If omitted, the Story will not try to look up any such data and may thus produce incomplete output. Defaults to null.

```
var story = new mupdf.Story(<contents>, <css>, <em>, <archive>)
```

#### Instance methods

#### Story.prototype.document()

Return an `DOM` for an unplaced story. This allows adding content before placing the `Story`.

- **Returns:** `DOM`

```
var xml = story.document()
```

#### Story.prototype.place(rect, flags)

Place (or continue placing) this Story into the supplied rectangle.
Call `draw()` to draw the placed content before calling `place()` again
to continue placing remaining content.

`more` in the returned object can take either of these values:

* 0 means that all the text was placed successfully.

* 1 means that some text was not placed due not fitting within the height of the rectangle.

* 2 means that some text was not placed due not fitting within the width of the rectangle. For this to be detected `flags` must be set to 1.

- **Rect rect**: Rectangle to place the story within.
- **number flags**: When set to 1, will detect when a word does not fit in the rectangle, instead `more` set to 2.

- **Returns:** `{ filled: Rect, more: number }`

```
do {
var result = story.place([0, 0, 100, 100])
// TODO: create device for this bit of story
story.draw(device, mupdf.Matrix.identity)
// TODO: close device
} while (result.more)
```

#### Story.prototype.draw(device, transform)

Draw the placed Story to the given `Device` with the given transform.

- **Device device**: The device
- **Matrix transform**: The transform matrix.

```
story.draw(device, mupdf.Matrix.identity)
```

### StrokeState

A StrokeState controls the properties of how stroking operations are performed.
Besides controlling the line width, it is also possible to control
:term:`line cap style`, :term:`line join style`, and the :term:`miter limit`.

#### Constructors

#### StrokeState(template)

Create a new empty stroke state object.

The Javascript object used as template allows for setting
the following attributes:

* "lineCap": string: The :term:`line cap style` to be used. One of `"Butt" | "Round" | "Square"`
* "lineJoin": string: The :term:`line join style` to be used. One of `"Miter" | "Round" | "Bevel"`
* "lineWidth": number: The line width to be used.
* "miterLimit": number: The :term:`miter limit` to be used.
* "dashPhase": number: The dash phase to be used.
* "dashPattern": Array of number: The sequence of dash lengths to be used.

- **Object template**: An object with the parameters to set.

```
var strokeState = new mupdf.StrokeState({
lineCap: "Square",
lineJoin: "Bevel",
lineWidth: 2.0,
miterLimit: 1.414,
dashPhase: 11,
dashPattern: [ 2, 3 ]
})
```

#### Instance methods

#### StrokeState.prototype.getLineCap()

Get the :term:`line cap style`.

- **Returns:** `"Butt" | "Round" | "Square"`

```
var lineCap = strokeState.getLineCap()
```

#### StrokeState.prototype.getLineJoin()

Get the :term:`line join style`.

- **Returns:** `"Miter" | "Round" | "Bevel"`

```
var lineJoin = strokeState.getLineJoin()
```

#### StrokeState.prototype.getLineWidth()

Get the line line width.

- **Returns:** number

```
var width = strokeState.getLineWidth()
```

#### StrokeState.prototype.getMiterLimit()

Get the :term:`miter limit`.

- **Returns:** number

```
var limit = strokeState.getMiterLimit()
```

#### StrokeState.prototype.getDashPhase()

Get the dash pattern phase (where in the dash pattern stroking starts).

- **Returns:** number

```js
var limit = strokeState.getDashPhase()
```

#### StrokeState.prototype.getDashPattern()

Get the dash pattern as an array of numbers specifying alternating
lengths of dashes and gaps, or null if none set.

- **Returns:** Array of number | null

```js
var dashPattern = strokeState.getDashPattern()
```

### StructuredText

StructuredText objects hold text from a page that has been analyzed and grouped
into blocks, lines and spans.

#### Constructors

#### StructuredText

*(not constructible with `new`)*

To obtain a StructuredText instance use `Page.prototype.toStructuredText()`.

#### Static properties

#### StructuredText.SEARCH_EXACT

used to search untransformed text

#### StructuredText.SEARCH_IGNORE_CASE

used to search text ignoring case differences

#### StructuredText.SEARCH_IGNORE_DIACRITICS

used to search text ignoring diacritics

#### StructuredText.SEARCH_REGEXP

used to search text with the needle being a regexp

#### StructuredText.SEARCH_KEEP_LINES

used to search text preserving line breaks.

#### StructuredText.SEARCH_KEEP_PARAGRAPHS

used to search text preserving paragraph breaks.

#### StructuredText.SEARCH_KEEP_HYPHENS

used to search text preserving hyphens and not joining lines.

#### Instance methods

#### StructuredText.prototype.search(needle, maxHits)

Search the text for all instances of needle, and return an array with all matches found on the page.

Each match in the result is an array containing one or more Quads that cover the matching text.

- **string needle**: The text to search for.
- **number options**: Optional options for the search. A logical or of options such as `StructuredText.SEARCH_EXACT`.

- **Returns:** Array of Array of `Quad`

```
var result = sText.search("Hello World!")
```

#### StructuredText.prototype.highlight(p, q, maxHits)

Return an array of `Quad` used to highlight a selection defined by the start and end points.

- **Point p**: Start point.
- **Point q**: End point.
- **number maxHits**: The maximum number of hits to return. Default 500.

- **Returns:** Array of `Quad`

```
var result = sText.highlight([100, 100], [200, 100])
```

#### StructuredText.prototype.copy(p, q)

Return the text from the selection defined by the start and end points.

- **Point p**: Start point.
- **Point q**: End point.

- **Returns:** string

```
var result = sText.copy([100, 100], [200, 100])
```

#### StructuredText.prototype.walk(walker)

- **StructuredTextWalker walker**: Callback object.

Walk through the blocks (images or text blocks) of the structured text.
For each text block walk over its lines of text, and for each line each
of its characters. For each block, line or character the walker will
have a method called.

```
var sText = page.toStructuredText()
sText.walk({
beginLine: function (bbox, wmode, direction) {
console.log("beginLine", bbox, wmode, direction)
},
endLine: function () {
console.log("endLine")
},
beginTextBlock: function (bbox) {
console.log("beginTextBlock", bbox)
},
endTextBlock: function () {
console.log("endTextBlock")
},
beginStruct: function (standard, raw, index) {
console.log("beginStruct", standard, raw, index)
},
endStruct: function () {
console.log("endStruct")
},
onChar: function (utf, origin, font, size, quad, argb, flags) {
console.log("onChar", utf, origin, font, size, quad, argb, flags)
},
onImageBlock: function (bbox, transform, image) {
console.log("onImageBlock", bbox, transform, image)
},
onVector: function (bbox, flags, argb) {
console.log("onVector", bbox, flags, argb)
},
})
```

#### StructuredText.prototype.asText()

Returns a plain text representation.

- **Returns:** string

#### StructuredText.prototype.asHTML(id)

Returns a string containing an HTML rendering of the text.

- **number id**: 
Used to number the "id" on the top div tag (as `"page" + id`).

- **Returns:** string

#### StructuredText.prototype.asJSON(scale)

Returns a JSON string representing the structured text data.

This is a simplified serialization of the information that
`StructuredText.prototype.walk()` provides.

Note: You must extract the structured text with "preserve-spans"!
If you forget to set this option, any font changes in the middle of the
line will not be present in the JSON output.

- **number scale**: Optional scaling factor to multiply all the coordinates by.

- **Returns:** string containing JSON of the following schema:

```typescript
type StructuredTextPage = {
blocks: StructuredTextBlock[]
}
type StructuredTextBlock = {
type: "image" | "text",
bbox: {
x: number,
y: number,
w: number,
h: number
},
lines: StructuredTextLine[],
}
type StructuredTextLine = {
wmode: 0 | 1,	// 0=horizontal, 1=vertical
bbox: {
x: number,
y: number,
w: number,
h: number
},
font: {
name: string,
family: "serif" | "sans-serif" | "monospace",
weight: "normal" | "bold",
style: "normal" | "italic",
size: number
},
// text origin point for first character in line
x: number,
y: number,
text: string
}
```

```
var data = JSON.parse(page.toStructuredText("preserve-spans").asJSON())
```

### StructuredTextWalker

A structured text walker is an object with (optional) callback methods
used to iterate over the contents of a `StructuredText`.

#### Constructors

#### StructuredTextWalker

*(interface type)*

On beginLine the direction parameter is a vector (e.g. [0, 1]) and
can you can calculate the rotation as an angle with some trigonometry on the vector.

#### beginTextBlock(bbox)

Called before every text block in the `StructuredText`.

- **Rect bbox**: 

#### endTextBlock()

Called after every text block.

#### beginLine(bbox, wmode, direction)

Called before every line of text in a block.

- **Rect bbox**: 
- **number wmode**: 
- **Point direction**: 

#### endLine()

Called after every line of text.

#### beginStruct(standard, raw, index)

Called to indicate that a new structure element begins. May not
be neatly nested within blocks or lines.

- **string standard**: 
- **string raw**: 
- **number index**: 

#### endStruct()

Called after every structure element.

#### onChar(c, origin, font, size, quad, color, flags)

Called for every character in a line of text.

- **string c**: 
- **Point origin**: 
- **Font font**: 
- **number size**: 
- **Quad quad**: 
- **Color color**: 
- **number flags**: 

#### onImageBlock(bbox, transform, image)

Called for every image in a `StructuredText` if its options were
set to preserve images.

- **Rect bbox**: 
- **Matrix transform**: 
- **Image image**: 

#### onVector(bbox, flags, rgb)

Called for every vector in a `StructuredText` if its options
were set to collect vectors.

- **Rect bbox**: 
- **Object flags**: 
- **Array of number rgb**: 

The flags object is of the form `{ isStroked: boolean, isRectangle: boolean }`.

### Text

A Text object contains text. See the `Device.prototype.fillText`.

#### Constructors

#### Text()

Create a new empty text object.

```
var text = new mupdf.Text()
```

#### Instance methods

#### Text.prototype.getBounds(strokeState, transform)

Get the bounds of the instance.

- **StrokeState strokeState**: The stroking state.
- **Matrix transform**: The transformation matrix.

#### Text.prototype.showGlyph(font, trm, gid, uni, wmode)

Add a glyph to the text object.

Transform is the text matrix, specifying font size and glyph location.
For example: `[size, 0, 0, -size, x, y]`.

Glyph and unicode may be `-1` for n-to-m cluster mappings. For
example, the "fi" ligature would be added in two steps: first the glyph
for the 'fi' ligature and the unicode value for 'f'; then glyph `-1`
and the unicode value for 'i'.

- **Font font**: Font object.
- **Matrix trm**: Transformation matrix.
- **number gid**: Glyph id.
- **number uni**: Unicode codepoint.
- **number wmode**: `0` (default) for horizontal writing, and `1` for vertical writing.

```
text.showGlyph(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, 21, 0x66, 0)
text.showGlyph(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, -1, 0x69, 0)
```

#### Text.prototype.showString(font, trm, str, wmode)

Add a simple string to this Text object. Will do font substitution if the font does not have all the unicode characters required.

- **Font font**: Font object.
- **Matrix trm**: Transformation matrix.
- **string str**: Content for Text object.
- **number wmode**: `0` (default) for horizontal writing, and `1` for vertical writing.

```
text.showString(new mupdf.Font("Times-Roman"), mupdf.Matrix.identity, "Hello World")
```

#### Text.prototype.walk(walker)

- **TextWalker walker**: Function with protocol methods, see example below for details.

```
function print(...args) {
console.log(args.join(" "))
}
```

var textPrinter = {
beginSpan: function (f, m, wmode, bidi, dir, lang) {
print("beginSpan", f, m, wmode, bidi, dir, Q(lang))
},
showGlyph: function (f, m, g, u, v, b) { print("glyph", f, m, g, String.fromCodePoint(u), v, b) },
endSpan: function () { print("endSpan") }
}

var traceDevice = {
fillText: function (text, ctm, colorSpace, color, alpha) {
print("fillText", ctm, colorSpace, color, alpha)
text.walk(textPrinter)
},
clipText: function (text, ctm) {
print("clipText", ctm)
text.walk(textPrinter)
},
strokeText: function (text, stroke, ctm, colorSpace, color, alpha) {
print("strokeText", Q(stroke), ctm, colorSpace, color, alpha)
text.walk(textPrinter)
},
clipStrokeText: function (text, stroke, ctm) {
print("clipStrokeText", Q(stroke), ctm)
text.walk(textPrinter)
},
ignoreText: function (text, ctm) {
print("ignoreText", ctm)
text.walk(textPrinter)
}
}

var doc = mupdf.PDFDocument.openDocument(fs.readFileSync("test.pdf"), "application/pdf")
var page = doc.loadPage(0)
var device = new mupdf.Device(traceDevice)
page.run(device, mupdf.Matrix.identity)

### TextWalker

#### TextWalker

An object implementing this interface of optional callback functions
can be used to get calls whenever `Text.prototype.walk()` iterates over a text object.

#### beginSpan(font, trm, wmode, bidiLevel, markupDirection, language)

Called before every text span in the `Text` being walked.

- **Font font**: 
- **Matrix trm**: 
- **number wmode**: 
- **int bidiLevel**: 
- **int markupDirection**: 
- **string language**: 

#### endSpan()

Called at the end of every span in the text.

#### showGlyph(font, trm, gid, ucs, wmode, bidiLevel)

Called once per character for in a text span.

- **Font font**: 
- **Matrix trm**: 
- **number gid**: 
- **number ucs**: 
- **number wmode**: 
- **number bidiLevel**: 

### TreeArchive

#### Constructors

#### TreeArchive()

Create a new empty tree archive.

```
var treeArchive = new mupdf.TreeArchive()
```

#### Instance methods

#### TreeArchive.prototype.add(name, buffer)

Add a named buffer to a tree archive.

- **string name**: Name of archive entry to add.
- **Buffer | ArrayBuffer | Uint8Array | string buffer**: Buffer of data to store with the entry.

```
var buf = new mupdf.Buffer()
buf.writeLine("hello world!")
var archive = new mupdf.TreeArchive()
archive.add("file2.txt", buf)
print(archive.hasEntry("file2.txt"))
```

### PDF

### PDFAnnotation

PDF Annotations belong to a specific `PDFPage` and may be
created/changed/removed. Because annotation appearances may change (for several
reasons) it is possible to scan through the annotations on a page and query
them to see whether a re-render is necessary.

Additionally redaction annotations can be applied to a `PDFPage`,
destructively removing content from the page.

### Annotation Types

These are the annotation types, and which attributes they have.

Text
	An icon with a popup of text.

	Set the appearance with the Icon attribute.

	Attributes: `Rect`, `color-attribute`, `icon-attribute`.

FreeText
	Text in a rectangle on the page.

	The text font and color is defined by DefaultAppearance.

	Attributes: `border-attribute`, `rect-attribute`, `default-appearance-attribute`.

Line
	A line with optional arrow heads.

	The line width is determined by the border attribute.

	The end points are defined by the Line attribute.

	Attributes: `border-attribute`, `color-attribute`, `line-attribute`, `line-ending-styles-attribute`.

Square
	A rectangle.

	Attributes: `rect-attribute`, `rect-attribute`, `color-attribute`, `interior-color-attribute`.

Circle
	An ellipse.

	Attributes: `rect-attribute`, `border-attribute`, `color-attribute`, `interior-color-attribute`.

Polygon, PolyLine
	A polygon shape (closed and open).

	The shape is defined by the Vertices attribute.

	The line width is defined by the Border attribute.

	Attributes: `vertices-attribute`, `border-attribute`, `color-attribute`, `interior-color-attribute`, LineEndingStyles.

Highlight, Underline, Squiggly, StrikeOut
	Text markups.

	The shape is defined by the `quadpoints-attribute`.

Stamp
	A rubber stamp.

	The appearance is either a stock name, or a `custom image <stamp-image-attribute>`.

Ink
	A free-hand line.

	The shape is defined by the `inklist-attribute` attribute.

FileAttachment
	A file attachment.

	The appearance is an icon on the page.

	Set the attached file contents with the `filespec-attribute` attribute,
	and the appearance with the `icon-attribute` attribute.

Redaction
	A black box.

	Redaction annotations are used to mark areas of the page that
	can be redacted. They do NOT redact any content by themselves,
	you MUST apply them using `PDFAnnotation.prototype.applyRedaction` or
	`PDFPage.prototype.applyRedactions`.

These annotation types are special and handled with other APIs:

- `Link`
- Popup -- see `PDFAnnotation.prototype.setPopup()`
- Widget -- see `PDFWidget`

### Constructors

#### PDFAnnotation

*(not constructible with `new`)*

To get the annotations on a page use `PDFPage.prototype.getAnnotations()`.

To create a new annotation call `PDFPage.prototype.createAnnotation()`.

### Instance methods

#### PDFAnnotation.prototype.getBounds()

Returns a rectangle containing the location and dimension of the annotation.

- **Returns:** `Rect`

```
var bounds = annotation.getBounds()
```

#### PDFAnnotation.prototype.run(device, matrix)

Calls the device functions to draw the annotation.

- **Device device**: The device to make device calls to while rendering the annotation.
- **Matrix matrix**: The transformation matrix.

```
annotation.run(device, mupdf.Matrix.identity)
```

#### PDFAnnotation.prototype.toPixmap(matrix, colorspace, alpha)

Render the annotation into a `Pixmap`, using the
`transform`, `colorspace` and `alpha` parameters.

- **Matrix matrix**: Transformation matrix.
- **ColorSpace colorspace**: The desired colorspace of the returned pixmap.
- **boolean alpha**: Whether the returned pixmap has transparency or not. If the pixmap handles transparency, it starts out transparent (otherwise it is filled white), before the contents of the display list are rendered onto the pixmap.

- **Returns:** `Pixmap`

```
var pixmap = annotation.toPixmap(mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, true)
```

#### PDFAnnotation.prototype.toDisplayList()

Record the contents of the annotation into a `DisplayList`.

- **Returns:** `DisplayList`

```
var displayList = annotation.toDisplayList()
```

#### PDFAnnotation.prototype.getObject()

Get the underlying `PDFObject` for an annotation.

- **Returns:** `PDFObject`

```
var obj = annotation.getObject()
```

#### PDFAnnotation.prototype.setAppearance(appearance, state, transform, bbox, resources, contents)

Set the annotation appearance stream for the given appearance. The
desired appearance is given as a transform along with a bounding box, a
PDF dictionary of resources and a content stream.

- **string | null appearance**: Appearance stream ("N" for normal, "R" for roll-over or "D" for down). Defaults to "N".
- **string | null state**: The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
- **Matrix transform**: The transformation matrix.
- **Rect bbox**: The bounding box.,
- **PDFObject resources**: Resources object.
- **Buffer | ArrayBuffer | Uint8Array | string contents**: Contents string.

```
annotation.setAppearance(
"N",
null,
mupdf.Matrix.identity,
[0, 0, 100, 100],
resources,
contents
)
```

#### PDFAnnotation.prototype.update()

Update the appearance stream to account for changes in the annotation.

Returns true if the annotation appearance changed during the call.

- **Returns:** boolean

```
annotation.update()
```

#### PDFAnnotation.prototype.setAppearanceFromDisplayList(appearance, state, transform, list)

Set the annotation appearance stream for the given appearance. The
desired appearance is given as a transform along with a display list.

- **string appearance**: Appearance stream ("N", "R" or "D").
- **string state**: The annotation state to set the appearance for or null for the current state. Only widget annotations of pushbutton, check box, or radio button type have states, which are "Off" or "Yes". For other types of annotations pass null.
- **Matrix transform**: The transformation matrix.
- **DisplayList list**: The display list.

```
annotation.setAppearanceFromDisplayList(
"N",
null,
mupdf.Matrix.identity,
displayList
)
```

#### PDFAnnotation.prototype.getHiddenForEditing()

Get a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

- **Returns:** boolean

```
var hidden = annotation.getHiddenForEditing()
```

#### PDFAnnotation.prototype.setHiddenForEditing(hidden)

Set a special annotation hidden flag for editing. This flag prevents the annotation from being rendered.

- **boolean hidden**: 

```
annotation.setHiddenForEditing(true)
```

#### PDFAnnotation.prototype.getHot()

Check if the annotation is hot, i.e. that the pointing device's cursor
is hovering over the annotation.

- **Returns:** boolean

```
annotation.getHot()
```

#### PDFAnnotation.prototype.setHot(hot)

Set the annotation as being hot, i.e. that the pointing device's cursor
is hovering over the annotation.

- **boolean hot**: 

```
annotation.setHot(true)
```

#### PDFAnnotation.prototype.requestSynthesis()

Request that if an annotation does not have an appearance stream, flag
the annotation to have one generated. The appearance stream
will be created during future calls to
`PDFAnnotation.prototype.update()` on or
`PDFPage.prototype.update()`.

```
annotation.requestSynthesis()
```

#### PDFAnnotation.prototype.requestResynthesis()

Request that an appearance stream shall be re-generated for an
annotation next time update() is called on
`PDFAnnotation.prototype.update()` or
`PDFPage.prototype.update()`.

This is a side-effect of setting annotation attributes through
the PDFAnnotation interface, so normally this call does not
need to be done explicitly.

```
annotation.requestResynthesis()
```

#### PDFAnnotation.prototype.process(processor)

Run through the annotation appearance stream and call methods
on the supplied `PDFProcessor`.

- **PDFProcessor processor**: User defined function.

```
annotation.process(processor)
```

#### PDFAnnotation.prototype.applyRedaction(blackBoxes, imageMethod, lineArtMethod, textMethod)

Applies a single Redaction annotation.

See `PDFPage.prototype.applyRedactions` for details.

### Annotation attributes

PDF Annotations have many attributes. Some of these are common to all
annotations, and some only exist on specific annotation types.

#### Common

#### PDFAnnotation.prototype.getType()

Return the :term:`annotation type` for this annotation.

- **Returns:** string

```
var type = annotation.getType()
```

#### PDFAnnotation.prototype.getFlags()

Get the annotation flags.

See `PDFAnnotation.prototype.setFlags`.

- **Returns:** number

```
var flags = annotation.getFlags()
```

#### PDFAnnotation.prototype.setFlags(flags)

Set the annotation flags.

- **number flags**: A bit mask with the flags (see below).

| Bit | Name |
| --- | --- |
| 1 | Invisible |
| 2 | Hidden |
| 3 | Print |
| 4 | NoZoom |
| 5 | NoRotate |
| 6 | NoView |
| 7 | ReadOnly |
| 8 | Locked |
| 9 | ToggleNoView |
| 10 | LockedContents |

```
annotation.setFlags(4) // Clears all other flags and sets "NoZoom".
```

#### PDFAnnotation.prototype.getContents()

Get the annotation contents.

- **Returns:** string

```
var contents = annotation.getContents()
```

#### PDFAnnotation.prototype.setContents(text)

Set the annotation contents.

- **string text**: 

```
annotation.setContents("Hello World")
```

#### PDFAnnotation.prototype.getCreationDate()

Get the annotation creation date as a Date object.

- **Returns:** Date

```
var date = annotation.getCreationDate()
```

#### PDFAnnotation.prototype.setCreationDate(date)

Set the creation date.

- **Date date**: A Date object.

```
annotation.setCreationDate(new Date())
```

#### PDFAnnotation.prototype.getModificationDate()

Get the annotation modification date as a Date object.

- **Returns:** Date

```
var date = annotation.getModificationDate()
```

#### PDFAnnotation.prototype.setModificationDate(date)

Set the modification date.

- **Date date**: 

```
annotation.setModificationDate(new Date())
```

#### PDFAnnotation.prototype.getLanguage()

Get the annotation :term:`language code` (or get the one
inherited from the document).

- **Returns:** string | null

```
var language = annotation.getLanguage()
```

#### PDFAnnotation.prototype.setLanguage(language)

Set the annotation :term:`language code`.

- **string language**: The desired language code.

```
annotation.setLanguage("en")
```

#### Rect

For annotations that can be resized by setting its bounding box rectangle
(e.g. Square and FreeText), `PDFAnnotation.prototype.hasRect()` returns `true`.

Other annotation types, (e.g. Line, Polygon, and InkList)
change size by adding/removing vertices.
Yet other annotations (e.g. Highlight and StrikeOut)
change size by adding/removing QuadPoints.

The underlying Rect attribute on the PDF object is automatically updated as needed
for these other annotation types.

#### PDFAnnotation.prototype.hasRect()

Checks whether the annotation can be resized by setting its
bounding box.

- **Returns:** boolean

```
var hasRect = annotation.hasRect()
```

#### PDFAnnotation.prototype.getRect()

Get the annotation bounding box.

- **Returns:** `Rect`

```
var rect = annotation.getRect()
```

#### PDFAnnotation.prototype.setRect(rect)

Set the annotation bounding box.

- **Rect rect**: The new desired bounding box.

```
annotation.setRect([0, 0, 100, 100])
```

#### Rich contents

#### PDFAnnotation.prototype.hasRichContents()

Returns whether the annotation is capable of supporting rich text
contents.

- **Returns:** boolean

```
var hasRichContents = annotation.hasRichContents()
```

#### PDFAnnotation.prototype.getRichContents()

Obtain the annotation's rich-text contents, as opposed to the plain
text contents obtained by `getContents()`.

- **Returns:** string

```
var richContents = annotation.getRichContents()
```

#### PDFAnnotation.prototype.setRichContents(plainText, richText)

Set the annotation's rich-text contents, as opposed to the plain
text contents set by `setContents()`.

- **string plainText**: 
- **string richText**: 

```
annotation.setRichContents("plain text", "<b><i>Rich-Text</i></b>")
```

#### PDFAnnotation.prototype.getRichDefaults()

Get the default style used for the annotation's rich-text contents.

- **Returns:** string

```
var richDefaults = annotation.getRichDefaults()
```

#### PDFAnnotation.prototype.setRichDefaults(style)

Set the default style used for the annotation's rich-text contents.

- **string style**: 

```
annotation.setRichDefaults("font-size: 16pt")
```

#### Color

The meaning of the color attribute depends on the annotation type. For some it is the color
of the border.

#### PDFAnnotation.prototype.getColor()

Get the annotation color, represented as an array of 0, 1, 3, or 4 component values.

- **Returns:** `Color`

```
var color = annotation.getColor()
```

#### PDFAnnotation.prototype.setColor(color)

Set the annotation color, represented as an array of 0, 1, 3, or 4 component values.

- **Color color**: The new color.

- **Throws:** TypeError if number of components is not 0, 1, 3, or 4.

```
annotation.setColor([0, 1, 0])
```

#### Opacity

#### PDFAnnotation.prototype.getOpacity()

Get the annotation :term:`opacity`.

- **Returns:** number

```
var opacity = annotation.getOpacity()
```

#### PDFAnnotation.prototype.setOpacity(opacity)

Set the annotation :term:`opacity`.

- **number opacity**: The desired opacity.

```
annotation.setOpacity(0.5)
```

#### Quadding

#### PDFAnnotation.prototype.hasQuadding()

Returns whether the annotation is capable of supporting
quadding (justification).

- **Returns:** boolean

```
var hasQuadding = annotation.hasQuadding()
```

#### PDFAnnotation.prototype.getQuadding()

Get the annotation quadding (justification). Quadding value, 0
for left-justified, 1 for centered, 2 for right-justified

- **Returns:** number

```
var quadding = annotation.getQuadding()
```

#### PDFAnnotation.prototype.setQuadding(value)

Set the annotation quadding (justification). Quadding value, 0
for left-justified, 1 for centered, 2 for right-justified.

- **number value**: The desired quadding.

```
annotation.setQuadding(1)
```

#### Author

#### PDFAnnotation.prototype.hasAuthor()

Returns whether the annotation is capable of supporting an author.

- **Returns:** boolean

```
var hasAuthor = annotation.hasAuthor()
```

#### PDFAnnotation.prototype.getAuthor()

Gets the annotation author.

- **Returns:** string

```
var author = annotation.getAuthor()
```

#### PDFAnnotation.prototype.setAuthor(author)

Sets the annotation author.

- **string author**: 

```
annotation.setAuthor("Jane Doe")
```

#### Border

#### PDFAnnotation.prototype.hasBorder()

Returns whether the annotation is capable of supporting a border.

- **Returns:** boolean

```
var hasBorder = annotation.hasBorder()
```

#### PDFAnnotation.prototype.getBorderStyle()

Get the annotation :term:`border style`.

- **Returns:** string

```
var borderStyle = annotation.getBorderStyle()
```

#### PDFAnnotation.prototype.setBorderStyle(style)

Set the annotation :term:`border style`.

- **string style**: The annotation style.

```
annotation.setBorderStyle("Dashed")
```

#### PDFAnnotation.prototype.getBorderWidth()

Get the border width in points.

- **Returns:** number

```
var w = annotation.getBorderWidth()
```

#### PDFAnnotation.prototype.setBorderWidth(width)

Set the border width in points. Retains any existing border effects.

- **number width**: 

```
annotation.setBorderWidth(1.5)
```

#### PDFAnnotation.prototype.getBorderDashCount()

Returns the number of items in the border dash pattern.

- **Returns:** number

```
var dashCount = annotation.getBorderDashCount()
```

#### PDFAnnotation.prototype.getBorderDashItem(idx)

Returns the length of dash pattern item idx.

- **number idx**: 

- **Returns:** number

```
var length = annotation.getBorderDashItem(0)
```

#### PDFAnnotation.prototype.setBorderDashPattern(list)

Set the annotation border dash pattern to the given array of dash item lengths. The supplied array represents the respective line stroke and gap lengths, e.g. [1, 1] sets a small dash and small gap, [2, 1, 4, 1] would set a medium dash, a small gap, a longer dash and then another small gap.

- **Array of number dashPattern**: 

```
annotation.setBorderDashPattern([2.0, 1.0, 4.0, 1.0])
```

#### PDFAnnotation.prototype.clearBorderDash()

Clear the entire border dash pattern for an annotation.

```
annotation.clearBorderDash()
```

#### PDFAnnotation.prototype.addBorderDashItem(length)

Append an item (of the given length) to the end of the border dash pattern.

- **number length**: 

```
annotation.addBorderDashItem(10.0)
```

#### PDFAnnotation.prototype.hasBorderEffect()

Returns whether the annotation is capable of supporting a border
effect.

- **Returns:** boolean

```
var hasEffect = annotation.hasBorderEffect()
```

#### PDFAnnotation.prototype.getBorderEffect()

Get the :term:`border effect`.

- **Returns:** string

```
var effect = annotation.getBorderEffect()
```

#### PDFAnnotation.prototype.setBorderEffect(effect)

Set the :term:`border effect`.

- **string effect**: The border effect.

```
annotation.setBorderEffect("None")
```

#### PDFAnnotation.prototype.getBorderEffectIntensity()

Get the annotation border effect intensity.

- **Returns:** number

```
var intensity = annotation.getBorderEffectIntensity()
```

#### PDFAnnotation.prototype.setBorderEffectIntensity(intensity)

Set the annotation border effect intensity. Recommended values are between 0 and 2 inclusive.

- **number intensity**: Border effect intensity.

```
annotation.setBorderEffectIntensity(1.5)
```

#### Callout

Callouts are used with FreeText annotations and
allow for a graphical line to point to an area on a page.

#### PDFAnnotation.prototype.hasCallout()

Returns whether the annotation is capable of supporting a callout.

- **Returns:** boolean

#### PDFAnnotation.prototype.setCalloutLine(line)

Takes an array of 2 or 3 `points <Point>`. Supply an empty array to
remove the callout line.

- **Array of Point points**: 

#### PDFAnnotation.prototype.getCalloutLine()

Returns the array of points.

- **Returns:** Array of `Point` | null

#### PDFAnnotation.prototype.setCalloutPoint(p)

Takes a point where the callout should point to.

- **Point p**: 

#### PDFAnnotation.prototype.getCalloutPoint()

Returns the callout point.

- **Returns:** `Point`

#### PDFAnnotation.prototype.setCalloutStyle(style)

Sets the :term:`line ending style` of the callout line.

- **string style**: 

#### PDFAnnotation.prototype.getCalloutStyle()

Returns the callout style.

- **Returns:** string

#### Default Appearance

#### PDFAnnotation.prototype.hasDefaultAppearance()

Returns whether the annotation is capable of supporting a default
appearance.

- **Returns:** boolean

```
var hasRect = annotation.hasDefaultAppearance()
```

#### PDFAnnotation.prototype.getDefaultAppearance()

Get the default text appearance used for free text annotations
as an object containing the font, size, and color.

- **Returns:** `{ font: string, size: number, color: Color }`

```
var appearance = annotation.getDefaultAppearance()
console.log("DA font:", appearance.font, appearance.size)
console.log("DA color:", appearance.color)
```

#### PDFAnnotation.prototype.setDefaultAppearance(font, size, color)

Set the default text appearance used for free text annotations.

- **string font: The desired default font**: `"Helv" | "TiRo" | "Cour"` for Helvetica, Times Roman, and Courier respectively.
- **number size**: The desired default font size.
- **Color color**: The desired default font color.

```
annotation.setDefaultAppearance("Helv", 16, [0, 0, 0])
```

#### Filespec

#### PDFAnnotation.prototype.hasFilespec()

Returns whether the annotation is capable of supporting a
:term:`file specification`.

- **Returns:** boolean

```
var hasFilespec = annotation.hasFilespec()
```

#### PDFAnnotation.prototype.getFilespec()

Get the :term:`file specification` PDF object for the file attachment, or null if none set.

- **Returns:** `PDFObject` | null

```
var fs = annotation.getFilespec()
```

#### PDFAnnotation.prototype.setFilespec(fs)

Set the :term:`file specification` PDF object for the file attachment.

- **`PDFObject` fs**: 

```
annotation.setFilespec(fs)
```

#### Icon

#### PDFAnnotation.prototype.hasIcon()

Returns whether the annotation is capable of supporting an icon.

- **Returns:** boolean

```
var hasIcon = annotation.hasIcon()
```

#### PDFAnnotation.prototype.getIcon()

Get the annotation :term:`icon name`, either a standard or custom name.

- **Returns:** string

```
var icon = annotation.getIcon()
```

#### PDFAnnotation.prototype.setIcon(name)

Set the annotation :term:`icon name`.

Note that standard icon names can be used to resynthesize the annotation appearance, but custom names cannot.

- **string name: An :term**: `icon name`.

```
annotation.setIcon("Note")
```

#### Ink List

Ink annotations consist of a number of strokes, each consisting of a sequence of vertices between which a smooth line will be drawn.

#### PDFAnnotation.prototype.hasInkList()

Returns whether the annotation is capable of supporting an ink list.

- **Returns:** boolean

```
var hasInkList = annotation.hasInkList()
```

#### PDFAnnotation.prototype.getInkList()

Get the annotation ink list, represented as an array of strokes.
Each stroke consists of an array of points.

- **Returns:** Array of Array of `Point`

```
var inkList = annotation.getInkList()
```

#### PDFAnnotation.prototype.setInkList(inkList)

Set the annotation ink list, represented as an array of strokes.
Each stroke consists of an array of points.

- **Array of Array of Point inkList**: 

```
// this draws a box with a cross in three strokes:
annotation.setInkList([
[
[0, 0], [10, 0], [10, 10], [0, 10], [0, 0]
],
[
[10, 0], [0, 10]
],
[
[0, 0], [10, 10]
]
])
```

#### PDFAnnotation.prototype.clearInkList()

Clear the list of ink strokes for the annotation.

```
annotation.clearInkList()
```

#### PDFAnnotation.prototype.addInkListStroke()

Add a new empty stroke to the ink annotation.

```
annotation.addInkListStroke()
```

#### PDFAnnotation.prototype.addInkListStrokeVertex(v)

Append a vertex to end of the last stroke in the ink annotation.

- **Point v**: 

```
annotation.addInkListStrokeVertex([0, 0])
```

#### Interior Color

#### PDFAnnotation.prototype.hasInteriorColor()

Returns whether the annotation is capable of supporting an interior
color.

- **Returns:** boolean

```
var hasInteriorColor = annotation.hasInteriorColor()
```

#### PDFAnnotation.prototype.getInteriorColor()

Get the annotation interior color, represented as an array of 0, 1, 3, or 4 component values.

- **Returns:** `Color`

```
var interiorColor = annotation.getInteriorColor()
```

#### PDFAnnotation.prototype.setInteriorColor(color)

Sets the annotation interior color.

- **Color color**: The new desired interior color.

- **Throws:** TypeError if number of components is not 0, 1, 3, or 4.

```
annotation.setInteriorColor([0, 1, 1])
```

#### Line

#### PDFAnnotation.prototype.hasLine()

Returns whether the annotation is capable of supporting a line.

- **Returns:** boolean

```
var hasLine = annotation.hasLine()
```

#### PDFAnnotation.prototype.getLine()

Get line end points, represented by an array of two points, each represented as an [x, y] array.

- **Returns:** Array of `Point`

```
var line = annotation.getLine()
```

#### PDFAnnotation.prototype.setLine(a, b)

Set the two line end points, each represented as an [x, y] array.

- **Point a**: The new point a.
- **Point b**: The new point b.

```
annotation.setLine([100, 100], [150, 175])
```

#### Line Ending Styles

#### PDFAnnotation.prototype.hasLineEndingStyles()

Returns whether the annotation is capable of supporting
:term:`line ending style`.

- **Returns:** boolean

```
var hasLineEndingStyles = annotation.hasLineEndingStyles()
```

#### PDFAnnotation.prototype.getLineEndingStyles()

Get the start and end :term:`line ending style` values for each end of the line annotation.

- **Returns:** `{ start: string, end: string }` Returns an object with the key/value pairs

```
var lineEndingStyles = annotation.getLineEndingStyles()
```

#### PDFAnnotation.prototype.setLineEndingStyles(start, end)

Sets the :term:`line ending style` values for each end of the line annotation.

- **string start**: 
- **string end**: 

```
annotation.setLineEndingStyles("Square", "OpenArrow")
```

#### Line Leaders

In a PDF line leaders refer to two lines at the ends of the line annotation,
oriented perpendicular to the line itself. These are common in technical
drawings when illustrating distances.

#### PDFAnnotation.prototype.setLineLeader(v)

Sets the line leader length.

- **number v**: 
The length of leader lines that extend from each endpoint of
the line perpendicular to the line itself. A positive value
means that the leader lines appear in the direction that is
clockwise when traversing the line from its starting point to
its ending point a negative value indicates the opposite
direction.

Setting a value of 0 effectively removes the line leader.

#### PDFAnnotation.prototype.getLineLeader()

Gets the line leader length.

- **Returns:** number

#### PDFAnnotation.prototype.setLineLeaderExtension(v)

Sets the line leader extension.

- **number v**: 
A non-negative number representing the length of leader line
extensions that extend from the line proper 180 degrees from
the leader lines.

Setting a value of 0 effectively removes the line leader extension.

#### PDFAnnotation.prototype.getLineLeaderExtension()

Gets the line leader extension.

- **Returns:** number

#### PDFAnnotation.prototype.setLineLeaderOffset(v)

Sets the line leader offset.

- **number v**: 
A non-negative number representing the length of the leader
line offset, which is the amount of empty space between the
endpoints of the annotation and the beginning of the leader
lines.

Setting a value of 0 effectively removes the line leader offset.

#### PDFAnnotation.prototype.getLineLeaderOffset()

Gets the line leader offset.

- **Returns:** number

#### PDFAnnotation.prototype.setLineCaption(on)

Sets whether line caption is enabled or not.

When line captions are enabled then calling the
`PDFAnnotation.prototype.setContents` on the line annotation will
render the contents onto the line as the caption text.

- **boolean on**: 

#### PDFAnnotation.prototype.getLineCaption()

Returns whether the line caption is enabled or not.

- **Returns:** boolean

#### PDFAnnotation.prototype.setLineCaptionOffset(point)

Sets the line caption offset.

The x value of the offset point is the horizontal offset along the
annotation line from its midpoint, with a positive value indicating
offset to the right and a negative value indicating offset to the
left. The y value of the offset point is the vertical offset
perpendicular to the annotation line, with a positive value
indicating a shift up and a negative value indicating a shift down.

Setting a point of [0, 0] removes the caption offset.

- **Point point**: A point specifying the offset of the caption text from its normal position.

#### PDFAnnotation.prototype.getLineCaptionOffset()

Returns the line caption offset as a point, [x, y].

- **Returns:** `Point`

#### Open

Open refers to whether the annotation is display in an open state when the
page is loaded. A Text Note annotation is considered open if the user has
clicked on it to view its contents.

#### PDFAnnotation.prototype.hasOpen()

Returns whether the annotation is capable of supporting annotation
open state.

- **Returns:** boolean

```
var hasOpen = annotation.hasOpen()
```

#### PDFAnnotation.prototype.getIsOpen()

Get annotation open state.

- **Returns:** boolean

```
var isOpen = annotation.getIsOpen()
```

#### PDFAnnotation.prototype.setIsOpen(state)

Set annotation open state.

- **boolean state**: 

```
annotation.setIsOpen(true)
```

#### Popup

#### PDFAnnotation.prototype.hasPopup()

Returns whether the annotation is capable of supporting a popup.

- **Returns:** boolean

```
var hasPopup = annotation.hasPopup()
```

#### PDFAnnotation.prototype.getPopup()

Get annotation popup rectangle.

- **Returns:** `Rect`

```
var popupRect = annotation.getPopup()
```

#### PDFAnnotation.prototype.setPopup(rect)

Set annotation popup rectangle.

- **Rect rect**: The desired area where the popup should appear.

```
annotation.setPopup([0, 0, 100, 100])
```

#### QuadPoints

Text markup and redaction annotations consist of a set of
quadadrilaterals, or :term:`QuadPoints <QuadPoint>`.
These are used in e.g. Highlight
annotations to mark up several disjoint spans of text.

In Javascript QuadPoints are represented with `Quad` objects.

#### PDFAnnotation.prototype.hasQuadPoints()

Returns whether the annotation is capable of supporting quadpoints.

- **Returns:** boolean

```
var hasQuadPoints = annotation.hasQuadPoints()
```

#### PDFAnnotation.prototype.getQuadPoints()

Get the annotation's quadpoints, describing the areas affected by
text markup annotations and link annotations.

- **Returns:** Array of `Quad`

```
var quadPoints = annotation.getQuadPoints()
```

#### PDFAnnotation.prototype.setQuadPoints(quadList)

Set the annotation quadpoints describing the areas affected by
text markup annotations and link annotations.

- **Array of Quad quadList**: The quadpoints to set.

```
// two quads, the first one wider than the second one
annotation.setQuadPoints([
[ 100, 100, 200, 100, 200, 150, 100, 150 ],
[ 125, 150, 175, 150, 175, 200, 125, 200 ]
])
```

#### PDFAnnotation.prototype.clearQuadPoints()

Clear the list of quadpoints for the annotation.

```
annotation.clearQuadPoints()
```

#### PDFAnnotation.prototype.addQuadPoint(quad)

Append a single quadpoint to the annotation.

- **Quad quad**: The quadpoint to add.

```
annotation.addQuadPoint([1, 2, 3, 4, 5, 6, 7, 8])
```

#### Vertices

Polygon and polyline annotations consist of a sequence of vertices with a straight line between them. Those can be controlled by:

#### PDFAnnotation.prototype.hasVertices()

Returns whether the annotation is capable of supporting vertices.

- **Returns:** boolean

```
var hasVertices = annotation.hasVertices()
```

#### PDFAnnotation.prototype.getVertices()

Get the annotation vertices, represented as an array of points.

- **Returns:** Array of `Point`

```
var vertices = annotation.getVertices()
```

#### PDFAnnotation.prototype.setVertices(vertices)

Set the annotation vertices, represented as an array of points.

- **Array of Point vertices**: 

```
annotation.setVertices([
[0, 0],
[10, 10],
[20, 20]
])
```

#### PDFAnnotation.prototype.clearVertices()

Clear the list of vertices for the annotation.

```
annotation.clearVertices()
```

#### PDFAnnotation.prototype.addVertex(vertex)

Append a single vertex point to the annotation.

- **Point vertex**: 

```
annotation.addVertex([0, 0])
```

#### Stamp image

#### PDFAnnotation.prototype.getStampImageObject()

If the annotation is a stamp annotation and it consists of an
image, return the `PDFObject` representing that image.

- **Returns:** `PDFObject` | null

```
var pdfobj = annotation.getStampImageObject()
```

#### PDFAnnotation.prototype.setStampImageObject(imgobj)

Create an appearance stream containing the image passed as
argument and set that as the normal appearance of the
annotation.

- **PDFObject imgobj**: PDFObject corresponding to the desired image.

```
annotation.setStampImageObject(imgobj)
```

#### PDFAnnotation.prototype.setStampImage(img)

Add the image passed as argument to the document as a PDF
object, and pass a reference to that object to when setting the
normal appearance of the stamp annotation.

- **Image img**: The image to become the stamp annotations appearance.

```
annotation.setStampImage(img)
```

#### Intent

#### PDFAnnotation.prototype.hasIntent()

Returns whether the annotation is capable of supporting an intent.

- **Returns:** boolean

```
var hasIntent = annotation.hasIntent()
```

#### PDFAnnotation.prototype.getIntent()

Get the annotation intent, one of the values below:

* "FreeTextCallout"
* "FreeTextTypeWriter"
* "LineArrow"
* "LineDimension"
* "PolyLineDimension"
* "PolygonCloud"
* "PolygonDimension"
* "StampImage"
* "StampSnapshot"

- **Returns:** string

```
var intent = annotation.getIntent()
```

#### PDFAnnotation.prototype.setIntent(intent)

Set the annotation intent.

- **string intent**: Intent value, see `getIntent()` for permissible values.

```
annotation.setIntent("LineArrow")
```

#### Events

PDF annotations can have different appearances depending on whether
the pointing device's cursor is hovering over an annotation, or if the
pointing device's button is pressed.

PDF widgets, which is a type of annotation, may also have associated
Javascript functions that are executed when certain events occur.

Therefore it is important to tell an PDFAnnotation when the pointing
device's cursor enters/exits an annotation, when it's button is
clicked, or when an annotation gains/loses input focus.

#### PDFAnnotation.prototype.eventEnter()

Trigger appearance changes and event handlers for
when the pointing device's cursor enters an
annotation's active area.

```
annot.eventEnter()
```

#### PDFAnnotation.prototype.eventExit()

Trigger appearance changes and event handlers for
when the pointing device's cursor exits an
annotation's active area.

```
annot.eventExit()
```

#### PDFAnnotation.prototype.eventDown()

Trigger appearance changes and event handlers for
when the pointing device's button is depressed within
an annotation's active area.

```
widget.eventDown()
```

#### PDFAnnotation.prototype.eventUp()

Trigger appearance changes and event handlers for
when the pointing device's button is released within
an annotation's active area.

```
widget.eventUp()
```

#### PDFAnnotation.prototype.eventFocus()

Trigger event handlers for when an annotation gains
input focus.

```
widget.eventFocus()
```

#### PDFAnnotation.prototype.eventBlur()

Trigger event handlers for when an annotation loses
input focus.

```
widget.eventBlur()
```

### PDFDocument

The PDFDocument is a specialized subclass of `Document` which has
additional methods that are only available for PDF files.

#### PDF Objects

A PDF document contains objects: dictionaries, arrays, names, strings, numbers,
booleans, and indirect references.
Some dictionaries also have attached data. These are called streams,
and may be compressed.

At the root of the PDF document is the trailer object; which contains pointers to the meta
data dictionary and the catalog object, which in turn contains references to the pages and
forms and everything else.

Pointers in PDF are called indirect references, and are of the form
32 0 R (where 32 is the object number, 0 is the generation, and R is
magic syntax). All functions in MuPDF dereference indirect
references automatically.

	PDFObjects are always bound to the document that created them. Do
	**NOT** mix and match objects from one document with another
	document!

#### Constructors

#### PDFDocument()

Create a brand new PDF document instance that begins empty with no pages.

To open an existing PDF document, use `Document.openDocument()`.

#### Static methods

#### PDFDocument.formatURIFromPathAndDest(path, destination)

Format a link URI given a
`system independent path <https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G8.1640868>`
to a remote document and a destination object or a destination
string suitable for `Page.prototype.createLink()`.

- **string path**: An absolute or relative path to a remote document file.
- **Link | string destination**: Link or string referring to a destination using either a destination object or a destination name in the remote document.

#### PDFDocument.appendDestToURI(uri, destination)

Append a fragment representing a document destination to a an
existing URI that points to a remote document. The resulting
string is suitable for `Page.prototype.createLink()`.

- **string uri**: An URI to a remote document file.
- **Link | string | number destination**: A Link or string referring to a destination using either a destination object or a destination name in the remote document, or a page number.

#### Instance methods

#### PDFDocument.prototype.needsPassword()

Returns true if a password is required to open a password protected PDF.

- **Returns:** boolean

```
var needsPassword = document.needsPassword()
```

#### PDFDocument.prototype.authenticatePassword(password)

Returns a bitfield value against the password authentication result.

The values returned by this interface are interpreted like this:

| Bit | Description |
| --- | --- |
| 0 | Failed |
| 1 | No password needed |
| 2 | User password is okay |
| 4 | Owner password is okay |

- **string password**: The password to attempt authentication with.

- **Returns:** number

```
var auth = document.authenticatePassword("abracadabra")
```

#### PDFDocument.prototype.hasPermission(permission)

Returns true if the document has permission for the supplied permission parameter.

These are the recognized permission strings:

| String | The Document may... |
| --- | --- |
| print | ... be printed. |
| edit | ... be edited. |
| copy | ... be copied. |
| annotate | ... have annotations added/removed. |
| form | ... have form field contents edited. |
| accessibility | ... be copied for accessibility. |
| assemble | ... have its pages rearranged. |
| print-hq | ... be printed in high quality be printed in high quality. |

- **string permission**: The permission to seek for, e.g. "edit".

- **Returns:** boolean

```
var canEdit = document.hasPermission("edit")
```

#### PDFDocument.prototype.getVersion()

Returns the PDF document version as an integer multiplied by
10, so e.g. a PDF-1.4 document would return 14.

- **Returns:** number

```
var version = pdfDocument.getVersion()
```

#### PDFDocument.prototype.setLanguage(lang)

Set the document's :term:`language code`.

- **string lang**: 

```
pdfDocument.setLanguage("en")
```

#### PDFDocument.prototype.getLanguage()

Get the document's :term:`language code`.

- **Returns:** string | null

```
var lang = pdfDocument.getLanguage()
```

#### PDFDocument.prototype.wasPureXFA()

Returns whether the document was an XFA form without AcroForm
fields.

- **Returns:** boolean

```
var wasPureXFA = pdfDocument.wasPureXFA()
```

#### PDFDocument.prototype.wasRepaired()

Returns whether the document was repaired when opened.

- **Returns:** boolean

```
var wasRepaired = pdfDocument.wasRepaired()
```

#### PDFDocument.prototype.loadNameTree(treeName)

Return an object whose properties and their values come from
corresponding names/values from the given name tree.

- **Returns:** Object

```
var dests = pdfDocument.loadNameTree("Dests")
for (var p in dests) {
console.log("Destination: " + p)
}
```

#### Objects

#### PDFDocument.prototype.newNull()

Create a new null object.

- **Returns:** `PDFObject`

```
var obj = doc.newNull()
```

#### PDFDocument.prototype.newBoolean(v)

Create a new boolean object.

- **boolean v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newBoolean(true)
```

#### PDFDocument.prototype.newInteger(v)

Create a new integer object.

- **number v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newInteger(1)
```

#### PDFDocument.prototype.newReal(v)

Create a new real number object.

- **number v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newReal(7.3)
```

#### PDFDocument.prototype.newString(v)

Create a new string object.

- **string v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newString("hello")
```

#### PDFDocument.prototype.newByteString(v)

Create a new byte string object.

- **Uint8Array | Array of number v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newByteString([21, 31])
```

#### PDFDocument.prototype.newName(v)

Create a new name object.

- **string v**: 

- **Returns:** `PDFObject`

```
var obj = doc.newName("hello")
```

#### PDFDocument.prototype.newIndirect(objectNumber, generation)

Create a new indirect object.

- **number objectNumber**: 
- **number generation**: 

- **Returns:** `PDFObject`

```
var obj = doc.newIndirect(42, 0)
```

#### PDFDocument.prototype.newArray()

Create a new array object.

- **Returns:** `PDFObject`

```
var obj = doc.newArray()
```

#### PDFDocument.prototype.newDictionary()

Create a new dictionary object.

- **Returns:** `PDFObject`

```
var obj = doc.newDictionary()
```

#### Indirect objects

#### PDFDocument.prototype.getTrailer()

The trailer dictionary. This contains indirect references to the "Root"
and "Info" dictionaries.

- **Returns:** `PDFObject`

```
var dict = doc.getTrailer()
```

#### PDFDocument.prototype.countObjects()

Return the number of objects in the PDF.

- **Returns:** number

```
var num = doc.countObjects()
```

#### PDFDocument.prototype.createObject()

Allocate a new numbered object in the PDF, and return an indirect
reference to it. The object itself is uninitialized.

- **Returns:** `PDFObject`

```
var obj = doc.createObject()
```

#### PDFDocument.prototype.deleteObject(num)

Delete the object referred to by an indirect reference or its object number.

- **PDFObject | number num**: Delete the referenced object number.

```
doc.deleteObject(obj)
```

#### PDFDocument.prototype.addObject(obj)

Add obj to the PDF as a numbered object, and return an indirect reference to it.

- **PDFObject obj**: Object to add.

- **Returns:** `PDFObject`

```
var ref = doc.addObject(obj)
```

#### PDFDocument.prototype.addStream(buf, obj)

Create a stream object with the contents of buffer, add it to the PDF, and return an indirect reference to it. If object is defined, it will be used as the stream object dictionary.

- **Buffer | ArrayBuffer | Uint8Array | string buf**: Buffer whose data to put into stream.
- **PDFObject obj**: The object to add the stream to.

- **Returns:** `PDFObject`

```
var stream = doc.addStream(buffer, object)
```

#### PDFDocument.prototype.addRawStream(buf, obj)

Create a stream object with the contents of buffer, add it to the PDF, and return an indirect reference to it. If object is defined, it will be used as the stream object dictionary. The buffer must contain already compressed data that matches "Filter" and "DecodeParms" set in the stream object dictionary.

- **Buffer | ArrayBuffer | Uint8Array | string buf**: Buffer whose data to put into stream.
- **PDFObject obj**: The object to add the stream to.

- **Returns:** `PDFObject`

```
var stream = doc.addRawStream(buffer, object)
```

#### Page Tree

#### PDFDocument.prototype.setPageTreeCache(enabled)

Enable or disable the page tree cache that is used to speed up page object lookups.
The page tree cache is used unless explicitly disabled with this function.

Disabling the page tree cache reduces the number objects that we need to read from the file when loading a single page.
However it will make page lookups slower overall!

- **boolean enabled**: 

#### PDFDocument.protoype.findPage(number)

Return the `PDFObject` for a page number.

- **number number**: The page number, the first page is number zero.

- **Throws:** Error on out of range page numbers.

- **Returns:** `PDFObject`

```
var obj = pdfDocument.findPage(0)
```

#### PDFDocument.prototype.findPageNumber(page)

Find a given `PDFPage` and return its page number.
If the page can not be found, returns -1.

- **PDFPage page**: 

- **Returns:** number

```
var pageNumber = pdfDocument.findPageNumber(page)
```

#### PDFDocument.prototype.lookupDest(obj)

Find the destination corresponding to a specific named
destination given as a name or byte string in the form of a
`PDFObject`.

Returns null if the named destination does not exist.

- **PDFObject obj**: 

- **Returns:** `PDFObject` | null

```
var destination = pdfDocument.lookupDest(nameobj)
```

#### PDFDocument.prototype.rearrangePages(pages)

Rearrange (re-order and/or delete) pages in the PDFDocument.

The pages in the document will be rearranged according to the
input list. Any pages not listed will be removed, and pages may
be duplicated by listing them multiple times.

The PDF objects describing removed pages will remain in the
file and take up space (and can be recovered by forensic tools)
unless you save with the "garbage" option set, see `PDFDocument.prototype.save()`.

- **Array of number pages**: An array of page numbers, each page number is 0-based.

```
var document = new Document.openDocument("my_pdf.pdf")
pdfDocument.rearrangePages([3,2])
pdfDocument.save("fewer_pages.pdf", "garbage")
```

#### PDFDocument.prototype.insertPage(at, page)

Insert the page's `PDFObject` into the page tree at the page
number specified by `at` (numbered from 0). If `at` is -1,
the page is inserted at the end of the document.

- **number at**: The index to insert at.
- **PDFObject page**: The PDFObject representing the page to insert.

```
pdfDocument.insertPage(-1, page)
```

#### PDFDocument.prototype.deletePage(index)

Delete the page at the given index.

- **number index**: The page number, the first page is number zero.

```
pdfDocument.deletePage(0)
```

#### PDFDocument.prototype.addPage(mediabox, rotate, resources, contents)

Create a new `PDFPage` object. Note: this function does NOT add
it to the page tree, use `PDFDocument.prototype.insertPage()`
to do that.

Creation of page contents is described in detail in the PDF
specification's section on `Content Streams
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G8.1913072>`_.

- **Rect mediabox**: Describes the dimensions of the page.
- **number rotate**: Rotation value.
- **PDFObject resources**: Resources dictionary object.
- **Buffer | ArrayBuffer | Uint8Array | string contents**: Contents string. This represents the page content stream.

- **Returns:** `PDFObject`

```
var helvetica = pdfDocument.addSimpleFont(new mupdf.Font("Helvetica"), "Latin")
var fonts = pdfDocument.newDictionary()
fonts.put("F1", helvetica)
var resources = pdfDocument.addObject(pdfDocument.newDictionary())
resources.put("Font", fonts)
var pageObject = pdfDocument.addPage(
[0,0,300,350],
0,
resources,
"BT /F1 12 Tf 100 100 Td (Hello, world!) Tj ET"
)
pdfDocument.insertPage(-1, pageObject)
```

#### Resources

#### PDFDocument.prototype.addSimpleFont(font, encoding)

Create a `PDFObject` from the `Font` object as a simple font.

- **Font font**: 
- **"Latin" | "Greek" | "Cyrillic" encoding**: Which 8-bit encoding to use. Defaults to "Latin".

See `Font.SIMPLE_ENCODING_LATIN`, etc.

- **Returns:** `PDFObject`

```
var obj = pdfDocument.addSimpleFont(new mupdf.Font("Times-Roman"), "Latin")
```

#### PDFDocument.prototype.addCJKFont(font, language, wmode, style)

Create a `PDFObject` from the `Font` object as a UTF-16 encoded
CID font for the given language ("zh-Hant", "zh-Hans", "ko", or
"ja"), writing mode ("H" or "V"), and style ("serif" or
"sans-serif").

- **Font font**: 
- **string language**: 
- **number wmode**: `0` for horizontal writing, and `1` for vertical writing.
- **string style**: 

- **Returns:** `PDFObject`

```
var obj = pdfDocument.addCJKFont(new mupdf.Font("ja"), "ja", 0, "serif")
```

#### PDFDocument.prototype.addFont(font)

Create a `PDFObject` from the `Font` object as an Identity-H
encoded CID font.

- **Font font**: 

- **Returns:** `PDFObject`

```
var obj = pdfDocument.addFont(new mupdf.Font("Times-Roman"))
```

#### PDFDocument.prototype.addImage(image)

Create a `PDFObject` from the `Image` object.

- **Image image**: 

- **Returns:** `PDFObject`

```
var obj = pdfDocument.addImage(new mupdf.Image(pixmap))
```

#### PDFDocument.prototype.loadImage(obj)

Load an `Image` from a `PDFObject` (typically an indirect
reference to an image resource).

- **PDFObject obj**: 

- **Returns:** `Image`

```
var image = pdfDocument.loadImage(obj)
```

#### Embedded/Associated files

#### PDFDocument.protoype.addEmbeddedFile(filename, mimetype, contents, creationDate, modificationDate, addChecksum)

Embedded a file into the document. If a checksum is added then
the file contents can be verified later. An indirect reference
to a :term:`file specification` object is returned.

The returned :term:`file specification` object can later be e.g.
connected to an annotation using
`PDFAnnotation.prototype.setFilespec()`.

- **string filename**: 
- **string mimetype: The :term**: `MIME-type`.
- **Buffer | ArrayBuffer | Uint8Array | string contents**: 
- **Date creationDate**: 
- **Date modificationDate**: 
- **boolean addChecksum**: Defaults to false.

- **Returns:** `PDFObject`

```
var fileSpecObject = pdfDocument.addEmbeddedFile(
"my_file.jpg",
 "image/jpeg",
 buffer,
 new Date(),
 new Date(),
 false
)
```

#### PDFDocument.prototype.getEmbeddedFiles()

Returns a record of any embedded files on the this PDFDocument.

- **Returns:** Record<string, PDFObject>

#### PDFDocument.prototype.deleteEmbeddedFile(filename)

Delete an embedded file by filename.

- **string filename**: Name of embedded file to delete.

```
doc.deleteEmbeddedFile("test.txt")
```

#### PDFDocument.prototype.insertEmbeddedFile(filename, fileSpecObject)

Insert the given file specification as an embedded file using
the given filename.

- **string filename**: Name of the file to insert.
- **PDFObject fileSpecObject: :term**: `File specification`.

```
pdfDocument.insertEmbeddedFile("test.txt", fileSpecObject)
pdfDocument.deleteEmbeddedFile("test.txt")
```

#### PDFDocument.prototype.getFilespecParams(fileSpecObject)

Get the file specification parameters from the :term:`file specification`.

- **PDFObject fileSpecObject: :term**: `file specification` object.

- **Returns:** `PDFFilespecParams`

```
var obj = pdfDocument.getFilespecParams(fileSpecObject)
```

#### PDFDocument.prototype.getEmbeddedFileContents(fileSpecObject)

Returns a `Buffer` with the contents of the embedded file
referenced by `fileSpecObject`.

- **Object fileSpecObject: :term**: `file specification`

- **Returns:** `Buffer` | null

```
var buffer = pdfDocument.getEmbeddedFileContents(fileSpecObject)
```

#### PDFDocument.prototype.verifyEmbeddedFileChecksum(fileSpecObject)

Verify the MD5 checksum of the embedded file contents.

- **Object fileSpecObject: :term**: `file specification`.

- **Returns:** boolean

```
var fileChecksumValid = pdfDocument.verifyEmbeddedFileChecksum(fileSpecObject)
```

#### PDFDocument.prototype.isFilespec(object)

Check if the given `object` is a :term:`file specification`.

- **PDFObject object**: 

- **Returns:** boolean

```
var isFilespec = pdfDocument.isFilespec(obj)
```

#### PDFDocument.prototype.isEmbeddedFile(object)

Check if the given `object` is a :term:`file specification` representing a file
embedded into the PDF document.

- **PDFObject object**: 

- **Returns:** boolean

```
var isFilespecObject = pdfDocument.isEmbeddedFile(obj)
```

#### PDFDocument.prototype.countAssociatedFiles()

Return the number of :term:`associated files <associated file>`
associated with this document. Note that this is the number of
files associated at the document level, not necessarily the
total number of files associated with elements throughout the
entire document.

- **Returns:** number

```
var count = pdfDocument.countAssociatedFiles()
```

#### PDFDocument.prototype.associatedFile(n)

Return the :term:`file specification` object that represents the nth
:term:`associated file` for this document.

`n` should be in the range `0 <= n < countAssociatedFiles()`.

Returns null if no associated file exists or index is out of range.

- **Returns:** `PDFObject` | null

```
var obj = pdfDocument.associatedFile(0)
```

#### Grafting

#### PDFDocument.prototype.newGraftMap()

Create a graft map on the destination document, so that objects that have already been copied can be found again. Each graft map should only be used with one source document. Make sure to create a new graft map for each source document used.

- **Returns:** `PDFGraftMap`

```
var graftMap = doc.newGraftMap()
```

#### PDFDocument.prototype.graftObject(obj)

Deep copy an object into the destination document. This function will
not remember previously copied objects. If you are copying several
objects from the same source document using multiple calls, you
should use a graft map instead, see
`PDFDocument.prototype.newGraftMap()`.

- **PDFObject obj**: The object to graft.

- **Returns:** `PDFObject`

```
var copiedObj = doc.graftObject(obj)
```

#### PDFDocument.prototype.graftPage(to, srcDoc, srcPage)

Graft a page and its resources at the given page number from the source document to the requested page number in the document.

- **number to**: The page number to insert the page before. Page numbers start at 0 and -1 means at the end of the document.
- **PDFDocument srcDoc**: Source document.
- **number srcPage**: Source page number.

This would copy the first page of the source document (0) to the last page (-1) of the current PDF document.

```
doc.graftPage(-1, srcDoc, 0)
```

#### Journalling

#### PDFDocument.prototype.enableJournal()

Activate journalling for the document.

```
pdfDocument.enableJournal()
```

#### PDFDocument.prototype.getJournal()

Returns a Javascript object with a property indicating the
current position, and the names of each entry in the
undo/redo journal history:

`{ position: number, steps: Array of string }`

- **Returns:** Object

```
var journal = pdfDocument.getJournal()
```

#### PDFDocument.prototype.beginOperation(op)

Begin a journal operation. Each call to begin an operation
should be paired with a call to either
`PDFDocument.prototype.endOperation()` if the operation was
successful, or to `PDFDocument.prototype.abandonOperation()` if
it failed.

- **string op**: The name of the operation.

```
pdfDocument.beginOperation("Change annotation color")
// Change the annotation
pdfDocument.endOperation()
```

#### PDFDocument.prototype.beginImplicitOperation()

Begin an implicit journal operation. Implicit operations are
operations that happen due to other operations, and that should
not be subdivided into separate undo steps. E.g. editing several
attributes of a `PDFAnnotation` might be desirable to do in a
single undo step. See `PDFDocument.prototype.beginOperation()`
for the requirements about paired calls.

```
pdfDocument.beginOperation("Complex operation")
pdfDocument.beginImplicitOperation()
pdfDocument.endOperation()
pdfDocument.beginImplicitOperation()
pdfDocument.endOperation()
pdfDocument.endOperation()
```

#### PDFDocument.prototype.endOperation()

End a previously started normal or implicit operation. After
this it can be `undone <PDFDocument.prototype.undo()>` and
`redone <PDFDocument.prototype.redo()>`.

```
pdfDocument.endOperation()
```

#### PDFDocument.prototype.abandonOperation()

Abandon a normal or implicit operation. Reverts to the state
before that operation began. This is normally called if an
operation failed for some reason.

```
pdfDocument.abandonOperation()
```

#### PDFDocument.prototype.canUndo()

Returns whether undo is possible in this state.

- **Returns:** boolean

```
var canUndo = pdfDocument.canUndo()
```

#### PDFDocument.prototype.canRedo()

Returns whether redo is possible in this state.

- **Returns:** boolean

```
var canRedo = pdfDocument.canRedo()
```

#### PDFDocument.prototype.undo()

Move backwards in the undo history. Changes to the document
after this call will throw away all subsequent undo history.

```
pdfDocument.undo()
```

#### PDFDocument.prototype.redo()

Move forwards in the undo history.

```
pdfDocument.redo()
```

#### PDFDocument.prototype.saveJournal(filename)

Save the undo/redo journal to a file.

- **string filename**: File to save the journal to.

```
pdfDocument.saveJournal("test.journal")
```

#### Layers

#### PDFDocument.prototype.countLayerConfigs()

Return the number of optional content layer configurations in this document.

- **Returns:** number

```
var configs = pdfDocument.countLayerConfigs()
```

#### PDFDocument.prototype.getLayerConfigName(n)

Return the name of configuration number `n`, where `n` is
`0 <= n < countLayerConfigs()`.

- **Returns:** string

```
var name = pdfDocument.getLayerConfigName(0)
```

#### PDFDocument.prototype.getLayerConfigInfo(n)

Return the creator of configuration number `n`, where `n` is
`0 <= n < countLayerConfigs()`.

- **Returns:** string

```
var creator = pdfDocument.getLayerConfigCreator(0)
```

#### PDFDocument.prototype.selectLayerConfig(n)

Select layer configuration number `n`, where `n` is
`0 <= n < countLayerConfigs()`.

```
var info = pdfDocument.selectLayerConfig(1)
```

#### PDFDocument.prototype.countLayerConfigUIs()

Return the number of optional content layer UI elements in this document
given the selected optional content layer configuration.

- **Returns:** number

#### PDFDocument.prototype.getLayerConfigUIInfo(n)

Return the information about optional content layer UI element number `n`,
where `n` is `0 <= n < countLayerConfigUIs()`.

- **Returns:** `{ type: number, depth: number, selected: boolean, locked: boolean, text: string }`

#### PDFDocument.prototype.countLayers()

Return the number of optional content layers in this document.

- **Returns:** number

```
var layers = pdfDocument.countLayers()
```

#### PDFDocument.prototype.isLayerVisible(n)

Return whether layer `n` is visible, where `n` should be in
the interval `0 <= n < countLayers()`.

- **number n**: What layer to check visibility of.

- **Returns:** boolean

```
var visible = pdfDocument.isLayerVisible(1)
```

#### PDFDocument.prototype.setLayerVisible(n, visible)

Set layer `n` to be visible or invisible, where `n` is
in the interval `0 <= n < countLayers()`.

Pages affected by a visibility change, need to be processed
again for the layers to be visible/invisible.

- **number n**: What layer to change visibility for.
- **boolean visible**: Whether the layer should be visible.

- **Returns:** number

```
pdfDocument.setLayerVisible(1, true)
```

#### getLayerName(n)

Return the name of layer number `n`, where `n` is
`0 <= n < countLayers()`.

- **Returns:** string

```
var name = pdfDocument.getLayerName(0)
```

#### Page Labels

#### PDFDocument.prototype.setPageLabels(index, style, prefix, start)

Sets the page label numbering for the page and all pages following it, until the next page with an attached label.

- **number index**: The start page index to start labeling from.
- **string style: Can be one of the following strings**: "" (none), "D" (decimal), "R" (roman numerals upper-case), "r" (roman numerals lower-case), "A" (alpha upper-case), or "a" (alpha lower-case).
- **string prefix**: Define a prefix for the labels.
- **number start**: The ordinal with which to start numbering.

```
doc.setPageLabels(0, "D", "Prefix", 1)
```

#### PDFDocument.prototype.deletePageLabels(index)

Removes any associated page label from the page.

- **number index**: 

```
doc.deletePageLabels(0)
```

#### Saving

#### PDFDocument.prototype.check()

Check the file for syntax errors, and run a repair pass if any are
found. This is a costly operation, but may be necessary to prevent any
changes to a document from being potentially lost.

If a syntax error is discovered after a file has been edited, those
edits may be lost during the file repair pass.
In practice this rarely happens because syntax errors that trigger a
repair usually happen either when first opening the document or when
loading a page; but you can never be certain!

#### PDFDocument.prototype.canBeSavedIncrementally()

Returns whether the document can be saved incrementally, e.g.
repaired documents or applying redactions prevents incremental
saves.

- **Returns:** boolean

```
var canBeSavedIncrementally = pdfDocument.canBeSavedIncrementally()
```

#### PDFDocument.prototype.saveToBuffer(options)

Saves the document to a Buffer.

- **string options: See :doc**: `/reference/common/pdf-write-options`.

- **Returns:** `Buffer`

```
var buffer = doc.saveToBuffer("garbage=2,compress=yes")
```

#### PDFDocument.prototype.save(filename, options)

Saves the document to a file.

- **string filename**: 
- **string options: See :doc**: `/reference/common/pdf-write-options`

```
doc.save("out.pdf", "incremental")
```

#### PDFDocument.prototype.countVersions()

Returns the number of versions of the document in a PDF file,
typically 1 + the number of updates.

- **Returns:** number

```
var versionNum = pdfDocument.countVersions()
```

#### PDFDocument.prototype.hasUnsavedChanges()

Returns true if the document has been changed since it was last
opened or saved.

- **Returns:** boolean

```
var hasUnsavedChanges = pdfDocument.hasUnsavedChanges()
```

#### PDFDocument.prototype.countUnsavedVersions()

Returns the number of unsaved updates to the document.

- **Returns:** number

```
var unsavedVersionNum = pdfDocument.countUnsavedVersions()
```

#### PDFDocument.prototype.validateChangeHistory()

Check the history of the document, and determine the last
version that checks out OK. Returns `0` if the entire history
is OK, `1` if the next to last version is OK, but the last
version has issues, etc.

- **Returns:** number

```
var changeHistory = pdfDocument.validateChangeHistory()
```

#### Processing

#### PDFDocument.prototype.subsetFonts()

Scan the document and establish which glyphs are used from each
font, next rewrite the font files such that they only contain
the used glyphs. By removing unused glyphs the size of the font
files inside the PDF will be reduced.

```
pdfDocument.subsetFonts()
```

#### PDFDocument.prototype.bake(bakeAnnots, bakeWidgets)

*Baking* a document changes all the annotations and/or form fields (otherwise known as widgets) in the document into static content. It "bakes" the appearance of the annotations and fields onto the page, before removing the interactive objects so they can no longer be changed.

Effectively this removes the "annotation or "widget" type of these objects, but keeps the appearance of the objects.

- **boolean bakeAnnots**: Whether to bake annotations or not. Defaults to true.
- **boolean bakeWidgets**: Whether to bake widgets or not. Defaults to true.

#### AcroForm Javascript

#### PDFDocument.prototype.enableJS()

Enable interpretation of document Javascript actions.

```
pdfDocument.enableJS()
```

#### PDFDocument.prototype.disableJS()

Disable interpretation of document Javascript actions.

```
pdfDocument.disableJS()
```

#### PDFDocument.prototype.isJSSupported()

Returns whether interpretation of document Javascript actions
is supported. Interpretation of Javascript may be disabled at
build time.

- **Returns:** boolean

```
var jsIsSupported = pdfDocument.isJSSupported()
```

#### PDFDocument.prototype.setJSEventListener(listener)

Calls the listener whenever a document Javascript action
triggers an event.

At present the only callback the listener will be used for
is an alert event.

- **Object listener**: The Javascript listener function.

```
pdfDocument.setJSEventListener({
onAlert: function(message) {
print(message)
}
})
```

#### ZUGFeRD

#### PDFDocument.prototype.zugferdProfile()

Determine if the current PDF is a ZUGFeRD PDF, and, if so,
return the profile type in use. Possible return values include:
"NOT ZUGFERD", "COMFORT", "BASIC", "EXTENDED", "BASIC WL",
"MINIMUM", "XRECHNUNG", and "UNKNOWN".

- **Returns:** string

```
var profile = pdfDocument.zugferdProfile()
```

#### PDFDocument.prototype.zugferdVersion()

Determine if the current PDF is a ZUGFeRD PDF, and, if so,
return the version of the spec it claims to conform to.
Returns 0 for non-zugferd PDFs.

- **Returns:** number

```
var version = pdfDocument.zugferdVersion()
```

#### PDFDocument.prototype.zugferdXML()

Return a buffer containing the embedded ZUGFeRD XML data from
this ZUGFeRD PDF.

- **Returns:** `Buffer` | null

```
var buf = pdfDocument.zugferdXML()
```

### PDFFilespecParams

This object represents the information found in a :term:`file specification`.
Such files may be both external files and files embedded inside the PDF document.

This Object has the properties below. If not present in the file
specification they are set to `undefined`.

`filename`
    The name of the file.

`mimetype`
    The MIME type for an embedded file.

`size`
    The size in bytes of the embedded file contents.

`creationDate`
    The creation date of the embedded file.

`modificationDate`
    The modification date of the embedded file.

### PDFGraftMap

The graft map is a structure used to copy objects between different PDF documents,
and track which objects have already been copied so that they can be re-used.

#### Constructors

#### PDFGraftMap

*(not constructible with `new`)*

Call `PDFDocument.prototype.newGraftMap` to create a graft map.

#### Instance methods

#### PDFGraftMap.prototype.graftObject(obj)

Return a deep copy of the given object suitable for use within
the graft map's target document.

- **PDFObject object**: The object to graft.

- **Returns:** `PDFObject`

```
var map = document.newGraftMap()
map.graftObject(obj)
```

#### PDFGraftMap.prototype.graftPage(to, srcDoc, srcPage)

Graft a page and its resources at the given page number from
the source document to the requested page number in the
destination document connected to the map.

Page numbers start at 0 and -1 means at the end of the
document.

- **number to**: The page number to insert the page before.
- **PDFDocument srcDoc**: Source document.
- **number srcPage**: Source page number.

```
var map = dstdoc.newGraftMap()
map.graftObject(-1, srcdoc, 0)
```

### PDFObject

All functions that take a `PDFObject` apply an automatic translation between
Javascript objects and `PDFObject` using a few basic rules:

-
	`null`, `true`, `false`, and numbers are translated directly.

-
	Strings are translated to PDF names, unless they are surrounded by
	parentheses: `"Foo"` becomes the `/Foo` and `"(Foo)"` becomes
	`(Foo)`.

-
	Arrays and dictionaries are recursively translated to PDF arrays and dictionaries.
	Be aware of cycles though! The translation does NOT cope with cyclic references!

This automatic translation goes both ways -- entries of PDF dictionaries and
arrays can be accessed like Javascript objects and arrays.

#### Constructors

#### PDFObject

*(not constructible with `new`)*

Use the methods on a `PDFDocument` instance to create new objects.

#### Instance properties

#### PDFObject.prototype.length

Number of entries in array PDFObjects. Zero for all other object types.

#### PDFObject.prototype.[n]

Get or set the element at index `n` in an array (0-indexed).

- **Throws:** Error on out of bounds accesses.

```
var pdfObject = pdfDocument.newArray()
pdfObject[0] = "hello"
pdfObject[1] = "world"
```

#### PDFObject.prototype.[name]

Access a key named `name` in a dictionary. It is both possible to
get and set its value, but also delete the key entirely.

```
var pages = doc.getTrailer().Root.Pages
pages.Hello = "world"
pages["Xyzzy"] = 42
delete pages["Hello"]
delete pages.Xyzzy
```

#### Instance methods

#### PDFObject.prototype.get(...path)

Access dictionaries and arrays in the `PDFObject`.

Returns null if the dictionary key does not exist, or if
the array index is out of range.

- **Array<number | string | PDFObject> ...path**: The path.

- **Returns:** `PDFObject` | null

```
var dict = pdfDocument.newDictionary()
var value = dict.get("my_key")
var arr = pdfDocument.newArray()
var value = arr.get(1)
var page7 = pdfDocument.getTrailer().get("Root", "Pages", "Kids", 7)
```

#### PDFObject.prototype.getInheritable(key)

For a dictionary, if the requested key does not exist,
getInheritable() will walk Parent references to parent
dictionaries and lookup the same key there.

If no key can be found in any parent or grand-parent or
grand-grand-parent, all the way up, `null` is returned.

- **PDFObject | string key**: 

- **Returns:** `PDFObject` | null

```js
var page = pdfDocument.loadPage(0)
var pageObj = page.getObject()
var rotate = pageObj.getInheritable("Rotate")
```

#### PDFObject.prototype.put(key, value)

Set values in `PDFObject` dictionaries or arrays.

- **PDFObject | string | number key**: Interpreted as an index for arrays or a key string for dictionaries.
- **PDFObject | Array | string | number | boolean | null value**: The value to set at the array index or for dictionary key.

```
var dict = pdfDocument.newDictionary()
dict.put("my_key", "my_value")
var arr = pdfDocument.newArray()
arr.put(0, 42)
```

#### PDFObject.prototype.delete(key)

Delete a reference from a `PDFObject`.

- **number | string | PDFObject key**: 

```
var dict = pdfDocument.newDictionary()
dict.put("my_key", "my_value")
dict.delete("my_key")
var arr = pdfDocument.newArray()
arr.put(1, 42)
arr.delete(1)
```

#### PDFObject.prototype.resolve()

If the object is an indirect reference, return the object it points to; otherwise return the object itself.

- **Returns:** `PDFObject`

```
var resolvedObj = obj.resolve()
```

#### PDFObject.prototype.isArray()

- **Returns:** boolean

```
var result = obj.isArray()
```

#### PDFObject.prototype.isDictionary()

- **Returns:** boolean

```
var result = obj.isDictionary()
```

#### PDFObject.prototype.forEach(callback)

Iterate over all the entries in a dictionary or array and call a function for each value-key pair.

- **callback: ``(val: PDFObject, key: number | string, self**: PDFObject) => void``

```
obj.forEach(function (value, key) {
console.log("value="+value+",key="+key)
})
```

#### PDFObject.prototype.push(item)

Append item to the end of the object.

- **PDFObject item**: 

```
obj.push("item")
```

#### PDFObject.prototype.toString(tight, ascii)

Returns the object as a pretty-printed string.

- **boolean tight**: Whether to print the object as tightly as possible, or as human-readably as possible.
- **boolean ascii**: Whether to print binary data as ascii or as binary data.

- **Returns:** string

```
var str = obj.toString()
```

#### PDFObject.prototype.valueOf()

Try to convert a PDF object into a corresponding primitive Javascript value.

Indirect references are converted to the string "obj 0 R" where obj
is the PDF object's object number.

Names are converted to strings.

Arrays and dictionaries are not converted.

- **Returns:** A Javascript value or this.

```
var val = obj.valueOf()
```

#### PDFObject.prototype.compare(other_obj)

Compare the object to another one. Returns 0 on match, non-zero
on mismatch.

- **PDFObject other**: 

- **Returns:** number

```js
var match = pdfObj.compare(other_obj)
```

#### Streams

The only way to access a stream is via an indirect object, since all streams are numbered objects.

#### PDFObject.prototype.isStream()

Returns whether the object is an indirect reference pointing to a stream.

- **Returns:** boolean

```
var val = obj.isStream()
```

#### PDFObject.prototype.readStream()

Read the contents of the stream object into a `Buffer`.

- **Returns:** `Buffer`

```
var buffer = obj.readStream()
```

#### PDFObject.prototype.readRawStream()

Read the raw, uncompressed, contents of the stream object into a
`Buffer`.

- **Returns:** `Buffer`

```
var buffer = obj.readRawStream()
```

#### PDFObject.prototype.writeObject(obj)

Update the object the indirect reference points to.

- **PDFObject obj**: 

```
obj.writeObject(obj)
```

#### PDFObject.prototype.writeStream(buf)

Update the contents of the stream the indirect reference points to.
This will update the "Length", "Filter" and "DecodeParms" automatically.

- **Buffer | ArrayBuffer | Uint8Array buf**: 

```
obj.writeStream(buffer)
```

#### PDFObject.prototype.writeRawStream(buf)

Update the contents of the stream the indirect reference points to.
The buffer must contain already compressed data that matches
the "Filter" and "DecodeParms". This will update the "Length"
automatically, but leave the "Filter" and "DecodeParms" untouched.

- **Buffer | ArrayBuffer | Uint8Array buf**: 

```
obj.writeRawStream(buffer)
```

#### Primitive Objects

Primitive PDF objects such as booleans, names, and numbers can usually be
treated like Javascript values (thanks to valueOf). When that is not sufficient
use these functions:

#### PDFObject.prototype.isNull()

Returns true if the object is null.

- **Returns:** boolean

```
var val = obj.isNull()
```

#### PDFObject.prototype.isBoolean()

Returns whether the object is a boolean.

- **Returns:** boolean

```
var val = obj.isBoolean()
```

#### PDFObject.prototype.asBoolean()

Get the boolean primitive value.

- **Returns:** boolean

```
var val = obj.asBoolean()
```

#### PDFObject.prototype.isInteger()

Returns whether the object is an integer.

- **Returns:** boolean

```
var val = obj.isInteger()
```

#### PDFObject.prototype.isReal()

Returns whether the object is a PDF real number.

- **Returns:** boolean

```js
var val = pdfObj.isReal()
```

#### PDFObject.prototype.isNumber()

Returns whether the object is a number (an integer or a real).

- **Returns:** boolean

```
var val = obj.isNumber()
```

#### PDFObject.prototype.asNumber()

Get the number primitive value.

- **Returns:** number

```
var val = obj.asNumber()
```

#### PDFObject.prototype.isName()

Returns whether the object is a name.

- **Returns:** boolean

```
var val = obj.isName()
```

#### PDFObject.prototype.asName()

Get the name as a string.

- **Returns:** string

```
var val = obj.asName()
```

#### PDFObject.prototype.isString()

Returns whether the object is a string.

- **Returns:** boolean

```
var val = obj.isString()
```

#### PDFObject.prototype.asString()

Convert a "text string" to a Javascript unicode string.

- **Returns:** string

```
var val = obj.asString()
```

#### PDFObject.prototype.asByteString()

Convert a string to an array of byte values.

- **Returns:** Uint8Array | Array of number

```
var val = obj.asByteString()
```

#### PDFObject.prototype.isIndirect()

Is the object an indirect reference.

- **Returns:** boolean

```
var val = obj.isIndirect()
```

#### PDFObject.prototype.asIndirect()

Return the object number the indirect reference points to.

- **Returns:** number

```
var val = obj.asIndirect()
```

### PDFPage

A PDFPage inherits `Page`, so its interface is also available for
PDFPage objects.

#### Constructors

#### PDFPage

*(not constructible with `new`)*

Instances of this class are returned by :js`~Document.prototype.loadPage` called on a `PDFDocument`.

#### Instance methods

#### PDFPage.prototype.getObject()

Get the underlying `PDFObject` for a `PDFPage`.

- **Returns:** `PDFObject`

```
var obj = page.getObject()
```

#### PDFPage.prototype.getAnnotations()

Returns an array of all annotations on the page.

- **Returns:** Array of `PDFAnnotation`

```
var annots = page.getAnnotations()
```

#### PDFPage.prototype.getWidgets()

Returns an array of all widgets on the page.

- **Returns:** Array of `PDFWidget`

```
var widgets = page.getWidgets()
```

#### PDFPage.prototype.setPageBox(box, rect)

Sets the type of box required for the page.

- **string box: The :term**: `page box` type to change.
- **Rect rect**: The new desired dimensions.

```
page.setPageBox("ArtBox", [10, 10, 585, 832])
```

#### PDFPage.prototype.createAnnotation(type)

Create a new blank annotation of a given annotation type.

- **string type: The :term**: `annotation type` to create.

- **Returns:** `PDFAnnotation`.

```
var annot = page.createAnnotation("Text")
```

#### PDFPage.prototype.deleteAnnotation(annot)

Delete a `PDFAnnotation` from the page.

- **PDFAnnotation annot**: 

```
var annots = getAnnotations()
page.delete(annots[0])
```

#### PDFPage.prototype.update()

Loop through all annotations of the page and update them.

Returns true if re-rendering is needed because at least one
annotation was changed (due to either events or Javascript
actions or annotation editing).

- **Returns:** boolean

```
// edit an annotation
var updated = page.update()
```

#### PDFPage.prototype.toPixmap(matrix, colorspace, alpha, showExtras, usage, box)

Render the page into a `Pixmap` with the PDF "usage" and :term:`page box`.

- **Matrix matrix**: The transformation matrix.
- **ColorSpace colorspace**: The desired colorspace.
- **boolean alpha**: Whether or not the returned pixmap will use alpha.
- **boolean renderExtra**: Whether annotations and widgets should be rendered. Defaults to true.
- **string usage**: If the output is destined for viewing or printing. Defaults to "View".
- **string box**: Default is "CropBox".

- **Returns:** `Pixmap`

```
var pixmap = page.toPixmap(
mupdf.Matrix.identity,
mupdf.ColorSpace.DeviceRGB,
true,
false,
"View",
"CropBox"
)
```

#### PDFPage.prototype.applyRedactions(blackBoxes, imageMethod, lineArtMethod, textMethod)

Applies all the Redaction annotations on the page.

Redactions are a special type of annotation used to permanently remove (or
"redact") content from a PDF.

- **boolean blackBoxes**: Whether to use black boxes at each redaction or not.
- **number imageMethod**: Default is `PDFPage.REDACT_IMAGE_PIXELS`.
- **number lineArtMethod**: Default is `PDFPage.REDACT_LINE_ART_REMOVE_IF_COVERED`.
- **number textMethod**: Default is `PDFPage.REDACT_TEXT_REMOVE`.

Once redactions are added to a page you can *apply* them, which is an
irreversible action, thus it is a two step process as follows:

```
var annot1 = page.createAnnotation("Redaction")
annot1.setRect([0, 0, 500, 100])
var annot2 = page.createAnnotation("Redaction")
annot2.setRect([0, 600, 500, 700])
page.applyRedactions(true, mupdf.PDFPage.REDACT_IMAGE_NONE)
```

#### PDFPage.prototype.process(processor)

Run through the page contents stream and call methods on the
supplied `PDFProcessor`.

- **PDFProcessor processor**: User defined function callback object.

```js
pdfPage.process(processor)
```

#### PDFPage.prototype.getTransform()

Return the transform from MuPDF page space (upper left page origin,
y descending, 72 dpi) to PDF user space (arbitrary page origin, y
ascending, UserUnit dpi).

- **Returns:** `Rect`

```js
var transform = pdfPage.getTransform()
```

#### PDFPage.prototype.createSignature(name)

Create a new signature widget with the given name as field
label.

- **string name**: The desired field label.

- **Returns:** `PDFWidget`

```js
var signatureWidget = pdfPage.createSignature("test")
```

#### PDFPage.prototype.countAssociatedFiles()

Return the number of :term:`associated files <associated file>`
associated with this page. Note that this is the number of
files associated with this page, not necessarily the
total number of files associated with elements throughout the
entire document.

- **Returns:** number

```js
var count = pdfPage.countAssociatedFiles()
```

#### PDFPage.prototype.associatedFile(n)

Return the :term:`file specification` object that represents the nth
Associated File on this page.

`n` should be in the range `0 <= n < countAssociatedFiles()`.

Returns null if no associated file exists or index is out of range.

- **Returns:** `PDFObject` | null

```js
var obj = pdfPage.associatedFile(0)
```

#### PDFPage.prototype.clip(rect)

Clip the page to the given rectangle.

- **Rect rect**: The new desired dimensions.

```js
pdfPage.clip([0, 0, 200, 300])
```

### PDFProcessor

A PDF processor object provides callbacks that will be called for each
PDF operator when it is passed to `PDFAnnotation.prototype.process` and
`PDFPage.prototype.process`. The callbacks correspond to the equivalent
PDF operator. Refer to the PDF specification's section on `graphic
operators
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G9.3859555>`_
for what these do and what the callback arguments are.

#### Constructors

#### PDFProcessor(callbacks)

*(interface type)*

#### Special resource tracking

These are not operators per se, but are called when the
current resource dictionary used changes such as when
executing XObject forms.

- push_resources(resources)
- pop_resources()

#### General graphics state callbacks

- op_w(lineWidth: number)
- op_j(lineJoin: number)
- op_J(lineCap: number)
- op_M(miterLimit: number)
- op_d(dashPattern: Array of number, phase: number)
- op_ri(intent: string)
- op_i(flatness: number)
- op_gs(name: string, extGState: `PDFObject`)

#### Special graphics state

- op_q()
- op_Q()
- op_cm(a: number, b: number, c: number, d: number, e: number, f: number)

#### Path construction

- op_m(x: number, y: number)
- op_l(x: number, y: number)
- op_c(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number)
- op_v(x2: number, y2: number, x3: number, y3: number)
- op_y(x1: number, y1: number, x3: number, y3: number)
- op_h()
- op_re(x: number, y: number, w: number, h: number)

#### Path painting

- op_S()
- op_s()
- op_F()
- op_f()
- op_fstar()
- op_B()
- op_Bstar()
- op_b()
- op_bstar()
- op_n()

#### Clipping paths

- op_W()
- op_Wstar()

#### Text objects

- op_BT()
- op_ET()

#### Text state

- op_Tc(charSpace: number)
- op_Tw(wordSpace: number)
- op_Tz(scale: number)
- op_TL(leading: number)
- op_Tf(name: string, size: number)
- op_Tr(render: number)
- op_Ts(rise: number)

#### Text positioning

- op_Td(tx: number, ty: number)
- op_TD(tx: number, ty: number)
- op_Tm(a: number, b: number, c: number, d: number, e: number, f: number)
- op_Tstar()

#### Text showing

- op_TJ(textArray: Array of (string | number))
- op_Tj(stringOrByteArray: string | Array of number)
- op_squote(stringOrByteArray: string | Array of number)
- op_dquote(wordSpace: number, charSpace: number, stringOrByteArray: string | Array of number)

#### Type 3 fonts

- op_d0(wx: number, wy: number)
- op_d1(wx: number, wy: number, llx: number, lly: number, urx: number, ury: number)

#### Color

- op_CS(name: string, colorspace: `ColorSpace`)
- op_cs(name: string, colorspace: `ColorSpace`)
- op_SC_color(color: Array of number)
- op_sc_color(color: Array of number)

- op_SC_pattern(name: string, patternID: number, color: Array of number)
- op_sc_pattern(name: string, patternID: number, color: Array of number)
- op_SC_shade(name: string, shade: `Shade`)
- op_sc_shade(name: string, shade: `Shade`)

- op_G(gray: number)
- op_g(gray: number)
- op_RG(r: number, g: number, b: number)
- op_rg(r: number, g: number, b: number)
- op_K(c: number, m: number, y: number, k: number)
- op_k(c: number, m: number, y: number, k: number)

#### Shadings

- op_sh(name: string, shade: Shade)

#### Inline images

- op_BI(image: `Image`, colorspace: `ColorSpace`)

#### XObjects (Images and Forms)

- op_Do_image(name: string, image: `Image`)
- op_Do_form(name: string, xobject: `PDFObject`, resources: `PDFObject`)

#### Marked content

- op_MP(tag: string)
- op_DP(tag: string, raw: string)
- op_BMC(tag: string)
- op_BDC(tag: string, raw: string)
- op_EMC()

#### Compatibility

- op_BX()
- op_EX()

### PDFWidget

:term:`Widgets <Widget Type>` are annotations that represent form
components such as buttons, text inputs and signature fields.

Because PDFWidget inherits `PDFAnnotation`, they also provide the
same interface as other annotation types.

Many widgets, e.g. text inputs or checkboxes, are the visual representation of
an associated form field. As the widget changes state, so does its
corresponding field value; for example when the text is edited in a text input
or a checkbox is checked. Note that widgets may be changed by Javascript event
handlers triggered by edits on other widgets.

The PDF specification has sections on `Widget annotations
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1951506>`_
and
`Interactive forms
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1951635>`_
with further details.

#### Constructors

#### PDFWidget

*(not constructible with `new`)*

To get the widgets on a page, see `PDFPage.prototype.getWidgets()`.

#### Instance methods

#### PDFWidget.prototype.getFieldType()

Return the widget type, one of the following:

`"button" | "checkbox" | "combobox" | "listbox" | "radiobutton" | "signature" | "text"`

- **Returns:** string

```
var type = widget.getFieldType()
```

#### PDFWidget.prototype.getFieldFlags()

Return the field flags which indicate attributes for the
field. There are convenience functions to check some of these:
:js`~PDFWidget.prototype.isReadOnly()`,
:js`~PDFWidget.prototype.isMultiline()`,
:js`~PDFWidget.prototype.isPassword()`,
:js`~PDFWidget.prototype.isComb()`,
:js`~PDFWidget.prototype.isButton()`,
:js`~PDFWidget.prototype.isPushButton()`,
:js`~PDFWidget.prototype.isCheckbox()`,
:js`~PDFWidget.prototype.isRadioButton()`,
:js`~PDFWidget.prototype.isText()`,
:js`~PDFWidget.prototype.isChoice()`,
:js`~PDFWidget.prototype.isListBox()`,
and
:js`~PDFWidget.prototype.isComboBox()`.

For details refer to the PDF specification's sections
on flags
`common to all field types
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1697681>`_,
`for button fields
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1697832>`_,
`for text fields
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1967484>`_,
and
`for choice fields
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G13.1873701>`_.

- **Returns:** number

```
var flags = widget.getFieldFlags()
```

#### PDFWidget.prototype.getName()

Retrieve the name of the field.

- **Returns:** string

```
var fieldName = widget.getName()
```

#### PDFWidget.prototype.getMaxLen()

Get maximum allowed length of the string value.

- **Returns:** number

```
var length = widget.getMaxLen()
```

#### PDFWidget.prototype.getOptions()

Returns an array of strings which represent the value for each corresponding radio button or checkbox field.

- **Returns:** Array of string

```
var options = widget.getOptions()
```

#### PDFWidget.prototype.getLabel()

Get the field name as a string.

- **Returns:** string

```
var label = widget.getLabel()
```

#### Editing

#### PDFWidget.prototype.getValue()

Get the widget value.

- **Returns:** string

```
var value = widget.getValue()
```

#### PDFWidget.prototype.setTextValue(value)

Set the widget string value.

- **string value**: New text value.

```
widget.setTextValue("Hello World!")
```

#### PDFWidget.prototype.setChoiceValue(value)

Sets the choice value against the widget.

- **string value**: New choice value.

```
widget.setChoiceValue("Yes")
```

#### PDFWidget.prototype.toggle()

Toggle the state of the widget, returns true if the state changed.

- **Returns:** boolean

```
var state = widget.toggle()
```

#### PDFWidget.prototype.getEditingState()

Get whether the widget is in editing state.

- **Returns:** boolean

```
var state = widget.getEditingState()
```

#### PDFWidget.prototype.setEditingState(state)

Set whether the widget is in editing state.

When in editing state any changes to the widget value will not
cause any side-effects such as changing other widgets or
running Javascript event handlers. This is intended for, e.g.
when a text widget is interactively having characters typed
into it. Once editing is finished the state should reverted
back, before updating the widget value again.

- **boolean state**: 

```
widget.getEditingState(false)
```

#### PDFWidget.prototype.layoutTextWidget()

Layout the value of a text widget. Returns a text layout object.

- **Returns:** Object

```
var layout = widget.layoutTextWidget()
```

#### Flags

#### PDFWidget.prototype.isReadOnly()

If the value is read only and the widget cannot be interacted with.

- **Returns:** boolean

```
var isReadOnly = widget.isReadOnly()
```

#### PDFWidget.prototype.isRadioButton()

Return whether the widget is of "radiobutton" type.

- **Returns:** boolean

#### Signatures

#### PDFWidget.prototype.isSigned()

Returns true if the signature is signed.

- **Returns:** boolean

```
var isSigned = widget.isSigned()
```

#### PDFWidget.prototype.validateSignature()

Returns number of updates ago when signature became invalid.
Returns 0 is signature is still valid, 1 if it became
invalid during the last save, etc.

- **Returns:** number

```
var validNum = widget.validateSignature()
```

#### PDFWidget.prototype.checkCertificate()

Returns "OK" if signature checked out OK, otherwise a text
string containing an error message, e.g. "Self-signed
certificate." or "Signature invalidated by change to
document.", etc.

- **Returns:** string

```
var result = widget.checkCertificate()
```

#### PDFWidget.prototype.checkDigest()

Returns "OK" if digest checked out OK, otherwise a text string
containing an error message.

- **Returns:** string

```
var result = widget.checkDigest()
```

#### PDFWidget.prototype.getSignatory()

Returns a text string with the distinguished name from a signed
signature, or a text string with an error message.

The returned string follows the format:

``"cn=Name, o=Organization, ou=Organizational Unit,
email=jane.doe@example.com, c=US"``

- **Returns:** string

```
var signatory = widget.getSignatory()
```

#### PDFWidget.prototype.previewSignature(signer, signatureConfig, image, reason, location)

Return a `Pixmap` preview of what the signature would look like
if signed with the given configuration. Reason and location may
be `undefined`, in which case they are not shown.

- **PDFPKCS7Signer signer**: 
- **Object signatureConfig**: 
- **Image image**: 
- **string reason**: 
- **string location**: 

- **Returns:** Pixmap

```
var pixmap = widget.previewSignature(
signer,
{
showLabels: true,
showDate: true
},
image,
"",
""
)
```

#### PDFWidget.prototype.sign(signer, signatureConfig, image, reason, location)

Sign the signature with the given configuration. Reason and
location may be `undefined`, in which case they are not shown.

- **PDFPKCS7Signer signer**: 
- **Object signatureConfig**: 
- **Image image**: 
- **string reason**: 
- **string location**: 

```
widget.sign(
signer,
{
showLabels: true,
showDate: true
},
image,
"",
""
)
```

#### PDFWidget.prototype.clearSignature()

Clear a signed signature, making it unsigned again.

```
widget.clearSignature()
```

#### PDFWidget.prototype.incrementalChangesSinceSigning()

Returns true if there have been incremental changes since the
signature widget was signed.

- **Returns:** boolean

```
var changed = widget.incrementalChangesSinceSigning()
```
