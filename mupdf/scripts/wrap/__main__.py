#!/usr/bin/env python3

'''
Support for generating C++ and python wrappers for the mupdf API.

Overview:

    We generate C++, Python and C# wrappers.


C++ wrapping:

    Namespaces:

        All generated functions and classes are in the 'mupdf' namespace.

    Wrapper classes:

        For each MuPDF C struct, we provide a wrapper class with a CamelCase
        version of the struct name, e.g. the wrapper for fz_display_list is
        mupdf::FzDisplayList.

        These wrapper classes generally have a member `m_internal` that is a
        pointer to an instance of the underlying struct.

        Member functions:

            Member functions are provided which wrap all relevant MuPDF C
            functions (those with first arg being a pointer to an instance of
            the C struct). These methods have the same name as the wrapped
            function.

            They generally take args that are references to wrapper classes
            instead of pointers to MuPDF C structs, and similarly return
            wrapper classes by value instead of returning a pointer to a MuPDF
            C struct.

        Reference counting:

            Wrapper classes automatically take care of reference counting, so
            user code can freely use instances of wrapper classes as required,
            for example making copies and allowing instances to go out of
            scope.

            Lifetime-related functions - constructors, copy constructors,
            operator= and destructors - make internal calls to
            `fz_keep_<structname>()` and `fz_drop_<structname>()` as required.

            Raw constructors that take a pointer to an underlying MuPDF struct
            do not call `fz_keep_*()` - it is expected that any supplied MuPDF
            struct is already owned. Most of the time user code will not need
            to use raw constructors directly.

            Debugging reference counting:

                If environmental variable MUPDF_check_refs is "1", we do
                runtime checks of the generated code's handling of structs that
                have a reference count (i.e. they have a `int refs;` member).

                If the number of wrapper class instances for a particular MuPDF
                struct instance is more than the `.ref` value for that struct
                instance, we generate a diagnostic and call `abort()`.

                We also output reference-counting diagnostics each time a
                wrapper class constructor, member function or destructor is
                called.

        POD wrappers:

            For simple POD structs such as `fz_rect` which are not reference
            counted, the wrapper class's `m_internal` can be an instance of
            the underlying struct instead of a pointer. Some wrappers for POD
            structs take this one step further and embed the struct members
            directly in the wrapper class.

    Wrapper functions:

        Class-aware wrappers:

            We provide a class-aware wrapper for each MuPDF C function; these
            have the same name as the MuPDF C function and are identical to
            the corresponding class member function except that they take an
            explicit first arg instead of the implicit C++ `this`.

        Low-level wrappers:

            We provide a low-level wrapper for each C MuPDF function; these
            have a `ll_` prefix, do not take a 'fz_context* ctx' arg, and
            convert any fz_try..fz_catch exceptions into C++ exceptions.

            Most calling code should use class-aware wrapper functions or
            wrapper class methods in preference to these low-level wrapper
            funtions.

    Text representation of POD data:

        For selected POD MuPDF structs, we provide functions that give a
        labelled text representation of the data, for example a `fz_rect` will
        be represented like:

            (x0=90.51 y0=160.65 x1=501.39 y1=215.6)

        Text representation of a POD wrapper class:

            * An `operator<< (std::ostream&, <wrapperclass>&)` overload for the wrapper class.
            * A member function `std::string to_string();` in the wrapper class.

        Text representation of a MuPDF POD C struct:

            * Function `std::string to_string( const <structname>&);`.
            * Function `std::string to_string_<structname>( const <structname>&);`.

    Examples:

        MuPDF C API:

            fz_device *fz_begin_page(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox);

        MuPDF C++ API:

            namespace mupdf
            {
                struct FzDevice
                {
                    ...
                    fz_device* m_internal;
                };

                struct FzDocumentWriter
                {
                    ...
                    FzDevice fz_begin_page(FzRect& mediabox);
                    ...
                    fz_document_writer* m_internal;
                };

                FzDevice fz_begin_page(const FzDocumentWriter& wri, FzRect& mediabox);

                fz_device *ll_fz_begin_page(fz_document_writer *wri, fz_rect mediabox);
            }

        Environmental variables control runtime diagnostics in debug builds of
        generated code:

            MUPDF_trace
                If "1", generated code outputs a diagnostic each time it calls
                a MuPDF function, showing the args.

            MUPDF_trace_director
                If "1", generated code outputs a diagnostic when doing special
                handling of MuPDF structs containing function pointers.

            MUPDF_trace_exceptions
                If "1", generated code outputs diagnostics when we catch a
                MuPDF setjmp/longjmp exception and convert it into a C++
                exception.

            MUPDF_check_refs
                If "1", generated code checks MuPDF struct reference counts at
                runtime. See below for details.

    Details:

        We use clang-python to parse the MuPDF header files, and generate C++
        headers and source code that gives wrappers for all MuPDF functions.

        We also generate C++ classes that wrap all MuPDF structs, adding in
        various constructors and methods that wrap auto-detected MuPDF C
        functions, plus explicitly-specified methods that wrap/use MuPDF C
        functions.

        More specifically, for each wrapper class:

            Copy constructors/operator=:

                If `fz_keep_<name>()` and `fz_drop_<name>()` exist, we generate
                copy constructor and `operator=()` that use these functions.

            Constructors:

                We look for all MuPDF functions called `fz_new_*()` or
                `pdf_new_*()` that return a pointer to the wrapped class, and
                wrap these into constructors. If any of these constructors have
                duplicate prototypes, we cannot provide them as constructors so
                instead we provide them as static methods. This is not possible
                if the class is not copyable, in which case we include the
                constructor code but commented-out and with an explanation.

            Methods:

                We look for all MuPDF functions that take the wrapped struct as
                a first arg (ignoring any `fz_context*` arg), and wrap these
                into auto-generated class methods. If there are duplicate
                prototypes, we comment-out all but the first.

                Auto-generated methods are omitted if a custom method is
                defined with the same name.

            Other:

                There are various subleties with wrapper classes for MuPDF
                structs that are not copyable etc.

        Internal `fz_context*`'s:

            `mupdf::*` functions and methods generally have the same args
            as the MuPDF functions that they wrap except that they don't
            take any `fz_context*` parameter. When required, per-thread
            `fz_context`'s are generated automatically at runtime, using
            `platform/c++/implementation/internal.cpp:internal_context_get()`.

        Extra items:

            `mupdf::metadata_keys`: This is a global const vector of
            strings contains the keys that are suitable for passing to
            `fz_lookup_metadata()` and its wrappers.

        Output parameters:

            We provide two different ways of wrapping functions with
            out-params.

            Using SWIG OUTPUT markers:

                First, in generated C++ prototypes, we use `OUTPUT` as
                the name of out-params, which tells SWIG to treat them as
                out-params. This works for basic out-params such as `int*`, so
                SWIG will generate Python code that returns a tuple and C# code
                that takes args marked with the C# keyword `out`.

            Unfortunately SWIG doesn't appear to handle out-params that
            are zero terminated strings (`char**`) and cannot generically
            handle binary data out-params (often indicated with `unsigned
            char**`). Also, SWIG-generated C# out-params are a little
            inconvenient compared to returning a C# tuple (requires C# 7 or
            later).

            So we provide an additional mechanism in the generated C++.

            Out-params in a struct:

                For each function with out-params, we provide a class
                containing just the out-params and a function taking just the
                non-out-param args, plus a pointer to the class. This function
                fills in the members of this class instead of returning
                individual out-params. We then generate extra Python or C# code
                that uses these special functions to get the out-params in a
                class instance and return them as a tuple in both Python and
                C#.

            Binary out-param data:

                Some MuPDF functions return binary data, typically with an
                `unsigned char**` out-param. It is not possible to generically
                handle these in Python or C# because the size of the returned
                buffer is specified elsewhere (for example in a different
                out-param or in the return value). So we generate custom Python
                and C# code to give a convenient interface, e.g. copying the
                returned data into a Python `bytes` object or a C# byte array.


Python wrapping:

    We generate a Python module called `mupdf` which directly wraps the C++ API,
    using identical names for functions, classes and methods.

    Out-parameters:

        Functions and methods that have out-parameters are modified to return
        the out-parameters directly, usually as a tuple.

        Examples:

            `fz_read_best()`:

                MuPDF C function:

                    `fz_buffer *fz_read_best(fz_context *ctx, fz_stream *stm, size_t initial, int *truncated);`

                Class-aware C++ wrapper:

                    `FzBuffer read_best(FzStream& stm, size_t initial, int *truncated);`

                Class-aware python wrapper:

                    `def read_best(stm, initial)`

                and returns: `(buffer, truncated)`, where `buffer` is a SWIG
                proxy for a `FzBuffer` instance and `truncated` is an integer.

            `pdf_parse_ind_obj()`:

                MuPDF C function:

                    `pdf_obj *pdf_parse_ind_obj(fz_context *ctx, pdf_document *doc, fz_stream *f, int *num, int *gen, int64_t *stm_ofs, int *try_repair);`

                Class-aware C++ wrapper:

                    `PdfObj pdf_parse_ind_obj(PdfDocument& doc, const FzStream& f, int *num, int *gen, int64_t *stm_ofs, int *try_repair);`

                Class-aware Python wrapper:

                    `def pdf_parse_ind_obj(doc, f)`

                and returns: (ret, num, gen, stm_ofs, try_repair)

    Special handing if `fz_buffer` data:

        Generic data access:

            `mupdf.python_buffer_data(b: bytes)`:
                Returns SWIG proxy for an `unsigned char*` that points to
                `<b>`'s data.

            `mupdf.raw_to_python_bytes(data, size):`
                Returns Python `bytes` instance containing copy of data
                specified by `data` (a SWIG proxy for a `const unsigned char*
                c`) and `size` (the length of the data).

        Wrappers for `fz_buffer_extract()`:

            These return a Python `bytes` instance containing a copy of the
            buffer's data and the buffer is left empty. This is equivalent to
            the underlying fz_buffer_extract() function, but it involves an
            internal copy of the data.

            New function `fz_buffer_extract_copy` and new method
            `FzBuffer.buffer_extract_copy()` are like `fz_buffer_extract()`
            except that they don't clear the buffer. They have no direct
            analogy in the C API.

        Wrappers for `fz_buffer_storage()`:

            These return `(size, data)` where `data` is a low-level
            SWIG representation of the buffer's storage. One can call
            `mupdf.raw_to_python_bytes(data, size)` to get a Python `bytes`
            object containing a copy of this data.

        Wrappers for `fz_new_buffer_from_copied_data()`:

            These take a Python `bytes` instance.

            One can create an MuPDF buffer that contains a copy of a Python
            `bytes` by using the special `mupdf.python_buffer_data()`
            function. This returns a SWIG proxy for an `unsigned char*` that
            points to the `bytes` instance's data:

                ```
                bs = b'qwerty'
                buffer_ = mupdf.new_buffer_from_copied_data(mupdf.python_buffer_data(bs), len(bs))
                ```

    Functions taking a `va_list` arg:

        We do not provide Python wrappers for functions such as `fz_vsnprintf()`.

    Details:

        The Python module is generated using SWIG.

        Out-parameters:

            Out-parameters are not implemented using SWIG typemaps because it's
            very difficult to make things work that way. Instead we internally
            create a struct containing the out-params together with C and
            Python wrapper functions that use the struct to pass the out-params
            back from C into Python.

            The Python function ends up returning the out parameters in the
            same order as they occur in the original function's args, prefixed
            by the original function's return value if it is not void.

            If a function returns void and has exactly one out-param, the
            Python wrapper will return the out-param directly, not as part of a
            tuple.


Tools required to build:

    Clang:

        Clang versions:

            We work with clang-6 or clang-7, but clang-6 appears to not be able
            to cope with function args that are themselves function pointers,
            so wrappers for MuPDF functions are ommited from the generated C++
            code.

        Unix:

            It seems that clang-python packages such as Debian's python-clang
            and OpenBSD's py3-llvm require us to explicitly specify the
            location of libclang, so we search in various locations.

            Alternatively on Linux one can (perhaps in a venv) do:

                pip install libclang

            This makes clang available directly as a Python module.

        On Windows, one must install clang-python with:

            pip install libclang

    SWIG for Python bindings:

        We work with swig-3 and swig-4. If swig-4 is used, we propogate
        doxygen-style comments for structures and functions into the generated
        C++ code.

    Mono for C# bindings on Unix.


Building Python bindings:

    Build and install the MuPDF Python bindings as module `mupdf` in a Python
    virtual environment, using MuPDF's `setup.py` script:

        Linux:
            > python3 -m venv pylocal
            > . pylocal/bin/activate
            (pylocal) > pip install pyqt5 libclang
            (pylocal) > cd .../mupdf
            (pylocal) > python setup.py install

        Windows:
            > py -m venv pylocal
            > pylocal\\Scripts\\activate
            (pylocal) > pip install libclang pyqt5
            (pylocal) > cd ...\mupdf
            (pylocal) > python setup.py install

        OpenBSD:
            [It seems that pip can't install pyqt5 or libclang so instead we
            install system packages and use --system-site-packages.]

            > sudo pkg_add py3-llvm py3-qt5
            > python3 -m venv --system-site-packages pylocal
            > . pylocal/bin/activate
            (pylocal) > cd .../mupdf
            (pylocal) > python setup.py install

        Use the mupdf module:
            (pylocal) > python
            >>> import mupdf
            >>>

    Build MuPDF Python bindings without a Python virtual environment, using
    scripts/mupdfwrap.py:

        [Have not yet found a way to use clang from python on Windows without a
        virtual environment, so this is Unix-only.]

        > cd .../mupdf

        Install required packages:
            Debian:
                > sudo apt install clang python3-clang python3-dev swig

            OpenBSD:
                > pkg_add py3-llvm py3-qt5

        Build and test:
            > ./scripts/mupdfwrap.py -d build/shared-release -b all --test-python

        Use the mupdf module by setting PYTHONPATH:
            > PYTHONPATH=build/shared-release python3
            >>> import mupdf
            >>>


Building C# bindings:

    Build MuPDF C# bindings using scripts/mupdfwrap.py:

        > cd .../mupdf

        Install required packages:
            Debian:
                > sudo apt install clang python3-clang python3-dev mono-devel

            OpenBSD:
                > sudo pkg_add py3-llvm py3-qt5 mono

        Build and test:
            > ./scripts/mupdfwrap.py -d build/shared-release -b --csharp all --test-csharp


Generated files:

    build/  [Generated by '-b m13']
        shared-release/     [Unix runtime files.]

            libmupdf.so         [MuPDF C library.]
            libmupdfcpp.so      [MuPDF C++ library, built from platform/c++/implementation/*.cpp.]

            _mupdf.so           [MuPDF Python internals, built from platform/python/mupdfcpp_swig.cpp.]
            mupdf.py            [MuPDF Python module from swig.]

            mupdfcsharp.so      [MuPDF C# internals, built from platform/csharp/mupdfcpp_swig.cpp.]
            mupdf.cs            [MuPDF C#, copied from platform/csharp/mupdf.cs.]

        shared-release-x64-py3.8/   [Windows runtime files.]

            mupdfcpp64.dll          [MuPDF C and C++ library.]

            _mupdf.pyd              [MuPDF Python internals, built from platform/python/mupdfcpp_swig.cpp.]
            mupdf.py                [MuPDF Python module from swig.]

            mupdfcsharp.dll         [MuPDF C# internals, built from platform/csharp/mupdfcpp_swig.cpp.]
            mupdf.cs                [MuPDF C#, copied from platform/csharp/mupdf.cs.]

    platform/
        c++/    [Files generated by '-b 0']
            include/ [C++ header files]
                mupdf/
                    classes.h
                    classes2.h
                    exceptions.h
                    functions.h
                    internal.h

            implementation/ [C++ source code]
                classes.cpp
                exceptions.cpp
                functions.cpp
                internal.cpp

            [Various misc auto-generated files, used in Windows builds.]

            generated.pickle    [Information from clang-parse of MuPDF headers.]
            windows_mupdf.def   [List of MuPDF public global data, used when linking mupdfcpp.dll.]

        csharp/ [Files generated by '-b --csharp 1 2'.]
            mupdf.cs            [Generated by swig.]
            mupdfcpp_swig.cpp   [Generated by swig.]
            mupdfcpp_swig.i     [Input to swig.]

        python/ [Files generated by '-b 2'.]
            mupdfcpp_swig.cpp   [Generated swig.]
            mupdfcpp_swig.i     [Input to swig.]

        win32/
            Release/    [Windows 32-bit .dll, .lib, .exp, .pdb etc.]
            x64/
                Release/    [Windows 64-bit .dll, .lib, .exp, .pdb etc.]


Windows builds:

    Required predefined macros:

        Code that will use the MuPDF DLL must be built with FZ_DLL_CLIENT
        predefined.

        The MuPDF DLL itself is built with FZ_DLL predefined.

    DLLs:

        There is no separate C library, instead the C and C++ API are
        both in mupdfcpp.dll, which is built by running devenv on
        platform/win32/mupdf.sln.

        The Python SWIG library is called _mupdf.pyd which,
        despite the name, is a standard Windows DLL, built from
        platform/python/mupdfcpp_swig.cpp.

    DLL export of functions and data:

        On Windows, include/mupdf/fitz/export.h defines FZ_FUNCTION and FZ_DATA
        to __declspec(dllexport) and/or __declspec(dllimport) depending on
        whether FZ_DLL or FZ_DLL_CLIENT are defined.

        All MuPDF headers prefix declarations of public global data with
        FZ_DATA.

        All generated C++ code prefixes functions with FZ_FUNCTION and data
        with FZ_DATA.

        When building mupdfcpp.dll on Windows we link with the auto-generated
        platform/c++/windows_mupdf.def file; this lists all C public global
        data.

        For reasons that i don't yet understand, we don't seem to need to tag
        C functions with FZ_FUNCTION, but this is required for C++ functions
        otherwise we get unresolved symbols when building MuPDF client code.

    Building the DLLs:

        We build Windows binaries by running devenv.com directly. We search
        for this using scripts/wdev.py.

        Building _mupdf.pyd is tricky because it needs to be built with a
        specific Python.h and linked with a specific python.lib. This is done
        by setting environmental variables MUPDF_PYTHON_INCLUDE_PATH and
        MUPDF_PYTHON_LIBRARY_PATH when running devenv.com, which are referenced
        by platform/win32/mupdfpyswig.vcxproj. Thus one cannot easily build
        _mupdf.pyd directly from the Visual Studio GUI.

        [In the git history there is code that builds _mupdf.pyd by running the
        Windows compiler and linker cl.exe and link.exe directly, which avoids
        the complications of going via devenv, at the expense of needing to
        know where cl.exe and link.exe are.]

Usage:

    Args:

        -b      [<args>] <actions>:
        --build [<args>] <actions>:
            Builds some or all of the C++ and python interfaces.

            By default we create source files in:
                mupdf/platform/c++/
                mupdf/platform/python/

            - and .so files in directory specified by --dir-so.

            We avoid unnecessary compiling or running of swig by looking at file
            mtimes. We also write commands to .cmd files which allows us to force
            rebuilds if commands change.

            args:
                --clang-verbose
                    Generate extra diagnostics in action=0 when looking for
                    libclang.so.
                -d <details>
                    If specified, we show extra diagnostics when wrapping
                    functions whose name contains <details>. Can be specified
                    multiple times.
                --devenv <path>
                    Set path of devenv.com script on Windows. If not specified,
                    we search for a suitable Visual Studio installation.
                -f
                    Force rebuilds.
                -j <N>
                    Set -j arg used when action 'm' calls make (not
                    Windows). If <N> is 0 we use the number of CPUs
                    (from Python's multiprocessing.cpu_count()).
                --regress
                    Checks for regressions in generated C++ code and SWIG .i
                    file (actions 0 and 2 below). If a generated file already
                    exists and its content differs from our generated content,
                    show diff and exit with an error. This can be used to check
                    for regressions when modifying this script.
                --refcheck-if <text>
                    Set text used to determine whether to enabling
                    reference-checking code. For example use `--refcheck-if
                    '#if 1'` to always enable, `--refcheck-if '#if 0'` to
                    always disable. Default is '#ifndef NDEBUG'.
                --python
                --csharp
                    Whether to generated bindings for python or C#. Default is
                    --python. If specified multiple times, the last wins.

            <actions> is list of single-character actions which are processed in
            order. If <actions> is 'all', it is replaced by m0123.

                m:
                    Builds libmupdf.so by running make in the mupdf/
                    directory. Default is release build, but this can be changed
                    using --dir-so.

                0:
                    Create C++ source for C++ interface onto the fz_* API. Uses
                    clang-python to parse the fz_* API.

                    Generates various files including:
                        mupdf/platform/c++/
                            implementation/
                                classes.cpp
                                exceptions.cpp
                                functions.cpp
                            include/
                                classes.h
                                classes2.h
                                exceptions.h
                                functions.h

                    If files already contain the generated text, they are not
                    updated, so that mtimes are unchanged.

                    Also removes any other .cpp or .h files from
                    mupdf/platform/c++/{implmentation,include}.

                1:
                    Compile and link source files created by action=0.

                    Generates:
                        <dir-so>/libmupdfcpp.so

                    This gives a C++ interface onto mupdf.

                2:
                    Run SWIG on the C++ source built by action=0 to generate source
                    for python interface onto the C++ API.

                    For example for Python this generates:

                        mupdf/platform/python/mupdfcpp_swig.i
                        mupdf/platform/python/mupdfcpp_swig.cpp
                        mupdf/build/shared-release/mupdf.py

                    Note that this requires action=0 to have been run previously.

                3:
                    Compile and links the mupdfcpp_swig.cpp file created by
                    action=2. Requires libmupdf.so to be available, e.g. built by
                    the --libmupdf.so option.

                    For example for Python this generates:

                        mupdf/build/shared-release/_mupdf.so

                    Along with mupdf/platform/python/mupdf.py (generated by
                    action=2), this implements the mupdf python module.

                .:
                    Ignores following actions; useful to quickly avoid unnecessary
                    rebuild if it is known to be unnecessary.

        --check-headers [-k] <which>
            Runs cc on header files to check they #include all required headers.

            -k:
                If present, we carry on after errors.
            which:
                If 'all', we run on all headers in .../mupdf/include. Otherwise
                if <which> ends with '+', we run on all remaining headers in
                .../mupdf/include starting with <which>. Otherwise the name of
                header to test.

        --compare-fz_usage <directory>
            Finds all fz_*() function calls in git files within <directory>, and
            compares with all the fz_*() functions that are wrapped up as class
            methods.

            Useful to see what functionality we are missing.

        --diff
            Compares generated files with those in the mupdfwrap_ref/ directory
            populated by --ref option.

        -d
        --dir-so <directory>
            Set build directory.

            Default is: build/shared-release

            We use different C++ compile flags depending on release or debug
            builds (specifically, the definition of NDEBUG is important because
            it must match what was used when libmupdf.so was built).

            If <directory> starts with `build/fpic-`, the C and C++ API are
            built as `.a` archives but compiled with -fPIC so that they can be
            linked into shared libraries.

            If <directory> is '-' we do not set any paths when running tests
            e.g. with --test-python. This is for testing after installing into
            a venv.

            Examples:
                -d build/shared-debug
                -d build/shared-release [default]

            On Windows one can specify the CPU and Python version; we then
            use 'py -0f' to find the matching installed Python along with its
            Python.h and python.lib. For example:

                -d build/shared-release-x32-py3.8
                -d build/shared-release-x64-py3.9

        --doc <languages>
            Generates documentation for the different APIs in
            mupdf/docs/generated/.

            <languages> is either 'all' or a comma-separated list of API languages:

                c
                    Generate documentation for the C API with doxygen:
                        include/html/index.html
                c++
                    Generate documentation for the C++ API with doxygen:
                        platform/c++/include/html/index.html
                python
                    Generate documentation for the Python API using pydoc3:
                        platform/python/mupdf.html

            Also see '--sync-docs' option for copying these generated
            documentation files elsewhere.

        --make <make-command>
            Override make command, e.g. `--make gmake`.
            If not specified, we use $MUPDF_MAKE. If this is not set, we use
            `make` (or `gmake` on OpenBSD).

        --ref
            Copy generated C++ files to mupdfwrap_ref/ directory for use by --diff.

        --run-py <arg> <arg> ...
            Runs command with LD_LIBRARY_PATH and PYTHONPATH set up for use with
            mupdf.py.

            Exits with same code as the command.

        --swig <swig>
            Sets the swig command to use.

            If this is version 4+, we use the <swig> -doxygen to copy
            over doxygen-style comments into mupdf.py. Otherwise we use
            '%feature("autodoc", "3");' to generate comments with type information
            for args in mupdf.py. [These two don't seem to be usable at the same
            time in swig-4.]

        --swig-windows-auto
            Downloads swig if not present in current directory, extracts
            swig.exe and sets things up to use it subsequently.

        --sync-docs <destination>
            Use rsync to copy contents of docs/generated/ to remote destination.

        --sync-pretty <destination>
            Use rsync to copy generated C++ and Python files to <destination>. Also
            uses generates and copies .html versions of these files that use
            run_prettify.js from cdn.jsdelivr.net to show embelished content.

        --test-csharp
            Tests the experimental C# API.

        --test-python
            Tests the python API.

        --test-python-fitz [<options>] all|iter|<script-name>
            Tests fitz.py with PyMuPDF. Requires 'pkg_add py3-test' or similar.
            options:
                Passed to py.test-3.
                    -x: stop at first error.
                    -s: show stdout/err.
            all:
                Runs all tests with py.test-3
            iter:
                Runs each test in turn until one fails.
            <script-name>:
                Runs a single test, e.g.: test_general.py

        --test-setup.py <arg>
            Tests that setup.py installs a usable Python mupdf module.

                * Creates a Python virtual environment.
                * Activates the Python environment.
                * Runs setup.py install.
                    * Builds C, C++ and Python librariess in build/shared-release.
                    * Copies build/shared-release/*.so into virtual envionment.
                * Runs scripts/mupdfwrap_test.py.
                    * Imports mupdf and checks basic functionality.
                * Deactivates the Python environment.

        --venv
            If specified, should be the first arg in the command line.

            Re-runs mupdfwrap.py in a Python venv containing libclang
            and swig, passing remaining args.

        --vs-upgrade 0 | 1
            If 1, we use a copy of the Windows build file tree
            `platform/win32/` called `platform/win32-vs-upgrade`, modifying the
            copied files with `devenv.com /upgrade`.

            For example this allows use with Visual Studio 2022 if it doesn't
            have the v142 tools installed.

        --windows-cmd ...
            Runs mupdfwrap.py via cmd.exe, passing remaining args. Useful to
            get from cygwin to native Windows.

            E.g.:
                --windows-cmd --venv --swig-windows-auto -b all

    Examples:

        ./scripts/mupdfwrap.py -b all -t
            Build all (release build) and test.

        ./scripts/mupdfwrap.py -d build/shared-debug -b all -t
            Build all (debug build) and test.

        ./scripts/mupdfwrap.py -b 0 --compare-fz_usage platform/gl
            Compare generated class methods with functions called by platform/gl
            code.

        python3 -m cProfile -s cumulative ./scripts/mupdfwrap.py -b 0
            Profile generation of C++ source code.

        ./scripts/mupdfwrap.py --venv --swig-windows-auto -b all -t
            Build and test on Windows.


'''

