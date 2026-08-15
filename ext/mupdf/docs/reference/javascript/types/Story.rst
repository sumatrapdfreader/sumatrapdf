.. default-domain:: js

.. highlight:: javascript

Story
=====

|only_mutool|

Constructors
------------

.. class:: Story(contents, userCSS, em, archive)

	Create a new story with the given contents, formatted according to the
	provided user-defined CSS and em size, and an archive to lookup images,
	etc.

	:param string contents: HTML source code. If omitted, a basic minimum is generated.
	:param string userCSS: CSS source code. Defaults to the empty string.
	:param number em: The default text font size. Default to 12.
	:param Archive archive: An archive from which to load resources for rendering. Currently supported resource types are images and text fonts. If omitted, the Story will not try to look up any such data and may thus produce incomplete output. Defaults to null.

	.. code-block::

		var story = new mupdf.Story(<contents>, <css>, <em>, <archive>)

Instance methods
----------------

.. method:: Story.prototype.document()

	Return an `DOM` for an unplaced story. This allows adding content before placing the `Story`.

	:returns: `DOM`

	.. code-block::

		var xml = story.document()

.. method:: Story.prototype.place(rect, flags)

	Place (or continue placing) this Story into the supplied rectangle.
	Call `draw()` to draw the placed content before calling `place()` again
	to continue placing remaining content.

	``more`` in the returned object can take either of these values:

	* 0 means that all the text was placed successfully.

	* 1 means that some text was not placed due not fitting within the height of the rectangle.

	* 2 means that some text was not placed due not fitting within the width of the rectangle. For this to be detected ``flags`` must be set to 1.

	:param Rect rect: Rectangle to place the story within.
	:param number flags: When set to 1, will detect when a word does not fit in the rectangle, instead ``more`` set to 2.

	:returns: ``{ filled: Rect, more: number }``

	.. code-block::

		do {
			var result = story.place([0, 0, 100, 100])
			// TODO: create device for this bit of story
			story.draw(device, mupdf.Matrix.identity)
			// TODO: close device
		} while (result.more)

.. method:: Story.prototype.draw(device, transform)

	Draw the placed Story to the given `Device` with the given transform.

	:param Device device: The device
	:param Matrix transform: The transform matrix.

	.. code-block::

		story.draw(device, mupdf.Matrix.identity)
