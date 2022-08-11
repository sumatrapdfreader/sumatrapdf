.. toctree::
   :maxdepth: 4


..
    We define crude substitutions that implement simple expand/contract blocks
    in html. Unfortunately it doesn't seem possible to pass parameters to
    substitutions so we can't specify text to be shown next to html's details
    triangle.

.. |expand_begin| raw:: html

    <details>
    <summary><strong>Show/hide</strong></summary>

.. |expand_end| raw:: html

    </details>


==============
MuPDF Bindings
==============


.. toctree::
   :maxdepth: 4


Overview
========


Auto-generated C++, Python and C# versions of the MuPDF C API are available.

These APIs are currently a beta release and liable to change.

The C++ MuPDF API
=================

* Auto-generated from the MuPDF C API's header files.

* Everything is in C++ namespace `mupdf`.

* Provides C++ functions that wrap most `fz_*()` and `pdf_*()` functions.

  * Functions and methods do not take `fz_context`
    arguments. (Automatically-generated per-thread contexts are used internally.)

  * MuPDF `setjmp()`/`longjmp()`-based exceptions are converted into C++ exceptions.

* Provides C++ classes that wrap most `fz_*` and `pdf_*` structs.

  * These C++ wrapper classes automatically handle reference counting of the
    underlying C structs. So there is no need for manual calls to `fz_keep_*()`
    and `fz_drop_*()`, and class instances can be treated as values and copied
    arbitrarily.

  * Class methods provide access to most of the underlying C API.

* Provides a small number of extensions beyond the basic C API:

  * Some generated classes have extra  `begin()` and `end()` methods to allow standard C++ iteration:
    |expand_begin|
    ::
        #include "mupdf/classes.h"
        #include "mupdf/functions.h"

        #include <iostream>

        void show_stext(mupdf::StextPage& page)
        {
            for (mupdf::StextPage::iterator it_page: page)
            {
                mupdf::StextBlock block = *it_page;
                for (mupdf::StextBlock::iterator it_block: block)
                {
                    mupdf::StextLine line = *it_block;
                    for (mupdf::StextLine::iterator it_line: line)
                    {
                        mupdf::StextChar stextchar = *it_line;
                        fz_stext_char* c = stextchar.m_internal;
                        using namespace mupdf;
                        std::cout << "StextChar("
                                << "c=" << c->c
                                << " color=" << c->color
                                << " origin=" << c->origin
                                << " quad=" << c->quad
                                << " size=" << c->size
                                << " font_name=" << c->font->name
                                << "\n";
                    }
                }
            }
        }
    |expand_end|

  * Some custom class methods and constructors.

  * Functions for generating a text representation of some simple 'POD' structs.

    For example for `fz_rect` we provide these functions::

        std::ostream& operator<< (std::ostream& out, const fz_rect& rhs);
        std::ostream& operator<< (std::ostream& out, const Rect& rhs);
        std::string to_string_fz_rect(const fz_rect& s);
        std::string to_string(const fz_rect& s);
        std::string Rect::to_string() const;

    These each generate text such as: `(x0=90.51 y0=160.65 x1=501.39 y1=1215.6)`

The Python and C# MuPDF APIs
============================

* A python module called `mupdf`.
* A C# namespace called `mupdf`.

  * C# bindings are experimental as of 2021-10-14.
* Generated from the C++ MuPDF API's header files, so inherit the abstractions of the C++ API:

  * No `fz_context` arguments.
  * Automatic reference counting, so no need to call `fz_keep_*()` or `fz_drop_*()`, and we have value-semantics for class instances.
  * Native Python and C# exceptions.
* Output parameters are returned as tuples.
* Allows implementation of mutool in Python - see `mupdf:scripts/mutool.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=mupdf:scripts/mutool.py>`_
  and `mupdf:scripts/mutool_draw.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=mupdf:scripts/mutool_draw.py>`_.

* Provides text representation of simple 'POD' structs::

    rect = mupdf.Rect(...)
    print(rect) # Will output text such as: (x0=90.51 y0=160.65 x1=501.39 y1=215.6)

  * This works for classes where the C++ API defines a `to_string()` method as described above.

    * Python classes will have a `__str__()` method.
    * C# classes will have a `ToString()` method.

* Uses SWIG Director classes to allow C function pointers in MuPDF structs to call Python code.

  * This has not been tested on C#.

Installing the Python mupdf module using `pip`
==============================================