import glob
import multiprocessing
import os
import pickle
import platform
import re
import shlex
import shutil
import sys
import sysconfig
import tempfile
import textwrap

if platform.system() == 'Windows':
    '''
    shlex.quote() is broken.
    '''
    def quote(text):
        if ' ' in text:
            if '"' not in text:
                return f'"{text}"'
            if "'" not in text:
                return f"'{text}'"
            assert 0, f'Cannot handle quotes in {text=}'
        return text
    shlex.quote = quote

try:
    import resource
except ModuleNotFoundError:
    # Not available on Windows.
    resource = None

import jlib
import pipcl
import wdev

from . import classes
from . import cpp
from . import make_cppyy
from . import parse
from . import state
from . import swig

clang = state.clang


# We use f-strings, so need python-3.6+.
assert sys.version_info[0] == 3 and sys.version_info[1] >= 6, (
        'We require python-3.6+')


def compare_fz_usage(
        tu,
        directory,
        fn_usage,
        ):
    '''
    Looks for fz_ items in git files within <directory> and compares to what
    functions we have wrapped in <fn_usage>.
    '''

    filenames = jlib.system( f'cd {directory}; git ls-files .', out='return')

    class FzItem:
        def __init__( self, type_, uses_structs=None):
            self.type_ = type_
            if self.type_ == 'function':
                self.uses_structs = uses_structs

    # Set fz_items to map name to info about function/struct.
    #
    fz_items = dict()
    for cursor in parse.get_members(tu.cursor):
        name = cursor.spelling
        if not name.startswith( ('fz_', 'pdf_')):
            continue
        uses_structs = False
        if (1
                and name.startswith( ('fz_', 'pdf_'))
                and cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL
                and (
                    cursor.linkage == clang.cindex.LinkageKind.EXTERNAL
                    or
                    cursor.is_definition()  # Picks up static inline functions.
                    )
                ):
            def uses_struct( type_):
                '''
                Returns true if <type_> is a fz struct or pointer to fz struct.
                '''
                if type_.kind == clang.cindex.TypeKind.POINTER:
                    type_ = type_.get_pointee()
                type_ = parse.get_name_canonical( type_)
                if type_.spelling.startswith( 'struct fz_'):
                    return True
            # Set uses_structs to true if fn returns a fz struct or any
            # argument is a fz struct.
            if uses_struct( cursor.result_type):
                uses_structs = True
            else:
                for arg in parse.get_args( tu, cursor):
                    if uses_struct( arg.cursor.type):
                        uses_structs = True
                        break
            if uses_structs:
                pass
                #log( 'adding function {name=} {uses_structs=}')
            fz_items[ name] = FzItem( 'function', uses_structs)

    directory_names = dict()
    for filename in filenames.split( '\n'):
        if not filename:
            continue
        path = os.path.join( directory, filename)
        jlib.log( '{filename!r=} {path=}')
        with open( path, 'r', encoding='utf-8', errors='replace') as f:
            text = f.read()
        for m in re.finditer( '(fz_[a-z0-9_]+)', text):

            name = m.group(1)
            info = fz_items.get( name)
            if info:
                if (0
                        or (info.type_ == 'function' and info.uses_structs)
                        or (info.type_ == 'fz-struct')
                        ):
                    directory_names.setdefault( name, 0)
                    directory_names[ name] += 1

    name_max_len = 0
    for name, n in sorted( directory_names.items()):
        name_max_len = max( name_max_len, len( name))

    n_missing = 0
    fnnames = sorted( fn_usage.keys())
    for fnname in fnnames:
        classes_n, cursor = fn_usage[ fnname]
        directory_n = directory_names.get( name, 0)
        if classes_n==0 and directory_n:
            n_missing += 1
            jlib.log( '    {fnname:40} {classes_n=} {directory_n=}')

    jlib.log( '{n_missing}')


