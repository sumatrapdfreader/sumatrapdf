.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


Quick Start Guide
===============================





.. _quick_start_acquire_source_code:

Get the :title:`MuPDF` source code
-------------------------------------

There are a few options for acquiring the source code as follows:

- Download an official :title:`MuPDF` release here: `MuPDF Releases`_.
- Check out the :title:`Github` repository here: `MuPDF on Github`_.
- Or check out the latest development source directly from our canonical git repository: `git clone --recursive git://git.ghostscript.com/mupdf.git`.


.. note::

      If you have checked out a :title:`git repo` then in the "mupdf" directory, update the third party libraries with: `git submodule update --init`.


.. _quick_start_building_the_library:

Building the library
-------------------------


:title:`Windows`
~~~~~~~~~~~~~~~~~~~~

On :title:`Windows` there is a :title:`Visual Studio` solution file in `platform/win32/mupdf.sln`.


**Using Microsoft Visual Studio**


To build the required DLLs, load `platform/win32/mupdf.sln` into :title:`Visual Studio`, and select the required architecture from the drop down - then right click on `libmupdf` in the solution explorer and choose “Build”.


:title:`macOS`
~~~~~~~~~~~~~~~~~~~~


Build for :title:`macOS` with the following :title:`Terminal` command:


.. code-block:: bash

   make prefix=/usr/local install


This will then install the viewer, command line tools, libraries, and header files on your system.




:title:`Linux`
~~~~~~~~~~~~~~~~~~~~


You will also need the X11 headers and libraries if you're building on :title:`Linux`. These can typically be found in the xorg-dev package.

Alternatively, if you only want the command line tools, you can build with `HAVE_X11=no`.

The new :title:`OpenGL` based viewer also needs :title:`OpenGL` headers and libraries. If you're building on :title:`Linux`, install the `mesa-common-dev`, `libgl1-mesa-dev` packages, and `libglu1-mesa-dev` packages. You'll also need several X11 development packages: `xorg-dev`, `libxcursor-dev`, `libxrandr-dev`, and `libxinerama-dev`. To skip building the :title:`OpenGL` viewer, build with `HAVE_GLUT=no`.

To install the viewer, command line tools, libraries, and header files on your system:


.. code-block:: bash

   make prefix=/usr/local install


To install only the command line tools, libraries, and headers invoke `make` like this:

.. code-block:: bash

   make HAVE_X11=no HAVE_GLUT=no prefix=/usr/local install


----


.. note::

   Results of the build can be found in `build/release` in your "mupdf" folder.



Validating your installation
--------------------------------

From the command line you should now have the following :title:`MuPDF` commands available:

- :ref:`mupdf-gl<mupdf_command_line_mupdf_gl>` A handy viewer UI.
- :ref:`muraster<mupdf_command_line_muraster>` Can be used to convert PDF pages to raster images.
- :ref:`mutool<mupdf_command_line_mutool>` An all purpose tool for dealing with PDF files.


.. _supported_file_formats:

Supported file formats
--------------------------------


.. _supported_document_formats:
.. _supported_image_formats:


:title:`MuPDF` supports the following file formats:

`pdf`, `epub`, `xps`, `cbz`, `mobi`, `fb2`, `svg`

And a suite of image types, e.g. `png`, `jpg`, `bmp` etc.







.. include:: footer.rst



.. External links

.. _MuPDF Releases: https://mupdf.com/releases/index.html?utm_source=rtd-mupdf&utm_medium=rtd&utm_content=inline-link
.. _MuPDF on Github: https://github.com/ArtifexSoftware/mupdf
.. _GNU website: https://www.gnu.org/software/automake/