The Python `mupdf` module is available on the `Python Package Index (PyPI) website <https://pypi.org/>`_.

* Install with: `pip install mupdf`
* Pre-built Wheels (binary Python packages) are provided for Windows and Linux.
* For more information on the latest release, see changelog below and: https://pypi.org/project/mupdf/

Doxygen/Pydoc API documentation
===============================

Auto-generated documentation for the C, C++ and Python APIs is available at:
https://ghostscript.com/~julian/mupdf-bindings/

* All content is generated from the comments in MuPDF header files.

* This documentation is generated from an internal development tree, so may
  contain features that are not yet publicly available.

* It is updated only intermittently.

Example client code
===================

Using the Python API
--------------------

Minimal Python code that uses the `mupdf` module::

    import mupdf
    document = mupdf.Document('foo.pdf')

A simple example Python test script (run by `scripts/mupdfwrap.py -t`) is:

* `scripts/mupdfwrap_test.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=scripts/mupdfwrap_test.py>`_

More detailed usage of the Python API can be found in:

* `scripts/mutool.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=scripts/mutool.py>`_
* `scripts/mutool_draw.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=scripts/mutool_draw.py>`_


**Example Python code that shows all available information about a document's Stext blocks, lines and characters.**

  |expand_begin|
  ::
    #!/usr/bin/env python3

    import mupdf

    def show_stext(document):
        '''
        Shows all available information about Stext blocks, lines and characters.
        '''
        for p in range(document.count_pages()):
            page = document.load_page(p)
            stextpage = mupdf.StextPage(page, mupdf.StextOptions())
            for block in stextpage:
                block_ = block.m_internal
                log(f'block: type={block_.type} bbox={block_.bbox}')
                for line in block:
                    line_ = line.m_internal
                    log(f'    line: wmode={line_.wmode}'
                            + f' dir={line_.dir}'
                            + f' bbox={line_.bbox}'
                            )
                    for char in line:
                        char_ = char.m_internal
                        log(f'        char: {chr(char_.c)!r} c={char_.c:4} color={char_.color}'
                                + f' origin={char_.origin}'
                                + f' quad={char_.quad}'
                                + f' size={char_.size:6.2f}'
                                + f' font=('
                                    +  f'is_mono={char_.font.flags.is_mono}'
                                    + f' is_bold={char_.font.flags.is_bold}'
                                    + f' is_italic={char_.font.flags.is_italic}'
                                    + f' ft_substitute={char_.font.flags.ft_substitute}'
                                    + f' ft_stretch={char_.font.flags.ft_stretch}'
                                    + f' fake_bold={char_.font.flags.fake_bold}'
                                    + f' fake_italic={char_.font.flags.fake_italic}'
                                    + f' has_opentype={char_.font.flags.has_opentype}'
                                    + f' invalid_bbox={char_.font.flags.invalid_bbox}'
                                    + f' name={char_.font.name}'
                                    + f')'
                                )

    document = mupdf.Document('foo.pdf')
    show_stext(document)
  |expand_end|

Basic PDF viewers written in Python and C#
------------------------------------------

* `scripts/mupdfwrap_gui.py <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=scripts/mupdfwrap_gui.py>`_
* `scripts/mupdfwrap_gui.cs <https://git.ghostscript.com/?p=mupdf.git;a=blob;f=scripts/mupdfwrap_gui.cs>`_
* Build and run with:

  * `./scripts/mupdfwrap.py -b all --test-python-gui`
  * `./scripts/mupdfwrap.py -b --csharp all --test-csharp-gui`

Changelog
=========

[Note that this is only for changes to the generation of the C++/Python/C#
APIs; changes to the main MuPDF API are not detailed here.]

* **2022-5-11**: Documented the experimental C# API.

* **2022-3-26**: New release of Python package **mupdf-1.19.0.20220326.1214**
  (from **mupdf-1.19.0** git 466e06fc7e01), with pre-built Wheels for Windows and
  Linux. See: https://pypi.org/project/mupdf/

  * Fixed SWIG Directors wrapping classes on Windows.


