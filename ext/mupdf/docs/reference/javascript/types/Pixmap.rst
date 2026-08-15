.. default-domain:: js

.. highlight:: javascript

Pixmap
======

A Pixmap object contains a color raster image (short for pixel map).
The components in a pixel in the Pixmap are all byte values,
with the transparency as the last component.

A Pixmap also has a location (x, y) in addition to its size;
so that they can easily be used to represent tiles of a page.

Constructors
------------

.. class::
	Pixmap(colorspace, bbox, alpha)
	Pixmap(pixmap, mask)

	Create a new empty Pixmap whose pixel data is **not**
	initialized. Alternatively create a new Pixmap based on an
	existing Pixmap without alpha and combine it with a single
	component soft mask of the same dimensions.

	:param ColorSpace | null colorspace: The desired colorspace for the new pixmap. ``null`` implies a single component alpha pixmap.
	:param Rect bbox: The desired dimensions of the new pixmap.
	:param boolean alpha: Whether the new pixmap should have an alpha component.
	:param Pixmap pixmap: The original pixmap without alpha.
	:param Pixmap mask: Soft mask used as alpha in the combined pixmap.

	.. code-block::

		var pixmap1 = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0, 0, 100, 100], true)
		var pixmap2 = new mupdf.Pixmap(
			new mupdf.Image("photo.png").toPixmap(),
			new mupdf.Image("softmask.png").toPixmap()
		)

Instance methods
----------------

.. method:: Pixmap.prototype.clear(value)

	Clear the pixels to the specified value. Pass 255 for white, 0 for black, or omit for transparent.

	:param number value: The value to use for clearing.

	.. code-block::

		pixmap.clear(255)

.. method:: Pixmap.prototype.getBounds()

	Return the pixmap bounds.

	:returns: `Rect`

	.. code-block::

		var rect = pixmap.getBounds()

.. method:: Pixmap.prototype.getWidth()

	Get the width of the pixmap.

	:returns: number

	.. code-block::

		var w = pixmap.getWidth()

.. method:: Pixmap.prototype.getHeight()

	Get the height of the pixmap.

	:returns: number

	.. code-block::

		var h = pixmap.getHeight()

.. method:: Pixmap.prototype.getNumberOfComponents()

	Number of colors; plus one if an alpha channel is present.

	:returns: number

	.. code-block::

		var num = pixmap.getNumberOfComponents()

.. method:: Pixmap.prototype.getAlpha()

	Returns whether an alpha channel is present.

	:returns: boolean

	.. code-block::

		var alpha = pixmap.getAlpha()

.. method:: Pixmap.prototype.getStride()

	Number of bytes per row.

	:returns: number

	.. code-block::

		var stride = pixmap.getStride()

.. method:: Pixmap.prototype.getColorSpace()

	Returns the colorspace of this pixmap. Returns null if the pixmap has
	no colors (for example if it is an opacity mask with only an alpha
	channel).

	:returns: `ColorSpace` | null

	.. code-block::

		var cs = pixmap.getColorSpace()

.. method:: Pixmap.prototype.setResolution(x, y)

	Set horizontal and vertical resolution.

	:param number x: Horizontal resolution in dots per inch.
	:param number y: Vertical resolution in dots per inch.

	.. code-block::

		pixmap.setResolution(300, 300)

.. method:: Pixmap.prototype.getX()

	Returns the x coordinate of the pixmap.

	.. code-block:: javascript

		var x = pixmap.getX()

.. method:: Pixmap.prototype.getY()

	Returns the y coordinate of the pixmap.

	:returns: number

	.. code-block:: javascript

		var y = pixmap.getY()

.. method:: Pixmap.prototype.getXResolution()

	Returns the horizontal resolution in dots per inch for this pixmap.

	:returns: number

	.. code-block::

		var xRes = pixmap.getXResolution()

.. method:: Pixmap.prototype.getYResolution()

	Returns the vertical resolution in dots per inch for this pixmap.

	:returns: number

	.. code-block::

		var yRes = pixmap.getYResolution()

.. method:: Pixmap.prototype.invert()

	Invert all pixels. All components are processed, except alpha which is unchanged.

	.. code-block::

		pixmap.invert()

.. method:: Pixmap.prototype.invertLuminance()

	Transform all pixels so that luminance of each pixel is inverted,
	and the chrominance remains as unchanged as possible.
	All components are processed, except alpha which is unchanged.

	.. code-block::

		pixmap.invertLuminance()

.. method:: Pixmap.prototype.gamma(p)

	Apply gamma correction to this pixmap. All components are processed,
	except alpha which is unchanged.

	Values ``>= 0.1 & < 1`` darkens the pixmap, ``> 1 & < 10`` lightens the pixmap.

	:param number p: Desired gamma level.

	.. code-block::

		pixmap.gamma(3.5)

.. method:: Pixmap.prototype.tint(black, white)

	Tint all pixels in RGB, BGR or Gray pixmaps.
	Map black and white respectively to the given hex RGB values.

	:param Color | number black: Map black to this color.
	:param Color | number white: Map white to this color.

	.. code-block::

		pixmap.tint(0xffff00, 0xffff00)

