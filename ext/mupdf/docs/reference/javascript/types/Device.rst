.. default-domain:: js

.. highlight:: javascript

Device
======

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

Constructors
------------

.. class:: Device(callbacks)

	Create a Device which calls back to Javascript.

	The callback object may provide functions matching the methods
	on the Device class. Any device calls that don't have a corresponding
	function will simply be ignored.

	:param callbacks: object containing callback functions

You can create other devices with `DrawDevice` and `DisplayListDevice`.

Constants
---------

:term:`Blend mode` constants for use with `Device.prototype.beginGroup`:

.. data:: Device.BLEND_NORMAL
.. data:: Device.BLEND_MULTIPLY
.. data:: Device.BLEND_SCREEN
.. data:: Device.BLEND_OVERLAY
.. data:: Device.BLEND_DARKEN
.. data:: Device.BLEND_LIGHTEN
.. data:: Device.BLEND_COLOR_DODGE
.. data:: Device.BLEND_COLOR_BURN
.. data:: Device.BLEND_HARD_LIGHT
.. data:: Device.BLEND_SOFT_LIGHT
.. data:: Device.BLEND_DIFFERENCE
.. data:: Device.BLEND_EXCLUSION
.. data:: Device.BLEND_HUE
.. data:: Device.BLEND_SATURATION
.. data:: Device.BLEND_COLOR
.. data:: Device.BLEND_LUMINOSITY

Instance methods
----------------

.. method:: Device.prototype.close()

	Tell this device that we are done, and flush any pending output.

	Before closing, ensure that there have been as many calls to
	`popClip()` as there have been to the clipping functions:
	`clipPath()`, `clipStrokePath()`, `clipText()`, etc.

	.. code-block::

		device.close()

Line art
^^^^^^^^

.. method:: Device.prototype.fillPath(path, evenOdd, ctm, colorspace, color, alpha)

	Fill a path.

	:param Path path: Path object.
	:param boolean evenOdd: Use :term:`even-odd rule` or :term:`non-zero winding number rule` to fill the path.
	:param Matrix ctm: The transform to apply.
	:param ColorSpace colorspace: The colorspace of the color to fill with.
	:param Color color: The color to fill the path with.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.fillPath(path, false, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1, 0, 0], true)

