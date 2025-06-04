# What is MuPDF?

MuPDF is an open source software framework for viewing, converting, and
manipulating PDF, XPS, and E-book documents. There are viewers for various
platforms, several command line tools, and a software library for building
tools and applications.

## Formats

As you can tell by the name, we support reading PDF files. But that's not all!

- PDF
- XPS and OpenXPS
- EPUB (DRM-free 2.0, limited support for 3.0)
- Mobipocket (MOBI)
- FictionBook 2 (FB2)
- ComicBook (CBZ and CBT)
- Images (TIFF, JPEG, PNG, etc)
- SVG (a limited subset only)

## Viewers

There are many different viewer applications that build on the MuPDF library
for a large variety of platforms.

### Linux, Windows, and BSD

For Linux and Windows there are two viewer applications.
The main viewer (mupdf-gl) has many features such as a table of contents
sidebar, full unicode search, annotation editing, redaction, etc.
On systems where this viewer cannot be built, we still support the older
legacy viewer which does not have as many features.

<dl>
<dt><a href="../tools/mupdf-gl.html">mupdf-gl</a>
<dd>The main viewer program that sports the most features.
<dt>mupdf-x11
<dd>The legacy X11 viewer that works everywhere.
<dt>mupdf-win32
<dd>The legacy win32 viewer for Windows.
</dl>

### Android

Android currently has two different viewers with varying degrees of complexity:

<dl>
<dt><a href="https://play.google.com/store/apps/details?id=com.artifex.mupdf.viewer.app">MuPDF viewer</a>
<dd>A high performance PDF viewer with a smooth and polished interface.
<dt><a href="https://play.google.com/store/apps/details?id=com.artifex.mupdf.mini.app">MuPDF mini</a>
<dd>An example of how to create a PDF viewer with the least amount of code.
</dl>

### Web Browser

There's a simple
<a href="https://mupdf.com/wasm/demo/?file=/docs/mupdf_explored.pdf">WebAssembly demo</a>
that runs MuPDF in the browser.

There's also a commercial license only <a href="https://webviewer.mupdf.com/">MuPDF WebViewer</a> product.

### Third party viewers

Here's a non-exhaustive list of other non-affiliated open source projects that use MuPDF:

- <a href="https://www.sumatrapdfreader.org/download-free-pdf-viewer">SumatraPDF</a> for Windows
- <a href="https://pwmt.org/projects/zathura/">Zathura</a> for Linux
- <a href="https://repo.or.cz/llpp.git">llpp</a>

## Command Line

The command line tools are all gathered into one umbrella command: mutool.
This swiss army knife has a lot of sub-commands for performing different
tasks on PDF documents.

For rendering and converting documents use these two commands:

<dl>
<dt><a href="../tools/mutool-draw.html">mutool draw</a>
<dd>This is the more customizable tool, but also has a more difficult set of command line options.
It is primarily used for rendering a document to image files.
<dt><a href="../tools/mutool-convert.html">mutool convert</a>
<dd>This tool is used for converting documents into other formats, and is easier to use.
</dl>

A highlight of some other tools useful for working with PDF documents:

<dl>
<dt><a href="../tools/mutool-show.html">mutool show</a>
<dd>A tool for displaying the internal objects in a PDF file.
<dt><a href="../tools/mutool-extract.html">mutool extract</a>
<dd>Extract images and embedded font resources.
<dt><a href="../tools/mutool-clean.html">mutool clean</a>
<dd>Rewrite PDF file. Used to fix broken files, or to make a PDF file human editable.
<dt><a href="../tools/mutool-create.html">mutool create</a>
<dd>Create a new PDF file from a text file with graphics commands.
<dt><a href="../tools/mutool-merge.html">mutool merge</a>
<dd>Merge pages from multiple input files into a new PDF.
<dt><a href="../tools/mutool-poster.html">mutool poster</a>
<dd>Divide pages of a PDF into pieces that can be printed and merged into a large poster.
</dl>

And finally, there is a tool for doing anything you can imagine:

<dl>
<dt><a href="../tools/mutool-run.html">mutool run</a>
<dd>A tool for running Javascript programs with access to the MuPDF library functions.
</dl>

## Library

The library is written in portable C.

To learn more about the C interface, read the <a href="../cookbook/mupdf-explored.html">MuPDF Explored</a> book.

### Javascript

There is a library available to use MuPDF from Javascript and Typescript,
powered by WebAssembly. You can use this library to build applications that run
in a web browser, or that run server side using Node or Bun.

The Javascript library is available as a module on NPM.

### Java

There is also a Java library, which uses JNI to provide access to the C library.

The Java classes provide an interface very similar to that available in the
<a href="../tools/mutool-run.html">mutool run</a> command line tool.
This Java library also powers the Android viewers.

If you want to build an application for Android, you have several options. You
can base it off one of the existing viewers, or build a new app using the Java
library directly.

### Python

The popular [PyMuPDF](https://pypi.org/project/PyMuPDF/) library makes it trivial to use MuPDF from Python!
