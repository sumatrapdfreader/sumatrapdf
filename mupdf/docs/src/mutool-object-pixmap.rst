.. _mutool_object_pixmap:

.. _mutool_run_js_api_pixmap:

`Pixmap`
----------------------------

A `Pixmap` object contains a color raster image (short for pixel map).
The components in a pixel in the `Pixmap` are all byte values,
with the transparency as the last component.
A `Pixmap` also has a location (x, y) in addition to its size;
so that they can easily be used to represent tiles of a page.


.. method:: new Pixmap(colorspace, bounds, alpha)

    *Constructor method*.

    Create a new pixmap. The pixel data is **not** initialized; and will contain garbage.

    :arg colorspace: `ColorSpace`.
    :arg bounds: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.
    :arg alpha: `Boolean`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pixmap = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0,0,100,100], true);

.. method:: new Pixmap(pixmap, mask)

    |mutool_tag|

    *Constructor method*.

    Create a new pixmap based on an existing pixmap without alpha, and combining it with a single component soft mask of the same dimensions.

    :arg pixmap: `Pixmap` Used to set the color of pixels in the result.
    :arg mask: `Pixmap` Used to set the alpha of pixels in the resul.t

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var pix = new mupdf.Pixmap(mupdf.ColorSpace.DeviceRGB, [0,0,100,100], false);
        var mask = new mupdf.Pixmap(mupdf.ColorSpace.DeviceGray, [0,0,100,100], false);
        // Set pixels in pix to desired RGB color values.
        // Set pixels in mask to desired alpha levels.
        var pixmapWithAlpha = new mupdf.Pixmap(pix, mask);


|instance_methods|


.. method:: clear(value)

    Clear the pixels to the specified value. Pass `255` for white, or omit for transparent.

    :arg value: Pixel value.

    |example_tag|

    .. code-block:: javascript

        pixmap.clear(255);
        pixmap.clear();


.. method:: getBounds()

    Return the pixmap bounds.

    :return: `[ulx,uly,lrx,lry]` :ref:`Rectangle<mutool_run_js_api_rectangle>`.

    |example_tag|

    .. code-block:: javascript

        var rect = pixmap.getBounds();


.. method:: getWidth()

    :return: `Int` The width value.

    |example_tag|

    .. code-block:: javascript

        var w = pixmap.getWidth();

.. method:: getHeight()

    :return: `Int` The height value.

    |example_tag|

    .. code-block:: javascript

        var h = pixmap.getHeight();

.. method:: getX()

    :return: `Int` The x coordinate of the pixmap.

    |example_tag|

    .. code-block:: javascript

        var x = pixmap.getX();

.. method:: getY()

    :return: `Int` The y coordinate of the pixmap.

    |example_tag|

    .. code-block:: javascript

        var y = pixmap.getY();

.. method:: getNumberOfComponents()

    Number of colors; plus one if an alpha channel is present.

    :return: `Int` Number of color components.

    |example_tag|

    .. code-block:: javascript

        var num = pixmap.getNumberOfComponents();

.. method:: getAlpha()



    *True* if alpha channel is present.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var alpha = pixmap.getAlpha();

.. method:: getStride()

    Number of bytes per row.

    :return: `Int`.

    |example_tag|

    .. code-block:: javascript

        var stride = pixmap.getStride();

.. method:: getColorSpace()

    Returns the `ColorSpace` for the `Pixmap`.

    :return: `ColorSpace`.

    |example_tag|

    .. code-block:: javascript

        var cs = pixmap.getColorSpace();


.. method:: setResolution(xRes, yRes)

    Set `x` & `y` resolution.

    :arg xRes: `Int` X resolution in dots per inch.
    :arg yRes: `Int` Y resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        pixmap.setResolution(300, 300);


.. method:: getXResolution()

    Returns the `x` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var xRes = pixmap.getXResolution();


.. method:: getYResolution()

    Returns the `y` resolution for the `Pixmap`.

    :return: `Int` Resolution in dots per inch.

    |example_tag|

    .. code-block:: javascript

        var yRes = pixmap.getYResolution();


.. method:: getSample(x, y, index)

    |mutool_tag|

    Get the value of component ``index`` at position `x`, `y` (relative to the image origin: 0, 0 is the top left pixel).

    :arg x: X coordinate.
    :arg y: Y coordinate.
    :arg index: Component index. i.e. For CMYK ColorSpaces 0 = Cyan, for RGB 0 = Red etc.
    :return: `Int`.

    |example_tag|

    .. code-block:: javascript

        var sample = pixmap.getSample(0,0,0);