.. method:: Device.prototype.strokePath(path, stroke, ctm, colorspace, color, alpha)

	Stroke a path.

	:param Path path: Path object.
	:param StrokeState stroke: Stroke state.
	:param Matrix ctm: The transform to apply.
	:param ColorSpace colorspace: Colorspace.
	:param Color color: The color to stroke the path with.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.strokePath(path,
			{dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
			mupdf.Matrix.identity,
			mupdf.ColorSpace.DeviceRGB,
			[0, 1, 0],
			0.5
		)

.. method:: Device.prototype.clipPath(path, evenOdd, ctm)

	Clip a path.

	:param Path path: Path object.
	:param boolean evenOdd: Use :term:`even-odd rule` or :term:`non-zero winding number rule` to fill the path.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.clipPath(path, true, mupdf.Matrix.identity)

.. method:: Device.prototype.clipStrokePath(path, stroke, ctm)

	Clip & stroke a path.

	:param Path path: Path object.
	:param StrokeState stroke: Stroke state.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.clipStrokePath(path, true, mupdf.Matrix.identity)

Text
^^^^

.. method:: Device.prototype.fillText(text, ctm, colorspace, color, alpha)

	Fill a text object.

	:param Text text: Text object.
	:param Matrix ctm: The transform to apply.
	:param ColorSpace colorspace: Colorspace
	:param Color color: The color used to fill the text.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.fillText(text, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [1, 0, 0], 1)

.. method:: Device.prototype.strokeText(text, stroke, ctm, colorspace, color, alpha)

	Stroke a text object.

	:param Text text: Text object.
	:param StrokeState stroke: Stroke state.
	:param Matrix ctm: The transform to apply.
	:param ColorSpace colorspace: Colorspace
	:param Color color: The color used to stroke the text.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.strokeText(text,
			{ dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
			mupdf.Matrix.identity,
			mupdf.ColorSpace.DeviceRGB,
			[1, 0, 0],
			1
		)

.. method:: Device.prototype.clipText(text, ctm)

	Clip a text object.

	:param Text text: Text object.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.clipText(text, mupdf.Matrix.identity)

.. method:: Device.prototype.clipStrokeText(text, stroke, ctm)

	Clip & stroke a text object.

	:param Text text: Text object.
	:param StrokeState stroke: stroke state.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.clipStrokeText(text,
			{ dashes: [5, 10], lineWidth: 3, lineCap: 'Round' },
			mupdf.Matrix.identity
		)

.. method:: Device.prototype.ignoreText(text, ctm)

	Invisible text that can be searched but should not be visible, such as for overlaying a scanned OCR image.

	:param Text text: Text object.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.ignoreText(text, mupdf.Matrix.identity)

Shadings
^^^^^^^^

.. method:: Device.prototype.fillShade(shade, ctm, alpha)

	Fill a shading, also known as a gradient.

	:param Shade shade: The gradient.
	:param Matrix ctm: The transform to apply.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.fillShade(shade, mupdf.Matrix.identity, true, { overPrinting: true })

Images
^^^^^^

.. method:: Device.prototype.fillImage(image, ctm, alpha)

	Draw an image. An image always fills a unit rectangle [0, 0, 1, 1], so must be transformed to be placed and drawn at the appropriate size.

	:param Image image: Image object.
	:param Matrix ctm: The transform to apply.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.fillImage(image, mupdf.Matrix.identity, false, { overPrinting: true })

.. method:: Device.prototype.fillImageMask(image, ctm, colorspace, color, alpha)

	An image mask is an image without color. Fill with the color where the image is opaque.

	:param Image image: Image object.
	:param Matrix ctm: The transform to apply.
	:param ColorSpace colorspace: Colorspace
	:param Color color: The color to be used.
	:param number alpha: The :term:`opacity`.

	.. code-block::

		device.fillImageMask(image, mupdf.Matrix.identity, mupdf.ColorSpace.DeviceRGB, [0, 1, 0], true)

.. method:: Device.prototype.clipImageMask(image, ctm)

	Clip graphics using the image to mask the areas to be drawn.

	:param Image image: Image object.
	:param Matrix ctm: The transform to apply.

	.. code-block::

		device.clipImageMask(image, mupdf.Matrix.identity)

Clipping and masking
^^^^^^^^^^^^^^^^^^^^

.. method:: Device.prototype.popClip()

	Pop the clip mask installed by the last clipping operation.

	.. code-block::

		device.popClip()

.. method:: Device.prototype.beginMask(area, luminosity, colorspace, color)

	Create a soft mask. Any drawing commands between `beginMask` and `endMask` are grouped and used as a clip mask.

	:param Rect area: Mask area.
	:param boolean luminosity: If luminosity is ``true``, the mask is derived from the luminosity (grayscale value) of the graphics drawn; otherwise the color is ignored completely and the mask is derived from the alpha of the group.
	:param ColorSpace colorspace: Colorspace
	:param Color color: The color to be used.

	.. code-block::

		device.beginMask([0, 0, 100, 100], true, mupdf.ColorSpace.DeviceRGB, [1, 0, 1])

.. method:: Device.prototype.endMask()

	Ends the mask.

	.. code-block::

		device.endMask()

Groups and transparency
^^^^^^^^^^^^^^^^^^^^^^^

.. method:: Device.prototype.beginGroup(area, colorspace, isolated, knockout, blendmode, alpha)

	Begin a transparency blending group. See :term:`knockout and isolation`
	and :term:`blend mode` in the glossary for a cursory overview of the
	concepts.

	:param Rect area: The blend area.
	:param ColorSpace colorspace: Colorspace
	:param boolean isolated: Whether the group is isolated.
	:param boolean knockout: Whether the group is knockout.
	:param string blendmode: The blend mode used when compositing this group with its backdrop.
	:param number alpha: The :term:`opacity`.

	The blendmode is one of these string values or the corresponding enum constants:
	Normal, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Hue, Saturation, Color, Luminosity.

	You can also use the `Device.BLEND_NORMAL` constant:

	.. code-block::

		device.beginGroup(
			[0, 0, 100, 100],
			mupdf.ColorSpace.DeviceRGB,
			true,
			true,
			"Multiply",
			0.5
		)

.. method:: Device.prototype.endGroup()

	Ends the blending group.

	.. code-block::

		device.endGroup()

Tiling
^^^^^^

.. method:: Device.prototype.beginTile(area, view, xstep, ystep, ctm, id, doc_id)

	Draw a tiling pattern. Any drawing commands between `beginTile` and `endTile` are grouped and then repeated across the whole page. Apply a clip mask to restrict the pattern to the desired shape.

	:param Rect area: Area
	:param Rect view: View
	:param number xstep: x step.
	:param number ystep: y step.
	:param Matrix ctm: The transform to apply.
	:param number id:
	:param number doc_id:
		The purpose of id/doc_id is to allow for efficient caching of
		rendered tiles. If id is 0, then no caching is performed. If
		it is non-zero, then id/doc_id are assumed to uniquely
		identify this tile.

	.. code-block::

		device.beginTile([0, 0, 100, 100], [100, 100, 200, 200], 10, 10, mupdf.Matrix.identity, 0)

.. method:: Device.prototype.endTile()

	Ends the tiling pattern.

	.. code-block::

		device.endTile()

Render flags
^^^^^^^^^^^^

.. method:: Device.prototype.renderFlags(set, clear)

	|only_mutool|

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

	:param Array of string set: Rendering flags to set.
	:param Array of string clear: Rendering flags to clear.

	.. code-block::

		device.renderFlags(["mask", "startcap-undefined"], [])

Device colorspaces
^^^^^^^^^^^^^^^^^^

.. method:: Device.prototype.setDefaultColorSpaces(defaultCS)

	|only_mutool|

	Change the set of default device colorspaces to one given.

	:param DefaultColorSpaces defaultCS: The new set of default colorspaces.

	.. code-block::

		var defaultCS = new DefaultColorSpaces()
		defaultCS.setDefaultRGB(defaultCS.getDefaultGray())
		device.setDefaultColorSpaces(new DefaultColorSpaces())

Layers
^^^^^^

.. method:: Device.prototype.beginLayer(name)

	Begin a marked-content layer with the given name.

	:param string name: Name of this marked-content layer.

	.. code-block::

		device.beginLayer("my tag")

.. method:: Device.prototype.endLayer()

	End a marked-content layer.

	.. code-block::

		device.endLayer()

Structures
^^^^^^^^^^

.. method:: Device.prototype.beginStructure(structure, raw, index)

	|only_mutool|

	Begin a :term:`standard structure type` with the raw tag name and a unique identifier.

	:param string structure: One of the pre-defined structure types in PDF.
	:param string raw: The tag name.
	:param number index: A unique identifier.

	.. code-block::

		device.beginStructure("Document", "my_tag_name", 123)

.. method:: Device.prototype.endStructure()

	|only_mutool|

	End a standard structure element.

	.. code-block::

		device.endStructure()

Metatext
^^^^^^^^

.. method:: Device.prototype.beginMetatext(meta, text)

	|only_mutool|

	Begin meta text where meta is either of:

	- "ActualText"
	- "Alt"
	- "Abbreviation"
	- "Title"

	:param string meta: The meta text type
	:param string text: The text value.

	.. code-block::

		device.beginMetatext("Title", "My title")

.. method:: Device.prototype.endMetatext()

	|only_mutool|

	End meta text information.

	.. code-block::

		device.endMetatext()