.. method:: Pixmap.prototype.warp(points, width, height)

	Return a warped subsection of this pixmap, where the corner of
	the input quadrilateral will be "warped" to become the four corner
	points of the returned pixmap defined by the requested dimensions.

	:param Quad points: The corners of a convex quadrilateral within the `Pixmap` to be warped.
	:param number width: Width of resulting pixmap.
	:param number height: Height of resulting pixmap.

	:returns: `Pixmap`

	.. code-block::

		var warpedPixmap = pixmap.warp([[0, 0], [100, 100], [130, 170], [150, 200]], 200, 200)

.. method:: Pixmap.prototype.autowarp(points)

	|only_mutool|

	Same as `Pixmap.prototype.warp()` except that width and height
	are automatically determined.

	:param Quad points: The corners of a convex quadrilateral within the `Pixmap` to be warped.

	:returns: `Pixmap`

	.. code-block:: javascript

		var warpedPixmap = pixmap.autowarp([0,0,100,0,0,100,100,100])

.. TODO murun has Pixmap.prototype.convertToColorSpace(colorspace, proofCS, defaultCS, colorParams, keepAlpha), not sure how to reconcile the docs. I could put keepAlpha as the second argument and just leave out the rest. thoughts?

.. method:: Pixmap.prototype.convertToColorSpace(colorspace, keepAlpha)

	Convert pixmap into a new pixmap of a desired colorspace.
	A proofing colorspace, a set of default colorspaces and color
	parameters used during conversion may be specified.
	Finally a boolean indicates if alpha should be preserved
	(default is to not preserve alpha).

	:param ColorSpace colorspace: The desired colorspace.
	:param boolean keepAlpha: Whether to keep the alpha component.

	:returns: `Pixmap`

.. method:: Pixmap.prototype.getPixels()

	Returns an array of pixels for this pixmap.

	:returns: Array of number

	.. code-block::

		var pixels = pixmap.getPixels()

.. method:: Pixmap.prototype.asPNG()

	Returns a buffer of this pixmap as a PNG.

	:returns: `Buffer`

	.. code-block::

		var buffer = pixmap.asPNG()

.. method:: Pixmap.prototype.asPSD()

	Returns a buffer of this pixmap as a PSD.

	:returns: `Buffer`

	.. code-block::

		var buffer = pixmap.asPSD()

.. method:: Pixmap.prototype.asPAM()

	Returns a buffer of this pixmap as a PAM.

	:returns: `Buffer`

	.. code-block::

		var buffer = pixmap.asPAM()

.. method:: Pixmap.prototype.asJPEG(quality, invert_cmyk)

	Returns a buffer of this pixmap as a JPEG.
	Note, if this pixmap has an alpha channel then an exception will be thrown.

	:param number quality: Desired compression quality, between ``0`` and ``100``.
	:param boolean invert_cmyk: How to handle polarity in :term:`CMYK JPEG` images.

	:returns: `Buffer`

	.. code-block::

		var buffer = pixmap.asJPEG(80, false)

.. method:: Pixmap.prototype.decodeBarcode(rotate)

	|only_mutool|

	Decodes a barcode detected in the pixmap, and returns an object with
	properties for barcode type and contents.

	:param number rotate: Degrees of rotation to rotate pixmap before detecting barcode. Defaults to 0.

	:returns: Object with barcode information.

	.. code-block:: javascript

		var barcodeInfo = displayList.decodeBarcode([0, 0, 100, 100 ], 0)

.. method:: Pixmap.prototype.encodeBarcode(barcodeType, contents, size, errorCorrectionLevel, quietZones, humanReadableText)

	|only_mutool|

	Encodes a barcode into a pixmap. The supported types of barcode is either one of:

	.. table::
		:align: left

		===============	=============	=======================	===============	=======================	================
			Matrix               			Linear Product				Linear Industrial
		-----------------------------	---------------------------------------	----------------------------------------
		String		Name   		String			Name		String			Name
		===============	=============	=======================	=============== =======================	================
		``qrcode``	QR Code    	``upca``		UPC-A		``code39``		Code 39
		``microqrcode``	Micro QR Code	``upce``		UPC-E		``code93``		Code 93
		``rmqrcode``	rMQR Code	``ean8``		EAN-8		``code128``		Code 128
		``aztec``	Aztec       	``ean13``		EAN-13		``codabar``		Codabar
		``datamatrix``	DataMatrix	``databar``		DataBar		``databarexpanded``	DataBar Expanded
		``pdf417``	PDF417     	``databarlimited``	DataBar Limited	``dxfilmedge``		DX Film Edge
		``maxicode``	MaxiCode 	\					``itf``			ITF
		===============	=============	=======================	===============	=======================	================

	:param string barcodeType: The desired barcode type.
	:param string contents: The textual content to encode into the barcode.
	:param number size: The size of the barcode in pixels.
	:param number errorCorrectionLevel: The error correction level (0-8).
	:param boolean quietZones: Whether to add an empty margin around the barcode.
	:param boolean humanReadableText: Whether to add human-readable text. Some barcodes, e.g. EAN-13, can have the barcode contents printed in human-readable text next to the barcode.

	:returns: `Pixmap`

	.. code-block:: javascript

		var pix = Pixmap.encodeBarcode("qrcode", "Hello world!", 100, 2, true, false)