def windows_find_python_py( cpu=None, version=None):
    '''
    Windows only. Looks for python matching `cpu` and `version`, by parsing
    output of `py -0p`.
    '''
    wp = wdev.WindowsPython(cpu=cpu, version=version)
    command = f'{wp.path} -c "import sysconfig; print( sysconfig.get_path(\'include\'))"'
    include = jlib.system( command, out='return').strip()
    return wp.cpu, wp.version, wp.path, wp.root, include


def windows_find_python( cpu=None, version=None):
    '''
    Windows only. Finds installed Python with specific word size and version.

    cpu:
        A Cpu instance. If None, we use whatever we are running on.
    version:
        Two-digit Python version as a string such as '3.8'. If None we use
        current Python's version.

    Returns (python, version, root, cpu, include):

        python:
            Path of python binary.
        version:
            Version as a string, e.g. '3.9'. Same as <version> if not None,
            otherwise the inferred version.
        root:
            The parent directory of <python>; allows
            Python headers to be found, for example
            <root>/include/Python.h.
        cpu:
            A Cpu instance, same as <cpu> if not None, otherwise the inferred
            cpu.
        include:
            Directory containing `Python.h`.

    We look at current Python first; if that doesn't match, we use
    windows_find_python_py() to parse the output from 'py -0p' to look at all
    available python installations.
    '''
    assert state.state_.windows
    if cpu is None:
        cpu = state.Cpu()
    if version is None:
        version = state.python_version()
    jlib.log( 'Looking for python matching {cpu=} {version=}')

    current_cpu = state.Cpu()
    current_version = f'{sys.version_info[0]}.{sys.version_info[1]}'
    if cpu.name == current_cpu.name and version == current_version:
        # Current python matches.
        jlib.log( 'This invocation of Python matches {=cpu version}')
        python = jlib.fs_find_in_paths( sys.executable)
        root = os.path.dirname( python)
        include = sysconfig.get_path('include')

    else:
        # Look for other installed python.
        jlib.log( 'Current python {=current_cpu current_version} does not match {=cpu version}')
        cpu, version, python, root, include = windows_find_python_py( cpu, version)

    jlib.log( '{cpu=} {version=}. Returning:')
    jlib.log( '    {python=}')
    jlib.log( '    {root=}')
    jlib.log( '    {include=}')
    return cpu, version, python, root, include


g_have_done_build_0 = False


def _test_get_m_command():
    '''
    Tests _get_m_command().
    '''
    def test( dir_so, expected_command):
        build_dirs = state.BuildDirs()
        build_dirs.dir_so = dir_so
        command, actual_build_dir = _get_m_command( build_dirs)
        assert command == expected_command, f'\nExpected: {expected_command}\nBut:      {command}'

    mupdf_root = os.path.abspath( f'{__file__}/../../../')
    infix = 'CXX=clang++ ' if state.state_.openbsd else ''

    test(
            'shared-release',
            f'cd {mupdf_root} && {infix}gmake HAVE_GLUT=no HAVE_PTHREAD=yes verbose=yes shared=yes build=release build_prefix=shared-',
            )
    test(
            'mupdfpy-amd64-shared-release',
            f'cd {mupdf_root} && {infix}gmake HAVE_GLUT=no HAVE_PTHREAD=yes verbose=yes shared=yes build=release build_prefix=mupdfpy-amd64-shared-',
            )
    test(
            'mupdfpy-amd64-fpic-release',
            f'cd {mupdf_root} && CFLAGS="-fPIC" {infix}gmake HAVE_GLUT=no HAVE_PTHREAD=yes verbose=yes build=release build_prefix=mupdfpy-amd64-fpic-',
            )
    jlib.log( '_get_m_command() ok')


def get_so_version( build_dirs):
    '''
    Returns `.<minor>.<patch>` from include/mupdf/fitz/version.h.

    Returns '' on macos.
    '''
    if state.state_.macos or state.state_.pyodide:
        return ''
    d = dict()
    def get_v( name):
        path = f'{build_dirs.dir_mupdf}/include/mupdf/fitz/version.h'
        with open( path) as f:
            for line in f:
                m = re.match(f'^#define {name} (.+)\n$', line)
                if m:
                    return m.group(1)
        assert 0, f'Cannot find #define of {name=} in {path=}.'
    major = get_v('FZ_VERSION_MAJOR')
    minor = get_v('FZ_VERSION_MINOR')
    patch = get_v('FZ_VERSION_PATCH')
    return f'.{minor}.{patch}'


def _get_m_command( build_dirs, j=None, make=None):
    '''
    Generates a `make` command for building with `build_dirs.dir_mupdf`.

    Returns `(command, actual_build_dir, suffix)`.
    '''
    assert not state.state_.windows, 'Cannot do "-b m" on Windows; C library is integrated into C++ library built by "-b 01"'
    #jlib.log( '{build_dirs.dir_mupdf=}')
    if not make:
        make = os.environ.get('MUPDF_MAKE')
        jlib.log('Overriding from $MUPDF_MAKE={make}.')
    if not make:
        if state.state_.openbsd:
            # Need to run gmake, not make. Also for some
            # reason gmake on OpenBSD sets CC to clang, but
            # CXX to g++, so need to force CXX=clang++ too.
            #
            make = 'CXX=clang++ gmake'
    if not make:
        make = 'make'

    if j is not None:
        if j == 0:
            j = multiprocessing.cpu_count()
            jlib.log('Setting -j to  multiprocessing.cpu_count()={j}')
        make += f' -j {j}'
    flags = os.path.basename( build_dirs.dir_so).split('-')
    actual_build_dir = f'{build_dirs.dir_mupdf}/build/'
    make_env = ''
    make_args = ' HAVE_GLUT=no HAVE_PTHREAD=yes verbose=yes'
    suffix = None
    build_prefix = ''
    in_prefix = True
    for i, flag in enumerate( flags):
        if flag in ('x32', 'x64') or re.match('py[0-9]', flag):
            # setup.py puts cpu and python version
            # elements into the build directory name
            # when creating wheels; we need to ignore
            # them.
            jlib.log('Ignoring {flag=}')
        else:
            if 0: pass  # lgtm [py/unreachable-statement]
            elif flag == 'debug':
                make_args += ' build=debug'
                in_prefix = False
            elif flag == 'release':
                make_args += ' build=release'
                in_prefix = False
            elif flag == 'memento':
                make_args += ' build=memento'
                in_prefix = False
            elif flag == 'shared':
                make_args += ' shared=yes'
                # `suffix` determines the name of libraries that we create, for
                # example libmupdfcpp.so, but not libmupdf.so itself - this is
                # created by `Makefile` etc. We do specify libmupdf.so when
                # linking but the suffix is unused and on macos we will use
                # libmupdf.dylib if present.
                suffix = '.so'
                build_prefix += f'{flag}-'
                in_prefix = False
            elif flag == 'tesseract':
                make_args += ' HAVE_LEPTONICA=yes HAVE_TESSERACT=yes'
                build_prefix += f'{flag}-'
            else:
                if not in_prefix:
                    raise Exception( f'Unrecognised flag {flag!r} in {flags!r} in {build_dirs.dir_so!r}')
                if flag == 'fpic':
                    make_env += ' CFLAGS="-fPIC"'
                    suffix = '.a'
                else:
                    #jlib.log(f'Ignoring unrecognised flag {flag!r} in {flags!r} in {build_dirs.dir_so!r}')
                    pass
                build_prefix += f'{flag}-'
            if i:
                actual_build_dir += '-'
            actual_build_dir += flag
    assert suffix, f'Leaf must contain "shared-" or "fpic-": build_dirs.dir_so={build_dirs.dir_so}'
    if build_prefix:
        make_args += f' build_prefix={build_prefix}'
    command = f'cd {build_dirs.dir_mupdf} &&'
    if make_env:
        command += make_env
    command += f' {make}{make_args}'

    return command, actual_build_dir, suffix

