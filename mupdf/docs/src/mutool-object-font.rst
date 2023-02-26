.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. _mutool_object_font:



.. _mutool_run_js_api_font:




`Font`
---------------

`Font` objects can be created from :title:`TrueType`, :title:`OpenType`, :title:`Type1` or :title:`CFF` fonts. In :title:`PDF` there are also special :title:`Type3` fonts.


.. method:: new Font(ref)

    *Constructor method*.

    Create a new font, either using a built-in font name or a file name.

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

    :arg ref: Font name or file name.

    :return: `Font`.

    **Example**

    .. code-block:: javascript

        var font = new Font("Times-Roman");



.. method:: getName()

    Get the font name.

    :return: `String`.


.. method:: encodeCharacter(unicode)

    Get the glyph index for a unicode character. Glyph zero (.notdef) is returned if the font does not have a glyph for the character.

    :arg unicode: The unicode character.

    :return: Glyph index.


.. method:: advanceGlyph(glyph, wmode)

    Return advance width for a glyph in either horizontal or vertical writing mode.

    :arg glyph: The glyph.
    :arg wmode: `0` for horizontal writing, and `1` for vertical writing.

    :return: Width for a glyph.


.. method:: isBold()

    Returns *true* if font is bold.

    :return: `Boolean`.

.. method:: isItalic()

    Returns *true* if font is italic.

    :return: `Boolean`.


.. method:: isMono()

    Returns *true* if font is monospaced.

    :return: `Boolean`.


.. method:: isSerif()

    Returns *true* if font is serif.

    :return: `Boolean`.
