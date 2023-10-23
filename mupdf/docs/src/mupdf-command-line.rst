.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.

.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub

.. _mupdf_command_line:


:title:`MuPDF` on the command line
==========================================


:title:`MuPDF` has few command line options available:

- :ref:`mupdf-gl<mupdf_command_line_mupdf_gl>` A handy viewer UI.
- :ref:`muraster<mupdf_command_line_muraster>` Can be used to convert :title:`PDF` pages to raster images.
- :ref:`mutool<mupdf_command_line_mutool>` An all purpose tool for dealing with :title:`PDF` files. Used in conjunction with the :ref:`mutool JavaScript API<mupdf_command_line_mutool_js_api>`

.. _mupdf_command_line_mupdf_gl:


:title:`mupdf-gl`
---------------------


The :title:`OpenGL` based viewer can read :title:`PDF`, :title:`XPS`, :title:`CBZ`, :title:`EPUB`, & :title:`FB2` documents as well as typical image formats including :title:`SVG`. It compiles on any platform that has a GLUT_ library. The latest release builds on :title:`Linux`, :title:`Windows`, and :title:`MacOS`.



Command Line Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

   mupdf-gl [options] document [page]



`[options]`


   `-p` password
      The password needed to open a password protected :title:`PDF` file.
   `-c` profile.cc
      Load an ICC profile to use for display (default is sRGB).
   `-r` resolution
      Set the initial zoom level, specified as DPI. The default value is `72`.
   `-W` width
      Set the page width in points for :title:`EPUB` layout.
   `-H` height
      Set the page height in points for :title:`EPUB` layout.
   `-S` size
      Set the default font size in points for :title:`EPUB` layout.
   `-U` stylesheet
      Specify a :title:`CSS` file containing user styles to load for :title:`EPUB` layout.
   `-X`
      Ignore publisher styles for :title:`EPUB` layout.
   `-J`
      Disable :title:`JavaScript` in :title:`PDF` forms.
   `-A` level
      Set anti-aliasing level or pixel rendering rule.

      - `0`: off.
      - `2`: 4 levels.
      - `4`: 16 levels.
      - `8`: 256 levels.
      - `9`: using centre of pixel rule.
      - `10`: using any part of a pixel rule.

   `-I`
      Start in inverted color mode.
   `-B` hex-color
      Set black tint color (default `303030`).
   `-C` hex-color
      Set white tint color (default `FFFFF0`).
   `-Y` factor
      Set UI scaling factor (default calculated from screen DPI).


`[page]`
   The initial page number to show.



**Example usage #1:**

This will open the viewer with a file browser which allows you to choose the file you need.

.. code-block:: bash

   mupdf-gl




**Example usage #1:**

This will open a specific file, in inverted color mode, on page 10.

.. code-block:: bash

   mupdf-gl -I mupdf_explored.pdf 10




Mouse Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The middle mouse button (scroll wheel button) pans the document view.

The right mouse button selects a region and copies the marked text to the clipboard.



Key Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Several commands can take a number argument entered before the key, to modify the command. For example, to zoom to 150 dpi, type "150z".


.. list-table::
   :header-rows: 0

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
     - Save document (only for :title:`PDF`).
   * - **q**
     - Quit viewer.