_windows_vs_upgrade_cache = dict()
def _windows_vs_upgrade( vs_upgrade, build_dirs, devenv):
    '''
    If `vs_upgrade` is true, creates new
    {build_dirs.dir_mupdf}/platform/win32-vs-upgrade/ tree with upgraded .sln
    and .vcxproj files. Returns 'win32-vs-upgrade'.

    Otherwise returns 'win32'.
    '''
    if not vs_upgrade:
        return 'win32'
    key = (build_dirs, devenv)
    infix = _windows_vs_upgrade_cache.get(key)
    if infix is None:
        infix = 'win32-vs-upgrade'
        prefix1 = f'{build_dirs.dir_mupdf}/platform/win32/'
        prefix2 = f'{build_dirs.dir_mupdf}/platform/{infix}/'
        for dirpath, dirnames, filenames in os.walk( prefix1):
            for filename in filenames:
                if os.path.splitext( filename)[ 1] in (
                        '.sln',
                        '.vcxproj',
                        '.props',
                        '.targets',
                        '.xml',
                        '.c',
                        ):
                    path1 = f'{dirpath}/{filename}'
                    assert path1.startswith(prefix1)
                    path2 = prefix2 + path1[ len(prefix1):]
                    os.makedirs( os.path.dirname(path2), exist_ok=True)
                    jlib.log('Calling shutil.copy2 {path1=} {path2=}')
                    shutil.copy2(path1, path2)
        for path in glob.glob( f'{prefix2}*.sln'):
            jlib.system(f'"{devenv}" {path} /upgrade', verbose=1)
        _windows_vs_upgrade_cache[ key] = infix
    jlib.log('returning {infix=}')
    return infix


def macos_patch( library, *sublibraries):
    '''
    Patches `library` so that all references to items in `sublibraries` are
    changed to `@rpath/<leafname>`.

    library:
        Path of shared library.
    sublibraries:
        List of paths of shared libraries; these have typically been
        specified with `-l` when `library` was created.
    '''
    jlib.log( f'macos_patch(): library={library}  sublibraries={sublibraries}')
    if not state.state_.macos:
        return
    # Find what shared libraries are used by `library`.
    jlib.system( f'otool -L {library}', out='log')
    command = 'install_name_tool'
    names = []
    for sublibrary in sublibraries:
        name = jlib.system( f'otool -D {sublibrary}', out='return').strip()
        name = name.split('\n')
        assert len(name) == 2 and name[0] == f'{sublibrary}:', f'{name=}'
        name = name[1]
        # strip trailing so_name.
        leaf = os.path.basename(name)
        m = re.match('^(.+[.]((so)|(dylib)))[0-9.]*$', leaf)
        assert m
        jlib.log(f'Changing {leaf=} to {m.group(1)}')
        leaf = m.group(1)
        command += f' -change {name} @rpath/{leaf}'
    command += f' {library}'
    jlib.system( command, out='log')
    jlib.system( f'otool -L {library}', out='log')


def build_0(
        build_dirs,
        header_git,
        check_regress,
        clang_info_verbose,
        refcheck_if,
        cpp_files,
        h_files,
        ):
    '''
    Handles `-b 0` - generate C++ bindings source.
    '''
    # Generate C++ code that wraps the fz_* API.

    if state.state_.have_done_build_0:
        # This -b 0 stage modifies global data, for example adding
        # begin() and end() methods to extras[], so must not be run
        # more than once.
        jlib.log( 'Skipping second -b 0')
        return

    jlib.log( 'Generating C++ source code ...')

    # On 32-bit Windows, libclang doesn't work. So we attempt to run 64-bit `-b
    # 0` to generate C++ code.
    jlib.log( '{state.state_.windows=} {build_dirs.cpu.bits=} {sys.maxsize=}')
    if state.state_.windows and build_dirs.cpu.bits == 32:
        try:
            jlib.log( 'Trying dummy call of clang.cindex.Index.create()')
            state.clang.cindex.Index.create()
        except Exception as e:
            py = f'py -{state.python_version()}'
            jlib.log( 'libclang not available on win32; attempting to run separate 64-bit invocation of {sys.argv[0]} with `-b 0`.')
            # We use --venv-force-reinstall to workaround a problem where `pip
            # install libclang` seems to fail to install in the new 64-bit venv
            # if we are in a 'parent' venv created by pip itself. Maybe venv's
            # created by pip are somehow more sticky than plain venv's?
            #
            jlib.system( f'{py} {sys.argv[0]} --venv-force-reinstall -b 0')
            return

    namespace = 'mupdf'
    generated = cpp.Generated()

    cpp.cpp_source(
            build_dirs.dir_mupdf,
            namespace,
            f'{build_dirs.dir_mupdf}/platform/c++',
            header_git,
            generated,
            check_regress,
            clang_info_verbose,
            refcheck_if,
            'debug' in build_dirs.dir_so,
            )

    generated.save(f'{build_dirs.dir_mupdf}/platform/c++')

    def check_lists_equal(name, expected, actual):
        expected.sort()
        actual.sort()
        if expected != actual:
            text = f'Generated {name} filenames differ from expected:\n'
            text += f'    expected {len(expected)}:\n'
            for i in expected:
                text += f'        {i}\n'
            text += f'    generated {len(actual)}:\n'
            for i in actual:
                text += f'        {i}\n'
            raise Exception(text)
    check_lists_equal('C++ source', cpp_files, generated.cpp_files)
    check_lists_equal('C++ headers', h_files, generated.h_files)

    for dir_ in (
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/',
            f'{build_dirs.dir_mupdf}/platform/c++/include/', '.h',
            ):
        for path in jlib.fs_paths( dir_):
            path = path.replace('\\', '/')
            _, ext = os.path.splitext( path)
            if ext not in ('.h', '.cpp'):
                continue
            if path in h_files + cpp_files:
                continue
            jlib.log( 'Removing unknown C++ file: {path}')
            os.remove( path)

    jlib.log( 'Wrapper classes that are containers: {generated.container_classnames=}')

    # Output info about fz_*() functions that we don't make use
    # of in class methods.
    #
    # This is superceded by automatically finding fuctions to wrap.
    #
    if 0:   # lgtm [py/unreachable-statement]
        jlib.log( 'functions that take struct args and are not used exactly once in methods:')
        num = 0
        for name in sorted( fn_usage.keys()):
            n, cursor = fn_usage[ name]
            if n == 1:
                continue
            if not fn_has_struct_args( tu, cursor):
                continue
            jlib.log( '    {n} {cursor.displayname} -> {cursor.result_type.spelling}')
            num += 1
        jlib.log( 'number of functions that we should maybe add wrappers for: {num}')


def link_l_flags(sos):
    ld_origin = None
    if state.state_.pyodide:
        # Don't add '-Wl,-rpath*' etc if building for Pyodide.
        ld_origin = False
    return jlib.link_l_flags( sos, ld_origin)