* **2022-3-23**: New release of Python package **mupdf-1.19.0.20220323.1255** (from
  **mupdf-1.19.0** git 58e2b82bf7d1e7), with pre-built Wheels for Windows and
  Linux. See: https://pypi.org/project/mupdf

  Details
  |expand_begin|

  * Use SWIG Director classes to support MuPDF structs that contain fn
    pointers. This allows MuPDF to call Python callback code. [.line-through]#Only
    available on Unix at the moment.#

    * This allows us to provide Python wrappers for `fz_set_warning_callback()`
      and `fz_set_error_callback()`.

  * Added alternative wrappers for MuPDF functions in the form of free-standing
    functions that operate on our wrapper classes. Useful when porting existing
    code to Python, and generally as a non-class-based API that still gives
    automatic handling of reference counting. New functions have same name as
    underlying MuPDF function with a `m` prefix; they do not take a `fz_context`
    arg and take/return references to wrapper classes instead of pointers to MuPDF
    structs.

    * Class methods now call these new free-standing wrappers.

  * Various improvements to enums and non-copyable class wrappers.

  * Use `/** ... */` comments in generated code so visible to Doxygen.

  * Improvements to and fixes to reference counting.

    * Use MuPDF naming conventions for detection of MuPDF functions that return
      borrowed references.

    * Improved detection of whether a MuPDF struct uses reference counting.

    * Fixed some reference counting issues when handling out-params.

  * Added optional runtime ref count checking.

  * For fns that return raw unsigned char array, provide C++ wrappers that
    return a `std::vector<unsigned char>`. This works much better with SWIG.

  * Allow construction of `Document` from `PdfDocument`.

  * Allow writes to `PdfWriteOptions::opwd_utf8` and
    `PdfWriteOptions::upwd_utf8`.

  * Added `Page::doc()` to return wrapper for `.doc` member.

  * Added `PdfPage::super()` to return `Page` wrapper for `.super`.

  * Added `PdfDocument::doc()` to return wrapper for `.doc` member.

  * Added `PdfObj::obj()` to return wrapper for `.obj` member.

  * Made Python wrappers for `fz_fill_text()` take Python tuple/list for `float*
    color` arg.

  * Improved wrapping of `pdf_lexbuf`.

  * Added `Page` downcast constructor from `PdfPage`.

  * Expose `pdf_widget_type` enum.

  * Improved python bindings for `*dict_getl()` and `*dict_putl()`. We now also
    provide `mpdf_dict_getl()` etc handling variable number of args.

  * Improvements to wrapping of `pdf_filter_options`, `pdf_redact_options`,
    `fz_pixmap`, `pdf_set_annot_color`, `pdf_obj`.

  * Allow direct use of `PDF_ENUM_NAME_*` enums as `PdfObj`'s in Python.

  * Added wrappers for `pdf_annot_type()` and `pdf_string_from_annot_type()`.

  * `Buffer.buffer_storage()` raises an exception with useful error info (it is
    not possible to use it from SWIG bindings).

  * Added various fns to give Python access to some raw pointer values, e.g. for
    passing to `mupdf.new_buffer_from_copied_data()`.

  * Avoid excluding class method wrappers for `pdf_*()` fns in python.
  |expand_end|

* **2022-02-05**: Uploaded Doxygen/Pydoc documentation for the C, C++ and Python
  APIs, from latest development tree.

* **2021-09-29**: Released Python bindings for **mupdf-1.19.0** (git 61b63d734a7)
  to pypi.org (**mupdf 1.19.0.20210929.1226**) with pre-built Wheels for Windows
  and Linux.

* **2021-08-05**: Released Python package **mupdf-1.18.0.20210805.1716** on
  pypi.org with pre-built Wheels for Windows and Linux.

  * Improved constructors of `fz_document_writer` wrapper class
    `DocumentWriter`.

  * Fixed `operator<<` for POD C structs - moved from `mupdf` namespace to
    top-level.

  * Added `scripts/mupdfwrap_gui.py` - a simple demo Python PDF viewer.

  * Cope with `fz_paint_shade()`'s new `fz_shade_color_cache **cache` arg.