.. method:: saveAsPNG(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PNG`. Only works for :title:`Gray` and :title:`RGB` images.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPNG("fileName.png");


.. method:: saveAsJPEG(fileName, quality)

    |mutool_tag|

    Save the `Pixmap` as a :title:`JPEG`. Only works for :title:`Gray`, :title:`RGB` and :title:`CMYK` images.

    :arg fileName: `String`.
    :arg quality: `Int`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsJPEG("fileName.jpg", 80);


.. method:: saveAsPAM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PAM`.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPAM("fileName.pam");

.. method:: saveAsPNM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PNM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPNM("fileName.pnm");

.. method:: saveAsPBM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PBM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPBM("fileName.pbm");

.. method:: saveAsPKM(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`PKM`. Only works for :title:`Gray` and :title:`RGB` images without alpha.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsPKM("fileName.pkm");

.. method:: saveAsJPX(fileName)

    |mutool_tag|

    Save the `Pixmap` as a :title:`JPX`.

    :arg fileName: `String`.

    |example_tag|

    .. code-block:: javascript

        pixmap.saveAsJPX("fileName.jpx");



.. method:: invert()

    Invert all pixels. All components are processed, except alpha which is unchanged.

    |example_tag|

    .. code-block:: javascript

        pixmap.invert();

.. method:: invertLuminance()

    Transform all pixels so that luminance of each pixel is inverted,
    and the chrominance remains as unchanged as possible.
    All components are processed, except alpha which is unchanged.

    |example_tag|

    .. code-block:: javascript

        pixmap.invertLuminance();

.. method:: gamma(gamma)

    Apply gamma correction to `Pixmap`. All components are processed,
    except alpha which is unchanged.

    Values `>= 0.1 & < 1` = darken, `> 1 & < 10` = lighten.

    :arg gamma: `Float`.

    |example_tag|

    .. code-block:: javascript

        pixmap.gamma(3);

.. method:: tint(black, white)

    Tint all pixels in a :title:`RGB`, :title:`BGR` or :title:`Gray` `Pixmap`.
     Map black and white respectively to the given hex :title:`RGB` values.

    :arg black: `Integer`.
    :arg white: `Integer`.

    |example_tag|

    .. code-block:: javascript

        pixmap.tint(0xffff00, 0xffff00);



.. _mutool_run_js_api_pixmap_warp:
.. method:: warp(points, width, height)

    Return a warped subsection of the `Pixmap`, where the result has the requested dimensions.

    :arg points: `[x0, y0, x1, y1, x2, y2, x3, y3, x4, y4]`
                 Points give the corner points of a convex quadrilateral within the `Pixmap` to be warped.
    :arg width: `Int`.
    :arg height: `Int`.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var warpedPixmap = pixmap.warp([0,0,100,0,0,100,100,100],200,200);


.. method:: autowarp(points)

    |mutool_tag|

    Same as :ref:`Pixmap.warp()<mutool_run_js_api_pixmap_warp>` except that width and height are automatically determined.

    :arg points: `[x0, y0, x1, y1, x2, y2, x3, y3, x4, y4]`
                 Points give the corner points of a convex quadrilateral within the `Pixmap` to be warped.

    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var warpedPixmap = pixmap.autowarp([0,0,100,0,0,100,100,100]);


.. method:: convertToColorSpace(colorspace, proof, defaultColorSpaces, colorParams, keepAlpha)

    Convert pixmap into a new pixmap of a desired colorspace.
    A proofing colorspace, a set of default colorspaces and color
    parameters used during conversion may be specified.
    Finally a boolean indicates if alpha should be preserved
    (default is to not preserve alpha).

    :arg colorspace: `Colorspace`.
    :arg proof: `Colorspace`.
    :arg defaultColorSpaces: `DefaultColorSpaces`.
    :arg colorParams: `[]`.
    :arg keepAlpha: `Boolean`.

    :return: `Pixmap`.


    .. TODO(tor): Can't get any joy out of this one because of `DefaultColorSpaces` not working for me.


.. method:: getPixels()

    Returns an array of pixels for the `Pixmap`.


    :return: `[...]`.

    |example_tag|

    .. code-block:: javascript

        var pixels = pixmap.getPixels();


.. method:: asPNG()

    Returns a buffer of the `Pixmap` as a :title:`PNG`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPNG();



.. method:: asPSD()

    Returns a buffer of the `Pixmap` as a :title:`PSD`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPSD();


.. method:: asPAM()

    Returns a buffer of the `Pixmap` as a :title:`PAM`.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asPAM();



.. method:: asJPEG(quality, invertCMYK)

    Returns a buffer of the `Pixmap` as a :title:`JPEG`.
    Note, if the `Pixmap` has an alpha channel then an exception will be thrown.

    :arg quality: `Integer`. The desired quality in percent.
    :arg invertCMYK: `Boolean`. Whether to invert CMYK.


    :return: `Buffer`.

    |example_tag|

    .. code-block:: javascript

        var buffer = pixmap.asJPEG(80);


.. method:: detectDocument(points)

    |mutool_tag|

    Detect a "document" in a `Pixmap` (either grayscale or rgb, without alpha)
    Note, if the `Pixmap` is not Greyscale with no alpha then an exception will be thrown.

    :return: `[x0,y0,x1,y1,x2,y2,x3,y3]`.

    |example_tag|

    .. code-block:: javascript

        var documentLocation = pixmap.detectDocument([0,0,100,0,100,100,0,100]);

.. method:: detectSkew()

    |mutool_tag|

    Returns the angle of skew detected from `Pixmap`.
    Note, if the `Pixmap` is not Greyscale with no alpha then an exception will be thrown.


    :return: `Float`.

    |example_tag|

    .. code-block:: javascript

        var angle = pixmap.detectSkew();


.. method:: deskew(angle, border)

    |mutool_tag|

    Returns a new `Pixmap` being the deskewed version of the supplied `Pixmap`.
    Note, if a `Pixmap` is supplied that is not RGB or Greyscale, or has alpha then an exception will be thrown.

    :arg angle: `Float`. The angle to deskew.
    :arg border: `String`. "increase" increases the size of the pixmap so no pixels are lost. "maintain" maintains the size of the pixmap. "decrease" decreases the size of the page so no new pixels are shown.
    :return: `Pixmap`.

    |example_tag|

    .. code-block:: javascript

        var deskewed = pixmap.deskew(angle, 0);


.. method:: computeMD5()

    |mutool_tag|

    Returns the MD5 digest of the pixmap pixel data.

    :return: `String` containing digest as 16 hex digits.

    |example_tag|

    .. code-block:: javascript

        var md5 = pixmap.computeMD5();



.. method:: decodeBarcode(rotate)

    |mutool_tag|

    Decodes a barcode detected in the pixmap, and returns an object with properties for barcode type and contents.

    :arg rotate: `Integer` Degrees of rotation to rotate pixmap before detecting barcode.

    :return: :ref:`BarcodeInfo<mutool_run_js_api_object_barcode_info>`.

    |example_tag|

    .. code-block:: javascript

        var barcodeInfo = pixmap.decodeBarcode(0);



.. method:: encodeBarcode(barcodeType, contents, size, errorCorrectionLevel, quietZones, humanReadableText)

    |mutool_tag|

    Encodes a barcode into a pixmap.

    :arg barcodeType: `String` The desired barcode type, one of:

      - `aztec`
      - `codabar`
      - `code39`
      - `code93`
      - `code128`
      - `databar`
      - `databarexpanded`
      - `datamatrix`
      - `ean8`
      - `ean13`
      - `itf`
      - `maxicode`
      - `pdf417`
      - `qrcode`
      - `upca`
      - `upce`
      - `microqrcode`
      - `rmqrcode`
      - `dxfilmedge`
      - `databarlimited`

    :arg contents: `String` The textual content to encode into the barcode.
    :arg size: `Integer` The size of the barcode in pixels.
    :arg errorCorrectionLevel: `Integer` The error correction level (0-8).
    :arg quietZones: `Boolean` Whether to add an empty margin around the barcode.
    :arg humanReadableText: `Boolean` Whether to add human-readable text. Some barcodes, e.g. EAN-13, can have the barcode contents printed in human-readable text next to the barcode.

    :return: :ref:`Pixmap<mutool_run_js_api_pixmap>`.

    |example_tag|

    .. code-block:: javascript

        var pix = Pixmap.encodeBarcode("qrcode", "Hello world!", 100, 2, true, false);