def build( build_dirs, swig_command, args, vs_upgrade, make_command):
    '''
    Handles -b ...
    '''
    cpp_files   = [
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/classes.cpp',
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/classes2.cpp',
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/exceptions.cpp',
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/functions.cpp',
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/internal.cpp',
            f'{build_dirs.dir_mupdf}/platform/c++/implementation/extra.cpp',
            ]
    h_files = [
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/classes.h',
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/classes2.h',
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/exceptions.h',
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/functions.h',
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/internal.h',
            f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/extra.h',
            ]
    build_python = True
    build_csharp = False
    check_regress = False
    clang_info_verbose = False
    force_rebuild = False
    header_git = False
    j = 0
    refcheck_if = '#ifndef NDEBUG'
    pyodide = state.state_.pyodide
    if pyodide:
        # Looks like Pyodide sets CXX to (for example) /tmp/tmp8h1meqsj/c++. We
        # don't evaluate it here, because that would force a rebuild each time
        # because of the command changing.
        assert os.environ.get('CXX', None), 'Pyodide build but $CXX not defined.'
        compiler = '$CXX'
    elif state.state_.macos:
        compiler = 'c++ -std=c++14'
        # Add extra flags for MacOS cross-compilation, where ARCHFLAGS can be
        # '-arch arm64'.
        #
        archflags = os.environ.get( 'ARCHFLAGS')
        if archflags:
            compiler += f' {archflags}'
    else:
        compiler = 'c++'

    state.state_.show_details = lambda name: False
    devenv = 'devenv.com'
    if state.state_.windows:
        # Search for devenv.com in standard locations.
        windows_vs = wdev.WindowsVS()
        devenv = windows_vs.devenv

    #jlib.log('{build_dirs.dir_so=}')
    details = list()

    while 1:
        actions = args.next()
        if 0:
            pass
        elif actions == '-f':
            force_rebuild = True
        elif actions == '--clang-verbose':
            clang_info_verbose = True
        elif actions == '-d':
            d = args.next()
            details.append( d)
            def fn(name):
                if not name:
                    return
                for detail in details:
                    if detail in name:
                        return True
            state.state_.show_details = fn
        elif actions == '--devenv':
            devenv = args.next()
            jlib.log( '{devenv=}')
            windows_vs = None
            if not state.state_.windows:
                jlib.log( 'Warning: --devenv was specified, but we are not on Windows so this will have no effect.')
        elif actions == '-j':
            j = int(args.next())
        elif actions == '--python':
            build_python = True
            build_csharp = False
        elif actions == '--csharp':
            build_python = False
            build_csharp = True
        elif actions == '--regress':
            check_regress = True
        elif actions == '--refcheck-if':
            refcheck_if = args.next()
            jlib.log( 'Have set {refcheck_if=}')
        elif actions.startswith( '-'):
            raise Exception( f'Unrecognised --build flag: {actions}')
        else:
            break

    if actions == 'all':
        actions = '0123' if state.state_.windows else 'm0123'

    dir_so_flags = os.path.basename( build_dirs.dir_so).split( '-')

    windows_build_type = build_dirs.windows_build_type()
    so_version = get_so_version( build_dirs)

    for action in actions:
        with jlib.LogPrefixScope( f'{action}: '):
            jlib.log( '{action=}', 1)
            if action == '.':
                jlib.log('Ignoring build actions after "." in {actions!r}')
                break

            elif action == 'm':
                # Build libmupdf.so.
                if state.state_.windows:
                    jlib.log( 'Ignoring `-b m` on Windows as not required.')
                else:
                    jlib.log( 'Building libmupdf.so ...')
                    command, actual_build_dir, suffix = _get_m_command( build_dirs, j, make_command)
                    jlib.system( command, prefix=jlib.log_text(), out='log', verbose=1)

                    suffix2 = '.dylib' if state.state_.macos else '.so'
                    p = f'{actual_build_dir}/libmupdf{suffix2}{so_version}'
                    assert os.path.isfile(p), f'Does not exist: {p=}'

                    if actual_build_dir != build_dirs.dir_so:
                        # This happens when we are being run by
                        # setup.py - it it might specify '-d
                        # build/shared-release-x64-py3.8' (which
                        # will be put into build_dirs.dir_so) but
                        # the above 'make' command will create
                        # build/shared-release/libmupdf.so, so we need
                        # to copy into build/shared-release-x64-py3.8/.
                        #
                        jlib.fs_copy( f'{actual_build_dir}/libmupdf{suffix2}', f'{build_dirs.dir_so}/libmupdf{suffix2}', verbose=1)

            elif action == '0':
                build_0(
                        build_dirs,
                        header_git,
                        check_regress,
                        clang_info_verbose,
                        refcheck_if,
                        cpp_files,
                        h_files,
                        )

            elif action == '1':
                # Compile and link generated C++ code to create libmupdfcpp.so.
                if state.state_.windows:
                    # We build mupdfcpp.dll using the .sln; it will
                    # contain all C functions internally - there is
                    # no mupdf.dll.
                    #
                    win32_infix = _windows_vs_upgrade( vs_upgrade, build_dirs, devenv)
                    jlib.log(f'Building mupdfcpp.dll by running devenv ...')
                    build = f'{windows_build_type}Python'
                    if 'tesseract' in dir_so_flags:
                        build += 'Tesseract'
                    build += f'|{build_dirs.cpu.windows_config}'
                    command = (
                            f'cd {build_dirs.dir_mupdf}&&'
                            f'"{devenv}"'
                            f' platform/{win32_infix}/mupdf.sln'
                            f' /Build "{build}"'
                            f' /Project mupdfcpp'
                            )
                    jlib.system(command, verbose=1, out='log')

                    jlib.fs_copy(
                            f'{build_dirs.dir_mupdf}/platform/{win32_infix}/{build_dirs.cpu.windows_subdir}{windows_build_type}/mupdfcpp{build_dirs.cpu.windows_suffix}.dll',
                            f'{build_dirs.dir_so}/',
                            verbose=1,
                            )

                else:
                    jlib.log( 'Compiling generated C++ source code to create libmupdfcpp.so ...')
                    include1 = f'{build_dirs.dir_mupdf}/include'
                    include2 = f'{build_dirs.dir_mupdf}/platform/c++/include'
                    cpp_files_text = ''
                    for i in cpp_files:
                        cpp_files_text += ' ' + os.path.relpath(i)
                    libmupdfcpp = f'{build_dirs.dir_so}/libmupdfcpp.so{so_version}'
                    libmupdf = f'{build_dirs.dir_so}/libmupdf.so{so_version}'
                    if pyodide:
                        # Compile/link separately. Otherwise
                        # emsdk/upstream/bin/llvm-nm: error: a.out: No such
                        # file or directory
                        o_files = list()
                        for cpp_file in cpp_files:
                            o_file = f'{os.path.relpath(cpp_file)}.o'
                            o_files.append(o_file)
                            command = textwrap.dedent(
                                    f'''
                                    {compiler}
                                        -c
                                        -o {o_file}
                                        {build_dirs.cpp_flags}
                                        -fPIC
                                        -I {include1}
                                        -I {include2}
                                        {cpp_file}
                                    ''').strip().replace( '\n', ' \\\n')
                            jlib.build(
                                    [include1, include2, cpp_file],
                                    o_file,
                                    command,
                                    force_rebuild,
                                    )
                        command = ( textwrap.dedent(
                                f'''
                                {compiler}
                                    -o {os.path.relpath(libmupdfcpp)}
                                    -sSIDE_MODULE
                                    {build_dirs.cpp_flags}
                                    -fPIC -shared
                                    -I {include1}
                                    -I {include2}
                                    {" ".join(o_files)}
                                    {link_l_flags(libmupdf)}
                                ''').strip().replace( '\n', ' \\\n')
                                )
                        jlib.build(
                                [include1, include2] + o_files,
                                libmupdfcpp,
                                command,
                                force_rebuild,
                                )

                    elif 'shared' in dir_so_flags:
                        command = ( textwrap.dedent(
                                f'''
                                {compiler}
                                    -o {os.path.relpath(libmupdfcpp)}
                                    {build_dirs.cpp_flags}
                                    -fPIC -shared
                                    -I {include1}
                                    -I {include2}
                                    {cpp_files_text}
                                    {link_l_flags(libmupdf)}
                                ''').strip().replace( '\n', ' \\\n')
                                )
                        jlib.build(
                                [include1, include2] + cpp_files,
                                libmupdfcpp,
                                command,
                                force_rebuild,
                                )
                        macos_patch( libmupdfcpp, f'{build_dirs.dir_so}/libmupdf.dylib{so_version}')
                        if so_version:
                            jlib.system(f'ln -sf libmupdfcpp.so{so_version} {build_dirs.dir_so}/libmupdfcpp.so')

                    elif 'fpic' in dir_so_flags:
                        # We build a .so containing the C and C++ API. This
                        # might be slightly faster than having separate C and
                        # C++ API .so files, but probably makes no difference.
                        #
                        libmupdfcpp = f'{build_dirs.dir_so}/libmupdfcpp.a'
                        libmupdf = []#[ f'{build_dirs.dir_so}/libmupdf.a', f'{build_dirs.dir_so}/libmupdf-third.a']

                        # Compile each .cpp file.
                        ofiles = []
                        for cpp_file in cpp_files:
                            ofile = f'{build_dirs.dir_so}/{os.path.basename(cpp_file)}.o'
                            ofiles.append( ofile)
                            command = ( textwrap.dedent(
                                    f'''
                                    {compiler}
                                        {build_dirs.cpp_flags}
                                        -fPIC
                                        -c
                                        -I {include1}
                                        -I {include2}
                                        -o {ofile}
                                        {cpp_file}
                                    ''').strip().replace( '\n', ' \\\n')
                                    )
                            jlib.build(
                                    [include1, include2, cpp_file],
                                    ofile,
                                    command,
                                    force_rebuild,
                                    verbose=True,
                                    )

                        # Create libmupdfcpp.a containing all .cpp.o files.
                        if 0:
                            libmupdfcpp_a = f'{build_dirs.dir_so}/libmupdfcpp.a'
                            command = f'ar cr {libmupdfcpp_a} {" ".join(ofiles)}'
                            jlib.build(
                                    ofiles,
                                    libmupdfcpp_a,
                                    command,
                                    force_rebuild,
                                    verbose=True,
                                    )

                        # Create libmupdfcpp.so from all .cpp and .c files.
                        libmupdfcpp_so = f'{build_dirs.dir_so}/libmupdfcpp.so'
                        alibs = [
                                f'{build_dirs.dir_so}/libmupdf.a',
                                f'{build_dirs.dir_so}/libmupdf-third.a'
                                ]
                        command = textwrap.dedent( f'''
                                {compiler}
                                    {build_dirs.cpp_flags}
                                    -fPIC -shared
                                    -o {libmupdfcpp_so}
                                    {' '.join(ofiles)}
                                    {' '.join(alibs)}
                                ''').strip().replace( '\n', ' \\\n')
                        jlib.build(
                                ofiles + alibs,
                                libmupdfcpp_so,
                                command,
                                force_rebuild,
                                verbose=True,
                                )
                    else:
                        assert 0, f'Leaf must start with "shared-" or "fpic-": build_dirs.dir_so={build_dirs.dir_so}'

            elif action == '2':
                # Use SWIG to generate source code for python/C# bindings.
                #generated = cpp.Generated(f'{build_dirs.dir_mupdf}/platform/c++')
                with open( f'{build_dirs.dir_mupdf}/platform/c++/generated.pickle', 'rb') as f:
                    generated = pickle.load( f)
                    generated.swig_cpp = generated.swig_cpp.getvalue()
                    generated.swig_cpp_python = generated.swig_cpp_python.getvalue()
                    generated.swig_python = generated.swig_python.getvalue()
                    generated.swig_csharp = generated.swig_csharp.getvalue()

                if build_python:
                    jlib.log( 'Generating mupdf_cppyy.py file.')
                    make_cppyy.make_cppyy( state.state_, build_dirs, generated)

                    jlib.log( 'Generating python module source code using SWIG ...')
                    with jlib.LogPrefixScope( f'swig Python: '):
                        # Generate C++ code for python module using SWIG.
                        swig.build_swig(
                                state.state_,
                                build_dirs,
                                generated,
                                language='python',
                                swig_command=swig_command,
                                check_regress=check_regress,
                                force_rebuild=force_rebuild,
                                )

                if build_csharp:
                    # Generate C# using SWIG.
                    jlib.log( 'Generating C# module source code using SWIG ...')
                    with jlib.LogPrefixScope( f'swig C#: '):
                        swig.build_swig(
                                state.state_,
                                build_dirs,
                                generated,
                                language='csharp',
                                swig_command=swig_command,
                                check_regress=check_regress,
                                force_rebuild=force_rebuild,
                                )

            elif action == 'j':
                # Just experimenting.
                build_swig_java()


            elif action == '3':
                # Compile code from action=='2' to create Python/C# shared library.
                #
                if build_python:
                    jlib.log( 'Compiling/linking generated Python module source code to create _mupdf.{"pyd" if state.state_.windows else "so"} ...')
                if build_csharp:
                    jlib.log( 'Compiling/linking generated C# source code to create mupdfcsharp.{"dll" if state.state_.windows else "so"} ...')

                if state.state_.windows:
                    if build_python:
                        cpu, python_version, python_path, python_root, include = windows_find_python(
                                build_dirs.cpu,
                                build_dirs.python_version,
                                )
                        jlib.log( '{include=}:')
                        if 0:
                            # Show contents of include directory.
                            for dirpath, dirnames, filenames in os.walk( include):
                                for f in filenames:
                                    p = os.path.join( dirpath, f)
                                    jlib.log( '    {p!r}')
                        assert os.path.isfile( os.path.join( include, 'Python.h'))
                        python_root = python_root.replace('\\', '/')
                        # Oddly there doesn't seem to be a
                        # `sysconfig.get_path('libs')`, but it seems to be next
                        # to `includes`:
                        libs = os.path.abspath( f'{include}/../libs')
                        jlib.log( 'Matching python for {build_dirs.cpu=} {python_version=}: {python_path=} {include=} {python_root=} {include=} {libs=}')
                        env_extra = {
                                'MUPDF_PYTHON_INCLUDE_PATH': f'{include}',
                                'MUPDF_PYTHON_LIBRARY_PATH': f'{libs}',
                                }
                        jlib.log('{env_extra=}')

                        # The swig-generated .cpp file must exist at
                        # this point.
                        #
                        cpp_path = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp_swig.cpp'
                        assert os.path.exists(cpp_path), f'SWIG-generated file does not exist: {cpp_path}'

                        # We need to update mtime of the .cpp file to
                        # force recompile and link, because we run
                        # devenv with different environmental variables
                        # depending on the Python for which we are
                        # building.
                        #
                        # [Using /Rebuild or /Clean appears to clean
                        # the entire solution even if we specify
                        # /Project.]
                        #
                        os.utime(cpp_path)

                        win32_infix = _windows_vs_upgrade( vs_upgrade, build_dirs, devenv)
                        jlib.log('Building mupdfpyswig project')
                        command = (
                                f'cd {build_dirs.dir_mupdf}&&'
                                f'"{devenv}"'
                                f' platform/{win32_infix}/mupdfpyswig.sln'
                                f' /Build "{windows_build_type}Python|{build_dirs.cpu.windows_config}"'
                                f' /Project mupdfpyswig'
                                )
                        jlib.system(command, verbose=1, out='log', env_extra=env_extra)

                        jlib.fs_copy(
                                f'{build_dirs.dir_mupdf}/platform/{win32_infix}/{build_dirs.cpu.windows_subdir}{windows_build_type}/mupdfpyswig.dll',
                                f'{build_dirs.dir_so}/_mupdf.pyd',
                                verbose=1,
                                )

                    if build_csharp:
                        # The swig-generated .cpp file must exist at
                        # this point.
                        #
                        cpp_path = f'{build_dirs.dir_mupdf}/platform/csharp/mupdfcpp_swig.cpp'
                        assert os.path.exists(cpp_path), f'SWIG-generated file does not exist: {cpp_path}'

                        win32_infix = _windows_vs_upgrade( vs_upgrade, build_dirs, devenv)
                        jlib.log('Building mupdfcsharp project')
                        command = (
                                f'cd {build_dirs.dir_mupdf}&&'
                                f'"{devenv}"'
                                f' platform/{win32_infix}/mupdfcsharpswig.sln'
                                f' /Build "ReleaseCsharp|{build_dirs.cpu.windows_config}"'
                                f' /Project mupdfcsharpswig'
                                )
                        jlib.system(command, verbose=1, out='log')

                        jlib.fs_copy(
                                f'{build_dirs.dir_mupdf}/platform/{win32_infix}/{build_dirs.cpu.windows_subdir}{windows_build_type}/mupdfcsharpswig.dll',
                                f'{build_dirs.dir_so}/mupdfcsharp.dll',
                                verbose=1,
                                )

                else:
                    # Not Windows.

                    # We use c++ debug/release flags as implied by
                    # --dir-so, but all builds output the same file
                    # mupdf:platform/python/_mupdf.so. We could instead
                    # generate mupdf.py and _mupdf.so in the --dir-so
                    # directory?
                    #
                    # [While libmupdfcpp.so requires matching
                    # debug/release build of libmupdf.so, it looks
                    # like _mupdf.so does not require a matching
                    # libmupdfcpp.so and libmupdf.so.]
                    #
                    flags_compile = ''
                    flags_link = ''
                    if build_python:
                        # We use python-config which appears to
                        # work better than pkg-config because
                        # it copes with multiple installed
                        # python's, e.g. manylinux_2014's
                        # /opt/python/cp*-cp*/bin/python*.
                        #
                        # But... it seems that we should not
                        # attempt to specify libpython on the link
                        # command. The manylinux docker containers
                        # don't actually contain libpython.so, and
                        # it seems that this deliberate. And the
                        # link command runs ok.
                        #
                        # todo: maybe instead use sysconfig.get_config_vars() ?
                        #
                        python_flags = pipcl.PythonFlags()
                        flags_compile = python_flags.includes
                        flags_link = python_flags.ldflags

                        if state.state_.macos:
                            # We need this to avoid numerous errors like:
                            #
                            # Undefined symbols for architecture x86_64:
                            #   "_PyArg_UnpackTuple", referenced from:
                            #       _wrap_ll_fz_warn(_object*, _object*) in mupdfcpp_swig-0a6733.o
                            #       _wrap_fz_warn(_object*, _object*) in mupdfcpp_swig-0a6733.o
                            #       ...
                            flags_link += ' -undefined dynamic_lookup'

                        jlib.log('flags_compile={flags_compile!r}')
                        jlib.log('flags_link={flags_link!r}')

                    # These are the input files to our g++ command:
                    #
                    include1        = f'{build_dirs.dir_mupdf}/include'
                    include2        = f'{build_dirs.dir_mupdf}/platform/c++/include'

                    dir_so_flags = os.path.basename( build_dirs.dir_so).split( '-')
                    if 'shared' in dir_so_flags:
                        libmupdf        = f'{build_dirs.dir_so}/libmupdf.so{so_version}'
                        libmupdfthird   = f''
                        libmupdfcpp     = f'{build_dirs.dir_so}/libmupdfcpp.so{so_version}'
                    elif 'fpic' in dir_so_flags:
                        libmupdf        = f'{build_dirs.dir_so}/libmupdf.a'
                        libmupdfthird   = f'{build_dirs.dir_so}/libmupdf-third.a'
                        libmupdfcpp     = f'{build_dirs.dir_so}/libmupdfcpp.a'
                    else:
                        assert 0, f'Leaf must start with "shared-" or "fpic-": build_dirs.dir_so={build_dirs.dir_so}'

                    if build_python:
                        cpp_path = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp_swig.cpp'
                        out_so = f'{build_dirs.dir_so}/_mupdf.so'
                    elif build_csharp:
                        cpp_path = f'{build_dirs.dir_mupdf}/platform/csharp/mupdfcpp_swig.cpp'
                        out_so = f'{build_dirs.dir_so}/mupdfcsharp.so'  # todo: append {so_version} ?

                    if state.state_.openbsd:
                        # clang needs around 2G on OpenBSD.
                        #
                        soft, hard = resource.getrlimit( resource.RLIMIT_DATA)
                        required = 3 * 2**30
                        if soft < required:
                            if hard < required:
                                jlib.log( 'Warning: RLIMIT_DATA {hard=} is less than {required=}.')
                            soft_new = min(hard, required)
                            resource.setrlimit( resource.RLIMIT_DATA, (soft_new, hard))
                            jlib.log( 'Have changed RLIMIT_DATA from {jlib.number_sep(soft)} to {jlib.number_sep(soft_new)}.')

                    # We use link_l_flags() to add -L options to search parent
                    # directories of each .so that we need, and -l with the .so
                    # leafname without leading 'lib' or trailing '.so'. This
                    # ensures that at runtime one can set LD_LIBRARY_PATH to
                    # parent directories and have everything work.
                    #

                    # Build mupdf2.so
                    if build_python:
                        cpp2_path = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp2_swig.cpp'
                        out2_so = f'{build_dirs.dir_so}/_mupdf2.so'
                        if jlib.fs_filesize( cpp2_path):
                            jlib.log( 'Compiling/linking mupdf2')
                            command = ( textwrap.dedent(
                                    f'''
                                    {compiler}
                                        -o {os.path.relpath(out2_so)}
                                        {os.path.relpath(cpp2_path)}
                                        {build_dirs.cpp_flags}
                                        -fPIC
                                        --shared
                                        -I {include1}
                                        -I {include2}
                                        {flags_compile}
                                        {flags_link2}
                                        {link_l_flags( [libmupdf, libmupdfcpp])}
                                        -Wno-deprecated-declarations
                                    ''').strip().replace( '\n', ' \\\n')
                            )
                            infiles = [
                                    cpp2_path,
                                    include1,
                                    include2,
                                    libmupdf,
                                    libmupdfcpp,
                                    ]
                            jlib.build(
                                    infiles,
                                    out2_so,
                                    command,
                                    force_rebuild,
                                    )
                        else:
                            jlib.fs_remove( out2_so)
                            jlib.fs_remove( f'{out2_so}.cmd')

                    # Build _mupdf.so.
                    #
                    # We define SWIG_PYTHON_SILENT_MEMLEAK to avoid generating
                    # lots of diagnostics `detected a memory leak of type
                    # 'mupdf::PdfObj *', no destructor found.` when used with
                    # mupdfpy. However it's not definitely known that these
                    # diagnostics are spurious - seems to be to do with two
                    # separate SWIG Python APIs (mupdf and mupdfpy's `extra`
                    # module) using the same underlying C library.
                    #
                    sos = []
                    sos.append( f'{build_dirs.dir_so}/libmupdfcpp.so{so_version}')
                    if os.path.basename( build_dirs.dir_so).startswith( 'shared-'):
                        sos.append( f'{build_dirs.dir_so}/libmupdf.so{so_version}')
                    if pyodide:
                        # Need to use separate compilation/linking.
                        o_file = f'{os.path.relpath(cpp_path)}.o'
                        command = ( textwrap.dedent(
                                f'''
                                {compiler}
                                    -c
                                    -o {o_file}
                                    {cpp_path}
                                    {build_dirs.cpp_flags}
                                    -fPIC
                                    -I {include1}
                                    -I {include2}
                                    {flags_compile}
                                    -Wno-deprecated-declarations
                                    -Wno-free-nonheap-object
                                    -DSWIG_PYTHON_SILENT_MEMLEAK
                                ''').strip().replace( '\n', ' \\\n')
                                )
                        infiles = [
                                cpp_path,
                                include1,
                                include2,
                                ]
                        jlib.build(
                                infiles,
                                o_file,
                                command,
                                force_rebuild,
                                )

                        command = ( textwrap.dedent(
                                f'''
                                {compiler}
                                    -o {os.path.relpath(out_so)}
                                    -sSIDE_MODULE
                                    {o_file}
                                    {build_dirs.cpp_flags}
                                    -shared
                                    {flags_link}
                                    {link_l_flags( sos)}
                                ''').strip().replace( '\n', ' \\\n')
                                )
                        infiles = [
                                o_file,
                                libmupdf,
                                ]
                        infiles += sos

                        jlib.build(
                                infiles,
                                out_so,
                                command,
                                force_rebuild,
                                )
                    else:
                        # Not Pyodide.
                        command = ( textwrap.dedent(
                                f'''
                                {compiler}
                                    -o {os.path.relpath(out_so)}
                                    {"-sMAIN_MODULE" if 0 and pyodide else ""}
                                    {cpp_path}
                                    {build_dirs.cpp_flags}
                                    -fPIC
                                    -shared
                                    -I {include1}
                                    -I {include2}
                                    {flags_compile}
                                    -Wno-deprecated-declarations
                                    -Wno-free-nonheap-object
                                    -DSWIG_PYTHON_SILENT_MEMLEAK
                                    {flags_link}
                                    {link_l_flags( sos)}
                                ''').strip().replace( '\n', ' \\\n')
                                )
                        infiles = [
                                cpp_path,
                                include1,
                                include2,
                                libmupdf,
                                ]
                        infiles += sos

                        jlib.build(
                                infiles,
                                out_so,
                                command,
                                force_rebuild,
                                )
                        macos_patch( out_so,
                                f'{build_dirs.dir_so}/libmupdf.dylib{so_version}',
                                f'{build_dirs.dir_so}/libmupdfcpp.so{so_version}',
                                )
            else:
                raise Exception( 'unrecognised --build action %r' % action)