* **2021-05-21**: First release of Python package, **mupdf-1.18.0.20210521.1738**,
  on pypi.org with pre-built Wheels for Windows and Linux.

  Details
  |expand_begin|
  * Changes that apply to both C++ and Python bindings:

    * Improved access to metadata - added `Document::lookup_metadata()`
      overload that returns a `std::string`. Also provided `extern const
      std::vector<std::string> metadata_keys;` containing a list of the supported
      keys.

    * Iterating over `Outline`'s now returns `OutlineIterator` objects so that
      depth information is also available.

    * Fixed a reference-counting bug in iterators.

    * `Page::search_page()` now returns a `std::vector<Quad>`.

    * `PdfDocument` now has a default constructor which uses
      `pdf_create_document()`.

    * Include wrappers for functions that return `fz_outline*`, e.g. `Outline
      Document::load_outline();`.

    * Removed potentially slow call of `getenv("MUPDF_trace")` in every C++
      wrapper function.

    * Removed special-case naming of wrappers for `fz_run_page()` - they are now
      called `mupdf::run_page()` and `mupdf::Page::run_page()`, not `mupdf::run()`
      etc.

    * Added text representation of POD structs.

    * Added support for 32 and 64-bit Windows.
    * Many improvements to C++ and Python code generation.

  * Changes that apply only to Python:

    * Improved handling of out-parameters:

      * If a function or method has out-parameters we now systematically return a
        Python tuple containing any return value followed by the out-parameters.

      * Don't treat `FILE*` or pointer-to-const as an out-parameter.

    * Added methods for getting the content of a `mupdf.Buffer` as a Python
      `bytes` instance.

    * Added Python access to nested unions in `fz_stext_block` wrapper class
      `mupdf.StextBlock`.

    * Allow the MuPDF Python bindings to be installed with `pip`.

      * This uses a source distribution of mupdf that has been uploaded to
        `pypi.org` in the normal way.

      * Installation involves compiling the C, C++ and Python bindings so will
        take a few minutes. It requires SWIG to be installed.

      * Pre-built wheels are not currently provided.

    * Write generated C++ information into Python pickle files to allow building
      on systems without clang-python.

    * Various changes to allow building in Python "Manylinux" containers.

    * Allow Python access to nested unions in `fz_stext_block` wrapper. SWIG
      doesn't handle nested unions so instead we provide accessor methods in our
      generated C++ class.

    * Added accessors to `fz_image`'s wrapper class.

    * Improved generated accessor methods - e.g. ignore functions and function
      pointers and return `int` instead of `int8_t` to avoid SWIG getting confused.
  |expand_end|

* **2020-10-07**: Experimental release of C++ and Python bindings in MuPDF-1.18.0.


Building the C++, Python and C# MuPDF APIs
==========================================

Setup
-----

General requirements
^^^^^^^^^^^^^^^^^^^^

* Linux, Windows or OpenBSD.

* Python development libraries.

* Python package `libclang` - a Python interface onto the libclang C/C++ parser.

* SWIG version 3 or 4.


Windows
^^^^^^^

Install Python using the Python Windows installer from the python.org website:

* http://www.python.org/downloads

Notes about other Python installers:

* Don't use other installers such as the Microsoft Store Python package.

* If Microsoft Store Python is already installed, leave it in place and install
  from python.org on top of it - uninstalling before running the python.org
  installer has been known to cause problems.

Installing with the Python Windows installer from python.org:

* A default installation is sufficient.

* If "Customize Installation" is chosen, make sure to include "py launcher" so
  that the `py` command will be available.

* Also see: https://docs.python.org/3/using/windows.html

Other:

* Run: `pip install libclang`

* We look for `devenv.com` in some hard-coded locations, which can be overriden
  with:

  * `scripts/mupdfwrap.py -b --devenv <devenv.com-location> ...`

* Run `scripts/mupdfwrap.py` with `--swig-windows-auto` so that we
  automatically download swig to local directory if not already present, and use
  it directly.


Linux
^^^^^

(Debian-specific; similar packages exist on other distributions.)

* `sudo apt install python3-dev swig python-clang`

Notes:

* One can do `pip install libclang` instead of installing the `python-clang`
  package in the above command.

* `pip install clang` does not provide a usable Python interface onto the clang
  parser.


OpenBSD
^^^^^^^

* `sudo pkg_add python py3-llvm swig`


Doing a build
-------------

Build MuPDF shared library, C++ and Python MuPDF APIs, and run basic tests::

    git clone --recursive git://git.ghostscript.com/mupdf.git
    cd mupdf
    ./scripts/mupdfwrap.py -b all --test-python
    ./scripts/mupdfwrap.py -b all --test-python-gui

As above but do a debug build::

    ./scripts/mupdfwrap.py -d build/shared-debug -b all --test-python

C# build and tests::

    ./scripts/mupdfwrap.py -b --csharp all --test-csharp
    ./scripts/mupdfwrap.py -b --csharp all --test-csharp-gui