.. method:: Pixmap.prototype.getSample(x, y, index)

	|only_mutool|

	Get the value of component ``index`` at position x, y (relative to
	the image origin: 0, 0 is the top left pixel).

	:param number x: X coordinate.
	:param number y: Y coordinate.
	:param number index: Component index. i.e. For CMYK ColorSpaces 0 = Cyan, 3 = Black, for RGB 0 = Red, 2 == Blue etc.

	:throws: RangeError if x, y, or index are out of range.

	:returns: number

	.. code-block:: javascript

		// Get green component of pixel at 10, 10
		var sample = rgbpixmap.getSample(10, 10, 1)

.. method:: Pixmap.prototype.saveAsPNG(filename)

	|only_mutool|

	Save this Pixmap as a PNG. Only works for gray and RGB images.

	:param string filename: Desired name of image file.

	.. code-block:: javascript

		pixmap.saveAsPNG("filename.png")

.. method:: Pixmap.prototype.saveAsJPEG(filename, quality)

	|only_mutool|

	Save this Pixmap as a JPEG file. Only works for gray, RGB and CMYK images.

	:param string filename: Desired name of image file.
	:param number quality: Desired quality between 0 and 100. Defaults to 90.

	.. code-block:: javascript

		pixmap.saveAsJPEG("filename.jpg", 80)

.. method:: Pixmap.prototype.saveAsPAM(filename)

	|only_mutool|

	Save this Pixmap as a PAM file.

	:param string filename: Desired name of image file.

	.. code-block:: javascript

		pixmap.saveAsPAM("filename.pam")

.. method:: Pixmap.prototype.saveAsPNM(filename)

	|only_mutool|

	Save this Pixmap as a PNM file. Only works for gray and RGB images without alpha.

	:param string filename: Desired name of image file.

	.. code-block:: javascript

		pixmap.saveAsPNM("filename.pnm")

.. method:: Pixmap.prototype.saveAsPBM(filename)

	|only_mutool|

	Save this Pixmap as a PBM file. Only works for alpha only, gray and CMYK images without alpha.

	:param string filename: Desired name of image file.

	.. code-block:: javascript

		pixmap.saveAsPBM("filename.pbm")

.. method:: Pixmap.prototype.saveAsPKM(filename)

	|only_mutool|

	Save this Pixmap as a PKM file. Only works for alpha only, gray and CMYK images without alpha.

	:param string filename: Desired name of image file.

	.. code-block:: javascript

		pixmap.saveAsPKM("filename.pkm")

.. method:: Pixmap.prototype.saveAsJPX(filename, quality)

	|only_mutool|

	Save this Pixmap as a JPX file.

	:param string filename: Desired name of image file.
	:param number quality: Desired quality between 0 and 100. Defaults to 90.

	.. code-block:: javascript

		pixmap.saveAsJPX("filename.jpx", 90)

.. method:: Pixmap.prototype.detectDocument(points)

	|only_mutool|

	Detect a "document" in a `Pixmap`. Only a grayscale `Pixmap`
	without alpha is supported, anything else will cause an exception
	to be thrown.

	Returns null if no document was detected.

	:returns: `Quad` | null

	.. code-block:: javascript

		var documentLocation = pixmap.detectDocument([0,0,100,0,100,100,0,100])

.. method:: Pixmap.prototype.detectSkew()

	|only_mutool|

	Returns the angle of skew detected from `Pixmap`.
	Note, if the `Pixmap` is not Greyscale with no alpha then an exception will be thrown.

	:returns: number

	.. code-block:: javascript

		var angle = pixmap.detectSkew()

.. method:: Pixmap.prototype.deskew(angle, border)

	|only_mutool|

	Returns a new `Pixmap` being the deskewed version of the supplied `Pixmap`.
	Note, if a `Pixmap` is supplied that is not RGB or Greyscale, or has alpha then an exception will be thrown.

	:param number angle: The angle to deskew.
	:param string border: "increase" increases the size of the pixmap so no pixels are lost. "maintain" maintains the size of the pixmap. "decrease" decreases the size of the page so no new pixels are shown.
	:returns: `Pixmap`

	.. code-block:: javascript

		var deskewed = pixmap.deskew(angle, 0)

.. method:: Pixmap.prototype.computeMD5()

	|only_mutool|

	Returns the MD5 digest of the pixmap pixel data.
	The digest is returned as a string of 16 hex digits.

	:returns: string

	.. code-block:: javascript

		var md5 = pixmap.computeMD5()