def python_settings(build_dirs, startdir=None):
    # We need to set LD_LIBRARY_PATH and PYTHONPATH so that our
    # test .py programme can load mupdf.py and _mupdf.so.
    if build_dirs.dir_so is None:
        # Use no extra environment and default python, e.g. in venv.
        jlib.log('build_dirs.dir_so is None, returning empty extra environment and "python"')
        return {}, 'python'

    env_extra = {}
    env_extra[ 'PYTHONPATH'] = os.path.relpath(build_dirs.dir_so, startdir)

    command_prefix = ''
    if state.state_.windows:
        # On Windows, it seems that 'py' runs the default
        # python. Also, Windows appears to be able to find
        # _mupdf.pyd in same directory as mupdf.py.
        #
        cpu, python_version, python_path, python_root, python_include = windows_find_python(
                build_dirs.cpu,
                build_dirs.python_version,
                )
        python_path = python_path.replace('\\', '/')    # Allows use on Cygwin.
        command_prefix = f'"{python_path}"'
    else:
        pass
        # We build _mupdf.so using `-Wl,-rpath='$ORIGIN,-z,origin` (see
        # link_l_flags()) so we don't need to set `LD_LIBRARY_PATH`.
        #
        # But if we did set `LD_LIBRARY_PATH`, it would be with:
        #
        #   env_extra[ 'LD_LIBRARY_PATH'] = os.path.abspath(build_dirs.dir_so)
        #
    return env_extra, command_prefix

def csharp_settings(build_dirs):
    '''
    Returns (csc, mono, mupdf_cs).

    csc: C# compiler.
    mono: C# interpreter ("" on Windows).
    mupdf_cs: MuPDF C# code.

    E.g. on Windows `csc` can be: C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/Roslyn/csc.exe
    '''
    # On linux requires:
    #   sudo apt install mono-devel
    #
    # OpenBSD:
    #   pkg_add mono
    # but we get runtime error when exiting:
    #   mono:build/shared-release/libmupdfcpp.so: undefined symbol '_ZdlPv'
    # which might be because of mixing gcc and clang?
    #
    if state.state_.windows:
        import wdev
        vs = wdev.WindowsVS()
        jlib.log('{vs.description_ml()=}')
        csc = vs.csc
        jlib.log('{csc=}')
        assert csc, f'Unable to find csc.exe'
        mono = ''
    else:
        mono = 'mono'
        if state.state_.linux:
            csc = 'mono-csc'
        elif state.state_.openbsd:
            csc = 'csc'
        else:
            assert 0, f'Do not know where to find mono. {platform.platform()=}'

    mupdf_cs = os.path.relpath(f'{build_dirs.dir_so}/mupdf.cs')
    return csc, mono, mupdf_cs


def make_docs( build_dirs, languages_original):

    languages = languages_original
    if languages == 'all':
        languages = 'c,c++,python'
    languages = languages.split( ',')

    def do_doxygen( name, outdir, path):
        '''
        name:
            Doxygen PROJECT_NAME of generated documentation
        outdir:
            Directory in which we run doxygen, so root of generated
            documentation will be in <outdir>/html/index.html
        path:
            Doxygen INPUT setting; this is the path of the directory which
            contains the API to document. If a relative path, it should be
            relative to <outdir>.
        '''
        # We generate a blank doxygen configuration file, make
        # some minimal changes, then run doxygen on the modified
        # configuration.
        #
        assert 'docs/generated/' in outdir
        jlib.fs_ensure_empty_dir( outdir)
        dname = f'{name}.doxygen'
        dname2 = os.path.join( outdir, dname)
        jlib.system( f'cd {outdir}; rm -f {dname}0; doxygen -g {dname}0', out='return')
        with open( dname2+'0') as f:
            dtext = f.read()
        dtext, n = re.subn( '\nPROJECT_NAME *=.*\n', f'\nPROJECT_NAME = {name}\n', dtext)
        assert n == 1
        dtext, n = re.subn( '\nEXTRACT_ALL *=.*\n', f'\nEXTRACT_ALL = YES\n', dtext)
        assert n == 1
        dtext, n = re.subn( '\nINPUT *=.*\n', f'\nINPUT = {path}\n', dtext)
        assert n == 1
        dtext, n = re.subn( '\nRECURSIVE *=.*\n', f'\nRECURSIVE = YES\n', dtext)
        with open( dname2, 'w') as f:
            f.write( dtext)
        #jlib.system( f'diff -u {dname2}0 {dname2}', raise_errors=False)
        command = f'cd {outdir}; doxygen {dname}'
        jlib.system( command, out='return', verbose=1)
        jlib.log( 'have created: {outdir}/html/index.html')

    out_dir = f'{build_dirs.dir_mupdf}/docs/generated'

    for language in languages:

        if language == 'c':
            do_doxygen( 'mupdf', f'{out_dir}/c', f'{build_dirs.dir_mupdf}/include')

        elif language == 'c++':
            do_doxygen( 'mupdfcpp', f'{out_dir}/c++', f'{build_dirs.dir_mupdf}/platform/c++/include')

        elif language == 'python':
            ld_library_path = os.path.abspath( f'{build_dirs.dir_so}')
            jlib.fs_ensure_empty_dir( f'{out_dir}/python')
            pythonpath = os.path.relpath( f'{build_dirs.dir_so}', f'{out_dir}/python')
            input_relpath = os.path.relpath( f'{build_dirs.dir_so}/mupdf.py', f'{out_dir}/python')
            jlib.system(
                    f'cd {out_dir}/python && LD_LIBRARY_PATH={ld_library_path} PYTHONPATH={pythonpath} pydoc3 -w {input_relpath}',
                    out='log',
                    verbose=True,
                    )
            path = f'{out_dir}/python/mupdf.html'
            assert os.path.isfile( path)

            # Add some styling.
            #
            with open( path) as f:
                text = f.read()

            m1 = re.search( '[<]/head[>][<]body[^>]*[>]\n', text)
            m2 = re.search( '[<]/body[>]', text)
            assert m1
            assert m2
            #jlib.log( '{=m1.start() m1.end() m2.start() m2.end()}')

            a = text[ : m1.start()]
            b = textwrap.dedent('''
                    <link href="../../../../../css/default.css" rel="stylesheet" type="text/css" />
                    <link href="../../../../../css/language-bindings.css" rel="stylesheet" type="text/css" />
                    ''')
            c = text[ m1.start() : m1.end()]
            d = textwrap.dedent('''
                    <main style="display:block;">
                        <a class="no-underline" href="../../../index.html">
                            <div class="banner" role="heading" aria-level="1">
                                <h1>MuPDF Python bindings</h1>
                            </div>
                        </a>
                        <div class="outer">
                            <div class="inner">
                    ''')
            e = text[ m1.end() : m2.end()]
            f = textwrap.dedent('''
                    </div></div>
                    </main>
                    ''')
            g = text[ m2.end() : ]
            text = a + b + c + d + e + f + g
            with open( path, 'w') as f:
                f.write( text)
            jlib.log( 'have created: {path}')

        else:
            raise Exception( f'unrecognised language param: {lang}')

    make_docs_index( build_dirs, languages_original)


