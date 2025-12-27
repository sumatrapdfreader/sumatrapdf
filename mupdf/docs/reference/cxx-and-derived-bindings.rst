.. Copyright (C) 2001-2025 Artifex Software, Inc.
.. All Rights Reserved.


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


C++, Python, and C#
===============================================================

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


Overview
---------------------------------------------------------------

Auto-generated abstracted :title:`C++`, :title:`Python` and :title:`C#`
versions of the :title:`MuPDF C API` are available.

*
  The C++ API is machine-generated from the C API header files and adds various
  abstractions such as automatic contexts and automatic reference counting.

*
  The Python and C# APIs are generated from the C++ API using SWIG, so
  automatically include the C++ API's abstractions.

.. graphviz::

    digraph
    {
      size="4,4";
      labeljust=l;

      "MuPDF C API" [shape="rectangle"]
      "MuPDF C++ API" [shape="rectangle"]
      "SWIG" [shape="oval"]
      "MuPDF Python API" [shape="rectangle"]
      "MuPDF C# API" [shape="rectangle"]

      "MuPDF C API" -> "MuPDF C++ API" [label=" Parse C headers with libclang,\l generate abstractions.\l"]

      "MuPDF C++ API" -> "SWIG" [label=" Parse C++ headers with SWIG."]
      "SWIG" -> "MuPDF Python API"
      "SWIG" -> "MuPDF C# API"
    }


The C++ MuPDF API
---------------------------------------------------------------

Basics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Auto-generated from the MuPDF C API's header files.

* Everything is in C++ namespace ``mupdf``.

* All functions and methods do not take ``fz_context*`` arguments.
  (Automatically-generated per-thread contexts are used internally.)

* All MuPDF ``setjmp()``/``longjmp()``-based exceptions are converted into C++ exceptions.

Low-level C++ API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The MuPDF C API is provided as low-level C++ functions with ``ll_`` prefixes.

* No ``fz_context*`` arguments.

* MuPDF exceptions are converted into C++ exceptions.

Class-aware C++ API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C++ wrapper classes wrap most ``fz_*`` and ``pdf_*`` C structs:

* Class names are camel-case versions of the wrapped struct's
  name, for example ``fz_document``'s wrapper class is ``mupdf::FzDocument``.

* Classes automatically handle reference counting of the underlying C structs,
  so there is no need for manual calls to ``fz_keep_*()`` and ``fz_drop_*()``, and
  class instances can be treated as values and copied arbitrarily.

Class-aware functions and methods take and return wrapper class instances
instead of MuPDF C structs:

* No ``fz_context*`` arguments.

* MuPDF exceptions are converted into C++ exceptions.

* Class-aware functions have the same names as the underlying C API function.

* Args that are pointers to a MuPDF struct will be changed to take a reference to
  the corresponding wrapper class.

* Where a MuPDF function returns a pointer to a struct, the class-aware C++
  wrapper will return a wrapper class instance by value.

* Class-aware functions that have a C++ wrapper class as their first parameter
  are also provided as a member function of the wrapper class, with the same
  name as the class-aware function.

* Wrapper classes are defined in ``mupdf/platform/c++/include/mupdf/classes.h``.

* Class-aware functions are declared in ``mupdf/platform/c++/include/mupdf/classes2.h``.

*
  Wrapper classes for reference-counted MuPDF structs:

  *
    The C++ wrapper classes will have a public ``m_internal`` member that is a
    pointer to the underlying MuPDF struct.

  *
    If a MuPDF C function returns a null pointer to a MuPDF struct, the
    class-aware C++ wrapper will return an instance of the wrapper class with a
    null ``m_internal`` member.

  *
    The C++ wrapper class will have an ``operator bool()`` that returns true if
    the ``m_internal`` member is non-null.

    [Introduced 2024-07-08.]

Usually it is more convenient to use the class-aware C++ API rather than the
low-level C++ API.

C++ Exceptions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C++ exceptions use classes for each ``FZ_ERROR_*`` enum, all derived from a class
``mupdf::FzErrorBase`` which in turn derives from ``std::exception``.

For example if MuPDF C code does ``fz_throw(ctx, FZ_ERROR_GENERIC,
"something failed")``, this will appear as a C++ exception with type
``mupdf::FzErrorGeneric``. Its ``what()`` method will return ``code=2: something
failed``, and it will have a public member ``m_code`` set to ``FZ_ERROR_GENERIC``.

Example wrappers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The MuPDF C API function ``fz_new_buffer_from_page()`` is available as these
C++ functions/methods:

.. code-block:: c++

    // MuPDF C function.
    fz_buffer *fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);

    // MuPDF C++ wrappers.
    namespace mupdf
    {
        // Low-level wrapper:
        ::fz_buffer *ll_fz_new_buffer_from_page(::fz_page *page, const ::fz_stext_options *options);

        // Class-aware wrapper:
        FzBuffer fz_new_buffer_from_page(const FzPage& page, FzStextOptions& options);

        // Method in wrapper class FzPage:
        struct FzPage
        {
            ...
            FzBuffer fz_new_buffer_from_page(FzStextOptions& options);
            ...
        };
    }