For more information:

* Run `./scripts/mupdfwrap.py -h`.
* Read the doc-string at beginning of `+scripts/wrap/__main__.py+`.

How building the APIs works
---------------------------

Building the MuPDF shared library
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* On Unix, runs `make` on MuPDF's Makefile.
* On Windows, runs `devenv.com` on `.sln` and `.vcxproj` files within `platform/win32/ <https://git.ghostscript.com/?p=mupdf.git;a=tree;f=platform/win32>`_.

Generation of the C++ MuPDF API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Uses clang-python to parse MuPDF's C API.

* Generates C++ code that wraps the basic C interface, converting MuPDF
  `setjmp()`/`longjmp()` exceptions into C++ exceptions and automatically
  handling ``fz_context``'s internally.

* Generates C++ classes for each `fz_*` and `pdf_*` struct, and uses various
  heuristics to define constructors, methods and static methods that call
  `fz_*()` and `pdf_*()` functions. These classes' constructors and destructors
  automatically handle reference counting so class instances can be copied
  arbitrarily.

* C header file comments are copied into the generated C++ header files.

* Compile and link the generated C++ code to create shared libraries.


Generation of the Python and C# MuPDF APIs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Uses SWIG to parse the previously-generated C++ headers and generate C++,
  Python and C# code.

* Defines some custom-written Python and C# functions and methods, e.g. so that
  out-params are returned as tuples.

* If SWIG is version 4+, C++ comments are converted into Python doc-comments.

* Compile and link the SWIG-generated C++ code to create shared libraries.


Building auto-generated MuPDF API documentation
-----------------------------------------------

Build HTML documentation for the C, C++ and Python APIs (using Doxygen and pydoc)::

    ./scripts/mupdfwrap.py --doc all

This will generate the following tree::

    mupdf/docs/generated/
        index.html
        c/
        c++/
        python/

All content is ultimately generated from the MuPDF C header file comments.

As of 2022-2-5, it looks like `swig -doxygen` (swig-4.02) ignores single-line `/** ... */` comments, so the generated Python code (and hence also Pydoc documentation) is missing information.

Generated files
---------------

File required at runtime are created in `mupdf/build/shared-<build>/`.

Other intermediate generated files are created in `mupdf/platform/`