def make_docs_index( build_dirs, languages_original):
    # Create index.html with links to the different bindings'
    # documentation.
    #
    #mupdf_dir = os.path.abspath( f'{__file__}/../../..')
    out_dir = f'{build_dirs.dir_mupdf}/docs/generated'
    top_index_html = f'{out_dir}/index.html'
    with open( top_index_html, 'w') as f:
        git_id = jlib.git_get_id( build_dirs.dir_mupdf)
        git_id = git_id.split( '\n')[0]
        f.write( textwrap.dedent( f'''
                <!DOCTYPE html>

                <html lang="en">
                    <head>
                        <link href="../../css/default.css" rel="stylesheet" type="text/css" />
                        <link href="../../css/language-bindings.css" rel="stylesheet" type="text/css" />
                    </head>
                    <body>
                        <main style="display:block;">
                            <div class="banner" role="heading" aria-level="1">
                                <h1>MuPDF bindings</h1>
                            </div>
                            <div class="outer">
                                <div class="inner">
                                    <ul>
                                        <li><a href="c/html/index.html">C</a> (generated by Doxygen).
                                        <li><a href="c++/html/index.html">C++</a> (generated by Doxygen).
                                        <li><a href="python/mupdf.html">Python</a> (generated by Pydoc).
                                    </ul>
                                    <small>
                                        <p>Generation:</p>
                                        <ul>
                                            <li>Date: {jlib.date_time()}
                                            <li>Git: {git_id}
                                            <li>Command: <code>./scripts/mupdfwrap.py --doc {languages_original}</code>
                                        </ul>
                                    </small>
                                </div>
                            </div>
                        </main>
                    </body>
                </html>
                '''
                ))
    jlib.log( 'Have created: {top_index_html}')