.. list-table::
   :header-rows: 0

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

   * - **/**
     - Start searching forward.
   * - **?**
     - Start searching backward.
   * - **n**
     - Continue searching forward.
   * - **N**
     - Continue searching backward.



.. _mupdf_command_line_muraster:


:title:`muraster`
---------------------

This is a much simpler version of `mutool draw` command. As such it can be used to quickly rasterize an input file with a set of options.



Command Line Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

   muraster [options] file [pages]

`[options]`

   `-p` password
      The password needed to open a password protected :title:`PDF` file.
   `-o` filename
      The output file name.
   `-F` format
      The output format (default inferred from output file name), e.g. `pam`, `pbm`, `pgm`, `pkm`, `ppm`.

   `-s` information
      Show extra information:
      - `m`: show memory use.
      - `t`: show timings.

   `-R` rotation
      Set a rotation for the output (default `auto`):
      - `auto`.
      - `0`.
      - `90`.
      - `180`.
      - `270`.
      - `clockwise`.


   `-r` x,y
      Comma separated x and y resolution in DPI (default: 300,300).
   `-w` width
      Printable width (in inches) (default: 8.27).
   `-h` height
      Printable height (in inches) (default: 11.69).
   `-f`
      Fit file to page if too large.
   `-B` height
      Set the minimum band height (e.g. 32).
   `-M` memory
      Sets the maximum band memory (e.g. 655360).

   `-W` width
      Page width for EPUB layout.
   `-H` height
      Page height for EPUB layout.
   `-S` size
      Font size for EPUB layout.
   `-U` filename
      File name of user stylesheet for EPUB layout
   `-X`
      Disable document styles for EPUB layout

   `-A` level
      Set anti-aliasing level.

      - `0`: off.
      - `2`: 4 levels.
      - `4`: 16 levels.
      - `8`: 256 levels.

   `-A` graphics level / text level
      Independently set the anti-aliasing levels for graphics and text.

      e.g. `-A 0/4`.


`[pages]`
   A comma separated list of page numbers and ranges.



**Example usage #1:**

This will render a raster file from page one of the input file "mupdf_explored.pdf". The output file will be called "test.ppm", have a clockwise rotation and specific graphics/text anti-aliasing applied.

.. code-block:: bash

   muraster -o test.ppm -R clockwise -A 0/8 mupdf_explored.pdf 1



.. _mupdf_command_line_mutool:

:title:`mutool`
---------------------



.. note::

  It is advised to use `rlwrap`_ with `mutool` for command line history and cursor navigation (this can be also installed via :title:`Homebrew` or :title:`MacPorts`).


.. note::

   Use `mutool -help` for summary usage.


For rendering and converting documents there are three commands available:



   .. toctree::

     mutool-draw.rst

   This is the more customizable tool, but also has a more difficult set of command line options. It is primarily used for rendering a document to image files.


   .. toctree::

     mutool-convert.rst

   This tool is used for converting documents into other formats, and is easier to use.


   .. toctree::

     mutool-trace.rst

   This is a debugging tool used for printing a trace of the graphics device calls on a page.


----


There are also several tools specifically for working with :title:`PDF` files:


   .. toctree::

     mutool-show.rst

   A tool for displaying the internal objects in a :title:`PDF` file.

   .. toctree::

     mutool-extract.rst

   Extract images and embedded font resources.

   .. toctree::

     mutool-clean.rst

   Rewrite :title:`PDF` files. Used to fix broken files, or to make a :title:`PDF` file human editable.

   .. toctree::

     mutool-merge.rst

   Merge pages from multiple input files into a new :title:`PDF`.

   .. toctree::

     mutool-poster.rst

   Divide pages of a :title:`PDF` into pieces that can be printed and merged into a large poster.

   .. toctree::

     mutool-create.rst

   Create a new :title:`PDF` file from a text file with graphics commands.

   .. toctree::

     mutool-sign.rst

   List, verify, and sign digital signatures in :title:`PDF` files.

   .. toctree::

     mutool-info.rst

   Prints details about objects on each page in a :title:`PDF` file.

   .. toctree::

     mutool-pages.rst

   Prints the size and rotation of each page in a PDF. Provides information about :ref:`MediaBox<mutool_trim_defined_boxes>`, :ref:`ArtBox<mutool_trim_defined_boxes>`, etc. for each page in a :title:`PDF` file.

   .. toctree::

     mutool-trim.rst

   This command allows you to make a modified version of a :title:`PDF` with content that falls inside or outside of :ref:`defined boxes<mutool_trim_defined_boxes>` removed.


   .. toctree::

     mutool-run.rst

   A tool for running :title:`JavaScript` programs with access to the :title:`MuPDF` library functions.

.. _mupdf_command_line_mutool_js_api:


   See the :ref:`JavaScript API<mutool_run_javascript_api>` for more.




.. include:: footer.rst



.. External links



.. _GLUT: https://freeglut.sourceforge.net
.. _rlwrap: https://github.com/hanslub42/rlwrap