**Details**
|expand_begin|
::
    mupdf/
        build/
            shared-release/    [Unix runtime files.]
                libmupdf.so    [MuPDF C API.]
                libmupdfcpp.so [MuPDF C++ API.]
                mupdf.py       [MuPDF Python API.]
                _mupdf.so      [MuPDF Python API internals.]
                mupdf.cs       [MuPDF C# API.]
                mupdfcsharp.so [MuPDF C# API internals.]

            shared-debug/
                [as shared-release but debug build.]

            shared-release-x32-py3.8/   [Windows runtime files.]
                mupdfcpp.dll            [MuPDF C and C++ API.]
                mupdf.py                [MuPDF Python API.]
                _mupdf.pyd              [MuPDF Python API internals.]
                mupdf.cs                [MuPDF C# API.]
                mupdfcsharp.dll         [MuPDF C# API internals.]

        platform/
            c++/
                include/    [MuPDF C++ API header files.]
                    mupdf/
                        classes.h
                        classes2.h
                        exceptions.h
                        functions.h
                        internal.h

                implementation/  [MuPDF C++ implementation source files.]
                    classes.cpp
                    classes2.cpp
                    exceptions.cpp
                    functions.cpp
                    internal.cpp

                generated.pickle    [Information from clang parse step, used by later stages.]
                windows_mupdf.def   [List of MuPDF public global data, used when linking mupdfcpp.dll.]

            python/ [SWIG Python input/output files.]
                mupdfcpp_swig.cpp
                mupdfcpp_swig.i

            csharp/  [SWIG C# input/output files.]
                mupdf.cs
                mupdfcpp_swig.cpp
                mupdfcpp_swig.i
|expand_end|


C++ bindings details
====================

Class-based API overview
------------------------

Class wrappers are defined for each MuPDF struct.

* These classes are defined in: `mupdf/platform/c++/include/mupdf/classes.h`

MuPDF functions that take a pointer to a MuPDF struct as their first arg
(ignoring any initial `fz_context*` arg), are usually available as a method of
the corresponding wrapper class.

Args that are pointers to a MuPDF struct will be changed to take a reference to
the corresponding wrapper class.

In addition there will be a non-member function called `mupdf::m<fnname>()`
which provides exactly the same functionality, taking a reference to the
wrapper class as an explicit first arg called `self`.

* These non-member functions are declared in:
  `mupdf/platform/c++/include/mupdf/classes2.h`


Details
^^^^^^^

* Class wrappers for a MuPDF struct `fz_bar` are called `mupdf::Bar`.

* Class wrappers for a MuPDF struct `pdf_bar` are called `mupdf::PdfBar`.

* All C++ functions omit any `fz_context*` arg.

* All C++ functions convert MuPDF exceptions into C++ exceptions.


Wrapper functions
-----------------

Wrappers for a MuPDF function `fz_foo()` are available in multiple forms:

* Functions in the `mupdf` namespace.

  * `mupdf::foo()`

    * Low-level wrapper:

      * Does not take `fz_context*` args.
      * Translates MuPDF exceptions into C++ exceptions.
      * Takes/returns pointers to MuPDF structs.
      * Code that uses these functions will need to make explicit calls to
        `fz_keep_*()` and `fz_drop_*()`.

  * `mupdf::mfz_foo()`

    * High-level wrapper:

      * Does not take `fz_context*` args.
      * Translates MuPDF exceptions into C++ exceptions.
      * Takes references to C++ wrapper class instances instead of pointers to
        MuPDF structs.
      * Where applicable, returns a C++ wrapper class instance instead of a
        pointer to a MuPDF struct.
      * Code that uses these functions does not need to call `fz_keep_*()`
        and `fz_drop_*()` - C++ wrapper class instances take care of reference
        counting internally.

* Class methods

  * Where `fz_foo()` has a first arg (ignoring any `fz_context*` arg) that
    takes a pointer to a MuPDF struct `foo_bar`, it is generally available as a
    member function of the wrapper class `mupdf::FooBar`:

    * `mupdf::FooBar::foo()`

  * Apart from being a member function, this is identical to
    `mupdf::mfz_foo()`, for example taking references to wrapper classes
    instead of pointers to MuPDF structs.


POD Wrapper classes
-------------------

Class wrappers for MuPDF structs default to having a `m_internal` member which
points to an instance of the wrapped struct. This works well for MuPDF structs
which support reference counting, because we can automatically create copy
constructors, `operator=` functions and destructors that call the associated
`fz_keep_*()` and `fz_drop_*()` functions.

However where a MuPDF struct does not support reference counting and contains
simple data, it is not safe to copy a pointer to the struct, so the class
wrapper will be a POD class. This is done in one of two ways:

* `m_internal` is an instance of the MuPDF struct, not a pointer.

  * Sometimes we provide members that give direct access to fields in
    `m_internal`.

* An 'inline' POD - there is no `m_internal` member; instead the wrapper class
  contains the same members as the MuPDF struct. This can be a little more
  convenient to use.



Wrapper class constructors
--------------------------

Wrapper class constructors are created for each MuPDF function that returns an
instance of a MuPDF struct.

Sometimes two such functions do not have different arg types so C++
overloading cannot distinguish between them as constructors (because C++
constructors do not have names).

We cope with this in two ways:

* Create a static method that returns a new instance of the wrapper class
  by value.

  * This is not possible if the underlying MuPDF struct is not reference counted
    or trivially copyable.

* Define an enum within the wrapper class, and provide a constructor that takes
  an instance of this enum to specify which MuPDF function to use.


Wrapper class default constructors
----------------------------------

Some POD classes have a default constructor that sets the various fields to
default values.

Where it is useful, non-POD wrapper classes can have a default constructor that
sets `m_internal` to null.


Extra static methods
--------------------

Where relevant, wrapper class can have static methods that wrap selected MuPDF
functions. For example `Matrix` does this for `fz_concat()`, `fz_scale()` etc,
because these return the result by value rather than modifying a `fz_matrix`
instance.


Miscellaneous custom wrapper classes
------------------------------------

The wrapper for `fz_outline_item` does not contain a `fz_outline_item` by
value or pointer. Instead it defines C++-style member equivalents to
``fz_outline_item``'s fields, to simplify usage from C++ and Python/C#.

The fields are initialised from a `fz_outline_item` when the wrapper class
is constructed. In this particular case there is no need to hold on to a
`fz_outline_item`, and the use of `std::string` ensures that value semantics
can work.


Python/C# bindings details
==========================

Custom methods for use by Python/C# bindings
--------------------------------------------

Python and C# code does not easily handle functions that return raw data, for example
as an `unsigned char*` that is not a zero-terminated string. Sometimes we provide a
C++ method that returns a `std::vector` by value, so that Python and C# code can
wrap it in a systematic way.

For example `Md5::md5_final2()`.


Making MuPDF function pointers call Python code
-----------------------------------------------

For MuPDF structs with function pointers, we provide a second C++ wrapper
class for use by the Python bindings.

Limitations:

* Problems have been seen on Windows when using these callbacks.

Description:

* The second wrapper class has a `2` suffix, for example `PdfFilterOptions2`.

* This second wrapper class has a virtual method for each function pointer, so
  it can be used as a `SWIG Director class <https://swig.org/Doc4.0/SWIGDocumentation.html#SWIGPlus_target_language_callbacks>`_.

* Overriding a virtual method in Python results in the Python method being
  called when MuPDF C code calls the corresponding function pointer.

* One needs to activate the use of a Python method as a callback by calling the
  special method `use_virtual_<method-name>()`. [It might be possible in future
  to remove the need to do this.]

* It may be possible to use similar techniques in C# but this has not been
  tested.

These Python callbacks have args that are more low-level than in the rest of
the Python API:

* Callbacks generally have a first arg that is a SWIG representation of a MuPDF
  `fz_context*`.

  * This is provided so that callbacks are able to call MuPDF C function
    pointers (which generally take a `fz_context *ctx` arg).

  * However as of 2022-06-03 if the C function throws a MuPDF exception, it
    will not be translated into a C++/Python exception, and so probably crash the
    process.

* Where the underlying MuPDF function pointer has an arg that is a pointer to
  an MuPDF struct, unlike elsewhere in the MuPDF bindings we do not translate
  this into an instance of the corresponding wrapper class. Instead Python
  callbacks will see a SWIG representation of the low-level C pointer.

  * It is not safe to construct a Python wrapper class instance directly from
    such a SWIG representation of a C pointer, because it will break MuPDF's
    reference counting - Python/C++ constructors that take a raw pointer to a
    MuPDF struct do not call `fz_keep_*()` but the corresponding Python/C++
    destructor will call `fz_drop_*()`.

  * It might be safe to create an wrapper class instance using an explicit call
    to `mupdf.keep_*()`, but this has not been tested.

Here is an example PDF filter written in Python that removes alternating items:

Details
|expand_begin|
::
    import mupdf

    def test_filter(path):
        class MyFilter( mupdf.PdfFilterOptions2):
            def __init__( self):
                super().__init__()
                self.use_virtual_text_filter()
                self.recurse = 1
                self.sanitize = 1
                self.state = 1
                self.ascii = True
            def text_filter( self, ctx, ucsbuf, ucslen, trm, ctm, bbox):
                print( f'text_filter(): ctx={ctx} ucsbuf={ucsbuf} ucslen={ucslen} trm={trm} ctm={ctm} bbox={bbox}')
                # Remove every other item.
                self.state = 1 - self.state
                return self.state

        filter_ = MyFilter()

        document = mupdf.PdfDocument(path)
        for p in range(document.count_pages()):
            page = document.load_page(p)
            print( f'Running document.filter_page_contents on page {p}')
            document.begin_operation('test filter')
            document.filter_page_contents(page, filter_)
            document.end_operation()

        document.save_document('foo.pdf', mupdf.PdfWriteOptions())
|expand_end|


Artifex Licensing
=================

Artifex offers a dual licensing model for MuPDF. Meaning we offer both
commercial licenses or the GNU Affero General Public License (AGPL).

While Open Source software may be free to use, that does not mean
it is free of obligation. To determine whether your intended use of
MuPDF is suitable for the AGPL, please read the full text of the
`AGPL license agreement on the FSF
web site <https://www.gnu.org/licenses/agpl-3.0.html>`_.

With a commercial license from Artifex, you maintain full ownership
and control over your products, while allowing you to distribute your
products to customers as you wish. You are not obligated to share your
proprietary source code and this saves you from having to conform to
the requirements and restrictions of the AGPL. For more information,
please see our `licensing page <https://artifex.com/licensing>`_, or
`contact our sales team <https://artifex.com/contact/>`_.

---

Please send any questions, comments or suggestions about this page to: mailto:julian.smith@artifex.com