def main2():

    assert not state.state_.cygwin, \
            f'This script does not run properly under Cygwin, use `py ...`'

    # Set default build directory. Can be overridden by '-d'.
    #
    build_dirs = state.BuildDirs()

    # Set default swig and make.
    #
    swig_command = 'swig'
    make_command = None

    # Whether to use `devenv.com /upgrade`.
    #
    vs_upgrade = False

    args = jlib.Args( sys.argv[1:])
    arg_i = 0
    while 1:
        try:
            arg = args.next()
        except StopIteration:
            break
        #log( 'Handling {arg=}')

        arg_i += 1

        with jlib.LogPrefixScope( f'{arg}: '):

            if arg == '-h' or arg == '--help':
                print( __doc__)

            elif arg == '--build' or arg == '-b':
                build( build_dirs, swig_command, args, vs_upgrade, make_command)

            elif arg == '--check-headers':
                keep_going = False
                path = args.next()
                if path == '-k':
                    keep_going = True
                    path = args.next()
                include_dir = os.path.relpath( f'{build_dirs.dir_mupdf}/include')
                def paths():
                    if path.endswith( '+'):
                        active = False
                        for p in jlib.fs_paths( include_dir):
                            if not active and p == path[:-1]:
                                active = True
                            if not active:
                                continue
                            if p.endswith( '.h'):
                                yield p
                    elif path == 'all':
                        for p in jlib.fs_paths( include_dir):
                            if p.endswith( '.h'):
                                yield p
                    else:
                        yield path
                failed_paths = []
                for path in paths():
                    if path.endswith( '/mupdf/pdf/name-table.h'):
                        # Not a normal header.
                        continue
                    if path.endswith( '.h'):
                        e = jlib.system( f'cc -I {include_dir} {path}', out='log', raise_errors=False, verbose=1)
                        if e:
                            if keep_going:
                                failed_paths.append( path)
                            else:
                                sys.exit( 1)
                if failed_paths:
                    jlib.log( 'Following headers are not self-contained:')
                    for path in failed_paths:
                        jlib.log( f'    {path}')
                    sys.exit( 1)

            elif arg == '--compare-fz_usage':
                directory = args.next()
                compare_fz_usage( tu, directory, fn_usage)

            elif arg == '--diff':
                for path in jlib.fs_paths( build_dirs.ref_dir):
                    #log( '{path=}')
                    assert path.startswith( build_dirs.ref_dir)
                    if not path.endswith( '.h') and not path.endswith( '.cpp'):
                        continue
                    tail = path[ len( build_dirs.ref_dir):]
                    path2 = f'{build_dirs.dir_mupdf}/platform/c++/{tail}'
                    command = f'diff -u {path} {path2}'
                    jlib.log( 'running: {command}')
                    jlib.system(
                            command,
                            raise_errors=False,
                            out='log',
                            )

            elif arg == '--diff-all':
                for a, b in (
                        (f'{build_dirs.dir_mupdf}/platform/c++/', f'{build_dirs.dir_mupdf}/platform/c++/'),
                        (f'{build_dirs.dir_mupdf}/platform/python/', f'{build_dirs.dir_mupdf}/platform/python/')
                        ):
                    for dirpath, dirnames, filenames in os.walk( a):
                        assert dirpath.startswith( a)
                        root = dirpath[len(a):]
                        for filename in filenames:
                            a_path = os.path.join(dirpath, filename)
                            b_path = os.path.join( b, root, filename)
                            command = f'diff -u {a_path} {b_path}'
                            jlib.system( command, out='log', raise_errors=False)

            elif arg == '--doc':
                languages = args.next()
                make_docs( build_dirs, languages)

            elif arg == '--doc-index':
                languages = args.next()
                make_docs_index( build_dirs, languages)

            elif arg == '--make':
                make_command = args.next()

            elif arg == '--ref':
                assert 'mupdfwrap_ref' in build_dirs.ref_dir
                jlib.system(
                        f'rm -r {build_dirs.ref_dir}',
                        raise_errors=False,
                        out='log',
                        )
                jlib.system(
                        f'rsync -ai {build_dirs.dir_mupdf}/platform/c++/implementation {build_dirs.ref_dir}',
                        out='log',
                        )
                jlib.system(
                        f'rsync -ai {build_dirs.dir_mupdf}/platform/c++/include {build_dirs.ref_dir}',
                        out='log',
                        )

            elif arg == '--dir-so' or arg == '-d':
                d = args.next()
                build_dirs.set_dir_so( d)
                #jlib.log('Have set {build_dirs=}')

            elif arg == '--py-package-multi':
                # Investigating different combinations of pip, pyproject.toml,
                # setup.py
                #
                def system(command):
                    jlib.system(command, verbose=1, out='log')
                system( '(rm -r pylocal-multi dist || true)')
                system( './setup.py sdist')
                system( 'cp -p pyproject.toml pyproject.toml-0')
                results = dict()
                try:
                    for toml in 0, 1:
                        for pip_upgrade in 0, 1:
                            for do_wheel in 0, 1:
                                with jlib.LogPrefixScope(f'toml={toml} pip_upgrade={pip_upgrade} do_wheel={do_wheel}: '):
                                    #print(f'jlib.g_log_prefixes={jlib.g_log_prefixes}')
                                    #print(f'jlib.g_log_prefix_scopes.items={jlib.g_log_prefix_scopes.items}')
                                    #print(f'jlib.log_text(""): {jlib.log_text("")}')
                                    result_key = toml, pip_upgrade, do_wheel
                                    jlib.log( '')
                                    jlib.log( '=== {pip_upgrade=} {do_wheel=}')
                                    if toml:
                                        system( 'cp -p pyproject.toml-0 pyproject.toml')
                                    else:
                                        system( 'rm pyproject.toml || true')
                                    system( 'ls -l pyproject.toml || true')
                                    system(
                                            '(rm -r pylocal-multi wheels || true)'
                                            ' && python3 -m venv pylocal-multi'
                                            ' && . pylocal-multi/bin/activate'
                                            ' && pip install clang'
                                            )
                                    try:
                                        if pip_upgrade:
                                            system( '. pylocal-multi/bin/activate && pip install --upgrade pip')
                                        if do_wheel:
                                            system( '. pylocal-multi/bin/activate && pip install check-wheel-contents')
                                            system( '. pylocal-multi/bin/activate && pip wheel --wheel-dir wheels dist/*')
                                            system( '. pylocal-multi/bin/activate && check-wheel-contents wheels/*')
                                            system( '. pylocal-multi/bin/activate && pip install wheels/*')
                                        else:
                                            system( '. pylocal-multi/bin/activate && pip install dist/*')
                                        #system( './scripts/mupdfwrap_test.py')
                                        system( '. pylocal-multi/bin/activate && python -m mupdf')
                                    except Exception as ee:
                                        e = ee
                                    else:
                                        e = 0
                                    results[ result_key] = e
                                    jlib.log( '== {e=}')
                    jlib.log( '== Results:')
                    for (toml, pip_upgrade, do_wheel), e in results.items():
                        jlib.log( '    {toml=} {pip_upgrade=} {do_wheel=}: {e=}')
                finally:
                    system( 'cp -p pyproject.toml-0 pyproject.toml')

            elif arg == '--run-py':
                command = ''
                while 1:
                    try:
                        command += ' ' + args.next()
                    except StopIteration:
                        break

                ld_library_path = os.path.abspath( f'{build_dirs.dir_so}')
                pythonpath = build_dirs.dir_so

                envs = f'LD_LIBRARY_PATH={ld_library_path} PYTHONPATH={pythonpath}'
                command = f'{envs} {command}'
                jlib.log( 'running: {command}')
                e = jlib.system(
                        command,
                        raise_errors=False,
                        verbose=False,
                        out='log',
                        )
                sys.exit(e)

            elif arg == '--show-ast':
                filename = args.next()
                includes = args.next()
                parse.show_ast( filename, includes)

            elif arg == '--swig':
                swig_command = args.next()

            elif arg == '--swig-windows-auto':
                if state.state_.windows:
                    import stat
                    import urllib.request
                    import zipfile
                    name = 'swigwin-4.0.2'

                    # Download swig .zip file if not already present.
                    #
                    if not os.path.exists(f'{name}.zip'):
                        url = f'http://prdownloads.sourceforge.net/swig/{name}.zip'
                        jlib.log(f'Downloading Windows SWIG from: {url}')
                        with urllib.request.urlopen(url) as response:
                            with open(f'{name}.zip-', 'wb') as f:
                                shutil.copyfileobj(response, f)
                        os.rename(f'{name}.zip-', f'{name}.zip')

                    # Extract swig from .zip file if not already extracted.
                    #
                    swig_local = f'{name}/swig.exe'
                    if not os.path.exists(swig_local):
                        # Extract
                        z = zipfile.ZipFile(f'{name}.zip')
                        jlib.fs_ensure_empty_dir(f'{name}-0')
                        z.extractall(f'{name}-0')
                        os.rename(f'{name}-0/{name}', name)
                        os.rmdir(f'{name}-0')

                        # Need to make swig.exe executable.
                        swig_local_stat = os.stat(swig_local)
                        os.chmod(swig_local, swig_local_stat.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

                    # Set our <swig> to be the local windows swig.exe.
                    #
                    swig_command = swig_local
                else:
                    jlib.log('Ignoring {arg} because not running on Windows')

            elif arg == '--sync-pretty':
                destination = args.next()
                jlib.log( 'Syncing to {destination=}')
                generated = cpp.Generated(f'{build_dirs.dir_mupdf}/platform/c++')
                files = generated.h_files + generated.cpp_files + [
                        f'{build_dirs.dir_so}/mupdf.py',
                        f'{build_dirs.dir_mupdf}/platform/c++/fn_usage.txt',
                        ]
                # Generate .html files with syntax colouring for source files. See:
                #   https://github.com/google/code-prettify
                #
                files_html = []
                for i in files:
                    if os.path.splitext( i)[1] not in ( '.h', '.cpp', '.py'):
                        continue
                    o = f'{i}.html'
                    jlib.log( 'converting {i} to {o}')
                    with open( i) as f:
                        text = f.read()
                    with open( o, 'w') as f:
                        f.write( '<html><body>\n')
                        f.write( '<script src="https://cdn.jsdelivr.net/gh/google/code-prettify@master/loader/run_prettify.js"></script>\n')
                        f.write( '<pre class="prettyprint">\n')
                        f.write( text)
                        f.write( '</pre>\n')
                        f.write( '</body></html>\n')
                    files_html.append( o)

                files += files_html

                # Insert extra './' into each path so that rsync -R uses the
                # 'mupdf/...' tail of each local path for the remote path.
                #
                for i in range( len( files)):
                    files[i] = files[i].replace( '/mupdf/', '/./mupdf/')
                    files[i] = files[i].replace( '/tmp/', '/tmp/./')

                jlib.system( f'rsync -aiRz {" ".join( files)} {destination}', verbose=1, out='log')

            elif arg == '--sync-docs':
                # We use extra './' so that -R uses remaining path on
                # destination.
                #
                destination = args.next()
                jlib.system( f'rsync -aiRz {build_dirs.dir_mupdf}/docs/generated/./ {destination}', verbose=1, out='log')

            elif arg == '--test-cpp':
                testfile = os.path.abspath( f'{__file__}/../../../thirdparty/zlib/zlib.3.pdf')
                testfile = testfile.replace('\\', '/')
                src = f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_test.cpp'
                exe = f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_test.cpp.exe'
                includes = (
                        f' -I {build_dirs.dir_mupdf}/include'
                        f' -I {build_dirs.dir_mupdf}/platform/c++/include'
                        )
                # Enable asserts in this test.
                cpp_flags = build_dirs.cpp_flags.replace( '-DNDEBUG', '')
                if state.state_.windows:
                    win32_infix = _windows_vs_upgrade( vs_upgrade, build_dirs, devenv=None)
                    windows_build_type = build_dirs.windows_build_type()
                    lib = f'{build_dirs.dir_mupdf}/platform/{win32_infix}/{build_dirs.cpu.windows_subdir}{windows_build_type}/mupdfcpp{build_dirs.cpu.windows_suffix}.lib'
                    vs = wdev.WindowsVS()
                    command = textwrap.dedent(f'''
                            "{vs.vcvars}"&&"{vs.cl}"
                                /Tp{src}
                                {includes}
                                -D FZ_DLL_CLIENT
                                {cpp_flags}
                                /link
                                {lib}
                                /out:{exe}
                            ''').replace('\n', ' ')
                    jlib.system(command, verbose=1)
                    path = os.environ.get('PATH')
                    env_extra = dict(PATH = f'{build_dirs.dir_so}{os.pathsep}{path}' if path else build_dirs.dir_so)
                    jlib.system(f'{exe} {testfile}', verbose=1, env_extra=env_extra)
                else:
                    dir_so_flags = os.path.basename( build_dirs.dir_so).split( '-')
                    if 'shared' in dir_so_flags:
                        libmupdf        = f'{build_dirs.dir_so}/libmupdf.so'
                        libmupdfthird   = f''
                        libmupdfcpp     = f'{build_dirs.dir_so}/libmupdfcpp.so'
                    elif 'fpic' in dir_so_flags:
                        libmupdf        = f'{build_dirs.dir_so}/libmupdf.a'
                        libmupdfthird   = f'{build_dirs.dir_so}/libmupdf-third.a'
                        libmupdfcpp     = f'{build_dirs.dir_so}/libmupdfcpp.a'
                    else:
                        assert 0, f'Leaf must start with "shared-" or "fpic-": build_dirs.dir_so={build_dirs.dir_so}'
                    command = textwrap.dedent(f'''
                            c++
                                -o {exe}
                                {cpp_flags}
                                {includes}
                                {src}
                                {link_l_flags( [libmupdf, libmupdfcpp])}
                            ''').replace('\n', '\\\n')
                    jlib.system(command, verbose=1)
                    jlib.system( 'pwd', verbose=1)
                    if state.state_.macos:
                        jlib.system( f'DYLD_LIBRARY_PATH={build_dirs.dir_so} {exe}', verbose=1)
                    else:
                        jlib.system( f'{exe} {testfile}', verbose=1, env_extra=dict(LD_LIBRARY_PATH=build_dirs.dir_so))

            elif arg == '--test-internal':
                _test_get_m_command()

            elif arg == '--test-internal-cpp':
                cpp.test()

            elif arg in ('--test-python', '-t', '--test-python-gui'):

                env_extra, command_prefix = python_settings(build_dirs)
                script_py = os.path.relpath( f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_gui.py')
                if arg == '--test-python-gui':
                    #env_extra[ 'MUPDF_trace'] = '1'
                    #env_extra[ 'MUPDF_check_refs'] = '1'
                    #env_extra[ 'MUPDF_trace_exceptions'] = '1'
                    command = f'{command_prefix} {script_py} {build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf'
                    jlib.system( command, env_extra=env_extra, out='log', verbose=1)

                else:
                    jlib.log( 'running scripts/mupdfwrap_test.py ...')
                    script_py = os.path.relpath( f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_test.py')
                    command = f'{command_prefix} {script_py}'
                    with open( f'{build_dirs.dir_mupdf}/platform/python/mupdf_test.py.out.txt', 'w') as f:
                        jlib.system( command, env_extra=env_extra, out='log', verbose=1)
                        # Repeat with pdf_reference17.pdf if it exists.
                        path = os.path.relpath( f'{build_dirs.dir_mupdf}/../pdf_reference17.pdf')
                        if os.path.exists(path):
                            jlib.log('Running mupdfwrap_test.py on {path}')
                            command += f' {path}'
                            jlib.system( command, env_extra=env_extra, out='log', verbose=1)

                    # Run mutool.py.
                    #
                    mutool_py = os.path.relpath( f'{__file__}/../../mutool.py')
                    zlib_pdf = os.path.relpath(f'{build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf')
                    for args2 in (
                            f'trace {zlib_pdf}',
                            f'convert -o zlib.3.pdf-%d.png {zlib_pdf}',
                            f'draw -o zlib.3.pdf-%d.png -s tmf -v -y l -w 150 -R 30 -h 200 {zlib_pdf}',
                            f'draw -o zlib.png -R 10 {zlib_pdf}',
                            f'clean -gggg -l {zlib_pdf} zlib.clean.pdf',
                            ):
                        command = f'{command_prefix} {mutool_py} {args2}'
                        jlib.log( 'running: {command}')
                        jlib.system( f'{command}', env_extra=env_extra, out='log', verbose=1)

                    jlib.log( 'Tests ran ok.')

            elif arg == '--test-csharp':
                csc, mono, mupdf_cs = csharp_settings(build_dirs)

                # Our tests look for zlib.3.pdf in their current directory.
                jlib.fs_copy(
                        f'{build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf',
                        f'{build_dirs.dir_so}/zlib.3.pdf' if state.state_.windows else 'zlib.3.pdf'
                        )

                if 1:
                    # Build and run simple test.
                    out = 'test-csharp.exe'
                    jlib.build(
                            (f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_test.cs', mupdf_cs),
                            out,
                            f'"{csc}" -out:{{OUT}} {{IN}}',
                            )
                    if state.state_.windows:
                        out_rel = os.path.relpath( out, build_dirs.dir_so)
                        jlib.system(f'cd {build_dirs.dir_so} && {mono} {out_rel}', verbose=1)
                    else:
                        command = f'LD_LIBRARY_PATH={build_dirs.dir_so} {mono} ./{out}'
                        if state.state_.openbsd:
                            e = jlib.system( command, verbose=1, raise_errors=False)
                            if e == 137:
                                jlib.log( 'Ignoring {e=} on OpenBSD because this occurs in normal operation.')
                            elif e:
                                raise Exception( f'command failed: {command}')
                        else:
                            jlib.system(f'LD_LIBRARY_PATH={build_dirs.dir_so} {mono} ./{out}', verbose=1)

            elif arg == '--test-csharp-gui':
                csc, mono, mupdf_cs = csharp_settings(build_dirs)

                # Build and run gui test.
                #
                # Don't know why Unix/Windows differ in what -r: args are
                # required...
                #
                # We need -unsafe for copying bitmap data from mupdf.
                #
                references = '-r:System.Drawing -r:System.Windows.Forms' if state.state_.linux else ''
                out = 'mupdfwrap_gui.cs.exe'
                jlib.build(
                        (f'{build_dirs.dir_mupdf}/scripts/mupdfwrap_gui.cs', mupdf_cs),
                        out,
                        f'"{csc}" -unsafe {references}  -out:{{OUT}} {{IN}}'
                        )
                if state.state_.windows:
                    # Don't know how to mimic Unix's LD_LIBRARY_PATH, so for
                    # now we cd into the directory containing our generated
                    # libraries.
                    jlib.fs_copy(f'{build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf', f'{build_dirs.dir_so}/zlib.3.pdf')
                    # Note that this doesn't work remotely.
                    out_rel = os.path.relpath( out, build_dirs.dir_so)
                    jlib.system(f'cd {build_dirs.dir_so} && {mono} {out_rel}', verbose=1)
                else:
                    jlib.fs_copy(f'{build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf', f'zlib.3.pdf')
                    jlib.system(f'LD_LIBRARY_PATH={build_dirs.dir_so} {mono} ./{out}', verbose=1)

            elif arg == '--test-python-fitz':
                opts = ''
                while 1:
                    arg = args.next()
                    if arg.startswith('-'):
                        opts += f' {arg}'
                    else:
                        tests = arg
                        break
                startdir = os.path.abspath('../PyMuPDF/tests')
                env_extra, command_prefix = python_settings(build_dirs, startdir)

                env_extra['PYTHONPATH'] += f':{os.path.relpath(".", startdir)}'
                env_extra['PYTHONPATH'] += f':{os.path.relpath("./scripts", startdir)}'

                #env_extra['PYTHONMALLOC'] = 'malloc'
                #env_extra['MUPDF_trace'] = '1'
                #env_extra['MUPDF_check_refs'] = '1'

                # -x: stop at first error.
                # -s: show stdout/err.
                #
                if tests == 'all':
                    jlib.system(
                            f'cd ../PyMuPDF/tests && py.test-3 {opts}',
                            env_extra=env_extra,
                            out='log',
                            verbose=1,
                            )
                elif tests == 'iter':
                    e = 0
                    for script in sorted(glob.glob( '../PyMuPDF/tests/test_*.py')):
                        script = os.path.basename(script)
                        ee = jlib.system(
                                f'cd ../PyMuPDF/tests && py.test-3 {opts} {script}',
                                env_extra=env_extra,
                                out='log',
                                verbose=1,
                                raise_errors=0,
                                )
                        if not e:
                            e = ee
                elif not os.path.isfile(f'../PyMuPDF/tests/{tests}'):
                    ts = glob.glob("../PyMuPDF/tests/*.py")
                    ts = [os.path.basename(t) for t in ts]
                    raise Exception(f'Unrecognised tests={tests}. Should be "all", "iter" or one of {ts}')
                else:
                    jlib.system(
                            f'cd ../PyMuPDF/tests && py.test-3 {opts} {tests}',
                            env_extra=env_extra,
                            out='log',
                            verbose=1,
                            )

            elif arg == '--test-setup.py':
                # We use the '.' command to run pylocal/bin/activate rather than 'source',
                # because the latter is not portable, e.g. not supported by ksh. The '.'
                # command is posix so should work on all shells.
                commands = [
                            f'cd {build_dirs.dir_mupdf}',
                            f'python3 -m venv pylocal',
                            f'. pylocal/bin/activate',
                            f'pip install clang',
                            f'python setup.py {extra} install',
                            f'python scripts/mupdfwrap_test.py',
                            f'deactivate',
                            ]
                command = 'true'
                for c in commands:
                    command += f' && echo == running: {c}'
                    command += f' && {c}'
                jlib.system( command, verbose=1, out='log')

            elif arg == '--test-swig':
                swig.test_swig()

            elif arg in ('--venv' '--venv-force-reinstall'):
                force_reinstall = ' --force-reinstall' if arg == '--venv-force-reinstall' else ''
                assert arg_i == 1, f'If specified, {arg} should be the first argument.'
                venv = f'venv-mupdfwrap-{state.python_version()}-{state.cpu_name()}'
                # Oddly, shlex.quote(sys.executable), which puts the name
                # inside single quotes, doesn't work - we get error `The
                # filename, directory name, or volume label syntax is
                # incorrect.`.
                if state.state_.openbsd:
                    # Need system py3-llvm.
                    jlib.system(f'"{sys.executable}" -m venv --system-site-packages {venv}', out='log', verbose=1)
                else:
                    jlib.system(f'"{sys.executable}" -m venv {venv}', out='log', verbose=1)
                if state.state_.windows:
                    command = f'{venv}\\Scripts\\activate.bat'
                else:
                    command = f'. {venv}/bin/activate'
                command += f' && python -m pip install --upgrade pip'
                if state.state_.openbsd:
                    jlib.log( 'Not installing libclang on openbsd; we assume py3-llvm is installed.')
                    command += f' && python -m pip install --upgrade swig'
                else:
                    command += f' && python -m pip install{force_reinstall} --upgrade libclang swig'
                command += f' && python {shlex.quote(sys.argv[0])}'
                while 1:
                    try:
                        command += f' {shlex.quote(args.next())}'
                    except StopIteration:
                        break
                command += f' && deactivate'
                jlib.system(command, out='log', verbose=1)

            elif arg == '--vs-upgrade':
                vs_upgrade = int(args.next())

            elif arg == '--windows-cmd':
                args_tail = ''
                while 1:
                    try:
                        args_tail += f' {args.next()}'
                    except StopIteration:
                        break
                command = f'cmd.exe /c "py {sys.argv[0]} {args_tail}"'
                jlib.system(command, out='log', verbose=1)

            else:
                raise Exception( f'unrecognised arg: {arg}')


def write_classextras(path):
    '''
    Dumps classes.classextras to file using json, with crude handling of class
    instances.
    '''
    import json
    with open(path, 'w') as f:
        class encoder(json.JSONEncoder):
            def default( self, obj):
                if type(obj).__name__.startswith(('Extra', 'ClassExtra')):
                    ret = list()
                    for i in dir( obj):
                        if not i.startswith( '_'):
                            ret.append( getattr( obj, i))
                    return ret
                if callable(obj):
                    return obj.__name__
                return json.JSONEncoder.default(self, obj)
        json.dump(
                classes.classextras,
                f,
                indent='    ',
                sort_keys=1,
                cls = encoder,
                )

def main():
    jlib.force_line_buffering()
    try:
        main2()
    except Exception:
        jlib.exception_info()
        sys.exit(1)

if __name__ == '__main__':
    main2()
