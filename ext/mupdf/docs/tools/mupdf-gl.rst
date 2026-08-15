mupdf-gl
========

The mupdf-gl viewer can show PDF, XPS, CBZ, EPUB, & FB2 documents as well as typical image formats including SVG.

Command Line Options
--------------------

.. code-block::

	mupdf-gl [options] document [page]

``[options]``
	The options are:

	``-p`` password
		The password needed to open a password protected PDF file.
	``-c`` profile.cc
		Load an ICC profile to use for display (default is sRGB).
	``-r`` resolution
		Set the initial zoom level, specified as DPI. The default value is ``72``.
	``-W`` width
		Set the page width in points for EPUB layout.
	``-H`` height
		Set the page height in points for EPUB layout.
	``-S`` size
		Set the default font size in points for EPUB layout.
	``-U`` stylesheet
		Specify a CSS file containing user styles to load for EPUB layout.
	``-X``
		Ignore publisher styles for EPUB layout.
	``-J``
		Disable Javascript in PDF forms.
	``-A`` level
		Set anti-aliasing level or pixel rendering rule.
		- ``0``: off.
		- ``2``: 4 levels.
		- ``4``: 16 levels.
		- ``8``: 256 levels.
		- ``9``: using centre of pixel rule.
		- ``10``: using any part of a pixel rule.

	``-I``
		Start in inverted color mode.
	``-B`` hex-color
		Set black tint color (default ``303030``).
	``-C`` hex-color
		Set white tint color (default ``FFFFF0``).
	``-Y`` factor
		Set UI scaling factor (default calculated from screen DPI).

``[page]``
	The initial page number to show.

Examples
--------

To open the viewer with a file browser, just start it:

.. code-block:: bash

	mupdf-gl

To open a specific file, in inverted color mode, on page 10:

.. code-block:: bash

	mupdf-gl -I mupdf_explored.pdf 10

Mouse Bindings
--------------

The middle mouse button (scroll wheel button) pans the document view.

The right mouse button selects a region and copies the marked text to the clipboard.

Key Bindings
------------

Several key bindings can take a number argument entered before the key, to modify the command.
For example, to zoom to 150 dpi, type "150z".

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **F1**
	  - Display help.
	* - **i**
	  - Show document information.
	* - **o**
	  - Show document outline.
	* - **L**
	  - Highlight links.
	* - **F**
	  - Highlight form fields.
	* - **a**
	  - Show annotation editor.
	* - **r**
	  - Reload document.
	* - **S**
	  - Save document (only for PDF).
	* - **q**
	  - Quit viewer.

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **<**
	  - Decrease E-book font size.
	* - **>**
	  - Increase E-book font size.
	* - **I**
	  - Toggle inverted color mode.
	* - **C**
	  - Toggle tinted color mode.
	* - **E**
	  - Toggle ICC color management.
	* - **e**
	  - Toggle spot color emulation.
	* - **A**
	  - Toggle anti-aliasing.

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **f**
	  - Toggle fullscreen.
	* - **w**
	  - Shrinkwrap window to fit page.
	* - **W**
	  - Fit page width to window.
	* - **H**
	  - Fit page height to window.
	* - **Z**
	  - Fit page size to window.
	* - **[number] z**
	  - Set zoom resolution in DPI.
	* - **+**
	  - Zoom in.
	* - **-**
	  - Zoom out.
	* - **[**
	  - Rotate counter-clockwise.
	* - **]**
	  - Rotate clockwise.
	* - **⬅️➡️⬆️⬇️** or **h**, **j**, **k**, **l**
	  - Pan page in small increments.

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **b**
	  - Smart move one screenful backward.
	* - **[space]**
	  - Smart move one screenful forward.
	* - **[comma]** or **[page up]**
	  - Go one page backward.
	* - **[period]** or **[page down]**
	  - Go one page forward.
	* - **[number] g**
	  - Go to page number.
	* - **G**
	  - Go to last page.

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **m**
	  - Save current page to navigation history.
	* - **t**
	  - Go back in navigation history.
	* - **T**
	  - Go forward in navigation history.
	* - **[number] m**
	  - Save current page in numbered bookmark.
	* - **[number] t**
	  - Go to numbered bookmark.

.. list-table::
	:header-rows: 0
	:align: left
	:width: 80%
	:widths: auto

	* - **/**
	  - Start searching forward.
	* - **?**
	  - Start searching backward.
	* - **n**
	  - Continue searching forward.
	* - **N**
	  - Continue searching backward.
