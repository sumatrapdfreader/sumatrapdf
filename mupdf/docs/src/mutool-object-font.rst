.. _mutool_object_font:



.. _mutool_run_js_api_font:




`Font`
---------------



`Font` objects can be created from :title:`TrueType`, :title:`OpenType`, :title:`Type1` or :title:`CFF` fonts. In :title:`PDF` there are also special :title:`Type3` fonts.


.. method:: new Font(name)

    *Constructor method*.

    Create a new font from a named built-in font.

    The built-in standard :title:`PDF` fonts are:

    - :title:`Times-Roman`.
    - :title:`Times-Italic`.
    - :title:`Times-Bold`.
    - :title:`Times-BoldItalic`.
    - :title:`Helvetica`.
    - :title:`Helvetica-Oblique`.
    - :title:`Helvetica-Bold`.
    - :title:`Helvetica-BoldOblique`.
    - :title:`Courier`.
    - :title:`Courier-Oblique`.
    - :title:`Courier-Bold`.
    - :title:`Courier-BoldOblique`.
    - :title:`Symbol`.
    - :title:`ZapfDingbats`.

    The built-in CJK fonts are referenced by language code: `zh-Hant`, `zh-Hans`, `ja`, `ko`.

    :arg name: `String` Font name.

    :return: `Font`.

    |example_tag|

    .. code-block:: javascript

        var times = new mupdf.Font("Times-Roman")

.. method:: new Font(name, buffer, index)

    *Constructor method*.

    Create a new font from a buffer.

    :arg name: `String` Name given to loaded font.
    :arg buffer: `Buffer` A buffer containing font data.
    :arg index: `Integer` Subfont index (only used for TTC fonts).

    :return: `Font`.

    |example_tag|

    .. code-block:: javascript

        var font = new mupdf.Font("Comic Sans", "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf")

.. method:: new Font(name, filename, index)

    |mutool_tag|

    *Constructor method*.

    Create a new font, a font file.

    :arg name: `String` Name given to loaded font.
    :arg filename: `String` Name of file containing font data.
    :arg index: `Integer` Subfont index (only used for TTC fonts).

    :return: `Font`.

    |example_tag|

    .. code-block:: javascript

        var font = new mupdf.Font("Comic Sans", "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf")



|instance_methods|


.. method:: getName()

    Get the font name.

    :return: `String`.

    |example_tag|

    .. code-block:: javascript

        var name = font.getName();


.. method:: encodeCharacter(unicode)

    Get the glyph index for a unicode character. Glyph zero (.notdef) is returned if the font does not have a glyph for the character.

    :arg unicode: The unicode character.

    :return: Glyph index.

    |example_tag|

    .. code-block:: javascript

        var index = font.encodeCharacter(0x42);


.. method:: advanceGlyph(glyph, wmode)

    Return advance width for a glyph in either horizontal or vertical writing mode.

    :arg glyph: The glyph as unicode character.
    :arg wmode: `0` for horizontal writing, and `1` for vertical writing.

    :return: Width for the glyph.

    |example_tag|

    .. code-block:: javascript

        var width = font.advanceGlyph(0x42, 0);


.. method:: isBold()

    Returns *true* if font is bold.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isBold = font.isBold();


.. method:: isItalic()

    Returns *true* if font is italic.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isItalic = font.isItalic();


.. method:: isMono()

    Returns *true* if font is monospaced.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isMono = font.isMono();


.. method:: isSerif()

    Returns *true* if font is serif.

    :return: `Boolean`.

    |example_tag|

    .. code-block:: javascript

        var isSerif = font.isSerif();