Extensions beyond the basic C API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Some generated classes have extra ``begin()`` and ``end()`` methods to allow
  standard C++ iteration:

  |expand_begin|

  .. code-block:: c++

      #include "mupdf/classes.h"
      #include "mupdf/functions.h"

      #include <iostream>

      void show_stext(mupdf::FzStextPage& page)
      {
          for (mupdf::FzStextPage::iterator it_page: page)
          {
              mupdf::FzStextBlock block = *it_page;
              for (mupdf::FzStextBlock::iterator it_block: block)
              {
                  mupdf::FzStextLine line = *it_block;
                  for (mupdf::FzStextLine::iterator it_line: line)
                  {
                      mupdf::FzStextChar stextchar = *it_line;
                      fz_stext_char* c = stextchar.m_internal;
                      using namespace mupdf;
                      std::cout << "FzStextChar("
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

* There are various custom class methods and constructors.

* There are extra functions for generating a text representation of 'POD'
  (plain old data) structs and their C++ wrapper classes.

  For example for ``fz_rect`` we provide these functions:

  .. code-block:: c++

      std::ostream& operator<< (std::ostream& out, const fz_rect& rhs);
      std::ostream& operator<< (std::ostream& out, const FzRect& rhs);
      std::string to_string_fz_rect(const fz_rect& s);
      std::string to_string(const fz_rect& s);
      std::string Rect::to_string() const;

  These each generate text such as: ``(x0=90.51 y0=160.65 x1=501.39 y1=1215.6)``

Runtime environmental variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All builds
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* **MUPDF_mt_ctx**

  Controls support for multi-threading on startup.

  * If set with value ``0``, a single ``fz_context*`` is used for all threads; this
    might give a small performance increase in single-threaded programmes, but
    will be unsafe in multi-threaded programmes.

  * Otherwise each thread has its own ``fz_context*``.

  One can instead call ``mupdf::reinit_singlethreaded()`` on startup to force
  single-threaded mode. This should be done before any other use of MuPDF.

Debug builds only
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Debug builds contain diagnostics/checking code that is activated via these
environmental variables:

* **MUPDF_check_refs**

  If ``1``, generated code checks MuPDF struct reference counts at
  runtime.

* **MUPDF_check_error_stack**

  If ``1``, generated code outputs a diagnostic if a MuPDF function changes the
  current ``fz_context``'s error stack depth.

* **MUPDF_trace**

  If ``1`` or ``2``, class-aware code outputs a diagnostic each time it calls a
  MuPDF function (apart from keep/drop functions).

  If ``2``, low-level wrappers output a diagnostic each time they are
  called. We also show arg POD and pointer values.

* **MUPDF_trace_director**

  If ``1``, generated code outputs a diagnostic when doing special
  handling of MuPDF structs containing function pointers.

* **MUPDF_trace_exceptions**

  If ``1``, generated code outputs diagnostics when it converts MuPDF
  ``setjmp()``/``longjmp()`` exceptions into C++ exceptions.

* **MUPDF_trace_keepdrop**

  If ``1``, generated code outputs diagnostics for calls to ``*_keep_*()`` and
  ``*_drop_*()``.

Limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* We do not wrap variadic functions such as `fz_write_printf()`.

* Global instances of C++ wrapper classes are not supported.

  This is because:

  * C++ wrapper class destructors generally call MuPDF functions (for example
    ``fz_drop_*()``).

  * The C++ bindings use internal thread-local objects to allow per-thread
    ``fz_context``'s to be efficiently obtained for use with underlying MuPDF
    functions.

  * C++ globals are destructed *after* thread-local objects are destructed.

  So if a global instance of a C++ wrapper class is created, its destructor
  will attempt to get a ``fz_context*`` using internal thread-local objects
  which will have already been destroyed.

  We attempt to display a diagnostic when this happens, but this cannot be
  relied on as behaviour is formally undefined.


The Python and C# MuPDF APIs
---------------------------------------------------------------

* A Python module called ``mupdf``.
* A C# namespace called ``mupdf``.

* Auto-generated from the C++ MuPDF API using SWIG, so inherits the abstractions of the C++ API:

  * No ``fz_context*`` arguments.
  * Automatic reference counting, so no need to call ``fz_keep_*()`` or ``fz_drop_*()``, and we have value-semantics for class instances.
  * Native Python and C# exceptions.
* Output parameters are returned as tuples.

  For example MuPDF C function ``fz_read_best()`` has prototype::

      fz_buffer *fz_read_best(fz_context *ctx, fz_stream *stm, size_t initial, int *truncated);

  The class-aware Python wrapper is::

      mupdf.fz_read_best(stm, initial)

  and returns ``(buffer, truncated)``, where ``buffer`` is a SWIG proxy for a
  ``mupdf::FzBuffer`` instance and ``truncated`` is an integer.

* Allows implementation of mutool in Python - see
  `mupdf:scripts/mutool.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mutool.py>`_
  and
  `mupdf:scripts/mutool_draw.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mutool_draw.py>`_.

* Provides text representation of simple 'POD' structs:

  .. code-block:: python

      rect = mupdf.FzRect(...)
      print(rect) # Will output text such as: (x0=90.51 y0=160.65 x1=501.39 y1=215.6)

  * This works for classes where the C++ API defines a ``to_string()`` method as described above.

    * Python classes will have a ``__str__()` method, and an identical `__repr__()`` method.
    * C# classes will have a ``ToString()`` method.

* Uses SWIG Director classes to allow C function pointers in MuPDF structs to call Python code.


Installing the Python mupdf module using ``pip``
---------------------------------------------------------------

The Python ``mupdf`` module is available on the `Python Package Index (PyPI) website <https://pypi.org/>`_.

* Install with ``pip install mupdf``.
* Pre-built Wheels (binary Python packages) are provided for Windows and Linux.
* For more information on the latest release, see changelog below and: https://pypi.org/project/mupdf/

Doxygen/Pydoc API documentation
---------------------------------------------------------------

Auto-generated documentation for the C, C++ and Python APIs is available at:
https://ghostscript.com/~julian/mupdf-bindings/

* All content is generated from the comments in MuPDF header files.

* This documentation is generated from an internal development tree, so may
  contain features that are not yet publicly available.

* It is updated only intermittently.

Example client code
---------------------------------------------------------------

Using the Python API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Minimal Python code that uses the ``mupdf`` module::

    import mupdf
    document = mupdf.FzDocument('foo.pdf')

A simple example Python test script (run by ``scripts/mupdfwrap.py -t``) is:

* `scripts/mupdfwrap_test.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mupdfwrap_test.py>`_

More detailed usage of the Python API can be found in:

* `scripts/mutool.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mutool.py>`_
* `scripts/mutool_draw.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mutool_draw.py>`_


**Example Python code that shows all available information about a document's Stext blocks, lines and characters**:

|expand_begin|
::

    #!/usr/bin/env python3

    import mupdf

    def show_stext(document):
        '''
        Shows all available information about Stext blocks, lines and characters.
        '''
        for p in range(document.fz_count_pages()):
            page = document.fz_load_page(p)
            stextpage = mupdf.FzStextPage(page, mupdf.FzStextOptions())
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

    document = mupdf.FzDocument('foo.pdf')
    show_stext(document)

|expand_end|

Basic PDF viewers written in Python and C#
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* `scripts/mupdfwrap_gui.py <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mupdfwrap_gui.py>`_
* `scripts/mupdfwrap_gui.cs <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/scripts/mupdfwrap_gui.cs>`_
* Build and run with:

  * ``./scripts/mupdfwrap.py -b all --test-python-gui``
  * ``./scripts/mupdfwrap.py -b --csharp all --test-csharp-gui``


Building the C++, Python and C# MuPDF APIs from source
---------------------------------------------------------------


General requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Windows, Linux, MacOS or OpenBSD.

*
  Build should take place inside a Python `venv <https://docs.python.org/3.8/library/venv.html>`_.

*
  `libclang Python interface onto <https://libclang.readthedocs.io/en/latest/index.html>`_ the `clang C/C++ parser <https://clang.llvm.org/>`_.

* `swig <https://swig.org/>`_, for Python and C# bindings.

*
  `Mono <https://www.mono-project.com/>`_, for C# bindings on platforms
  other than Windows.


Setting up
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Windows only
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* Install Python.

  *
    Use the Python Windows installer from the python.org website:
    http://www.python.org/downloads

  * Don't use other installers such as the Microsoft Store Python package.

    *
      If Microsoft Store Python is already installed, leave it in place and install
      from python.org on top of it - uninstalling before running the python.org
      installer has been known to cause problems.

  * A default installation is sufficient.

  * Debug binaries are required for debug builds of the MuPDF Python API.

  *
    If "Customize Installation" is chosen, make sure to include "py launcher" so
    that the ``py`` command will be available.

  * Also see: https://docs.python.org/3/using/windows.html

*
  Install Visual Studio 2019. Later versions may not work with MuPDF's
  solution and build files.


All platforms
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* Get the latest version of MuPDF in git.

  .. code-block:: shell

      git clone --recursive git://git.ghostscript.com/mupdf.git

*
  Create and enter a `Python venv <https://docs.python.org/3.8/library/venv.html>`_ and upgrade pip.

  * Windows.

    .. code-block:: bat

        py -m venv pylocal
        .\pylocal\Scripts\activate
        python -m pip install --upgrade pip

  * Linux, MacOS, OpenBSD

    .. code-block:: shell

        python3 -m venv pylocal
        . pylocal/bin/activate
        python -m pip install --upgrade pip


General build flags
~~~~~~~~~~~~~~~~~~~

In all of the commands below, one can set environmental variables to control
the build of the underlying MuPDF C API, for example ``USE_SYSTEM_LIBJPEG=yes``.

In addition, ``XCXXFLAGS`` can be used to set additional C++ compiler flags when
building the C++ and Python bindings (the name is analogous to the ``XCFLAGS``
used by MuPDF's makefile when compiling the core library).


Building and installing the Python bindings using ``pip``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Windows, Linux, MacOS.

  .. code-block:: shell

      cd mupdf && pip install -vv .

* OpenBSD.

  Building using ``pip`` is not supported because ``libclang`` is not
  available from pypi.org so pip will fail to install prerequisites from
  ``pypackage.toml``.

  Instead one can run ``setup.py`` directly:

  .. code-block:: shell

      cd mupdf && setup.py install


Building the Python bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Windows, Linux, MacOS.

  .. code-block:: shell

      pip install libclang swig setuptools
      cd mupdf && python scripts/mupdfwrap.py -b all

* OpenBSD.

  ``libclang`` is not available from pypi.org, but we can instead use
  the system ``py3-llvm`` package.

  .. code-block:: shell

      sudo pkg_add py3-llvm
      pip install swig setuptools
      cd mupdf && python scripts/mupdfwrap.py -b all

Building the C++ bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Windows, Linux, MacOS.

  .. code-block:: shell

      pip install libclang setuptools
      cd mupdf && python scripts/mupdfwrap.py -b m01

* OpenBSD.

  ``libclang`` is not available from pypi.org, but we can instead use
  the system ``py3-llvm`` package.

  .. code-block:: shell

      sudo pkg_add py3-llvm
      pip install setuptools
      cd mupdf && python scripts/mupdfwrap.py -b m01


Building the C# bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Windows.

  .. code-block:: shell

      pip install libclang swig setuptools
      cd mupdf && python scripts/mupdfwrap.py -b --csharp all

* Linux.

  .. code-block:: shell

      sudo apt install mono-devel
      pip install libclang swig
      cd mupdf && python scripts/mupdfwrap.py -b --csharp all

* MacOS.

  Building the C# bindings on MacOS is not currently supported.

* OpenBSD.

  .. code-block:: shell

      sudo pkg_add py3-llvm mono
      pip install swig setuptools
      cd mupdf && python scripts/mupdfwrap.py -b --csharp all


Using the bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use the bindings, one has to tell the OS where to find the MuPDF
runtime files.

* C++ and C# bindings:

  * Windows.

    .. code-block:: shell

        set PATH=.../mupdf/build/shared-release-x64-py3.11;%PATH%

    * Replace ``x64`` with ``x32`` if using 32-bit.

    * Replace ``3.11`` with the appropriate python version number.


  * Linux, OpenBSD.

    .. code-block:: shell

        LD_LIBRARY_PATH=.../mupdf/build/shared-release

    (``LD_LIBRARY_PATH`` must be an absolute path.)

  * MacOS.

    .. code-block:: shell

        DYLD_LIBRARY_PATH=.../mupdf/build/shared-release

* Python bindings:

  If the bindings have been built and installed using ``pip install``,
  they will already be available within the venv.

  Otherwise:

  * Windows.

    .. code-block:: shell

        PYTHONPATH=.../mupdf/build/shared-release-x64-py3.11

    * Replace ``x64`` with ``x32`` if using 32-bit.

    * Replace ``3.11`` with the appropriate python version number.

  * Linux, MacOS, OpenBSD.

    .. code-block:: shell

        PYTHONPATH=.../mupdf/build/shared-release


Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Running tests.

  Basic tests can be run by appending args to the ``scripts/mupdfwrap.py``
  command.

  This will also demonstrate how to set environment variables such as
  ``PYTHONPATH`` or ``LD_LIBRARY_PATH`` to the MuPDF build directory.

  * Python tests.

    * ``--test-python``
    * ``--test-python-gui``

  * C# tests.

    * ``--test-csharp``
    * ``--test-csharp-gui``

  * C++ tests.

    * ``--test-cpp``

* C++ bindings and ``NDEBUG``.

  When building client code that uses the C++ bindings, ``NDEBUG`` must
  be defined/undefined to match how the C++ bindings were built. By
  default the C++ bindings are a release build with ``NDEBUG`` defined, so
  usually client code must also be built with ``NDEBUG`` defined. Otherwise
  there will be build errors for missing C++ destructors, for example
  ``mupdf::FzMatrix::~FzMatrix()``.

  [This is because we define some destructors in debug builds only; this allows
  internal reference counting checks.]

* Specifying the location of Visual Studio's ``devenv.com`` on Windows.

  ``scripts/mupdfwrap.py`` looks for Visual Studio's ``devenv.com`` in
  standard locations; this can be overridden with:

  .. code-block:: shell

      python scripts/mupdfwrap.py -b --devenv <devenv.com-location> ...

* Specifying compilers.

  On non-Windows, we use ``cc`` and ``c++`` as default C and C++ compilers;
  override by setting environment variables ``$CC`` and ``$CXX``.

* OpenBSD ``libclang``.

  *
    ``libclang`` cannot be installed with pip on OpenBSD - wheels are not
    available and building from source fails.

    However unlike on other platforms, the system python-clang package
    (``py3-llvm``) is integrated with the system's libclang and can be
    used directly.

    So the above examples use ``pkg_add py3-llvm``.

* Alternatives to Python package ``libclang`` generally do not work.

  For example pypi.org's `clang <https://pypi.org/project/clang/>`_, or
  Debian's `python-clang <https://packages.debian.org/search?keywords=python+clang&searchon=names&suite=stable&section=all>`_.

  These are inconvenient to use because they require explicit setting of
  ``LD_LIBRARY_PATH`` to point to the correct libclang dynamic library.

* Debug builds.

  One can specify a debug build using the ``-d <build-directory>`` arg
  before ``-b``.

  .. code-block:: shell

      python ./scripts/mupdfwrap.py -d build/shared-debug -b ...

  *
    Debug builds of the Python and C# bindings on Windows have not been
    tested. There may be issues with requiring a debug version of the Python
    interpreter, for example ``python311_d.lib``.

*
  C# build failure: ``cstring.i not implemented for this target`` and/or
  ``Unknown directive '%cstring_output_allocate'``.

  This is probably because SWIG does not include support for C#. This
  has been seen in the past but as of 2023-07-19 pypi.org's default swig
  seems ok.

  A possible solution is to install SWIG using the system package
  manager, for example ``sudo apt install swig`` on Linux, or use
  ``./scripts/mupdfwrap.py --swig-windows-auto ...`` on Windows.

*
  C# ommisions.

  Some functions are ommited from the C# API due to C# restrictions, for
  example functions that return void* and have out-params (because tuples
  cannot contain void* items). These will be marked with comments in the
  generated mupdf.cs file.


* More information about running ``scripts/mupdfwrap.py``.

  * Run ``python ./scripts/mupdfwrap.py -h``.
  * Read the doc-string at beginning of ``scripts/wrap/__main__.py+``.


How ``scripts/mupdfwrap.py`` builds the APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Building the MuPDF C API
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* On Unix, runs ``make`` on MuPDF's ``Makefile`` with ``shared=yes``.

* On Windows, runs ``devenv.com`` on ``.sln`` and ``.vcxproj`` files within MuPDF's `platform/win32/ <https://cgit.ghostscript.com/cgi-bin/cgit.cgi/mupdf.git/tree/platform/win32>`_
  directory.

Generation of the MuPDF C++ API
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* Uses clang-python to parse MuPDF's C API.

* Generates C++ code that wraps the basic C interface, converting MuPDF
  ``setjmp()``/``longjmp()`` exceptions into C++ exceptions and automatically
  handling ``fz_context``'s internally.

* Generates C++ wrapper classes for each ``fz_*`` and ``pdf_*`` struct, and uses various
  heuristics to define constructors, methods and static methods that call
  ``fz_*()`` and ``pdf_*()`` functions. These classes' constructors and destructors
  automatically handle reference counting so class instances can be copied
  arbitrarily.

* C header file comments are copied into the generated C++ header files.

* Compile and link the generated C++ code to create shared libraries.


Generation of the MuPDF Python and C# APIs
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* Uses SWIG to parse the previously-generated C++ headers and generate C++,
  Python and C# code.

*
  Defines some custom-written Python and C# functions and methods, for
  example so that out-params are returned as tuples.

* If SWIG is version 4+, C++ comments are converted into Python doc-comments.

* Compile and link the SWIG-generated C++ code to create shared libraries.


Building auto-generated MuPDF API documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Build HTML documentation for the C, C++ and Python APIs (using Doxygen and pydoc):

.. code-block:: shell

    python ./scripts/mupdfwrap.py --doc all

This will generate the following tree:

.. code-block:: text

    mupdf/docs/generated/
        index.html
        c/
        c++/
        python/

All content is ultimately generated from the MuPDF C header file comments.

As of 2022-2-5, it looks like ``swig -doxygen`` (swig-4.02) ignores
single-line ``/** ... */`` comments, so the generated Python code (and
hence also Pydoc documentation) is missing information.

Generated files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All generated files are within the MuPDF checkout.

* C++ headers for the MuPDF C++ API are in ``platform/c++/include/``.

* Files required at runtime are in ``build/shared-release/``.

**Details**

.. code-block:: text

    mupdf/
        build/
            shared-release/    [Unix runtime files.]
                libmupdf.so    [MuPDF C API, not MacOS.]
                libmupdf.dylib [MuPDF C API, MacOS.]
                libmupdfcpp.so [MuPDF C++ API.]
                mupdf.py       [MuPDF Python API.]
                _mupdf.so      [MuPDF Python API internals.]
                mupdf.cs       [MuPDF C# API.]
                mupdfcsharp.so [MuPDF C# API internals.]

            shared-debug/
                [as shared-release but debug build.]

            shared-release-x*-py*/      [Windows runtime files.]
                mupdfcpp.dll            [MuPDF C and C++ API, x32.]
                mupdfcpp64.dll          [MuPDF C and C++ API, x64.]
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

                implementation/ [MuPDF C++ implementation source files.]
                    classes.cpp
                    classes2.cpp
                    exceptions.cpp
                    functions.cpp
                    internal.cpp

                generated.pickle    [Information from clang parse step, used by later stages.]
                windows_mupdf.def   [List of MuPDF public global data, used when linking mupdfcpp.dll.]

            python/ [SWIG Python files.]
                mupdfcpp_swig.i     [SWIG input file.]
                mupdfcpp_swig.i.cpp [SWIG output file.]

            csharp/  [SWIG C# files.]
                mupdf.cs            [SWIG output file, no out-params helpers.]
                mupdfcpp_swig.i     [SWIG input file.]
                mupdfcpp_swig.i.cpp [SWIG output file.]

            win32/
                Release/    [Windows 32-bit .dll, .lib, .exp, .pdb etc.]
                x64/
                    Release/    [Windows 64-bit .dll, .lib, .exp, .pdb etc.]
                        mupdfcpp64.dll  [Copied to build/shared-release*/mupdfcpp64.dll]
                        mupdfpyswig.dll [Copied to build/shared-release*/_mupdf.pyd]
                        mupdfcpp64.lib
                        mupdfpyswig.lib

            win32-vs-upgrade/   [used instead of win32/ if PYMUPDF_SETUP_MUPDF_VS_UPGRADE is '1'.]


Windows-specifics
---------------------------------------------------------------

Required predefined macros
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Code that will use the MuPDF DLL must be built with ``FZ_DLL_CLIENT``
predefined.

The MuPDF DLL itself is built with ``FZ_DLL`` predefined.

DLLs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There is no separate C library, instead the C and C++ APIs are
both in ``mupdfcpp.dll``, which is built by running devenv on
``platform/win32/mupdf.sln``.

The Python SWIG library is called ``_mupdf.pyd`` which, despite the name, is a
standard Windows DLL, built from ``platform/python/mupdfcpp_swig.i.cpp``.

DLL export of functions and data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Windows, ``include/mupdf/fitz/export.h`` defines ``FZ_FUNCTION`` and
``FZ_DATA` to `__declspec(dllexport)` and/or `__declspec(dllimport)``
depending on whether ``FZ_DLL`` or ``FZ_DLL_CLIENT`` are defined.

All MuPDF C headers prefix declarations of public global data with ``FZ_DATA``.

In generated C++ code:

* Data declarations and definitions are prefixed with ``FZ_DATA``.
* Function declarations and definitions are prefixed with ``FZ_FUNCTION``.
* Class method declarations and definitions are prefixed with ``FZ_FUNCTION``.

When building ``mupdfcpp.dll`` on Windows we link with the auto-generated
``platform/c++/windows_mupdf.def`` file; this lists all C public global data.

For reasons that are not fully understood, we don't seem to need to tag
C functions with ``FZ_FUNCTION``, but this is required for C++ functions
otherwise we get unresolved symbols when building MuPDF client code.

Building the DLLs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We build Windows binaries by running ``devenv.com`` directly.

Building ``_mupdf.pyd`` is tricky because it needs to be built with a
specific ``Python.h`` and linked with a specific ``python.lib``. This is
done by setting environmental variables ``MUPDF_PYTHON_INCLUDE_PATH`` and
``MUPDF_PYTHON_LIBRARY_PATH`` when running ``devenv.com``, which are referenced
by ``platform/win32/mupdfpyswig.vcxproj``. Thus one cannot easily build
``_mupdf.pyd`` directly from the Visual Studio GUI.

[In the git history there is code that builds ``_mupdf.pyd`` by running the
Windows compiler and linker ``cl.exe`` and ``link.exe`` directly, which avoids
the complications of going via devenv, at the expense of needing to know where
``cl.exe`` and ``link.exe`` are.]


C++ bindings details
---------------------------------------------------------------

Wrapper functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Wrappers for a MuPDF function ``fz_foo()`` are available in multiple forms:

* Functions in the ``mupdf`` namespace.

  * ``mupdf::ll_fz_foo()``

    * Low-level wrapper:

      * Does not take ``fz_context*`` arg.
      * Translates MuPDF exceptions into C++ exceptions.
      * Takes/returns pointers to MuPDF structs.
      * Code that uses these functions will need to make explicit calls to
        ``fz_keep_*()`` and ``fz_drop_*()``.

  * ``mupdf::fz_foo()``

    * High-level class-aware wrapper:

      * Does not take ``fz_context*`` arg.
      * Translates MuPDF exceptions into C++ exceptions.
      * Takes references to C++ wrapper class instances instead of pointers to
        MuPDF structs.
      * Where applicable, returns C++ wrapper class instances instead of
        pointers to MuPDF structs.
      * Code that uses these functions does not need to call ``fz_keep_*()``
        and ``fz_drop_*()`` - C++ wrapper class instances take care of reference
        counting internally.

* Class methods

  * Where ``fz_foo()`` has a first arg (ignoring any ``fz_context*`` arg) that
    takes a pointer to a MuPDF struct ``foo_bar``, it is generally available as a
    member function of the wrapper class ``mupdf::FooBar``:

    * ``mupdf::FooBar::fz_foo()``

  * Apart from being a member function, this is identical to class-aware
    wrapper ``mupdf::fz_foo()``, for example taking references to wrapper classes
    instead of pointers to MuPDF structs.


Constructors using MuPDF functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Wrapper class constructors are created for each MuPDF function that returns an
instance of a MuPDF struct.

Sometimes two such functions do not have different arg types so C++
overloading cannot distinguish between them as constructors (because C++
constructors do not have names).

We cope with this in two ways:

* Create a static method that returns a new instance of the wrapper class
  by value.

  * This is not possible if the underlying MuPDF struct is not copyable - i.e.
    not reference counted and not POD.

* Define an enum within the wrapper class, and provide a constructor that takes
  an instance of this enum to specify which MuPDF function to use.


Default constructors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All wrapper classes have a default constructor.

* For POD classes each member is set to a default value with ``this->foo =
  {};``. Arrays are initialised by setting all bytes to zero using
  ``memset()``.
* For non-POD classes, class member ``m_internal`` is set to ``nullptr``.
* Some classes' default constructors are customized, for example:

  * The default constructor for ``fz_color_params`` wrapper
    ``mupdf::FzColorParams`` sets state to a copy of
    ``fz_default_color_params``.
  * The default constructor for ``fz_md5`` wrapper ``mupdf::FzMd5`` sets
    state using ``fz_md5_init()``.
  * These are described in class definition comments in
    ``platform/c++/include/mupdf/classes.h``.


Raw constructors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many wrapper classes have constructors that take a pointer to the underlying
MuPDF C struct. These are usually for internal use only. They do not call
``fz_keep_*()`` - it is expected that any supplied MuPDF struct is already
owned.


POD wrapper classes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Class wrappers for MuPDF structs default to having a ``m_internal`` member which
points to an instance of the wrapped struct. This works well for MuPDF structs
which support reference counting, because we can automatically create copy
constructors, ``operator=`` functions and destructors that call the associated
``fz_keep_*()`` and ``fz_drop_*()`` functions.

However where a MuPDF struct does not support reference counting and contains
simple data, it is not safe to copy a pointer to the struct, so the class
wrapper will be a POD class. This is done in one of two ways:

* ``m_internal`` is an instance of the MuPDF struct, not a pointer.

  * Sometimes we provide members that give direct access to fields in
    ``m_internal``.

* An 'inline' POD - there is no ``m_internal`` member; instead the wrapper class
  contains the same members as the MuPDF struct. This can be a little more
  convenient to use.


Extra static methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Where relevant, wrapper class can have static methods that wrap selected MuPDF
functions. For example ``FzMatrix`` does this for ``fz_concat()``, ``fz_scale()`` etc,
because these return the result by value rather than modifying a ``fz_matrix``
instance.


Miscellaneous custom wrapper classes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The wrapper for ``fz_outline_item`` does not contain a ``fz_outline_item`` by
value or pointer. Instead it defines C++-style member equivalents to
``fz_outline_item``'s fields, to simplify usage from C++ and Python/C#.

The fields are initialised from a ``fz_outline_item`` when the wrapper class
is constructed. In this particular case there is no need to hold on to a
``fz_outline_item``, and the use of ``std::string`` ensures that value semantics
can work.


Extra functions in C++, Python and C#
---------------------------------------------------------------

[These functions are available as low-level functions, class-aware
functions and class methods.]

.. code-block:: c++

        /**
        C++ alternative to ``fz_lookup_metadata()`` that returns a ``std::string``
        or calls ``fz_throw()`` if not found.
        */
        FZ_FUNCTION std::string fz_lookup_metadata2(fz_context* ctx, fz_document* doc, const char* key);

        /**
        C++ alternative to ``pdf_lookup_metadata()`` that returns a ``std::string``
        or calls ``fz_throw()`` if not found.
        */
        FZ_FUNCTION std::string pdf_lookup_metadata2(fz_context* ctx, pdf_document* doc, const char* key);

        /**
        C++ alternative to ``fz_md5_pixmap()`` that returns the digest by value.
        */
        FZ_FUNCTION std::vector<unsigned char> fz_md5_pixmap2(fz_context* ctx, fz_pixmap* pixmap);

        /**
        C++ alternative to fz_md5_final() that returns the digest by value.
        */
        FZ_FUNCTION std::vector<unsigned char> fz_md5_final2(fz_md5* md5);

        /** */
        FZ_FUNCTION long long fz_pixmap_samples_int(fz_context* ctx, fz_pixmap* pixmap);

        /**
        Provides simple (but slow) access to pixmap data from Python and C#.
        */
        FZ_FUNCTION int fz_samples_get(fz_pixmap* pixmap, int offset);

        /**
        Provides simple (but slow) write access to pixmap data from Python and
        C#.
        */
        FZ_FUNCTION void fz_samples_set(fz_pixmap* pixmap, int offset, int value);

        /**
        C++ alternative to fz_highlight_selection() that returns quads in a
        std::vector.
        */
        FZ_FUNCTION std::vector<fz_quad> fz_highlight_selection2(fz_context* ctx, fz_stext_page* page, fz_point a, fz_point b, int max_quads);

        struct fz_search_page2_hit
        {{
            fz_quad quad;
            int mark;
        }};

        /**
        C++ alternative to fz_search_page() that returns information in a std::vector.
        */
        FZ_FUNCTION std::vector<fz_search_page2_hit> fz_search_page2(fz_context* ctx, fz_document* doc, int number, const char* needle, int hit_max);

        /**
        C++ alternative to fz_string_from_text_language() that returns information in a std::string.
        */
        FZ_FUNCTION std::string fz_string_from_text_language2(fz_text_language lang);

        /**
        C++ alternative to fz_get_glyph_name() that returns information in a std::string.
        */
        FZ_FUNCTION std::string fz_get_glyph_name2(fz_context* ctx, fz_font* font, int glyph);

        /**
        Extra struct containing fz_install_load_system_font_funcs()'s args,
        which we wrap with virtual_fnptrs set to allow use from Python/C# via
        Swig Directors.
        */
        typedef struct fz_install_load_system_font_funcs_args
        {{
            fz_load_system_font_fn* f;
            fz_load_system_cjk_font_fn* f_cjk;
            fz_load_system_fallback_font_fn* f_fallback;
        }} fz_install_load_system_font_funcs_args;

        /**
        Alternative to fz_install_load_system_font_funcs() that takes args in a
        struct, to allow use from Python/C# via Swig Directors.
        */
        FZ_FUNCTION void fz_install_load_system_font_funcs2(fz_context* ctx, fz_install_load_system_font_funcs_args* args);

        /** Internal singleton state to allow Swig Director class to find
        fz_install_load_system_font_funcs_args class wrapper instance. */
        FZ_DATA extern void* fz_install_load_system_font_funcs2_state;

        /** Helper for calling ``fz_document_handler::open`` function pointer via
        Swig from Python/C#. */
        FZ_FUNCTION fz_document* fz_document_handler_open(fz_context* ctx, const fz_document_handler *handler, fz_stream* stream, fz_stream* accel, fz_archive* dir, void* recognize_state);

        /** Helper for calling a ``fz_document_handler::recognize`` function
        pointer via Swig from Python/C#. */
        FZ_FUNCTION int fz_document_handler_recognize(fz_context* ctx, const fz_document_handler *handler, const char *magic);

        /** Swig-friendly wrapper for pdf_choice_widget_options(), returns the
        options directly in a vector. */
        FZ_FUNCTION std::vector<std::string> pdf_choice_widget_options2(fz_context* ctx, pdf_annot* tw, int exportval);

        /** Swig-friendly wrapper for fz_new_image_from_compressed_buffer(),
        uses specified ``decode`` and ``colorkey`` if they are not null (in which
        case we assert that they have size ``2*fz_colorspace_n(colorspace)``). */
        FZ_FUNCTION fz_image* fz_new_image_from_compressed_buffer2(
                fz_context* ctx,
                int w,
                int h,
                int bpc,
                fz_colorspace* colorspace,
                int xres,
                int yres,
                int interpolate,
                int imagemask,
                const std::vector<float>& decode,
                const std::vector<int>& colorkey,
                fz_compressed_buffer* buffer,
                fz_image* mask
                );

        /** Swig-friendly wrapper for pdf_rearrange_pages(). */
        void pdf_rearrange_pages2(
                fz_context* ctx,
                pdf_document* doc,
                const std::vector<int>& pages,
                pdf_clean_options_structure structure
                );

        /** Swig-friendly wrapper for pdf_subset_fonts(). */
        void pdf_subset_fonts2(fz_context *ctx, pdf_document *doc, const std::vector<int>& pages);

        /** Swig-friendly and typesafe way to do fz_snprintf(fmt, value). ``fmt``
        must end with one of 'efg' otherwise we throw an exception. */
        std::string fz_format_double(fz_context* ctx, const char* fmt, double value);

        struct fz_font_ucs_gid
        {{
            unsigned long ucs;
            unsigned int gid;
        }};

        /** SWIG-friendly wrapper for fz_enumerate_font_cmap(). */
        std::vector<fz_font_ucs_gid> fz_enumerate_font_cmap2(fz_context* ctx, fz_font* font);

        /** SWIG-friendly wrapper for pdf_set_annot_callout_line(). */
        void pdf_set_annot_callout_line2(fz_context *ctx, pdf_annot *annot, std::vector<fz_point>& callout);

        /** SWIG-friendly wrapper for fz_decode_barcode_from_display_list(),
        avoiding leak of the returned string. */
        std::string fz_decode_barcode_from_display_list2(fz_context *ctx, fz_barcode_type *type, fz_display_list *list, fz_rect subarea, int rotate);

        /** SWIG-friendly wrapper for fz_decode_barcode_from_pixmap(), avoiding
        leak of the returned string. */
        std::string fz_decode_barcode_from_pixmap2(fz_context *ctx, fz_barcode_type *type, fz_pixmap *pix, int rotate);

        /** SWIG-friendly wrapper for fz_decode_barcode_from_page(), avoiding
        leak of the returned string. */
        std::string fz_decode_barcode_from_page2(fz_context *ctx, fz_barcode_type *type, fz_page *page, fz_rect subarea, int rotate);


Python/C# bindings details
---------------------------------------------------------------

Extra Python functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Access to raw C arrays
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

The following functions can be used from Python to get access to raw data:

*
  ``mupdf.bytes_getitem(array, index)``: Gives access to individual items
  in an array of ``unsigned char``'s, for example in the data returned by
  ``mupdf::FzPixmap``'s ``samples()`` method.

*
  ``mupdf.floats_getitem(array, index)``: Gives access to individual items in an
  array of ``float``'s, for example in ``fz_stroke_state``'s ``float dash_list[32]``
  array. Generated with SWIG code ``carrays.i`` and ``array_functions(float,
  floats);``.

*
  ``mupdf.python_buffer_data(b)``: returns a SWIG wrapper for a ``const unsigned
  char*`` pointing to a Python buffer instance's raw data. For example ``b`` can
  be a Python ``bytes`` or ``bytearray`` instance.

*
  ``mupdfpython_mutable_buffer_data(b)``: returns a SWIG wrapper for an ``unsigned
  char*`` pointing to a Python buffer instance's raw data. For example ``b`` can
  be a Python ``bytearray`` instance.

[These functions are implemented internally using SWIG's ``carrays.i`` and
``pybuffer.i``.


Python differences from C API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

[The functions described below are also available as class methods.]


Custom methods
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Python and C# code does not easily handle functions that return raw data, for example
as an ``unsigned char*`` that is not a zero-terminated string. Sometimes we provide a
C++ method that returns a ``std::vector`` by value, so that Python and C# code can
wrap it in a systematic way.

For example ``Md5::fz_md5_final2()``.

For all functions described below, there is also a ``ll_*`` variant that
takes/returns raw MuPDF structs instead of wrapper classes.


New functions
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* ``fz_buffer_extract_copy()``: Returns copy of buffer data as a Python ``bytes``.
* ``fz_buffer_storage_memoryview(buffer, writable)``: Returns a readonly/writable Python memoryview onto ``buffer``.
  Relies on ``buffer`` existing and not changing size while the memory view is used.
* ``fz_pixmap_samples_memoryview()``: Returns Python ``memoryview`` onto ``fz_pixmap`` data.

* ``fz_lookup_metadata2(fzdocument, key)``: Return key value or raise an exception if not found:
* ``pdf_lookup_metadata2(pdfdocument, key)``: Return key value or raise an exception if not found:

Implemented in Python
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* ``fz_format_output_path()``
* ``fz_story_positions()``
* ``pdf_dict_getl()``
* ``pdf_dict_putl()``

Non-standard API or implementation
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

* ``fz_buffer_extract()``: Returns a *copy* of the original buffer data as a Python ``bytes``. Still clears the buffer.
* ``fz_buffer_storage()``: Returns ``(size, data)`` where ``data`` is a low-level SWIG representation of the buffer's storage.
* ``fz_convert_color()``: No ``float* fv`` param, instead returns ``(rgb0, rgb1, rgb2, rgb3)``.
* ``fz_fill_text()``: ``color`` arg is tuple/list of 1-4 floats.
* ``fz_lookup_metadata(fzdocument, key)``: Return key value or None if not found:
* ``fz_new_buffer_from_copied_data()``: Takes a Python ``bytes`` (or other Python buffer) instance.
* ``fz_set_error_callback()``: Takes a Python callable; no ``void* user`` arg.
* ``fz_set_warning_callback()``: Takes a Python callable; no ``void* user`` arg.
* ``fz_warn()``: Takes single Python ``str`` arg.
* ``pdf_dict_putl_drop()``: Always raises exception because not useful with automatic ref-counts.
* ``pdf_load_field_name()``: Uses extra C++ function ``pdf_load_field_name2()`` which returns ``std::string`` by value.
* ``pdf_lookup_metadata(pdfdocument, key)``: Return key value or None if not found:
* ``pdf_set_annot_color()``: Takes single ``color`` arg which must be float or tuple of 1-4 floats.
* ``pdf_set_annot_interior_color()``: Takes single ``color`` arg which must be float or tuple of 1-4 floats.
* ``fz_install_load_system_font_funcs()``: Takes Python callbacks with no ``ctx`` arg,
  which can return ``None``, ``fz_font*`` or a ``mupdf.FzFont``.

  Example usage (from ``scripts/mupdfwrap_test.py:test_install_load_system_font()``)::

    def font_f(name, bold, italic, needs_exact_metrics):
        print(f'font_f(): Looking for font: {name=} {bold=} {italic=} {needs_exact_metrics=}.')
        return mupdf.fz_new_font_from_file(...)
    def f_cjk(name, ordering, serif):
        print(f'f_cjk(): Looking for font: {name=} {ordering=} {serif=}.')
        return None
    def f_fallback(script, language, serif, bold, italic):
        print(f'f_fallback(): looking for font: {script=} {language=} {serif=} {bold=} {italic=}.')
        return None
    mupdf.fz_install_load_system_font_funcs(font_f, f_cjk, f_fallback)


Making MuPDF function pointers call Python code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Overview
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

For MuPDF structs with function pointers, we provide a second C++ wrapper
class for use by the Python bindings.

* The second wrapper class has a ``2`` suffix, for example ``PdfFilterOptions2``.

* This second wrapper class has a virtual method for each function pointer, so
  it can be used as a `SWIG Director class <https://swig.org/Doc4.0/SWIGDocumentation.html#SWIGPlus_target_language_callbacks>`_.

* Overriding a virtual method in Python results in the Python method being
  called when MuPDF C code calls the corresponding function pointer.

* One needs to activate the use of a Python method as a callback by calling the
  special method ``use_virtual_<method-name>()``. [It might be possible in future
  to remove the need to do this.]

* It may be possible to use similar techniques in C# but this has not been
  tried.


Callback args
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Python callbacks have args that are more low-level than in the rest of the
Python API:

* Callbacks generally have a first arg that is a SWIG representation of a MuPDF
  ``fz_context*``.

* Where the underlying MuPDF function pointer has an arg that is a pointer to
  an MuPDF struct, unlike elsewhere in the MuPDF bindings we do not translate
  this into an instance of the corresponding wrapper class. Instead Python
  callbacks will see a SWIG representation of the low-level C pointer.

  * It is not safe to construct a Python wrapper class instance directly from
    such a SWIG representation of a C pointer, because it will break MuPDF's
    reference counting - Python/C++ constructors that take a raw pointer to a
    MuPDF struct do not call ``fz_keep_*()`` but the corresponding Python/C++
    destructor will call ``fz_drop_*()``.

  * It might be safe to create an wrapper class instance using an explicit call
    to ``mupdf.fz_keep_*()``, but this has not been tried.

* As of 2023-02-03, exceptions from Python callbacks are propagated back
  through the Python, C++, C, C++ and Python layers. The resulting Python
  exception will have the original exception text, but the original Python
  backtrace is lost.


Exceptions in callbacks
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Python exceptions in Director callbacks are propagated back through the
language layers (from Python to C++ to C, then back to C++ and finally to
Python).

For convenience we add a text representation of the original Python backtrace
to the exception text, but the C layer's fz_try/catch exception handling only
holds 256 characters of exception text, so this backtrace information may be
truncated by the time the exception reaches the original Python code's ``except ...`` block.

Example
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Here is an example PDF filter written in Python that removes alternating items:

**Details**

|expand_begin|

.. code-block::

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
        for p in range(document.pdf_count_pages()):
            page = document.pdf_load_page(p)
            print( f'Running document.pdf_filter_page_contents on page {p}')
            document.pdf_begin_operation('test filter')
            document.pdf_filter_page_contents(page, filter_)
            document.pdf_end_operation()

        document.pdf_save_document('foo.pdf', mupdf.PdfWriteOptions())

|expand_end|








.. External links
