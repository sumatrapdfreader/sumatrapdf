#!/usr/bin/env python3

'''
Support for generating C++ and python wrappers for the mupdf API.

C++ wrapping:

    Namespaces:

        All generated functions and classes are in the 'mupdf' namespace, e.g.
        mupdf::atof() is the wrapper for fz_atof().

    Functions:

        We provide a wrapper for each fz_*() and pdf_*() function.

        These wrappers do not take a 'fz_context* ctx' arg, and convert any
        fz_try..fz_catch exceptions into C++ exceptions.

        Wrappers for fz_*() functions are named by omitting the leading 'fz_'. Wrappers
        for pdf_*() functions are named by prefixing a 'p'.

        Examples:

            The wrappers for these functions:

                fz_device *fz_begin_page(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox);

                void pdf_insert_page(fz_context *ctx, pdf_document *doc, int at, pdf_obj *page);

            are:

                namespace mupdf
                {
                    fz_device *begin_page(fz_document_writer *wri, fz_rect mediabox);

                    void ppdf_insert_page(pdf_document *doc, int at, pdf_obj *page);
                }

        Text representation of structs:

            For selected POD structs, we provide an operator<< (std::ostream&)
            function which writes a labelled representation of the struct's
            members.

            For example for a fz_rect it will write text such as:

                (x0=90.51 y0=160.65 x1=501.39 y1=215.6)

            We also provide to_string() overloads and individual
            to_string_<structname>() functions that return this text
            representation as a std::string.

    Classes:

        For each fz_* and pdf_* struct, we provide a wrapper class with
        a CamelCase version of the struct name, e.g. the wrapper for
        fz_display_list is called mupdf::DisplayList.

        These wrapper classes generally have a member <m_internal> that is a
        pointer to an instance of the underlying struct.

        Member functions:

            Member functions are provided which wrap all relevant fz_*() and
            pdf_*() functions. These methods have the same name as the wrapped
            function but without the initial fz_ or pdf_. They generally take
            args that are references to wrapper classes instead of pointers to
            fz_* and pdf_* structs, and similarly return wrapper classes by
            value instead of returning a pointer to a fz_* or pdf_* struct.

        Reference counting:

            Wrapper classes automatically take care of reference counting, so
            user code can freely use instances of wrapper classes as required,
            for example making copies and allowing instances to go out of
            scope.

            Lifetime-related functions - constructors, copy constructors,
            operator= and destructors - make internal calls to
            fz_keep_<structname>() and fz_drop_<structname>() as required.

            Constructors that take a raw pointer to an underlying fz_* struct
            do not call fz_keep_*() - it is expected that any supplied fz_*
            pointer is already owned. Most of the time user code will not need
            to use these low-level constructors directly.

        POD wrappers:

            For simple POD structs such as fz_rect where reference counting is
            not used, the wrapper class's m_internal can be an instance of the
            underlying struct instead of a pointer.

        Text representation of wrapper classes:

            For wrappers of POD structs where we provide a
            to_string_<structname>() function as described above, we provide a
            similar to_string() member function.

    Details:

        We use clang-python to parse the fz header files, and generate C++
        headers and source code that gives wrappers for all fz_* functions.

        We also generate C++ classes that wrap all fz_* structs, adding
        in various constructors and methods that wrap auto-detected fz_*
        functions, plus explicitly-specified methods that wrap/use fz_*
        functions.

        More specifically, for each wrapping class:

            Copy constructors/operator=:

                If fz_keep_<name>() and fz_drop_<name>() exist, we generate
                copy constructor and operator= that use these functions.

            Constructors:

                We look for all fz_*() functions called fz_new*() that
                return a pointer to the wrapped class, and wrap these into
                constructors. If any of these constructors have duplicate
                prototypes, we cannot provide them as constructors so instead
                we provide them as static methods. This is not possible if the
                class is not copyable, in which case we include the constructor
                code but commented-out and with an explanation.

            Methods:

                We look for all fz_*() functions that take the wrapped struct
                as a first arg (ignoring any fz_context* arg), and wrap these
                into auto-generated class methods. If there are duplicate
                prototypes, we comment-out all but the first.

                Auto-generated methods are omitted if a custom method is
                defined with the same name.

            Other:

                There are various subleties with wrapping classes that are not
                copyable etc.

        mupdf::* functions methods generally have the same args as the fz_*
        functions that they wrap except that they don't take any fz_context*
        parameter. If the fz_* functions takes a fz_context*, one appropriate
        for the current thread is generated automatically at runtime, using
        platform/c++/implementation/internal.cpp:internal_context_get().

        Extra items:

            metadata_keys: This contains the keys that are suitable for passing to
            fz_lookup_metadata() and its wrappers, mupdf::fz_lookup_metadata()
            and mupdf::Document::lookup_metadata().


Python wrapping:

    We provide a Python module called 'mupdf' which directly wraps the C++ API,
    using identical names for functions, classes and methods.

    Out-parameters:

        Functions and methods that have out-parameters are modified to return
        the out-parameters directly, usually as a tuple.

        Examples:

            fz_read_best():

                The MuPDF C function is:

                    fz_buffer *fz_read_best(fz_context *ctx, fz_stream *stm, size_t initial, int *truncated);

                The C++ wrapper is:

                    fz_buffer *read_best(fz_stream *stm, size_t initial, int *truncated);

                The python wrapper is:

                    def read_best(stm, initial)

                and returns: (buffer, truncated), where <buffer> is a SWIG
                proxy for a fz_buffer instance and <truncated> is an integer.

            pdf_parse_ind_obj:

                The MuPDF C function is:

                    pdf_obj *pdf_parse_ind_obj(fz_context *ctx, pdf_document *doc, fz_stream *f, pdf_lexbuf *buf, int *num, int *gen, int64_t *stm_ofs, int *try_repair);

                The C++ wrapper is:

                    pdf_obj *ppdf_parse_ind_obj(pdf_document *doc, fz_stream *f, pdf_lexbuf *buf, int *num, int *gen, int64_t *stm_ofs, int *try_repair);

                The Python wrapper is:

                    def ppdf_parse_ind_obj(doc, f, buf)

                and returns: (ret, num, gen, stm_ofs, try_repair)

            Where MuPDF functions are wrapped as C++ class methods, the python class methods
            return out-parameters in a similar way.

    Access to buffer data:

        Wrappers for fz_buffer_extract():

            mupdf.Buffer.buffer_extract() returns a Python bytes instance.

            An extra method mupdf.Buffer.buffer_extract_raw() is provided
            which returns (size, data) from the underlying fz_buffer_extract()
            function. One use for this is to pass back into C/C++ with a call
            to mupdf.Stream(data, size).

        Wrappers for fz_buffer_storage():

            mupdf.Buffer.buffer_storage() is not provided to Python because the
            semantics are not useful - creating a Python bytes object always
            takes a copy of the underlying data.

            An extra method mupdf.Buffer.buffer_extract_raw() is provided
            which returns (size, data) from the underlying fz_buffer_extract()
            function.

    Functions taking a va_list arg:

        We do not provide Python wrappers for functions such as fz_vsnprintf().

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

        We work with clang-6 or clang-7, but clang-6 appears to not be able
        to cope with function args that are themselves function pointers, so
        wrappers for these fz_*() functions are ommited from the generated C++
        code.

        On unix it seems that clang-python packages such as Debian's
        python-clang and OpenBSD's py3-llvm require us to explicitly specify
        the location of libclang. Alternatively on Linux one can use our --venv
        option to run in a venv that has done 'pip install libclang', which
        makes clang available directly as a Python module.

    SWIG:

        We work with swig-3 and swig-4. If swig-4 is used, we propogate
        doxygen-style comments for structures and functions into the generated
        C++ code.


Building Python bindings:

    Build and install the MuPDF Python bindings as module 'mupdf' in a Python
    virtual environment, using MuPDF's setup.py script:

        Linux:
            > python3 -m venv pylocal
            > . pylocal/bin/activate
            (pylocal) > pip install pyqt5 libclang
            (pylocal) > cd .../mupdf
            (pylocal) > python setup.py install

        Windows:
            > py -m venv pylocal
            > pylocal\Scripts\activate
            (pylocal) > pip install libclang pyqt5
            (pylocal) > cd ...\mupdf
            (pylocal) > python setup.py install

        OpenBSD:
            [It seems that pip can't install py1t5 or libclang so instead we
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
                > sudo apt install clang clang-python python3-dev swig

            OpenBSD:
                > pkg_add py3-llvm py3-qt5

        Build:
            > ./scripts/mupdfwrap.py -d build/shared-release -b all

        Use the mupdf module by setting PYTHONPATH:
            > PYTHONPATH=build/shared-release python3
            >>> import mupdf
            >>>


Generated files:

    build/  [Generated by '-b m13']
        shared-release/     [Unix runtime files.]
            libmupdf.so     [MuPDF C library.]
            libmupdfcpp.so  [MuPDF C++ library.]
            mupdf.py        [MuPDF Python module.]
            _mupdf.so       [MuPDF Python internals.]

        shared-release-x32-py3.8/   [Windows runtime files.]
            mupdfcpp.dll            [MuPDF C and C++ library.]
            mupdf.py                [MuPDF Python module.]
            _mupdf.pyd              [MuPDF Python internals.]

    platform/
        c++/    [Files generated by '-b 0']
            include/ [C++ header files]
                mupdf/
                    classes.h
                    exceptions.h
                    functions.h
                    internal.h

            implementation/ [C++ source code]
                classes.cpp
                exceptions.cpp
                functions.cpp
                internal.cpp

            [Various misc auto-generated files, used in Windows builds.]
            c_functions.pickle
            c_globals.pickle
            container_classnames.pickle
            swig_c.pickle
            swig_python.pickle
            to_string_structnames.pickle
            windows_mupdf.def   [List of MuPDF public global data, used when linking mupdfcpp.dll.]

        python/
            mupdfcpp_swig.cpp   [Generated by '-b 2' running swig.]
            mupdfcpp_swig.i     [Generated by '-b 2'.]

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

        We build Windows binaries by running devenv.com directly. As of
        2021-05-17 the location of devenv.com is hard-coded in this Python
        script.

        Building _mupdf.pyd is tricky because it needs to be built with a
        specific Pyhthon.h and linked with a specific python.lib. This is
        done by setting environmental variables MUPDF_PYTHON_INCLUDE_PATH and
        MUPDF_PYTHON_LIBRARY_PATH when running devenv.com, which are referenced
        by platform/win32/mupdfpyswig.vcxproj. Thus one cannot easily build
        _mupdf.pyd directly from the Visual Studio GUI.

        [There is some older code that builds _mupdf.pyd by running the Windows
        compiler and linker cl.exe and link.exe directly, which avoids the
        complications of going via devenv, at the expense of needing to know
        where cl.exe and link.exe are.]

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
                -d <details>:
                    If specified, we show extra diagnostics when wrapping
                    functions whose name contains <details>.
                -f:
                    Force rebuilds.

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

                    Generates:
                        mupdf/platform/python/mupdfcpp_swig.cpp
                        mupdf/platform/python/mupdf.py

                    Note that this requires action=0 to have been run previously.

                3:
                    Compile and links the mupdfcpp_swig.cpp file created by
                    action=2. Requires libmupdf.so to be available, e.g. built by
                    the --libmupdf.so option.

                    Generates:
                        mupdf/platform/python/_mupdf.so

                    Along with mupdf/platform/python/mupdf.py (generated by
                    action=2), this implements the mupdf python module.

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
            Set directory containing shared libraries.

            Default is: build/shared-release

            We use different C++ compile flags depending on release or debug
            builds (specifically, the definition of NDEBUG is important because
            it must match what was used when libmupdf.so was built).

            Examples:
                -d build/shared-debug
                -d build/shared-release [default]

            On Windows one can specify the CPU and Python version; we then
            use 'py -0f' to find the matching installed Python along with its
            Python.h and python.lib. For example:

                -d build/shared-release-x32-py3.8
                -d build/shared-release-x64-py3.9

        --doc <languages>
            Generates documentation for the different APIs.

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

        --sync [-d] <destination>
            Use rsync to copy C++ source generated by action=0 to
            <destination>. Also copies syntax-coloured .html versions.

            If '-d' is specified, also sync all files generated by --doc.

            E.g. --sync julian@casper.ghostscript.com:~julian/public_html/

            As of 2020-03-09 requires patched mupdf/ checkout.

        -t
        --test
            Tests the python API.

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

        --venv <venv> ...
            Runs mupdfwrap.py in a venv containing clang installed with 'pip
            install libclang', passing remaining args. This seems to be the
            only way to use clang from python on Windows.

            E.g.:
                --venv pylocal --swig-windows-auto -b all -t

        --windows-cmd ...
            Runs mupdfwrap.py via cmd.exe, passing remaining args. Useful to
            get from cygwin to native Windows.

            E.g.:
                --windows-cmd --venv pylocal --swig-windows-auto -b all

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

        ./scripts/mupdfwrap.py --venv pylocal --swig-windows-auto -b all -t
            Build and test on Windows.


'''


import glob
import io
import os
import pickle
import platform
import re
import shutil
import sys
import textwrap
import time
import traceback

import jlib

os_name = platform.system()

g_windows = (os_name == 'Windows' or os_name.startswith('CYGWIN'))
g_cygwin = os_name.startswith('CYGWIN')
g_openbsd = os_name == 'OpenBSD'
g_linux = os_name == 'Linux'

log = jlib.log
log0 = jlib.log0
log1 = jlib.log1
log2 = jlib.log2
log3 = jlib.log3
log4 = jlib.log4
log5 = jlib.log5
logx = jlib.logx

# We use f-strings, so need python-3.6+.
assert sys.version_info[0] == 3 and sys.version_info[1] >= 6, (
        'We require python-3.6+')

try:
    try:
        import clang.cindex
    except ModuleNotFoundError as e:

        # On devuan, clang-python isn't on python3's path, but python2's
        # clang-python works fine with python3, so we deviously get the path by
        # running some python 2.
        #
        e, clang_path = jlib.system( 'python2 -c "import clang; print clang.__path__[0]"', out='return', raise_errors=0)

        if e == 0:
            log( 'Retrying import of clang using info from python2 {clang_path=}')
            sys.path.append( os.path.dirname( clang_path))
            import clang.cindex
        else:
            raise

except Exception as e:
    log('Warning: failed to import clang.cindex: {e=}\n'
            f'We need Clang Python to build MuPDF python.\n'
            f'Install with "pip install libclang" or use the --venv option, or:\n'
            f'    OpenBSD: pkg_add py3-llvm\n'
            f'    Linux:debian/devuan: apt install python-clang\n'
            )
    clang = None


# Code should show extra information if g_show_details(name) returns true.
#
g_show_details = lambda name: False

class ClangInfo:
    '''
    Sets things up so we can import and use clang.

    Members:
        .libclang_so
        .resource_dir
        .include_path
        .clang_version
    '''
    def __init__( self):
        '''
        We look for different versions of clang until one works.

        Searches for libclang.so and registers with
        clang.cindex.Config.set_library_file(). This appears to be necessary
        even when clang is installed as a standard package.
        '''
        if g_windows:
            # We require 'pip install libclang' which avoids the need to look
            # for libclang.
            return
        for version in 11, 10, 9, 8, 7, 6,:
            ok = self._try_init_clang( version)
            if ok:
                break
        else:
            raise Exception( 'cannot find libclang.so')

    def _try_init_clang( self, version):
        if g_openbsd:
            clang_bin = glob.glob( f'/usr/local/bin/clang-{version}')
            if not clang_bin:
                log('Cannot find {clang_bin=}')
                return
            clang_bin = clang_bin[0]
            self.clang_version = version
            libclang_so = glob.glob( f'/usr/local/lib/libclang.so*')
            assert len(libclang_so) == 1
            self.libclang_so = libclang_so[0]
            self.resource_dir = jlib.system(
                    f'{clang_bin} -print-resource-dir',
                    out='return',
                    ).strip()
            self.include_path = os.path.join( self.resource_dir, 'include')
            logx('{self.libclang_so=} {self.resource_dir=} {self.include_path=}')
            if os.environ.get('VIRTUAL_ENV'):
                clang.cindex.Config.set_library_file( self.libclang_so)
            return True

        for p in os.environ.get( 'PATH').split( ':'):
            clang_bins = glob.glob( os.path.join( p, f'clang-{version}*'))
            if not clang_bins:
                continue
            clang_bins.sort()
            for clang_bin in clang_bins:
                e, clang_search_dirs = jlib.system(
                        f'{clang_bin} -print-search-dirs',
                        #verbose=log,
                        out='return',
                        raise_errors=False,
                        )
                if e:
                    log( '[could not find {clang_bin}: {e=}]')
                    return
                if version == 10:
                    m = re.search( '\nlibraries: =(.+)\n', clang_search_dirs)
                    assert m
                    clang_search_dirs = m.group(1)
                clang_search_dirs = clang_search_dirs.strip().split(':')
                for i in ['/usr/lib', '/usr/local/lib'] + clang_search_dirs:
                    for leaf in f'libclang-{version}.*so*', f'libclang.so.{version}.*':
                        p = os.path.join( i, leaf)
                        p = os.path.abspath( p)
                        log( '{p=}')
                        libclang_so = glob.glob( p)
                        if not libclang_so:
                            continue

                        # We have found libclang.so.
                        self.libclang_so = libclang_so[0]
                        log( 'Using {self.libclang_so=}')
                        clang.cindex.Config.set_library_file( self.libclang_so)
                        self.resource_dir = jlib.system(
                                f'{clang_bin} -print-resource-dir',
                                out='return',
                                ).strip()
                        self.include_path = os.path.join( self.resource_dir, 'include')
                        self.clang_version = version
                        return True


clang_info_cache = None

def clang_info():
    global clang_info_cache
    if not clang_info_cache:
        clang_info_cache = ClangInfo()
    return clang_info_cache



def snake_to_camel( name, initial):
    '''
    Converts foo_bar to FooBar or fooBar.
    '''
    items = name.split( '_')
    ret = ''
    for i, item in enumerate( items):
        if not item:
            item = '_'
        elif i or initial:
            item = item[0].upper() + item[1:]
        ret += item
    return ret

assert snake_to_camel( 'foo_bar', True) == 'FooBar'
assert snake_to_camel( 'foo_bar_q__a', False) == 'fooBarQ_A'


def clip( text, prefixes, suffixes=''):
    '''
    Returns <text> with prefix(s) and suffix(s) removed if present.
    '''
    if isinstance( prefixes, str):
        prefixes = prefixes,
    if isinstance( suffixes, str):
        suffixes = suffixes,
    for prefix in prefixes:
        if text.startswith( prefix):
            text = text[ len( prefix):]
            break
    for suffix in suffixes:
        if suffix and text.endswith( suffix):
            text = text[ :-len( suffix)]
            break
    return text

def fileline( cursor):
    return f'{cursor.location.file}:{cursor.location.line}'


class Rename:
    '''
    Rename functions that generate camelCase class names and lower-case
    function names with underscores.

    Using camel case in function names seems to result in gcc errors when
    compiling the code created by swig -python. e.g. in _wrap_vthrow_fn()

    mupdfcpp_swig.cpp: In function PyObject* _wrap_vthrow_fn(PyObject*, PyObject*)
    mupdfcpp_swig.cpp:88571:15: error: invalid array assignment
           arg3 = *temp;
    '''
    def function_raw( self, name):
        '''
        Name used by wrapper function when calling C function <name>.
        '''
        return f'::{name}'
    def function( self, name):
        '''
        Name of wrapper function that calls C function <name>.
        '''
        if name.startswith( 'pdf_'):
            return 'p' + name
        ret = f'{clip( name, "fz_")}'
        if ret in ('stdin', 'stdout', 'stderr'):
            logx( 'appending underscore to {ret=}')
            ret += '_'
        return ret
    def function_call( self, name):
        '''
        Name used by class methods when calling wrapper function - we call our
        wrapper function in the mupdf:: namespace.
        '''
        return f'mupdf::{self.function(name)}'
    def class_( self, structname):
        '''
        Name of class that wraps <structname>.
        '''
        structname = clip( structname, 'struct ')
        if structname.startswith( 'fz_'):
            return snake_to_camel( clip( structname, 'fz_'), initial=True)
        elif structname.startswith( 'pdf_'):
            # Retain Pdf prefix.
            return snake_to_camel( structname, initial=True)
    def internal( self, name):
        return f'internal_{name}'
    def method( self, structname, fnname):
        if structname.startswith( 'fz_'):
            ret = clip( fnname, "fz_")
            if ret in ('stdin', 'stdout', 'stderr'):
                log( 'appending underscore to {ret=}')
                ret += '_'
            return ret
        if structname.startswith( 'pdf_'):
            return clip( fnname, "pdf_")
        assert 0, f'unrecognised structname={structname}'

rename = Rename()

def prefix( name):
    if name.startswith( 'fz_'):
        return 'fz_'
    if name.startswith( 'pdf_'):
        return 'pdf_'
    assert 0, f'unrecognised prefix (not fz_ or pdf_) in name={name}'


# Specify extra methods to be included in generated classes.
#

class ExtraMethod:
    '''
    Defines a prototype and implementation of a custom method in a generated
    class.
    '''
    def __init__( self, return_, name_args, body, comment=None, overload=None):
        '''
        return_:
            Return type as a string.
        name_args:
            A string describing name and args of the method:
                <method-name>(<args>)
        body:
            Implementation code including the enclosing '{...}'.
        comment:
            Optional comment; should include /* and */ or //.
        overload:
            If true, we allow auto-generation of methods with same name.
        '''
        assert name_args
        self.return_ = return_
        self.name_args = name_args
        self.body = body
        self.comment = comment
        self.overload = overload
        assert '\t' not in body


class ExtraConstructor:
    '''
    Defines a prototype and implementation of a custom method in a generated
    class.
    '''
    def __init__( self, name_args, body, comment=None):
        '''
        name_args:
            A string of the form: (<args>)
        body:
            Implementation code including the enclosing '{...}'.
        comment:
            Optional comment; should include /* and */ or //.
        '''
        self.return_ = ''
        self.name_args = name_args
        self.body = body
        self.comment = comment
        assert '\t' not in body


class ClassExtra:
    '''
    Information about extra methods to be added to an auto-generated class.
    '''
    def __init__( self,
            accessors=None,
            class_bottom='',
            class_post='',
            class_pre='',
            class_top='',
            constructor_prefixes=None,
            constructor_raw=True,
            constructor_excludes=None,
            constructors_extra=None,
            constructors_wrappers=None,
            copyable=True,
            extra_cpp='',
            iterator_next=None,
            methods_extra=None,
            methods_extra_overload=None,
            method_wrappers=None,
            method_wrappers_static=None,
            opaque=False,
            pod=False,
            ):
        '''
        accessors:
            If true, we generate accessors methods for all items in the
            underlying struct.

            Defaults to True if pod is True, else False.

        class_bottom:
            Extra text at end of class definition, e.g. for member variables.

        class_post:
            Extra text after class definition, e.g. complete definition of
            iterator class.

        class_pre:
            Extra text before class definition, e.g. forward declaration of
            iterator class.

        class_top:
            Extra text at start of class definition, e.g. for enums.

        constructor_prefixes:
            Extra fz_*() function name prefixes that can be used by class
            constructors_wrappers. We find all functions whose name starts with one of
            the specified prefixes and which returns a pointer to the relevant
            fz struct.

            For each function we find, we make a constructor that takes the
            required arguments and set m_internal to what this function
            returns.

            If there is a '-' item, we omit the default 'fz_new_<type>' prefix.

        constructor_raw:
            If true, create a constructor that takes a pointer to an instance
            of the wrapped fz_ struct. If 'default', this constructor arg
            defaults to NULL.

        constructor_excludes:
            Lists of constructor functions to ignore.

        constructors_extra:
            List of ExtraConstructor's, allowing arbitrary constructors_wrappers to be
            specified.

        constructors_wrappers:
            List of fns to use as constructors_wrappers.

        copyable:
            If 'default' we allow default copy constructor to be created by C++
            compiler.

            Otherwise if true, generated wrapping class must be copyable. If
            pod is false, we generate a copy constructor by looking for a
            fz_keep_*() function; it's an error if we can't find this function.

            Otherwise if false we create a private copy constructor.

            [todo: need to check docs for interaction of pod/copyable.]

        extra_cpp:
            Extra text for .cpp file, e.g. implementation of iterator class
            methods.

        iterator_next:
            Support for iterating forwards over linked list.

            Should be (first, last).

            first:
                Name of element within the wrapped class that points to the
                first item in the linked list. We assume that this element will
                have 'next' pointers that terminate in NULL.

                If <first> is '', the container is itself the first element in
                the linked list.

            last:
                Currently unused, but could be used for reverse iteration in
                the future.

            We generate begin() and end() methods, plus a separate iterator
            class, to allow iteration over the linked list starting at
            <structname>::<first> and iterating to ->next until we reach NULL.

        methods_extra:
            List of ExtraMethod's, allowing arbitrary methods to be specified.

        method_wrappers:
            Extra fz_*() function names that should be wrapped in class
            methods.

            E.g. 'fz_foo_bar' is converted to a method called foo_bar()
            that takes same parameters as fz_foo_bar() except context and
            any pointer to struct and fz_context*. The implementation calls
            fz_foo_bar(), converting exceptions etc.

            The first arg that takes underlying fz_*_s type is omitted and
            implementation passes <this>.

        method_wrappers_static:
            Like <method_wrappers>, but generates static methods, where no args
            are replaced by <this>.

        opaque:
            If true, we generate a wrapper even if there's no definition
            available for the struct, i.e. it's only available as a forward
            declaration.

        pod:
            If True, underlying class is POD and m_internal is an instance of
            the underlying class instead of a pointer to it.

            In addition if 'inline', there is no m_internal; instead, each
            member of the underlying class is placed in the wrapping class.

        '''
        if accessors is None and pod is True:
            accessors = True
        self.accessors = accessors
        self.class_bottom = class_bottom
        self.class_post = class_post
        self.class_pre = class_pre
        self.class_top = class_top
        self.constructor_excludes = constructor_excludes or []
        self.constructor_prefixes = constructor_prefixes or []
        self.constructor_raw = constructor_raw
        self.constructors_extra = constructors_extra or []
        self.constructors_wrappers = constructors_wrappers or []
        self.copyable = copyable
        self.extra_cpp = extra_cpp
        self.iterator_next = iterator_next
        self.methods_extra = methods_extra or []
        self.method_wrappers = method_wrappers or []
        self.method_wrappers_static = method_wrappers_static or []
        self.opaque = opaque
        self.pod = pod

        assert self.pod in (False, True, 'inline'), f'{self.pod}'

        assert isinstance( self.constructor_prefixes, list)
        for i in self.constructor_prefixes:
            assert isinstance( i, str)
        assert isinstance( self.method_wrappers, list)
        for i in self.method_wrappers:
            assert isinstance( i, str)
        assert isinstance( self.method_wrappers_static, list)
        assert isinstance( self.methods_extra, list)
        for i in self.methods_extra:
            assert isinstance( i, ExtraMethod)
        assert isinstance( self.constructors_extra, list)
        for i in self.constructors_extra:
            assert isinstance( i, ExtraConstructor)


class ClassExtras:
    '''
    Information about the various extra methods to be added to auto-generated
    classes.
    '''
    def __init__( self, **namevalues):
        '''
        namevalues:
            Named args mapping from struct name (e.g. fz_document) to a
            ClassExtra.
        '''
        self.items = dict()
        for name, value in namevalues.items():
            self.items[ name] = value
    def get( self, name):
        '''
        If <name> is in omit_class_names0, returns None.

        Otherwise searches for <name>; if found, returns ClassExtra instance,
        else empty ClassExtra instance.
        '''
        name = clip( name, 'struct ')
        if name in omit_class_names0:
            return
        if not name.startswith( ('fz_', 'pdf_')):
            return
        return self.items.get( name, ClassExtra())
    def get_or_none( self, name):
        return self.items.get( name)


# These functions are known to return a pointer to a fz_* struct that must not
# be dropped.
#
# This matters if we wrap in a class method, because this will return class
# wrapper for the struct, whose destructor will call fz_drop_*(). In this case,
# we need to call fz_keep_*() before returning the class wrapper.
#
functions_that_return_non_kept = [
        'fz_default_cmyk',
        'fz_default_rgb',
        'fz_default_cmyk',
        'fz_default_output_intent',
        'fz_document_output_intent',
        ]


classextras = ClassExtras(

        fz_aa_context = ClassExtra(
                pod='inline',
                ),

        fz_band_writer = ClassExtra(
                class_top = '''
                    enum Cm
                    {
                        MONO,
                        COLOR,
                    };
                    enum P
                    {
                        PNG,
                        PNM,
                        PAM,
                        PBM,
                        PKM,
                        PS,
                        PSD,
                    };
                    ''',
                constructors_extra = [
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, Cm cm, const {rename.class_("fz_pcl_options")}& options)',
                        f'''
                        {{
                            fz_output*              out2 = out.m_internal;
                            const fz_pcl_options*   options2 = options.m_internal;
                            if (0)  {{}}
                            else if (cm == MONO)    m_internal = {rename.function_call('fz_new_mono_pcl_band_writer' )}( out2, options2);
                            else if (cm == COLOR)   m_internal = {rename.function_call('fz_new_color_pcl_band_writer')}( out2, options2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s Cm type");
                        }}
                        ''',
                        comment = f'/* Calls fz_new_mono_pcl_band_writer() or fz_new_color_pcl_band_writer(). */',
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, P p)',
                        f'''
                        {{
                            fz_output*              out2 = out.m_internal;
                            if (0)  {{}}
                            else if (p == PNG)  m_internal = {rename.function_call('fz_new_png_band_writer')}( out2);
                            else if (p == PNM)  m_internal = {rename.function_call('fz_new_pnm_band_writer')}( out2);
                            else if (p == PAM)  m_internal = {rename.function_call('fz_new_pam_band_writer')}( out2);
                            else if (p == PBM)  m_internal = {rename.function_call('fz_new_pbm_band_writer')}( out2);
                            else if (p == PKM)  m_internal = {rename.function_call('fz_new_pkm_band_writer')}( out2);
                            else if (p == PS)   m_internal = {rename.function_call('fz_new_ps_band_writer' )}( out2);
                            else if (p == PSD)  m_internal = {rename.function_call('fz_new_psd_band_writer')}( out2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s P type");
                        }}
                        ''',
                        comment = f'/* Calls fz_new_p*_band_writer(). */',
                        ),
                    ExtraConstructor(
                        f'({rename.class_("fz_output")}& out, Cm cm, const {rename.class_("fz_pwg_options")}& options)',
                        f'''
                        {{
                            fz_output*              out2 = out.m_internal;
                            const fz_pwg_options*   options2 = &options.m_internal;
                            if (0)  {{}}
                            else if (cm == MONO)    m_internal = {rename.function_call('fz_new_mono_pwg_band_writer' )}( out2, options2);
                            else if (cm == COLOR)   m_internal = {rename.function_call('fz_new_pwg_band_writer')}( out2, options2);
                            else throw std::runtime_error( "Unrecognised fz_band_writer_s Cm type");
                        }}
                        ''',
                        comment = f'/* Calls fz_new_mono_pwg_band_writer() or fz_new_pwg_band_writer(). */',
                        ),
                    ],
                copyable = False,
                ),

        fz_bitmap = ClassExtra(
                accessors = True,
                ),

        fz_buffer = ClassExtra(
                constructors_wrappers = [
                    'fz_read_file',
                    ],
                ),

        fz_color_params = ClassExtra(
                pod='inline',
                ),

        fz_colorspace = ClassExtra(
                constructors_extra = [
                    ExtraConstructor(
                        '(Fixed fixed)',
                        f'''
                        {{
                            if (0) {{}}
                            else if ( fixed == Fixed_GRAY)  m_internal = {rename.function_call( 'fz_device_gray')}();
                            else if ( fixed == Fixed_RGB)   m_internal = {rename.function_call( 'fz_device_rgb' )}();
                            else if ( fixed == Fixed_BGR)   m_internal = {rename.function_call( 'fz_device_bgr' )}();
                            else if ( fixed == Fixed_CMYK)  m_internal = {rename.function_call( 'fz_device_cmyk')}();
                            else if ( fixed == Fixed_LAB)   m_internal = {rename.function_call( 'fz_device_lab' )}();
                            else {{
                                std::string message = "Unrecognised fixed colorspace id";
                                throw ErrorGeneric(message.c_str());
                            }}
                            {rename.function_call('fz_keep_colorspace')}(m_internal);
                        }}
                        ''',
                        ),
                        ExtraConstructor(
                        '()',
                        '''
                        : m_internal( NULL)
                        {
                        }
                        ''',
                        comment = '/* Sets m_internal = NULL. */',
                        ),
                    ],
                constructor_raw=1,
                class_top = '''
                        enum Fixed
                        {
                            Fixed_GRAY,
                            Fixed_RGB,
                            Fixed_BGR,
                            Fixed_CMYK,
                            Fixed_LAB,
                        };
                        ''',
                ),

        fz_cookie = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                    '''
                    {
                        this->m_internal.abort = 0;
                        this->m_internal.progress = 0;
                        this->m_internal.progress_max = (size_t) -1;
                        this->m_internal.errors = 0;
                        this->m_internal.incomplete = 0;
                    }
                    ''',
                    comment = '/* Sets all fields to default values. */',
                    ),
                    ],
                constructor_raw = False,
                methods_extra = [
                    ExtraMethod( 'void',    'set_abort()',                  '{ m_internal.abort = 1; }\n'),

                    # do we need these? - already provided by accessors.
                    ExtraMethod( 'int',     'get_progress()',               '{ return m_internal.progress; }\n'),
                    ExtraMethod( 'size_t',  'get_progress_max()',           '{ return m_internal.progress_max; }\n'),
                    ExtraMethod( 'int',     'get_errors()',                 '{ return m_internal.errors; }\n'),
                    ExtraMethod( 'int',     'get_incomplete()',             '{ return m_internal.incomplete; }\n'),

                    ExtraMethod( 'void',    'increment_errors(int delta)',  '{ m_internal.errors += delta; }\n'),
                ],
                pod = True,
                # I think other code asyncronously writes to our fields, so we
                # are not be copyable. todo: maybe tie us to all objects to
                # which we have been associated?
                #
                copyable=False,
                ),

        fz_device = ClassExtra(
                constructor_raw = True,
                method_wrappers_static = [
                        ],
                constructors_extra = [
                    ExtraConstructor( '()',
                        '''
                        : m_internal( NULL)
                        {
                        }
                        ''',
                        comment = '/* Sets m_internal = NULL. */',
                        ),
                    ]
                ),

        fz_display_list = ClassExtra(
                ),

        fz_document = ClassExtra(
                constructor_excludes = [
                    'fz_new_xhtml_document_from_document',
                    ],
                constructor_prefixes = [
                    'fz_open_accelerated_document',
                    'fz_open_document',
                    ],
                method_wrappers = [
                    'fz_load_outline',
                ],
                method_wrappers_static = [
                    'fz_new_xhtml_document_from_document',
                    ],
                methods_extra = [
                    # This duplicates our creation of extra lookup_metadata()
                    # function in make_function_wrappers(). Maybe we could
                    # parse the generated functions.h instead of fitz.h so that
                    # we pick up extra C++ wrappers automatically, but this
                    # would be a fairly major change.
                    #
                    ExtraMethod(
                            'std::string',
                            'lookup_metadata(const char* key, int* o_out=NULL)',
                            f'''
                            {{
                                return {rename.function_call("lookup_metadata")}(m_internal, key, o_out);
                            }}
                            ''',
                           textwrap.dedent('''
                            /* Wrapper for fz_lookup_metadata() that returns a std::string and sets
                            *o_out to length of string plus one. If <key> is not found, returns empty
                            string with *o_out=-1. <o_out> can be NULL if caller is not interested in
                            error information. */
                            ''')
                            ),
                    ],
                ),

        # This is a little complicated. Many of the functions that we would
        # like to wrap to form constructors, have the same set of args. C++
        # does not support named constructors so we differentiate between
        # constructors with identical args using enums.
        #
        # Also, fz_document_writer is not reference counted so the wrapping
        # class is not copyable or assignable, so our normal approach of making
        # static class functions that return a newly constructed instance by
        # value, does not work.
        #
        # So instead we define enums that are passed to our constructors,
        # allowing the constructor to decide which fz_ function to use to
        # create the new fz_document_writer.
        #
        # There should be no commented-out constructors in the generated code
        # marked as 'Disabled because same args as ...'.
        #
        fz_document_writer = ClassExtra(
                class_top = '''
                    /* Used for constructor that wraps fz_ functions taking (const char *path, const char *options). */
                    enum PathType
                    {
                        PathType_CBZ,
                        PathType_DOCX,
                        PathType_ODT,
                        PathType_PAM_PIXMAP,
                        PathType_PBM_PIXMAP,
                        PathType_PCL,
                        PathType_PCLM,
                        PathType_PDF,
                        PathType_PDFOCR,
                        PathType_PGM_PIXMAP,
                        PathType_PKM_PIXMAP,
                        PathType_PNG_PIXMAP,
                        PathType_PNM_PIXMAP,
                        PathType_PPM_PIXMAP,
                        PathType_PS,
                        PathType_PWG,
                        PathType_SVG,
                    };

                    /* Used for constructor that wraps fz_ functions taking (Output& out, const char *options). */
                    enum OutputType
                    {
                        OutputType_CBZ,
                        OutputType_DOCX,
                        OutputType_ODT,
                        OutputType_PCL,
                        OutputType_PCLM,
                        OutputType_PDF,
                        OutputType_PDFOCR,
                        OutputType_PS,
                        OutputType_PWG,
                    };

                    /* Used for constructor that wraps fz_ functions taking (const char *format, const char *path, const char *options). */
                    enum FormatPathType
                    {
                        FormatPathType_DOCUMENT,
                        FormatPathType_TEXT,
                    };
                ''',
                # These excludes should match the functions called by the
                # extra constructors defined below. This ensures that we don't
                # generate commented-out constructors with a comment saying
                # 'Disabled because same args as ...'.
                constructor_excludes = [
                    'fz_new_cbz_writer',
                    'fz_new_docx_writer',
                    'fz_new_odt_writer',
                    'fz_new_pam_pixmap_writer',
                    'fz_new_pbm_pixmap_writer',
                    'fz_new_pcl_writer',
                    'fz_new_pclm_writer',
                    'fz_new_pdfocr_writer',
                    'fz_new_pdf_writer',
                    'fz_new_pgm_pixmap_writer',
                    'fz_new_pkm_pixmap_writer',
                    'fz_new_png_pixmap_writer',
                    'fz_new_pnm_pixmap_writer',
                    'fz_new_ppm_pixmap_writer',
                    'fz_new_ps_writer',
                    'fz_new_pwg_writer',
                    'fz_new_svg_writer',

                    'fz_new_cbz_writer_with_output',
                    'fz_new_docx_writer_with_output',
                    'fz_new_odt_writer_with_output',
                    'fz_new_pcl_writer_with_output',
                    'fz_new_pclm_writer_with_output',
                    'fz_new_pdf_writer_with_output',
                    'fz_new_pdfocr_writer_with_output',
                    'fz_new_ps_writer_with_output',
                    'fz_new_pwg_writer_with_output',

                    'fz_new_document_writer',
                    'fz_new_text_writer',

                    'fz_new_document_writer_with_output',
                    'fz_new_text_writer_with_output',
                    ],

                copyable=False,
                methods_extra = [
                    ExtraMethod( 'Device', 'begin_page(Rect& mediabox)',
                        f'''
                        {{
                            /* fz_begin_page() doesn't transfer ownership, so
                            we have to call fz_keep_device() before creating
                            the Device instance. */
                            fz_device* dev = {rename.function_call('fz_begin_page')}(m_internal, *(fz_rect*) &mediabox.x0);
                            dev = {rename.function_call('fz_keep_device')}(dev);
                            return Device(dev);
                        }}
                        ''',
                        textwrap.dedent(f'''
                        /*
                        Custom wrapper for fz_begin_page().

                        Called to start the process of writing a page to
                        a document.

                        mediabox: page size rectangle in points.

                        Returns a {rename.class_('fz_device')} to write page contents to.
                        */
                        '''),
                        ),
                        ],
                constructors_extra = [
                    ExtraConstructor(
                        '(const char *path, const char *options, PathType path_type)',
                        f'''
                        {{
                            if (0) {{}}
                            else if (path_type == PathType_CBZ)         m_internal = {rename.function_call( 'fz_new_cbz_writer')}(path, options);
                            else if (path_type == PathType_DOCX)        m_internal = {rename.function_call( 'fz_new_docx_writer')}(path, options);
                            else if (path_type == PathType_ODT)         m_internal = {rename.function_call( 'fz_new_odt_writer')}(path, options);
                            else if (path_type == PathType_PAM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_pam_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PBM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_pbm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PCL)         m_internal = {rename.function_call( 'fz_new_pcl_writer')}(path, options);
                            else if (path_type == PathType_PCLM)        m_internal = {rename.function_call( 'fz_new_pclm_writer')}(path, options);
                            else if (path_type == PathType_PDF)         m_internal = {rename.function_call( 'fz_new_pdf_writer')}(path, options);
                            else if (path_type == PathType_PDFOCR)      m_internal = {rename.function_call( 'fz_new_pdfocr_writer')}(path, options);
                            else if (path_type == PathType_PGM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_pgm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PKM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_pkm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PNG_PIXMAP)  m_internal = {rename.function_call( 'fz_new_png_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PNM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_pnm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PPM_PIXMAP)  m_internal = {rename.function_call( 'fz_new_ppm_pixmap_writer')}(path, options);
                            else if (path_type == PathType_PS)          m_internal = {rename.function_call( 'fz_new_ps_writer')}(path, options);
                            else if (path_type == PathType_PWG)         m_internal = {rename.function_call( 'fz_new_pwg_writer')}(path, options);
                            else if (path_type == PathType_SVG)         m_internal = {rename.function_call( 'fz_new_svg_writer')}(path, options);
                            else throw ErrorAbort( "Unrecognised Type value");
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_cbz_writer()
                            fz_new_docx_writer()
                            fz_new_odt_writer()
                            fz_new_pam_pixmap_writer()
                            fz_new_pbm_pixmap_writer()
                            fz_new_pcl_writer()
                            fz_new_pclm_writer()
                            fz_new_pdf_writer()
                            fz_new_pdfocr_writer()
                            fz_new_pgm_pixmap_writer()
                            fz_new_pkm_pixmap_writer()
                            fz_new_png_pixmap_writer()
                            fz_new_pnm_pixmap_writer()
                            fz_new_ppm_pixmap_writer()
                            fz_new_ps_writer()
                            fz_new_pwg_writer()
                            fz_new_svg_writer()
                        */'''),
                        ),
                    ExtraConstructor(
                        '(Output& out, const char *options, OutputType output_type)',
                        f'''
                        {{
                            /* All fz_new_*_writer_with_output() functions take
                            ownership of the fz_output, even if they throw an
                            exception. So we need to set out.m_internal to null
                            here so its destructor does nothing. */
                            fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            if (0) {{}}
                            else if (output_type == OutputType_CBZ)     m_internal = {rename.function_call( 'fz_new_cbz_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_DOCX)    m_internal = {rename.function_call( 'fz_new_docx_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_ODT)     m_internal = {rename.function_call( 'fz_new_odt_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PCL)     m_internal = {rename.function_call( 'fz_new_pcl_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PCLM)    m_internal = {rename.function_call( 'fz_new_pclm_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PDF)     m_internal = {rename.function_call( 'fz_new_pdf_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PDFOCR)  m_internal = {rename.function_call( 'fz_new_pdfocr_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PS)      m_internal = {rename.function_call( 'fz_new_ps_writer_with_output')}(out2, options);
                            else if (output_type == OutputType_PWG)     m_internal = {rename.function_call( 'fz_new_pwg_writer_with_output')}(out2, options);
                            else
                            {{
                                /* Ensure that out2 is dropped before we return. */
                                {rename.function_call( 'fz_drop_output')}(out2);
                                throw ErrorAbort( "Unrecognised OutputType value");
                            }}
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_cbz_writer_with_output()
                            fz_new_docx_writer_with_output()
                            fz_new_odt_writer_with_output()
                            fz_new_pcl_writer_with_output()
                            fz_new_pclm_writer_with_output()
                            fz_new_pdf_writer_with_output()
                            fz_new_pdfocr_writer_with_output()
                            fz_new_ps_writer_with_output()
                            fz_new_pwg_writer_with_output()

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */
                        '''),
                        ),
                    ExtraConstructor(
                        '(const char *format, const char *path, const char *options, FormatPathType format_path_type)',
                        f'''
                        {{
                            if (0) {{}}
                            else if (format_path_type == FormatPathType_DOCUMENT)   m_internal = {rename.function_call( 'fz_new_document_writer')}(format, path, options);
                            else if (format_path_type == FormatPathType_TEXT)       m_internal = {rename.function_call( 'fz_new_text_writer')}(format, path, options);
                            else throw ErrorAbort( "Unrecognised OutputType value");
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using one of:
                            fz_new_document_writer()
                            fz_new_text_writer()
                        */'''),
                        ),
                    ExtraConstructor(
                        '(Output& out, const char *format, const char *options)',
                        f'''
                        {{
                            /* Need to transfer ownership of <out>. */
                            fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            m_internal = {rename.function_call( 'fz_new_document_writer_with_output')}(out2, format, options);
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using fz_new_document_writer_with_output().

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */'''),
                        ),
                    ExtraConstructor(
                        '(const char *format, Output& out, const char *options)',
                        f'''
                        {{
                            /* Need to transfer ownership of <out>. */
                            fz_output* out2 = out.m_internal;
                            out.m_internal = NULL;
                            m_internal = {rename.function_call( 'fz_new_text_writer_with_output')}(format, out2, options);
                        }}
                        ''',
                        comment = textwrap.dedent('''
                        /* Constructor using fz_new_text_writer_with_output().

                        This constructor takes ownership of <out> -
                        out.m_internal is set to NULL after this constructor
                        returns so <out> must not be used again.
                        */'''),
                        ),
                    ],

                ),

        fz_draw_options = ClassExtra(
                constructors_wrappers = [
                    'fz_parse_draw_options',
                    ],
                copyable=False,
                pod='inline',
                ),

        fz_font = ClassExtra(
                ),

        fz_glyph = ClassExtra(
                class_top =
                    '''
                    enum Bpp
                    {
                        Bpp_1,
                        Bpp_8,
                    };
                    ''',
                ),

        fz_halftone = ClassExtra(
                constructor_raw = 'default',
                ),

        fz_image = ClassExtra(
                accessors=True,
                ),

        fz_irect = ClassExtra(
                constructor_prefixes = [
                    'fz_irect_from_rect',
                    'fz_make_irect',
                    ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_link = ClassExtra(
                accessors = True,
                iterator_next = ('', ''),
                constructor_raw = True,
                copyable = True,
                ),

        fz_location = ClassExtra(
                constructor_prefixes = [
                    'fz_make_location',
                    ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_matrix = ClassExtra(
                constructor_prefixes = [
                    'fz_make_matrix',
                    ],
                method_wrappers_static = [
                    'fz_concat',
                    'fz_scale',
                    'fz_shear',
                    'fz_rotate',
                    'fz_translate',
                    'fz_transform_page',
                    ],
                constructors_extra = [
                    ExtraConstructor( '()',
                        '''
                        : a(1), b(0), c(0), d(1), e(0), f(0)
                        {
                        }
                        ''',
                        comment = '/* Constructs identity matrix (like fz_identity). */'),
                ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_outline = ClassExtra(
                # We add various methods to give depth-first iteration of outlines.
                #
                constructor_prefixes = [
                    'fz_load_outline',
                    ],
                methods_extra = [
                    ExtraMethod( 'OutlineIterator', 'begin()',
                    '''
                    {
                        return OutlineIterator(*this);
                    }
                    ''',
                    ),
                    ExtraMethod( 'OutlineIterator', 'end()',
                    '''
                    {
                        return OutlineIterator();
                    }
                    '''),
                    ],
                class_bottom = 'typedef OutlineIterator iterator;\n',
                class_pre =
                    '''
                    struct OutlineIterator;
                    ''',
                class_post =
                    '''
                    struct OutlineIterator
                    {
                        FZ_FUNCTION OutlineIterator();
                        FZ_FUNCTION OutlineIterator(const Outline& item);
                        FZ_FUNCTION OutlineIterator& operator++();
                        FZ_FUNCTION bool operator==(const OutlineIterator& rhs);
                        FZ_FUNCTION bool operator!=(const OutlineIterator& rhs);
                        FZ_FUNCTION OutlineIterator& operator*();
                        FZ_FUNCTION OutlineIterator* operator->();
                        Outline m_outline;
                        int m_depth;
                        private:
                        std::vector<fz_outline*> m_up;
                    };
                    ''',
                extra_cpp =
                    f'''
                    FZ_FUNCTION OutlineIterator::OutlineIterator(const Outline& item)
                    : m_outline(item), m_depth(0)
                    {{
                    }}
                    FZ_FUNCTION OutlineIterator::OutlineIterator()
                    : m_outline(NULL)
                    {{
                    }}
                    FZ_FUNCTION OutlineIterator& OutlineIterator::operator++()
                    {{
                        if (m_outline.m_internal->down)
                        {{
                            m_up.push_back(m_outline.m_internal);
                            {rename.function_call("fz_keep_outline")}(m_outline.m_internal->down);
                            m_outline = Outline(m_outline.m_internal->down);
                            m_depth += 1;
                        }}
                        else if (m_outline.m_internal->next)
                        {{
                            {rename.function_call("fz_keep_outline")}(m_outline.m_internal->next);
                            m_outline = Outline(m_outline.m_internal->next);
                        }}
                        else
                        {{
                            /* Go up and across in the tree. */
                            for(;;)
                            {{
                                if (m_up.empty())
                                {{
                                    m_outline = Outline(NULL);
                                    assert(m_depth == 0);
                                    break;
                                }}
                                fz_outline* p = m_up.back();
                                m_up.pop_back();
                                m_depth -= 1;
                                if (p->next)
                                {{
                                    {rename.function_call("fz_keep_outline")}(p->next);
                                    m_outline = Outline(p->next);
                                    break;
                                }}
                            }}
                        }}
                        return *this;
                    }}
                    FZ_FUNCTION bool OutlineIterator::operator==(const OutlineIterator& rhs)
                    {{
                        bool ret = m_outline.m_internal == rhs.m_outline.m_internal;
                        return ret;
                    }}
                    FZ_FUNCTION bool OutlineIterator::operator!=(const OutlineIterator& rhs)
                    {{
                        return m_outline.m_internal != rhs.m_outline.m_internal;
                    }}
                    FZ_FUNCTION OutlineIterator& OutlineIterator::operator*()
                    {{
                        return *this;
                    }}
                    FZ_FUNCTION OutlineIterator* OutlineIterator::operator->()
                    {{
                        return this;
                    }}

                    void test(Outline& item)
                    {{
                        for( OutlineIterator it = item.begin(); it != item.end(); ++it)
                        {{
                            (void) *it;
                        }}
                        for (auto i: item)
                        {{
                            (void) i;
                        }}
                    }}

                    ''',
                accessors=True,
                ),

        fz_output = ClassExtra(
                constructor_excludes = [
                    # These all have the same prototype, so are used by
                    # constructors_extra below.
                    'fz_new_asciihex_output',
                    'fz_new_ascii85_output',
                    'fz_new_rle_output',
                    ],
                constructors_extra = [
                    ExtraConstructor( '(Fixed out)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (out == Fixed_STDOUT) {{
                                m_internal = {rename.function_call('fz_stdout')}();
                            }}
                            else if (out == Fixed_STDERR) {{
                                m_internal = {rename.function_call('fz_stderr')}();
                            }}
                            else {{
                                throw ErrorAbort("Unrecognised Fixed value");
                            }}
                        }}
                        '''
                        # Note that it's ok to call fz_drop_output() on fz_stdout and fz_stderr.
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("fz_output")}& chain, Filter filter)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (filter == Filter_HEX) {{
                                m_internal = {rename.function_call('fz_new_asciihex_output')}(chain.m_internal);
                            }}
                            else if (filter == Filter_85) {{
                                m_internal = {rename.function_call('fz_new_ascii85_output')}(chain.m_internal);
                            }}
                            else if (filter == Filter_RLE) {{
                                m_internal = {rename.function_call('fz_new_rle_output')}(chain.m_internal);
                            }}
                            else {{
                                throw ErrorAbort("Unrecognised Filter value");
                            }}
                        }}
                        ''',
                        comment = '/* Calls one of: fz_new_asciihex_output(), fz_new_ascii85_output(), fz_new_rle_output(). */',
                        ),
                    ],
                class_top = '''
                    enum Fixed
                    {
                        Fixed_STDOUT=1,
                        Fixed_STDERR=2,
                    };
                    enum Filter
                    {
                        Filter_HEX,
                        Filter_85,
                        Filter_RLE,
                    };
                    '''
                    ,
                copyable=False, # No fz_keep_output() fn?
                ),

        fz_page = ClassExtra(
                constructor_prefixes = [
                    'fz_load_page',
                    'fz_load_chapter_page',
                    ],
                methods_extra = [
                    ExtraMethod(
                        f'std::vector<{rename.class_("fz_quad")}>',
                        f'search_page(const char* needle, int max)',
                        f'''
                        {{
                            std::vector<{rename.class_("fz_quad")}> ret(max);
                            fz_quad* hit_bbox = ret[0].internal();
                            int n = {rename.function_call('fz_search_page')}(m_internal, needle, hit_bbox, (int) ret.size());
                            ret.resize(n);
                            return ret;
                        }}
                        ''',
                        comment=f'/* Wrapper for fz_search_page(). */',
                        ),
                ],
                constructor_raw = True,
                ),

        fz_pcl_options = ClassExtra(
                constructors_wrappers = [
                    'fz_parse_pcl_options',
                    ],
                copyable=False,
                ),

        fz_pclm_options = ClassExtra(
                constructor_prefixes = [
                    'fz_parse_pclm_options',
                    ],
                copyable=False,
                constructors_extra = [
                    ExtraConstructor( '(const char *args)',
                        f'''
                        {{
                            {rename.function_call('fz_parse_pclm_options')}(m_internal, args);
                        }}
                        '''
                        )
                    ],
                ),

        fz_pixmap = ClassExtra(
                methods_extra = [
                    ExtraMethod( 'std::string', 'md5_pixmap()',
                        f'''
                        {{
                            unsigned char   digest[16];
                            {rename.function_call( 'fz_md5_pixmap')}( m_internal, digest);
                            return std::string( (char*) digest);
                        }}
                        ''',
                        f'/* Wrapper for fz_md5_pixmap(). */',
                        ),
                    ],
                constructor_raw = True,
                accessors = True,
                ),

        fz_point = ClassExtra(
                method_wrappers_static = [
                    'fz_transform_point',
                    'fz_transform_point_xy',
                    'fz_transform_vector',

                    ],
                constructors_extra = [
                    ExtraConstructor( '(float x, float y)',
                        '''
                        : x(x), y(y)
                        {
                        }
                        '''),
                        ],
                methods_extra = [
                    ExtraMethod(
                        f'{rename.class_("fz_point")}&',
                        f'transform(const {rename.class_("fz_matrix")}& m)',
                        '''
                        {
                            double  old_x = x;
                            x = old_x * m.a + y * m.c + m.e;
                            y = old_x * m.b + y * m.d + m.f;
                            return *this;
                        }
                        '''),
                ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_pwg_options = ClassExtra(
                pod=True,
                ),

        fz_quad = ClassExtra(
                constructor_prefixes = [
                    'fz_transform_quad',
                    'fz_quad_from_rect'
                    ],
                constructors_extra = [
                    ExtraConstructor(
                        '()',
                        '''
                        : ul{0,0}, ur{0,0}, ll{0,0}, lr{0,0}
                        {
                        }''',
                        comment = '/* Default constructor. */',
                        ),
                ],
                pod='inline',
                constructor_raw = True,
                ),

        fz_rect = ClassExtra(
                constructor_prefixes = [
                    'fz_transform_rect',
                    'fz_bound_display_list',
                    'fz_rect_from_irect',
                    'fz_rect_from_quad',
                    ],
                method_wrappers_static = [
                    'fz_intersect_rect',
                    'fz_union_rect',
                    ],
                constructors_extra = [
                    ExtraConstructor(
                        '(double x0, double y0, double x1, double y1)',
                        '''
                        :
                        x0(x0),
                        x1(x1),
                        y0(y0),
                        y1(y1)
                        {
                        }
                        '''),
                    ExtraConstructor(
                        f'(const {rename.class_("fz_rect")}& rhs)',
                        '''
                        :
                        x0(rhs.x0),
                        x1(rhs.x1),
                        y0(rhs.y0),
                        y1(rhs.y1)
                        {
                        }
                        '''),
                    ExtraConstructor( '(Fixed fixed)',
                        f'''
                        {{
                            if (0)  {{}}
                            else if (fixed == Fixed_UNIT)       *this->internal() = {rename.function_raw('fz_unit_rect')};
                            else if (fixed == Fixed_EMPTY)      *this->internal() = {rename.function_raw('fz_empty_rect')};
                            else if (fixed == Fixed_INFINITE)   *this->internal() = {rename.function_raw('fz_infinite_rect')};
                            else throw ErrorAbort( "Unrecognised From value");
                        }}
                        '''),
                        ],
                methods_extra = [
                    ExtraMethod(
                        'void',
                        f'transform(const {rename.class_("fz_matrix")}& m)',
                        f'''
                        {{
                            *(fz_rect*) &this->x0 = {rename.function_raw('fz_transform_rect')}(*(fz_rect*) &this->x0, *(fz_matrix*) &m.a);
                        }}
                        '''),
                    ExtraMethod( 'bool', 'contains(double x, double y)',
                        '''
                        {
                            if (is_empty()) {
                                return false;
                            }
                            return true
                                    && x >= x0
                                    && x < x1
                                    && y >= y0
                                    && y < y1
                                    ;
                        }
                        '''),
                    ExtraMethod( 'bool', f'contains({rename.class_("fz_rect")}& rhs)',
                        f'''
                        {{
                            return {rename.function_raw('fz_contains_rect')}(*(fz_rect*) &x0, *(fz_rect*) &rhs.x0);
                        }}
                        '''),
                    ExtraMethod( 'bool', 'is_empty()',
                        f'''
                        {{
                            return {rename.function_raw('fz_is_empty_rect')}(*(fz_rect*) &x0);
                        }}
                        '''),
                    ExtraMethod( 'void', f'union_({rename.class_("fz_rect")}& rhs)',
                        f'''
                        {{
                            *(fz_rect*) &x0 = {rename.function_raw('fz_union_rect')}(*(fz_rect*) &x0, *(fz_rect*) &rhs.x0);
                        }}
                        '''),
                    ],
                pod='inline',
                constructor_raw = True,
                copyable = True,
                class_top = '''
                    enum Fixed
                    {
                        Fixed_UNIT,
                        Fixed_EMPTY,
                        Fixed_INFINITE,
                    };
                    ''',
                ),

        fz_separations = ClassExtra(
                constructor_raw = True,
                opaque = True,
                ),

        fz_shade = ClassExtra(
                methods_extra = [
                    ExtraMethod( 'void',
                        'paint_shade_no_cache(const Colorspace& override_cs, Matrix& ctm, const Pixmap& dest, ColorParams& color_params, Irect& bbox, const Overprint& eop)',
                        '''
                        {
                            return mupdf::paint_shade(
                                    this->m_internal,
                                    override_cs.m_internal,
                                    *(fz_matrix*) &ctm.a,
                                    dest.m_internal,
                                    *(fz_color_params*) &color_params.ri,
                                    *(fz_irect*) &bbox.x0,
                                    eop.m_internal,
                                    NULL /*cache*/
                                    );
                        }
                        ''',
                        comment = f'/* Extra wrapper for fz_paint_shade(), passing cache=NULL. */',
                        ),
                ],
                ),

        fz_shade_color_cache = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                        '''
                        : m_internal( NULL)
                        {
                        }
                        ''',
                        comment = f'/* Constructor that sets m_internal to NULL; can then be passed to {rename.class_("fz_shade")}::{rename.method("fz_shade_color_cache", "fz_paint_shade")}(). */',
                        ),
                    ],
                ),

        # Our wrappers of the fz_stext_* structs all have a default copy
        # constructor - there are no fz_keep_stext_*() functions.
        #
        # We define explicit accessors for fz_stext_block::u.i.* because SWIG
        # does not handle nested unions.
        #
        fz_stext_block = ClassExtra(
                iterator_next = ('u.t.first_line', 'u.t.last_line'),
                copyable='default',
                methods_extra = [
                    ExtraMethod( f'{rename.class_("fz_matrix")}', 'i_transform()',
                        f'''
                        {{
                            if (m_internal->type != FZ_STEXT_BLOCK_IMAGE) {{
                                throw std::runtime_error("Not an image");
                            }}
                            return m_internal->u.i.transform;
                        }}
                        ''',
                        comment=f'/* Returns m_internal.u.i.transform if m_internal->type is FZ_STEXT_BLOCK_IMAGE, else throws. */',
                        ),
                    ExtraMethod( f'{rename.class_("fz_image")}', 'i_image()',
                        f'''
                        {{
                            if (m_internal->type != FZ_STEXT_BLOCK_IMAGE) {{
                                throw std::runtime_error("Not an image");
                            }}
                            return keep_image(m_internal->u.i.image);
                        }}
                        ''',
                        comment=f'/* Returns m_internal.u.i.image if m_internal->type is FZ_STEXT_BLOCK_IMAGE, else throws. */',
                        ),
                        ],
                ),

        fz_stext_char = ClassExtra(
                copyable='default',
                ),

        fz_stext_line = ClassExtra(
                iterator_next = ('first_char', 'last_char'),
                copyable='default',
                constructor_raw=True,
                ),

        fz_stext_options = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                        '''
                        : flags( 0)
                        {
                        }
                        '''),
                    ExtraConstructor( '(int flags)',
                        '''
                        : flags( flags)
                        {
                        }
                        '''),
                    ],
                pod='inline',
                ),

        fz_stext_page = ClassExtra(
                methods_extra = [
                    ExtraMethod( 'std::string', 'copy_selection(Point& a, Point& b, int crlf)',
                        f'''
                        {{
                            char* text = {rename.function_call('fz_copy_selection')}(m_internal, *(fz_point *) &a.x, *(fz_point *) &b.x, crlf);
                            std::string ret(text);
                            {rename.function_call('fz_free')}(text);
                            return ret;
                        }}
                        ''',
                        comment = f'/* Wrapper for fz_copy_selection(). */',
                        ),
                    ExtraMethod( 'std::string', 'copy_rectangle(Rect& area, int crlf)',
                        f'''
                        {{
                            char* text = {rename.function_call('fz_copy_rectangle')}(m_internal, *(fz_rect*) &area.x0, crlf);
                            std::string ret(text);
                            {rename.function_call('fz_free')}(text);
                            return ret;
                        }}
                        ''',
                        comment = f'/* Wrapper for fz_copy_rectangle(). */',
                        ),
                    ],
                iterator_next = ('first_block', 'last_block'),
                copyable=False,
                constructor_raw = True,
                ),

        fz_text_span = ClassExtra(
                copyable=False,
                ),

        fz_stream = ClassExtra(
                constructor_prefixes = [
                    'fz_open_file',
                    'fz_open_memory',
                    ],
                constructors_extra = [
                    ExtraConstructor( '(const std::string& filename)',
                    f'''
                    : m_internal({rename.function_call('fz_open_file')}(filename.c_str()))
                    {{
                    }}
                    '''
                    )
                    ],
                ),

        fz_transition = ClassExtra(
                pod='inline',
                constructor_raw = True,
                ),

        pdf_document = ClassExtra(
                constructor_prefixes = [
                    'pdf_open_document',
                    'pdf_create_document',
                    ],
                ),

        pdf_write_options = ClassExtra(
                constructors_extra = [
                    ExtraConstructor( '()',
                        f'''
                        {{
                            /* Use memcpy() otherwise we get 'invalid array assignment' errors. */
                            memcpy(this->internal(), &pdf_default_write_options, sizeof(*this->internal()));
                        }}
                        ''',
                        comment = '/* Default constructor, makes copy of pdf_default_write_options. */'
                        ),
                    ExtraConstructor(
                        f'(const {rename.class_("pdf_write_options")}& rhs)',
                        f'''
                        {{
                            /* Use memcpy() otherwise we get 'invalid array assignment' errors. */
                            *this = rhs;
                        }}
                        ''',
                        ),
                    ],
                    methods_extra = [
                        ExtraMethod(
                            f'{rename.class_("pdf_write_options")}&',
                            f'operator=(const {rename.class_("pdf_write_options")}& rhs)',
                            f'''
                            {{
                                memcpy(this->internal(), rhs.internal(), sizeof(*this->internal()));
                                return *this;
                            }}
                            ''',
                            ),
                    ],
                pod = 'inline',
                copyable = 'default',
                )
        )

def get_fz_extras( fzname):
    '''
    Finds ClassExtra for <fzname>, coping if <fzname> starts with 'const ' or
    'struct '.
    '''
    fzname = clip( fzname, 'const ')
    fzname = clip( fzname, 'struct ')
    ce = classextras.get( fzname)
    return ce

def get_field0( type_):
    '''
    Returns cursor for first field in <type_> or None if <type_> has no fields.
    '''
    assert isinstance( type_, clang.cindex.Type)
    type_ = type_.get_canonical()
    for field in type_.get_fields():
        return field

get_base_type_cache = dict()
def get_base_type( type_):
    '''
    Follows pointer to get ultimate type.
    '''
    # Caching reduces time from to 0.24s to 0.1s.
    key = type_.spelling
    ret = get_base_type_cache.get( key)
    if ret is None:
        while 1:
            type_ = type_.get_canonical()
            if type_.kind != clang.cindex.TypeKind.POINTER:
                break
            type_ = type_.get_pointee()
        ret = type_
        get_base_type_cache[ key] = ret

    return ret

def get_base_typename( type_):
    '''
    Follows pointer to get ultimate type, and returns its name, with any
    leading 'struct ' or 'const ' removed.
    '''
    type_ = get_base_type( type_)
    ret = type_.spelling
    ret = clip( ret, 'const ')
    ret = clip( ret, 'struct ')
    return ret

def is_double_pointer( type_):
    '''
    Returns true if <type_> is double pointer.
    '''
    type_ = type_.get_canonical()
    if type_.kind == clang.cindex.TypeKind.POINTER:
        type_ = type_.get_pointee().get_canonical()
        if type_.kind == clang.cindex.TypeKind.POINTER:
            return True


def write_call_arg(
        arg,
        classname,
        have_used_this,
        out_cpp,
        verbose=False,
        python=False,
        ):
    '''
    Write an arg of a function call, translating between raw and wrapping
    classes as appropriate.

    If the required type is a fz_ struct that we wrap, we assume that the
    <name> is a reference to an instance of the wrapping class. If the wrapping
    class is the same as <classname>, we use 'this->' instead of <name>. We
    also generate slightly different code depending on whether the wrapping
    class is pod or inline pod.

    (cursor, name, separator, alt) should be as if from get_args().

    arg:
        Arg from get_args().
    classname:
        Name of wrapping class available as 'this'.
    have_used_this:
        If true, we never use 'this->...'.
    out_cpp:
        .
    python:
        If true, we write python code, not C.

    Returns True if we have used 'this->...', else return <have_used_this>.
    '''
    assert isinstance( arg, Arg)
    assert isinstance( arg.cursor, clang.cindex.Cursor)
    if not arg.alt:
        # Arg is a normal type; no conversion necessary.
        if python:
            out_cpp.write( arg.name_python)
        else:
            out_cpp.write( arg.name)
        return have_used_this

    if verbose:
        log( '{cursor.spelling=} {arg.name=} {arg.alt.spelling=} {classname=}')
    type_ = arg.cursor.type.get_canonical()
    ptr = '*'
    if type_.kind == clang.cindex.TypeKind.POINTER:
        type_ = type_.get_pointee().get_canonical()
        ptr = ''
    extras = get_fz_extras( type_.spelling)
    assert extras, f'No extras for type_.spelling={type_.spelling}'
    if verbose:
        log( 'param is fz: {type_.spelling=} {extras2.pod=}')
    if python:
        if extras.pod == 'inline':
            out_cpp.write( f'{arg.name_python}.internal()')
        elif extras.pod:
            out_cpp.write( f'{arg.name_python}.m_internal')
        else:
            out_cpp.write( f'{arg.name_python}')

    elif extras.pod == 'inline':
        # We use the address of the first class member, casting it to a pointer
        # to the wrapped type. Not sure this is guaranteed safe, but should
        # work in practise.
        name_ = f'{arg.name}.'
        if not have_used_this and rename.class_(arg.alt.type.spelling) == classname:
            have_used_this = True
            name_ = 'this->'
        field0 = get_field0(type_).spelling
        out_cpp.write( f'{ptr}({arg.cursor.type.spelling}{ptr}) &{name_}{field0}')
    else:
        if verbose:
            log( '{arg.cursor=} {arg.name=} {classname=} {extras2.pod=}')
        if extras.pod and arg.cursor.type.get_canonical().kind == clang.cindex.TypeKind.POINTER:
            out_cpp.write( '&')
        elif arg.out_param:
            out_cpp.write( '&')
        if not have_used_this and rename.class_(arg.alt.type.spelling) == classname:
            have_used_this = True
            out_cpp.write( 'this->')
        else:
            out_cpp.write( f'{arg.name}.')
        out_cpp.write( 'm_internal')

    return have_used_this


omit_fns = [
        'fz_open_file_w',
        'fz_set_stderr',
        'fz_set_stdout',
        'fz_colorspace_name_process_colorants', # Not implemented in mupdf.so?
        'fz_clone_context_internal',            # Not implemented in mupdf?
        'fz_arc4_final',
        'fz_assert_lock_held',      # Is a macro if NDEBUG defined.
        'fz_assert_lock_not_held',  # Is a macro if NDEBUG defined.
        'fz_lock_debug_lock',       # Is a macro if NDEBUG defined.
        'fz_lock_debug_unlock',     # Is a macro if NDEBUG defined.
        ]

omit_methods = [
        'fz_encode_character_with_fallback',    # Has 'fz_font **out_font' arg.
        'fz_new_draw_device_with_options',      # Has 'fz_pixmap **pixmap' arg.
        ]

# Be able to exclude some structs from being wrapped.
omit_class_names0 = []
omit_class_names = omit_class_names0[:]

def omit_class( fzname):
    '''
    Returns true if we ommit <fzname> *and* we haven't been called for <fzname>
    before.
    '''
    try:
        omit_class_names.remove( fzname)
    except Exception:
        return False
    return True

def get_value( item, name):
    '''
    Enhanced wrapper for getattr().

    We call ourselves recursively if name contains one or more '.'. If name
    ends with (), makes fn call to get value.
    '''
    if not name:
        return item
    dot = name.find( '.')
    if dot >= 0:
        item_sub = get_value( item, name[:dot])
        return get_value( item_sub, name[dot+1:])
    if name.endswith('()'):
        value = getattr( item, name[:-2])
        assert callable(value)
        return value()
    return getattr( item, name)

def get_list( item, *names):
    '''
    Uses get_value() to find values of specified fields in <item>.

    Returns list of (name,value) pairs.
    '''
    ret = []
    for name in names:
        value = get_value( item, name)
        ret.append((name, value))
    return ret

def get_text( item, prefix, sep, *names):
    '''
    Returns text describing <names> elements of <item>.
    '''
    ret = []
    for name, value in get_list( item, *names):
        ret.append( f'{name}={value}')
    return prefix + sep.join( ret)

class Clang6FnArgsBug( Exception):
    def __init__( self, text):
        Exception.__init__( self, f'clang-6 unable to walk args for fn type. {text}')

def declaration_text( type_, name, nest=0, name_is_simple=True, arg_names=False, verbose=False):
    '''
    Returns text for C++ declaration of <type_> called <name>.

    type:
        a clang.cindex.Type.
    name:
        name of type; can be empty.
    nest:
        for internal diagnostics.
    name_is_simple:
        true iff <name> is an identifier.

    If name_is_simple is false, we surround <name> with (...) if type is a
    function.
    '''
    if verbose:
        log( '{nest=} {arg_names=} {name=} {type_.spelling=} {type_.get_declaration().get_usr()=}')
        log( '{type_.kind=} {type_.get_array_size()=}')
    def log2( text):
        jlib.log( nest*'    ' + text, 2)

    array_n = type_.get_array_size()
    if verbose:
        log( '{array_n=}')
    if array_n >= 0 or type_.kind == clang.cindex.TypeKind.INCOMPLETEARRAY:
        # Not sure this is correct.
        if verbose: log( '{array_n=}')
        text = declaration_text( type_.get_array_element_type(), name, nest+1, name_is_simple, arg_names, verbose=verbose)
        if array_n < 0:
            array_n = ''
        text += f'[{array_n}]'
        return text

    pointee = type_.get_pointee()
    if pointee and pointee.spelling:
        if verbose: log( '{pointee.spelling=}')
        return declaration_text( pointee, f'*{name}', nest+1, name_is_simple=False, arg_names=arg_names, verbose=verbose)

    if type_.get_typedef_name():
        if verbose: log( '{type_.get_typedef_name()=}')
        const = 'const ' if type_.is_const_qualified() else ''
        return f'{const}{type_.get_typedef_name()} {name}'

    if type_.get_result().spelling:
        log1( 'function: {type_.spelling=} {type_.kind=} {type_.get_result().spelling=} {type_.get_declaration().spelling=}')
        # <type> is a function. We call ourselves with type=type_.get_result()
        # and name=<name>(<args>).
        #
        if 0 and verbose:
            nc = 0
            for nci in type_.get_declaration().get_arguments():
                nc += 1
            nt = 0
            for nti in type_.argument_types():
                nt += 1
            if nt == nc:
                log( '*** {nt=} == {nc=}')
            if nt != nc:
                log( '*** {nt=} != {nc=}')

        ret = ''
        i = 0

        #for arg_cursor in type_.get_declaration().get_arguments():
        #    arg = arg_cursor
        try:
            args = type_.argument_types()
        except Exception as e:
            if 'libclang-6' in clang_info().libclang_so:
                raise Clang6FnArgsBug( f'type_.spelling is {type_.spelling}: {e!r}')

        for arg in args:
            if i:
                ret += ', '
            #name2 = arg.spelling if arg_names else ''
            #if verbose:
            #    log( '{name2=}')
            #ret += declaration_text( arg, name2, nest+1, arg_names=arg_names)
            ret += declaration_text( arg, '', nest+1, arg_names=arg_names)
            i += 1
        if verbose: log( '{ret!r=}')
        if not name_is_simple:
            # If name isn't a simple identifier, put it inside braces, e.g.
            # this crudely allows function pointers to work.
            name = f'({name})'
        ret = f'{name}({ret})'
        if verbose: log( '{type_.get_result()=}')
        ret = declaration_text( type_.get_result(), ret, nest+1, name_is_simple=False, arg_names=arg_names, verbose=verbose)
        if verbose:
            log( 'returning {ret=}')
        return ret

    ret = f'{type_.spelling} {name}'
    if verbose: log( 'returning {ret=}')
    return ret


def dump_ast( cursor, depth=0):
    indent = depth*4*' '
    for cursor2 in cursor.get_children():
        jlib.log( indent * ' ' + '{cursor2.kind=} {cursor2.mangled_name=} {cursor2.displayname=} {cursor2.spelling=}')
        dump_ast( cursor2, depth+1)


def show_ast( filename):
    index = clang.cindex.Index.create()
    tu = index.parse( filename,
            args=( '-I', clang_info().include_path),
            )
    dump_ast( tu.cursor)

class Arg:
    '''
        .cursor:
            Cursor for the argument.
        .name:
            Arg name, or an invented name if none was present.
        .separator:
            '' for first returned argument, ', ' for the rest.
        .alt:
            Cursor for underlying fz_ struct type if <arg> is a pointer to or
            ref/value of a fz_ struct type that we wrap. Else None.
        .out_param:
            True if this looks like an out-parameter, e.g. alt is set and
            double pointer, or arg is pointer other than to char.
        .name_python:
            Same as .name or .name+'_' if .name is a Python keyword.
    '''
    def __init__(self, cursor, name, separator, alt, out_param):
        self.cursor = cursor
        self.name = name
        self.separator = separator
        self.alt = alt
        self.out_param = out_param
        if name in ('in', 'is'):
            self.name_python = f'{name}_'
        else:
            self.name_python = name

    def __str__(self):
        return f'Arg(cursor={self.cursor} name={self.name} alt={self.alt} out_param={self.out_param})'

get_args_cache = dict()

def get_args( tu, cursor, include_fz_context=False, verbose=False):
    '''
    Yields Arg instance for each arg of the function at <cursor>.

    Args:
        tu:
            A clang.cindex.TranslationUnit instance.
        cursor:
            Clang cursor for the function.
        include_fz_context:
            If false, we skip args that are 'struct fz_context*'
        verbose:
            .
    '''
    # We are called a few times for each function, and the calculations we do
    # are slow, so we cache the returned items. E.g. this reduces total time of
    # --build 0 from 3.5s to 2.1s.
    #
    key = tu, cursor.location.file, cursor.location.line, include_fz_context
    ret = get_args_cache.get( key)

    if ret is None:
        ret = []
        i = 0
        separator = ''
        for arg_cursor in cursor.get_arguments():
            assert arg_cursor.kind == clang.cindex.CursorKind.PARM_DECL
            if not include_fz_context and is_pointer_to( arg_cursor.type, 'fz_context'):
                # Omit this arg because our generated mupdf_*() wrapping functions
                # use internalContextGet() to get a context.
                continue
            name = arg_cursor.mangled_name or f'arg_{i}'
            if 0 and name == 'stmofsp':
                verbose = True
            alt = None
            out_param = False
            # Set <alt> to wrapping class if possible.
            base_type = get_base_type( arg_cursor.type)
            base_type_cursor = base_type.get_declaration()
            base_typename = get_base_typename( base_type)
            extras = classextras.get( base_typename)
            if verbose:
                log( '{extras=}')
            if extras:
                if verbose:
                    log( '{extras.opaque=} {base_type_cursor.kind=} {base_type_cursor.is_definition()=}')
                if extras.opaque:
                    # E.g. we don't have access to defintion of fz_separation,
                    # but it is marked in classextras with opaque=true, so
                    # there will be a wrapper class.
                    alt = base_type_cursor
                elif (1
                        and base_type_cursor.kind == clang.cindex.CursorKind.STRUCT_DECL
                        #and base_type_cursor.is_definition()
                        ):
                    alt = base_type_cursor
            if verbose:
                log( '{arg_cursor.type.spelling=} {base_type.spelling=} {arg_cursor.type.kind=}')
            if alt:
                if is_double_pointer( arg_cursor.type):
                    out_param = True
            elif get_base_typename( arg_cursor.type) in ('char', 'unsigned char', 'signed char', 'void', 'FILE'):
                if is_double_pointer( arg_cursor.type):
                    #log( 'setting outparam: {cursor.spelling=} {arg_cursor.type=}')
                    if cursor.spelling == 'pdf_clean_file':
                        # Don't mark char** argv as out-param, which will also
                        # allow us to tell swig to convert python lists into
                        # (argc,char**) pair.
                        pass
                    else:
                        out_param = True
            elif base_typename.startswith( ('fz_', 'pdf_')):
                # Pointer to fz_ struct is not usually an out-param.
                if verbose: log( 'not out-param because arg is: {arg_cursor.displayname=} {base_type.spelling=} {extras}')
            elif arg_cursor.type.kind == clang.cindex.TypeKind.POINTER:
                if verbose:
                    log( 'clang.cindex.TypeKind.POINTER')
                if arg_cursor.type.get_pointee().get_canonical().kind == clang.cindex.TypeKind.FUNCTIONPROTO:
                    # Don't mark function-pointer args as out-params.
                    if verbose:
                        log( 'clang.cindex.TypeKind.FUNCTIONPROTO')
                    pass
                elif arg_cursor.type.get_pointee().is_const_qualified():
                    if verbose:
                        log( 'is_const_qualified()')
                    pass
                else:
                    if verbose:
                        log( 'setting out_param = True')
                    out_param = True
            if verbose:
                log( '*** returning {(arg_cursor.displayname, name, separator, alt, out_param)}')
            ret.append( Arg(arg_cursor, name, separator, alt, out_param))
            i += 1
            separator = ', '

        get_args_cache[ key] = ret

    for arg in ret:
        yield arg


def fn_has_struct_args( tu, cursor):
    '''
    Returns true if fn at <cursor> takes any fz_* struct args.
    '''
    for arg in get_args( tu, cursor):
        if arg.alt:
            if arg.alt.spelling in omit_class_names0:
                pass
                #log( '*** omitting {alt.spelling=}')
            else:
                return True

def get_first_arg( tu, cursor):
    '''
    Returns (arg, n), where <arg> is from get_args() for first argument (or
    None if no arguments), and <n> is number of arguments.
    '''
    n = 0
    ret = None
    for arg in get_args( tu, cursor):
        if n == 0:
            ret = arg
        n += 1
    return ret, n


is_pointer_to_cache = dict()

def is_pointer_to( type_, destination, verbose=False):
    '''
    Returns true if <type> is a pointer to <destination>.

    We do this using text for <destination>, rather than a clang.cindex.Type
    or clang.cindex.Cursor, so that we can represent base types such as int or
    char without having clang parse system headers. This involves stripping any
    initial 'struct ' text.

    Also, clang's representation of mupdf's varying use of typedef, struct and
    forward-declarations is rather difficult to work with directly.

    type_:
        A clang.cindex.Type.
    destination:
        Text typename.
    '''
    # Use cache - reduces time from 0.6s to 0.2.
    #
    key = type_.spelling, destination
    ret = is_pointer_to_cache.get( key)
    if ret is None:
        assert isinstance( type_, clang.cindex.Type)
        ret = None
        destination = clip( destination, 'struct ')
        if type_.kind == clang.cindex.TypeKind.POINTER:
            pointee = type_.get_pointee().get_canonical()

            if verbose:
                jlib.log( "{declaration_text( pointee, '')!r=} {destination + ' '!r=}")
            d = declaration_text( pointee, '')
            d = clip( d, 'const ')
            d = clip( d, 'struct ')
            ret = d == f'{destination} ' or d == f'const {destination} '
        is_pointer_to_cache[ key] = ret

    return ret


def make_fncall( tu, cursor, return_type, fncall, out):
    '''
    Writes a function call to <out>, using fz_context_s from
    internal_context_get() and with fz_try...fz_catch that converts to C++
    exceptions by calling throw_exception().

    return_type:
        Text return type of function, e.g. 'void' or 'double'.
    fncall:
        Text containing function call, e.g. 'function(a, b, 34)'.
    out:
        Stream to which we write generated code.
    '''
    icg = rename.internal( 'context_get')
    te = rename.internal( 'throw_exception')
    out.write(      f'    fz_context* auto_ctx = {icg}();\n')
    out.write(      f'    fz_var(auto_ctx);\n')

    out.write( '    if (s_trace) {\n')
    out.write( f'        std::cerr << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ": calling {cursor.mangled_name}():"')
    for arg in get_args( tu, cursor, include_fz_context=True):
        if is_pointer_to( arg.cursor.type, 'fz_context'):
            out.write( f' << " auto_ctx=" << auto_ctx')
        else:
            use_address = False
            if arg.cursor.type.kind == clang.cindex.TypeKind.POINTER:
                pass
            elif arg.alt:
                # If not a pod, there will not be an operator<<, so just show
                # the address of this arg.
                #
                extras = get_fz_extras(arg.alt.type.spelling)
                if not extras.pod:
                    use_address = True
            if use_address:
                out.write( f' << " &{arg.name}=" << &{arg.name}')
            else:
                out.write( f' << " {arg.name}=" << {arg.name}')
    out.write( f' << "\\n";\n')
    out.write( '    }\n')

    if return_type != 'void':
        out.write(  f'    {return_type} ret;\n')
        out.write(  f'    fz_var(ret);\n')
    out.write(      f'    fz_try(auto_ctx) {{\n')
    if return_type == 'void':
        out.write(  f'        {fncall};\n')
    else:
        out.write(  f'        ret = {fncall};\n')
    out.write(      f'    }}\n')
    out.write(      f'    fz_catch(auto_ctx) {{\n')
    out.write(      f'        {te}(auto_ctx);\n')
    out.write(      f'    }}\n')
    if return_type != 'void':
        out.write(  f'    return ret;\n')



def make_python_outparam_helpers( tu, cursor, fnname, out_h, out_cpp, out_swig_c, out_swig_python):
    '''
    Create extra C++ and Python code to make tuple-returning wrapper of
    specified function.

    We write to out_swig_c and out_swig_python.
    '''
    verbose = False
    main_name = rename.function(cursor.mangled_name)
    out_swig_c.write( '\n')

    # Write struct.
    out_swig_c.write(f'/* Helper for out-params of {cursor.mangled_name}(). */\n')
    out_swig_c.write(f'typedef struct\n')
    out_swig_c.write( '{\n')
    for arg in get_args( tu, cursor):
        if not arg.out_param:
            continue
        decl = declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        if verbose:
            log( '{decl=}')
        assert arg.cursor.type.kind == clang.cindex.TypeKind.POINTER
        pointee = arg.cursor.type.get_pointee() #.get_canonical()
        out_swig_c.write(f'    {declaration_text( pointee, arg.name)};\n')
    out_swig_c.write(f'}} {main_name}_outparams;\n')
    out_swig_c.write('\n')

    # Write C wrapper fn.

    # decl.
    name_args = f'{main_name}_outparams_fn('
    sep = ''
    for arg in get_args( tu, cursor):
        if arg.out_param:
            continue
        name_args += sep
        name_args += declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        sep = ', '
    name_args += f'{sep}{main_name}_outparams* outparams'
    name_args += ')'
    out_swig_c.write(declaration_text( cursor.result_type, name_args))
    out_swig_c.write('\n')

    # body.
    out_swig_c.write('{\n')
    # Set all pointer fields to NULL.
    for arg in get_args( tu, cursor):
        if not arg.out_param:
            continue
        if arg.cursor.type.get_pointee().kind == clang.cindex.TypeKind.POINTER:
            out_swig_c.write(f'    outparams->{arg.name} = NULL;\n')
    # Make call.
    out_swig_c.write(f'    return {rename.function_call(cursor.mangled_name)}(')
    sep = ''
    for arg in get_args( tu, cursor):
        out_swig_c.write(sep)
        if arg.out_param:
            out_swig_c.write(f'&outparams->{arg.name}')
        else:
            out_swig_c.write(f'{arg.name}')
        sep = ', '
    out_swig_c.write(');\n')
    out_swig_c.write('}\n')
    out_swig_c.write('\n')

    # Write python wrapper.
    out_swig_python.write('')
    out_swig_python.write(f'def {main_name}(')
    sep = ''
    for arg in get_args( tu, cursor):
        if arg.out_param:
            continue
        out_swig_python.write(f'{sep}{arg.name_python}')
        sep = ', '
    out_swig_python.write('):\n')
    out_swig_python.write(f'    """\n')
    out_swig_python.write(f'    Wrapper for out-params of {cursor.mangled_name}().\n')
    sep = ''
    out_swig_python.write(f'    Returns: ')
    return_void = cursor.result_type.spelling == 'void'
    sep = ''
    if not return_void:
        out_swig_python.write( f'{cursor.result_type.spelling}')
        sep = ', '
    for arg in get_args( tu, cursor):
        if arg.out_param:
            out_swig_python.write(f'{sep}{declaration_text(arg.cursor.type.get_pointee(), arg.name_python)}')
            sep = ', '
    out_swig_python.write(f'\n')
    out_swig_python.write(f'    """\n')
    out_swig_python.write(f'    outparams = {main_name}_outparams()\n')
    out_swig_python.write(f'    ret = {main_name}_outparams_fn(')
    sep = ''
    for arg in get_args( tu, cursor):
        if arg.out_param:
            continue
        out_swig_python.write(f'{sep}{arg.name_python}')
        sep = ', '
    out_swig_python.write(f'{sep}outparams)\n')
    out_swig_python.write(f'    return ')
    sep = ''
    if not return_void:
        out_swig_python.write(f'ret')
        sep = ', '
    for arg in get_args( tu, cursor):
        if arg.out_param:
            out_swig_python.write(f'{sep}outparams.{arg.name_python}')
            sep = ', '
    out_swig_python.write('\n')
    out_swig_python.write('\n')


def make_python_class_method_outparam_override(
        tu,
        cursor,
        fnname,
        out,
        structname,
        classname,
        return_type,
        ):
    main_name = rename.function(cursor.mangled_name)
    out.write( f'def {classname}_{main_name}_outparams_fn( self')
    for arg in get_args( tu, cursor):
        if arg.out_param:
            continue
        if is_pointer_to( arg.cursor.type, structname):
            continue
        out.write(f', {arg.name_python}')
    out.write('):\n')
    out.write( '    """\n')
    out.write(f'    Helper for out-params of {structname}::{main_name}() [{cursor.mangled_name}()].\n')
    out.write( '    """\n')

    # ret, a, b, ...
    out.write(f'    ')
    sep = ''
    if cursor.result_type.spelling != 'void':
        out.write( 'ret')
        sep = ', '
    for arg in get_args( tu, cursor):
        if not arg.out_param:
            continue
        out.write( f'{sep}{arg.name_python}')
        sep = ', '
    # = foo::bar(self.m_internal, p, q, r, ...)
    out.write( f' = {main_name}( self.m_internal')
    for arg in get_args( tu, cursor):
        if arg.out_param:
            continue
        if is_pointer_to( arg.cursor.type, structname):
            continue
        out.write( ', ')
        write_call_arg(arg, classname, False, out, python=True)
    out.write( ')\n')

    # return ret, a, b
    out.write( '    return ')
    sep = ''
    if cursor.result_type.spelling != 'void':
        if return_type:
            out.write( f'{return_type}(ret)')
        else:
            out.write( f'ret')
        sep = ', '
    for arg in get_args( tu, cursor):
        if not arg.out_param:
            continue
        if arg.alt:
            out.write( f'{sep}{rename.class_(arg.alt.type.spelling)}({arg.name_python})')
        else:
            out.write(f'{sep}{arg.name_python}')
        sep = ', '
    out.write('\n')
    out.write('\n')

    # foo.bar = foo_bar_outparams_fn
    out.write(f'{classname}.{rename.method(structname, cursor.mangled_name)} = {classname}_{main_name}_outparams_fn\n')
    out.write('\n')
    out.write('\n')


def make_function_wrapper( tu, cursor, fnname, out_h, out_cpp, out_swig_c, out_swig_python):
    '''
    Writes simple C++ wrapper fn, converting any fz_try..fz_catch exception
    into a C++ exception and getting internal context.

    cursor:
        Clang cursor for function to wrap.
    fnname:
        Name of function to create.
    out_h:
        Stream to which we write header output.
    out_cpp:
        Stream to which we write cpp output.
    out_swig_c:
        Stream to which we write extra C++ code for SWIG .i file.
    out_swig_python:
        Stream to which we write extra Python code for SWIG .i file.

    Example generated function:

        fz_band_writer * mupdf_new_band_writer_of_size(fz_context *ctx, size_t size, fz_output *out)
        {
            fz_band_writer * ret;
            fz_var(ret);
            fz_try(ctx) {
                ret = fz_new_band_writer_of_size(ctx, size, out);
            }
            fz_catch(ctx) {
                mupdf_throw_exception(ctx);
            }
            return ret;
        }
    '''
    assert cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL

    verbose = fnname == 'pdf_add_annot_ink_list'

    # Write first line: <result_type> <fnname> (<args>...)
    #
    for out in out_h, out_cpp:
        out.write( f'/* Wrapper for {cursor.mangled_name}(). */\n')

    # Copy any comment into .h file before declaration.
    if cursor.raw_comment:
        out_h.write( f'{cursor.raw_comment}')
        if not cursor.raw_comment.endswith( '\n'):
            out_h.write( '\n')

    name_args_h = f'{fnname}('
    name_args_cpp = f'{fnname}('
    comma = ''
    num_out_params = 0
    for arg in get_args( tu, cursor, include_fz_context=True):
        if verbose:
            log( '{arg.cursor=} {arg.name=} {arg.separator=} {arg.alt=} {arg.out_param=}')
        if is_pointer_to(arg.cursor.type, 'fz_context'):
            continue
        if arg.out_param:
            num_out_params += 1
        decl = declaration_text( arg.cursor.type, arg.name, verbose=verbose)
        if verbose:
            log( '{decl=}')
        name_args_h += f'{comma}{decl}'
        decl = declaration_text( arg.cursor.type, arg.name)
        name_args_cpp += f'{comma}{decl}'
        comma = ', '

    name_args_h += ')'
    name_args_cpp += ')'
    declaration_h = declaration_text( cursor.result_type, name_args_h)
    declaration_cpp = declaration_text( cursor.result_type, name_args_cpp)
    out_h.write( f'FZ_FUNCTION {declaration_h};\n')
    out_h.write( '\n')

    # Write function definition.
    #
    out_cpp.write( f'FZ_FUNCTION {declaration_cpp}\n')
    out_cpp.write( '{\n')
    return_type = cursor.result_type.spelling
    fncall = ''
    fncall += f'{rename.function_raw(cursor.mangled_name)}('
    for arg in get_args( tu, cursor, include_fz_context=True):
        if is_pointer_to( arg.cursor.type, 'fz_context'):
            fncall += f'{arg.separator}auto_ctx'
        else:
            fncall += f'{arg.separator}{arg.name}'
    fncall += ')'
    make_fncall( tu, cursor, return_type, fncall, out_cpp)
    out_cpp.write( '}\n')
    out_cpp.write( '\n')

    if num_out_params:
        make_python_outparam_helpers(
                tu,
                cursor,
                fnname,
                out_h,
                out_cpp,
                out_swig_c,
                out_swig_python,
                )


def make_namespace_open( namespace, out):
    if namespace:
        out.write( '\n')
        out.write( f'namespace {namespace}\n')
        out.write( '{\n')


def make_namespace_close( namespace, out):
    if namespace:
        out.write( '\n')
        out.write( f'}} /* End of namespace {namespace}. */\n')



def make_internal_functions( namespace, out_h, out_cpp):
    '''
    Writes internal support functions.

    out_h:
        Stream to which we write header output.
    out_cpp:
        Stream to which we write cpp output.
    '''
    out_h.write(
            textwrap.dedent(
            f'''
            /* Internal use only. Returns fz_context* for use by current thread. */
            fz_context* {rename.internal('context_get')}();


            '''
            ))

    out_cpp.write(
            textwrap.dedent(
            '''
            #include "mupdf/exceptions.h"
            #include "mupdf/internal.h"

            #include <thread>
            #include <mutex>


            '''))

    make_namespace_open( namespace, out_cpp)

    state_t = rename.internal( 'state')
    thread_state_t = rename.internal( 'thread_state')

    cpp = textwrap.dedent(
            '''
            struct state_t
            {
                state_t()
                {
                    m_locks.user = this;
                    m_locks.lock = lock;
                    m_locks.unlock = unlock;
                    m_ctx = fz_new_context(NULL /*alloc*/, &m_locks, FZ_STORE_DEFAULT);
                    fz_register_document_handlers(m_ctx);
                }
                static void lock(void *user, int lock)
                {
                    state_t*    self = (state_t*) user;
                    self->m_mutexes[lock].lock();
                }
                static void unlock(void *user, int lock)
                {
                    state_t*    self = (state_t*) user;
                    self->m_mutexes[lock].unlock();
                }
                ~state_t()
                {
                    fz_drop_context(m_ctx);
                }

                fz_context*         m_ctx;
                std::mutex          m_mutex;    /* Serialise access to m_ctx. */

                /* Provide thread support to mupdf. */
                std::mutex          m_mutexes[FZ_LOCK_MAX];
                fz_locks_context    m_locks;
            };

            static state_t  s_state;

            struct thread_state_t
            {
                thread_state_t()
                : m_ctx(NULL)
                {}
                fz_context* get_context()
                {
                    if (!m_ctx) {
                        /* Make a context for this thread by cloning the global
                        context. */
                        std::lock_guard<std::mutex> lock( s_state.m_mutex);
                        m_ctx = fz_clone_context(s_state.m_ctx);
                    }
                    return m_ctx;
                }
                ~thread_state_t()
                {
                    if (m_ctx) {
                        fz_drop_context( m_ctx);
                    }
                }
                fz_context* m_ctx;
            };

            static thread_local thread_state_t  s_thread_state;

            fz_context* context_get()
            {
                return s_thread_state.get_context();
            }


            ''')
    cpp = cpp.replace( 'thread_state_t', thread_state_t)
    cpp = cpp.replace( 'state_t', state_t)
    cpp = cpp.replace( 'context_get', rename.internal('context_get'))
    out_cpp.write( cpp)

    make_namespace_close( namespace, out_cpp)


# Maps from <tu> to dict of fnname: cursor.
g_functions_cache = dict()
g_global_data = dict()

def functions_cache_populate( tu):
    if tu in g_functions_cache:
        return
    fns = dict()
    global_data = dict()

    for cursor in tu.cursor.get_children():
        if (cursor.linkage == clang.cindex.LinkageKind.EXTERNAL
                or cursor.is_definition()  # Picks up static inline functions.
                ):
            if cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL:
                fnname = cursor.mangled_name
                #if fnname.startswith( ('fz_', 'pdf_')) and fnname not in omit_fns:
                if fnname not in omit_fns:
                    fns[ fnname] = cursor
            else:
                global_data[ cursor.mangled_name] = cursor

    g_functions_cache[ tu] = fns
    g_global_data[tu] = global_data


def find_functions_starting_with( tu, name_prefix, method):
    '''
    Yields (name, cursor) for all functions in <tu> whose names start with
    name_prefix.
    method:
        If true, we omit names that are in omit_methods
    '''
    functions_cache_populate( tu)
    fn_to_cursor = g_functions_cache[ tu]
    for fnname, cursor in fn_to_cursor.items():
        if method and fnname in omit_methods:
            continue
        if not fnname.startswith( name_prefix):
            continue
        yield fnname, cursor

def find_global_data_starting_with( tu, prefix):
    for name, cursor in g_global_data[tu].items():
        if name.startswith( prefix):
            yield name, cursor

def find_function( tu, fnname, method):
    '''
    Returns cursor for function called <name> in <tu>, or None if not found.
    '''
    assert ' ' not in fnname, f'fnname={fnname}'
    if method and fnname in omit_methods:
        return
    functions_cache_populate( tu)
    return g_functions_cache[ tu].get( fnname)



class MethodExcludeReason_VARIADIC:
    pass
class MethodExcludeReason_OMIT_CLASS:
    pass
class MethodExcludeReason_NO_EXTRAS:
    pass
class MethodExcludeReason_NO_RAW_CONSTRUCTOR:
    pass
class MethodExcludeReason_NOT_COPYABLE:
    pass
class MethodExcludeReason_NO_WRAPPER_CLASS:
    pass
class MethodExcludeReason_ENUM:
    pass
class MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
    pass

# Maps from <structname> to list of functions satisfying conditions specified
# by find_wrappable_function_with_arg0_type() below.
#
find_wrappable_function_with_arg0_type_cache = None

# Maps from fnname to list of strings, each string being a description of why
# this fn is not suitable for wrapping by class method.
#
find_wrappable_function_with_arg0_type_excluded_cache = None

def find_wrappable_function_with_arg0_type_cache_populate( tu):

    global find_wrappable_function_with_arg0_type_cache
    global find_wrappable_function_with_arg0_type_excluded_cache

    if find_wrappable_function_with_arg0_type_cache:
        return

    t0 = time.time()

    find_wrappable_function_with_arg0_type_cache = dict()
    find_wrappable_function_with_arg0_type_excluded_cache = dict()

    for fnname, cursor in find_functions_starting_with( tu, ('fz_', 'pdf_'), method=True):

        exclude_reasons = []

        if fnname.startswith( 'fz_drop_') or fnname.startswith( 'fz_keep_'):
            continue
        if fnname.startswith( 'pdf_drop_') or fnname.startswith( 'pdf_keep_'):
            continue

        if cursor.type.is_function_variadic():
            exclude_reasons.append(
                    (
                    MethodExcludeReason_VARIADIC,
                    'function is variadic',
                    ))

        # Look at resulttype.
        #
        result_type = cursor.type.get_result().get_canonical()
        if result_type.kind == clang.cindex.TypeKind.POINTER:
            result_type = result_type.get_pointee().get_canonical()
        result_type = clip( result_type.spelling, 'struct ')
        if result_type in omit_class_names0:
            exclude_reasons.append(
                    (
                    MethodExcludeReason_OMIT_CLASS,
                    f'result_type={result_type} is in omit_class_names0',
                    ))
        if result_type.startswith( ('fz_', 'pdf_')):
            result_type_extras = get_fz_extras( result_type)
            if not result_type_extras:
                exclude_reasons.append(
                        (
                        MethodExcludeReason_NO_EXTRAS,
                        f'no extras defined for result_type={result_type}',
                        ))
            else:
                if not result_type_extras.constructor_raw:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_NO_RAW_CONSTRUCTOR,
                            f'wrapper for result_type={result_type} does not have raw constructor.',
                            ))
                if not result_type_extras.copyable:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_NOT_COPYABLE,
                                f'wrapper for result_type={result_type} is not copyable.',
                            ))

        # Look at args
        #
        i = 0
        arg0_cursor = None
        for arg in get_args( tu, cursor):

            base_typename = get_base_typename( arg.cursor.type)
            if not arg.alt and base_typename.startswith( ('fz_', 'pdf_')):
                if arg.cursor.type.get_canonical().kind==clang.cindex.TypeKind.ENUM:
                    # We don't (yet) wrap fz_* enums, but for now at least we
                    # still wrap functions that take fz_* enum parameters -
                    # callers will have to use the fz_* type.
                    #
                    # For example this is required by mutool_draw.py because
                    # mudraw.c calls fz_set_separation_behavior().
                    #
                    logx( 'not excluding {fnname=} with enum fz_ param : {arg.cursor.spelling=} {arg.cursor.type.kind} {arg.cursor.type.get_canonical().kind=}')
                else:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_NO_WRAPPER_CLASS,
                            f'no wrapper class for arg i={i}: {arg.cursor.type.get_canonical().spelling} {arg.cursor.type.get_canonical().kind}',
                            ))
            if i == 0:
                if arg.alt:
                    arg0_cursor = arg.alt
                else:
                    exclude_reasons.append(
                            (
                            MethodExcludeReason_FIRST_ARG_NOT_STRUCT,
                            'first arg is not fz_* struct',
                            ))
            i += 1

        if exclude_reasons:
            find_wrappable_function_with_arg0_type_excluded_cache[ fnname] = exclude_reasons
            #if fnname == 'fz_load_outline':   # lgtm [py/unreachable-statement]
            if g_show_details(fnname):
                log( 'Excluding {fnname=} from possible class methods because:')
                for i in exclude_reasons:
                    log( '    {i}')
        else:
            if i > 0:
                # <fnname> is ok to wrap.
                arg0 = arg0_cursor.type.get_canonical().spelling
                arg0 = clip( arg0, 'struct ')

                items = find_wrappable_function_with_arg0_type_cache.setdefault( arg0, [])
                items.append( fnname)

    logx( f'populating find_wrappable_function_with_arg0_type_cache took {time.time()-t0}s')


def find_wrappable_function_with_arg0_type( tu, structname):
    '''
    Return list of fz_*() function names which could be wrapped as a method of
    our wrapper class for <structname>.

    The functions whose names we return, satisfy all of the following:

        First non-context param is <structname> (by reference, pointer or value).

        No arg type (by reference, pointer or value) is in omit_class_names0.

        Return type (by reference, pointer or value) is not in omit_class_names0.

        If return type is a fz_* struc (by reference, pointer or value), the
        corresponding wrapper class has a raw constructor.
    '''
    find_wrappable_function_with_arg0_type_cache_populate( tu)

    ret = find_wrappable_function_with_arg0_type_cache.get( structname, [])
    if g_show_details(structname):
        log('{structname=}: {len(ret)=}:')
        for i in ret:
            log('    {i}')
    return ret



def make_function_wrappers(
        tu,
        namespace,
        out_exceptions_h,
        out_exceptions_cpp,
        out_functions_h,
        out_functions_cpp,
        out_internal_h,
        out_internal_cpp,
        out_swig_c,
        out_swig_python,
        ):
    '''
    Generates C++ source code containing wrappers for all fz_*() functions.

    We also create a function throw_exception(fz_context* ctx) that throws a
    C++ exception appropriate for the error in ctx.

    Wrappers for functions that have first arg fz_context*, convert
    fz_try..fz_catch exceptions into C++ exceptions by calling
    throw_exception(). Others are just plain wrappers.

    We remove any fz_context* argument and make implementation call
    internal_get_context() to get the fz_context* to use.

    We generate a class for each exception type.

    Returned source is just the raw fucntions, e.g. it does not contain
    required #include's.

    Args:
        tu:
            Clang translation unit.
        out_exceptions_h:
            Stream to which we write exception class definitions.
        out_exceptions_cpp:
            Stream to which we write exception class implementation.
        out_functions_h:
            Stream to which we write function declarations.
        out_functions_cpp:
            Stream to which we write function definitions.
    '''

    # Look for FZ_ERROR_* enums. We generate an exception class for each of
    # these.
    #
    error_name_prefix = 'FZ_ERROR_'
    fz_error_names = []
    fz_error_names_maxlen = 0   # Used for padding so generated code aligns.
    for cursor in tu.cursor.get_children():
        if cursor.kind == clang.cindex.CursorKind.ENUM_DECL:
            #log( 'enum: {cursor.spelling=})
            for child in cursor.get_children():
                #log( 'child:{ child.spelling=})
                if child.spelling.startswith( error_name_prefix):
                    name = child.spelling[ len(error_name_prefix):]
                    fz_error_names.append( name)
                    if len( name) > fz_error_names_maxlen:
                        fz_error_names_maxlen = len( name)

    def errors():
        '''
        Yields (enum, typename, padding) for each error.
        E.g.:
            enum=FZ_ERROR_MEMORY
            typename=mupdf_error_memory
            padding='  '
        '''
        for name in fz_error_names:
            enum = f'{error_name_prefix}{name}'
            typename = rename.class_( f'fz_error_{name.lower()}')
            padding = (fz_error_names_maxlen - len(name)) * ' '
            yield enum, typename, padding

    # Declare base exception class and define its methods.
    #
    base_name = rename.class_('fz_error_base')

    out_exceptions_h.write( textwrap.dedent(
            f'''
            /* Base class for {rename.class_( '')} exceptions */
            struct {base_name} : std::exception
            {{
                int         m_code;
                std::string m_text;
                const char* what() const throw();
                {base_name}(int code, const char* text);
            }};
            '''))

    out_exceptions_cpp.write( textwrap.dedent(
            f'''
            {base_name}::{base_name}(int code, const char* text)
            : m_code(code)
            {{
                char    code_text[32];
                snprintf(code_text, sizeof(code_text), "%i", code);
                m_text = std::string("code=") + code_text + ": " + text;
            }};

            const char* {base_name}::what() const throw()
            {{
                return m_text.c_str();
            }};

            '''))

    # Declare exception class for each FZ_ERROR_*.
    #
    for enum, typename, padding in errors():
        out_exceptions_h.write( textwrap.dedent(
                f'''
                /* For {enum}. */
                struct {typename} : {base_name}
                {{
                    {typename}(const char* message);
                }};

                '''))

    # Define constructor for each exception class.
    #
    for enum, typename, padding in errors():
        out_exceptions_cpp.write( textwrap.dedent(
                f'''
                {typename}::{typename}(const char* text)
                : {base_name}({enum}, text)
                {{
                }}

                '''))

    # Generate function that throws an appropriate exception from a fz_context.
    #
    te = rename.internal( 'throw_exception')
    out_exceptions_h.write( textwrap.dedent(
            f'''
            /* Throw exception appropriate for error in <ctx>. */
            void {te}(fz_context* ctx);

            '''))
    out_exceptions_cpp.write( textwrap.dedent(
            f'''
            void {te}(fz_context* ctx)
            {{
                int code = fz_caught(ctx);
                const char* text = fz_caught_message(ctx);
            '''))
    for enum, typename, padding in errors():
        out_exceptions_cpp.write( f'    if (code == {enum}) {padding}throw {typename}{padding}(text);\n')
    out_exceptions_cpp.write( f'    throw {base_name}(code, fz_caught_message(ctx));\n')
    out_exceptions_cpp.write( f'}}\n')
    out_exceptions_cpp.write( '\n')

    make_internal_functions( namespace, out_internal_h, out_internal_cpp)

    # Generate wrappers for each function that we find.
    #
    functions = []
    for fnname, cursor in find_functions_starting_with( tu, ('fz_', 'pdf_'), method=False):
        assert fnname not in omit_fns
        if cursor.type.is_function_variadic():
            # We don't attempt to wrap variadic functions - would need to find
            # the equivalent function that takes a va_list.
            continue
        if fnname == 'fz_push_try':
            # This is partof implementation of fz_try/catch so doesn't make
            # sense to provide a wrapper. Also it is OS-dependent so including
            # it makes our generated code OS-specific.
            continue

        functions.append( (fnname, cursor))

    log( '{len(functions)=}')

    # Sort by function-name to make output easier to read.
    functions.sort()
    for fnname, cursor in functions:
        fnname_wrapper = rename.function( fnname)
        # clang-6 appears not to be able to handle fn args that are themselves
        # function pointers, so for now we allow make_function_wrapper() to
        # fail, so we need to use temporary buffers, otherwise out_functions_h
        # and out_functions_cpp can get partial text written.
        #
        temp_out_h = io.StringIO()
        temp_out_cpp = io.StringIO()
        try:
            make_function_wrapper(
                    tu,
                    cursor,
                    fnname_wrapper,
                    temp_out_h,
                    temp_out_cpp,
                    out_swig_c,
                    out_swig_python,
                    )
        except Clang6FnArgsBug as e:
            #log( jlib.exception_info())
            log( 'Unable to wrap function {cursor.spelling} becase: {e}')
            continue

        out_functions_h.write( temp_out_h.getvalue())
        out_functions_cpp.write( temp_out_cpp.getvalue())
        if fnname == 'fz_lookup_metadata':
            # Output convenience wrapper for fz_lookup_metadata() that is
            # easily SWIG-able - it returns a std::string by value, and uses an
            # out-param for the integer error/length value.
            out_functions_h.write(
                    textwrap.dedent(
                    f'''
                    /* Extra wrapper for fz_lookup_metadata() that returns a std::string and sets
                    *o_out to length of string plus one. If <key> is not found, returns empty
                    string with *o_out=-1. <o_out> can be NULL if caller is not interested in
                    error information. */
                    FZ_FUNCTION std::string lookup_metadata(fz_document *doc, const char *key, int* o_out=NULL);

                    '''))
            out_functions_cpp.write(
                    textwrap.dedent(
                    f'''
                    FZ_FUNCTION std::string lookup_metadata(fz_document *doc, const char *key, int* o_out)
                    {{
                        int e = lookup_metadata(doc, key, NULL /*buf*/, 0 /*size*/);
                        if (e < 0) {{
                            // Not found.
                            if (o_out)  *o_out = e;
                            return "";
                        }}
                        assert(e != 0);
                        char* buf = (char*) malloc(e);
                        assert(buf);    // mupdf::malloc() throws on error.
                        int e2 = lookup_metadata(doc, key, buf, e);
                        assert(e2 = e);
                        std::string ret = buf;
                        free(buf);
                        if (o_out)  *o_out = e;
                        return ret;
                    }}
                    '''))


find_struct_cache = None
def find_struct( tu, structname, require_definition=True):
    '''
    Finds definition of structure.
    Args:
        tu:
            Translation unit.
        structname:
            Name of structure to find.
        require_definition:
            Only return cursor if it is for definition of structure.

    Returns cursor for definition or None.
    '''
    structname = clip( structname, 'struct ')   # Remove any 'struct ' prefix.
    global find_struct_cache
    if find_struct_cache is None:
        find_struct_cache = dict()
        for cursor in tu.cursor.get_children():
            already = find_struct_cache.get( cursor.spelling)
            if already is None:
                find_struct_cache[ cursor.spelling] = cursor
            elif cursor.is_definition() and not already.is_definition():
                find_struct_cache[ cursor.spelling] = cursor
    ret = find_struct_cache.get( structname)
    if not ret:
        return
    if require_definition and not ret.is_definition():
        return
    return ret


def find_name( cursor, name, nest=0):
    '''
    Returns cursor for specified name within <cursor>, or None if not found.

    name:
        Name to search for. Can contain '.' characters; we look for each
        element in turn, calling ourselves recursively.

    cursor:
        Item to search.
    '''
    if cursor.spelling == '':
        # Anonymous item; this seems to occur for (non-anonymous) unions.
        #
        # We recurse into children directly.
        #
        for c in cursor.get_children():
            ret = find_name_internal( c, name, nest+1)
            if ret:
                return ret

    d = name.find( '.')
    if d >= 0:
        head, tail = name[:d], name[d+1:]
        # Look for first element then for remaining.
        c = find_name( cursor, head, nest+1)
        if not c:
            return
        ret = find_name( c, tail, nest+2)
        return ret

    for c in cursor.type.get_canonical().get_fields():
        if c.spelling == '':
            ret = find_name( c, name, nest+1)
            if ret:
                return ret
        if c.spelling == name:
            return c



def class_add_iterator( struct, structname, classname, extras):
    '''
    Add begin() and end() methods so that this generated class is iterable
    from C++ with:

        for (auto i: foo) {...}

    We modify <extras> to create an iterator class and add begin() and end()
    methods that each return an instance of the iterator class.
    '''
    it_begin, it_end = extras.iterator_next

    # Figure out type of what the iterator returns by looking at type of
    # <it_begin>.
    if it_begin:
        c = find_name( struct, it_begin)
        assert c.type.kind == clang.cindex.TypeKind.POINTER
        it_internal_type = c.type.get_pointee().get_canonical().spelling
        it_internal_type = clip( it_internal_type, 'struct ')
        it_type = rename.class_( it_internal_type)
    else:
        # The container is also the first item in the linked list.
        it_internal_type = structname
        it_type = classname

    # We add to extras.methods_extra().
    #
    extras.methods_extra.append(
            ExtraMethod( f'{classname}Iterator', 'begin()',
                    f'''
                    {{
                        return {classname}Iterator(m_internal{'->'+it_begin if it_begin else ''});
                    }}
                    ''',
                    f'/* Used for iteration over linked list of {it_type} items starting at {it_internal_type}::{it_begin}. */',
                    ),
            )
    extras.methods_extra.append(
            ExtraMethod( f'{classname}Iterator', 'end()',
                    f'''
                    {{
                        return {classname}Iterator(NULL);
                    }}
                    ''',
                    f'/* Used for iteration over linked list of {it_type} items starting at {it_internal_type}::{it_begin}. */',
                    ),
            )

    extras.class_bottom += f'\n    typedef {classname}Iterator iterator;\n'

    extras.class_pre += f'\nstruct {classname}Iterator;\n'

    extras.class_post += f'''
            struct {classname}Iterator
            {{
                FZ_FUNCTION {classname}Iterator(const {it_type}& item);
                FZ_FUNCTION {classname}Iterator& operator++();
                FZ_FUNCTION bool operator==( const {classname}Iterator& rhs);
                FZ_FUNCTION bool operator!=( const {classname}Iterator& rhs);
                FZ_FUNCTION {it_type} operator*();
                FZ_FUNCTION {it_type}* operator->();
                private:
                {it_type} m_item;
            }};
            '''
    keep_text = ''
    if extras.copyable and extras.copyable != 'default':
        # Our operator++ needs to create it_type from m_item.m_internal->next,
        # so we need to call fz_keep_<it_type>().
        #
        # [Perhaps life would be simpler if our generated constructors always
        # called fz_keep_*() as necessary? In some circumstances this would
        # require us to call fz_drop_*() when constructing an instance, but
        # that might be simpler?]
        #
        base_name = clip( structname, ('fz_', 'pdf_'))
        if structname.startswith( 'fz_'):
            keep_name = f'fz_keep_{base_name}'
        elif structname.startswith( 'pdf_'):
            keep_name = f'pdf_keep_{base_name}'
        keep_name = rename.function_call(keep_name)
        keep_text = f'{keep_name}(m_item.m_internal->next);'

    extras.extra_cpp += f'''
            FZ_FUNCTION {classname}Iterator::{classname}Iterator(const {it_type}& item)
            : m_item( item)
            {{
            }}
            FZ_FUNCTION {classname}Iterator& {classname}Iterator::operator++()
            {{
                {keep_text}
                m_item = {it_type}(m_item.m_internal->next);
                return *this;
            }}
            FZ_FUNCTION bool {classname}Iterator::operator==( const {classname}Iterator& rhs)
            {{
                return m_item.m_internal == rhs.m_item.m_internal;
            }}
            FZ_FUNCTION bool {classname}Iterator::operator!=( const {classname}Iterator& rhs)
            {{
                return m_item.m_internal != rhs.m_item.m_internal;
            }}
            FZ_FUNCTION {it_type} {classname}Iterator::operator*()
            {{
                return m_item;
            }}
            FZ_FUNCTION {it_type}* {classname}Iterator::operator->()
            {{
                return &m_item;
            }}

            void test({classname}& item)
            {{
                for( {classname}Iterator it = item.begin(); it != item.end(); ++it) {{
                    (void) *it;
                }}
                for ( auto i: item) {{
                    (void) i;
                }}
            }}

            '''

def class_find_constructor_fns( tu, classname, structname, base_name, extras):
    '''
    Returns list of functions that could be used by constructor.

    tu:
        .
    classname:
        Name of our wrapper class.
    structname:
        Name of underlying mupdf struct.
    base_name:
        Name of struct without 'fz_' prefix.
    extras:
        .
    '''
    assert structname == f'fz_{base_name}' or structname == f'pdf_{base_name}'
    constructor_fns = []
    if '-' not in extras.constructor_prefixes:
        # Add default constructor fn prefix.
        if structname.startswith( 'fz_'):
            extras.constructor_prefixes.insert( 0, f'fz_new_')
        elif structname.startswith( 'pdf_'):
            extras.constructor_prefixes.insert( 0, f'pdf_new_')
    for fnprefix in extras.constructor_prefixes:
        for fnname, cursor in find_functions_starting_with( tu, fnprefix, method=True):
            # Check whether this has identical signature to any fn we've
            # already found.
            duplicate_type = None
            duplicate_name = False
            for f, c, is_duplicate in constructor_fns:
                #jlib.log( '{cursor.type=} {c.type=}')
                if f == fnname:
                    duplicate_name = True
                    break
                if c.type == cursor.type:
                    #jlib.log( '{structname} wrapper: ignoring candidate constructor {fnname}() because prototype is indistinguishable from {f=}()')
                    duplicate_type = f
                    break
            if duplicate_name:
                continue
            ok = False

            arg, n = get_first_arg( tu, cursor)
            if arg and n == 1 and is_pointer_to( arg.cursor.type, structname):
                # This avoids generation of bogus copy constructor wrapping
                # function fz_new_pixmap_from_alpha_channel() introduced
                # 2021-05-07.
                #
                logx('ignoring possible constructor because looks like copy constructor: {fnname}')
            elif fnname in extras.constructor_excludes:
                pass
            elif extras.pod and cursor.result_type.get_canonical().spelling == f'{structname}':
                ok = True
            elif not extras.pod and is_pointer_to( cursor.result_type, f'{structname}'):
                ok = True

            if ok:
                if duplicate_type and extras.copyable:
                    log1( 'adding static method wrapper for {fnname}')
                    extras.method_wrappers_static.append( fnname)
                else:
                    if duplicate_type:
                        logx( 'not able to provide static factory fn {structname}::{fnname} because wrapper class is not copyable.')
                    log1( 'adding constructor wrapper for {fnname}')
                    constructor_fns.append( (fnname, cursor, duplicate_type))
            else:
                log3( 'ignoring possible constructor for {classname=} because does not return required type: {fnname=} -> {cursor.result_type.spelling=}')

    constructor_fns.sort()
    return constructor_fns


def class_find_destructor_fns( tu, structname, base_name):
    '''
    Returns list of functions that could be used by destructor - must be called
    'fz_drop_<typename>', must take a <struct>* arg, may take a fz_context*
    arg.
    '''
    if structname.startswith( 'fz_'):
        destructor_prefix = f'fz_drop_{base_name}'
    elif structname.startswith( 'pdf_'):
        destructor_prefix = f'pdf_drop_{base_name}'
    destructor_fns = []
    for fnname, cursor in find_functions_starting_with( tu, destructor_prefix, method=True):
        arg_struct = False
        arg_context = False
        args_num = 0
        for arg in get_args( tu, cursor):
            if not arg_struct and is_pointer_to( arg.cursor.type, structname):
                arg_struct = True
            elif not arg_context and is_pointer_to( arg.cursor.type, 'fz_context'):
                arg_context = True
            args_num += 1
        if arg_struct:
            if args_num == 1 or (args_num == 2 and arg_context):
                # No params other than <struct>* and fz_context* so this is
                # candidate destructor.
                #log( 'adding candidate destructor: {fnname}')
                fnname = rename.function( fnname)
                destructor_fns.append( (fnname, cursor))

    destructor_fns.sort()
    return destructor_fns


def class_copy_constructor(
        tu,
        functions,
        structname,
        base_name,
        classname,
        constructor_fns,
        out_h,
        out_cpp,
        ):
    '''
    Generate a copy constructor and operator= by finding a suitable fz_keep_*()
    function.

    We raise an exception if we can't find one.
    '''
    if structname.startswith( 'fz_'):
        keep_name = f'fz_keep_{base_name}'
        drop_name = f'fz_drop_{base_name}'
    elif structname.startswith( 'pdf_'):
        keep_name = f'pdf_keep_{base_name}'
        drop_name = f'pdf_drop_{base_name}'
    for name in keep_name, drop_name:
        cursor = find_function( tu, name, method=True)
        if not cursor:
            #log( 'marking non-copyable: {structname}, because no function {name}().')
            classextras.get( structname).copyable = False
            return
        if name == keep_name:
            pvoid = is_pointer_to( cursor.result_type, 'void')
            assert ( pvoid
                    or is_pointer_to( cursor.result_type, structname)
                    ), (
                    f'result_type not void* or pointer to {name}: {cursor.result_type.spelling}'
                    )
        arg, n = get_first_arg( tu, cursor)
        assert n == 1, f'should take exactly one arg: {cursor.spelling}()'
        assert is_pointer_to( arg.cursor.type, structname), (
                f'arg0 is not pointer to {structname}: {cursor.spelling}(): {arg.cursor.spelling} {arg.name}')

    for fnname, cursor, duplicate_type in constructor_fns:
        fnname2 = rename.function_call(fnname)
        if fnname2 == keep_name:
            log( 'not generating copy constructor with {keep_name=} because already used by a constructor.')
            break
    else:
        functions( keep_name)
        comment = f'Copy constructor using {keep_name}().'
        out_h.write( '\n')
        out_h.write( f'    /* {comment} */\n')
        out_h.write( f'    FZ_FUNCTION {classname}(const {classname}& rhs);\n')
        out_h.write( '\n')

        cast = ''
        if pvoid:
            # Need to cast the void* to the correct type.
            cast = f'({structname}*) '

        out_cpp.write( f'/* {comment} */\n')
        out_cpp.write( f'FZ_FUNCTION {classname}::{classname}(const {classname}& rhs)\n')
        out_cpp.write( f': m_internal({cast}{rename.function_call(keep_name)}(rhs.m_internal))\n')
        out_cpp.write( '{\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')

    # Make operator=().
    #
    comment = f'operator= using {keep_name}() and {drop_name}().'
    out_h.write( f'    /* {comment} */\n')
    out_h.write( f'    FZ_FUNCTION {classname}& operator=(const {classname}& rhs);\n')

    out_cpp.write( f'/* {comment} */\n')
    out_cpp.write( f'FZ_FUNCTION {classname}& {classname}::operator=(const {classname}& rhs)\n')
    out_cpp.write(  '{\n')
    out_cpp.write( f'    {rename.function_call(drop_name)}(this->m_internal);\n')
    out_cpp.write( f'    {rename.function_call(keep_name)}(rhs.m_internal);\n')
    out_cpp.write( f'    this->m_internal = {cast}rhs.m_internal;\n')
    out_cpp.write( f'    return *this;\n')
    out_cpp.write(  '}\n')
    out_cpp.write(  '\n')


def class_write_method_body(
        tu,
        structname,
        classname,
        fnname,
        out_cpp,
        static,
        constructor,
        extras,
        struct,
        fn_cursor,
        construct_from_temp,
        fnname2,
        return_cursor,
        return_type,
        ):
    out_cpp.write( f'{{\n')

    # Write function call.
    if constructor:
        if extras.pod:
            if extras.pod == 'inline':
                out_cpp.write( f'    *({structname}*) &this->{get_field0(struct.type).spelling} = ')
            else:
                out_cpp.write( f'    this->m_internal = ')
            if fn_cursor.result_type.kind == clang.cindex.TypeKind.POINTER:
                out_cpp.write( f'*')
        else:
            out_cpp.write( f'    this->m_internal = ')
            if fn_cursor.result_type.kind == clang.cindex.TypeKind.POINTER:
                pass
            else:
                assert 0, 'cannot handle underlying fn returning by value when not pod.'
        out_cpp.write( f'{rename.function_call(fnname)}(')

    elif construct_from_temp == 'address_of_value':
        out_cpp.write( f'    {return_cursor.spelling} temp = mupdf::{fnname2}(')
    elif construct_from_temp == 'pointer':
        out_cpp.write( f'    {return_cursor.spelling}* temp = mupdf::{fnname2}(')
    else:
        out_cpp.write( f'    return mupdf::{fnname2}(')

    have_used_this = False
    sep = ''
    for arg in get_args( tu, fn_cursor):
        arg_classname = classname
        if static or constructor:
            arg_classname = None
        out_cpp.write( sep)
        have_used_this = write_call_arg(
                arg,
                arg_classname,
                have_used_this,
                out_cpp,
                )
        sep = ', '
    out_cpp.write( f');\n')

    if fnname in functions_that_return_non_kept:
        return_type_base = clip( return_cursor.spelling, ('fz_', 'pdf_'))
        keep_fn = f'{prefix(structname)}keep_{return_type_base}'
        out_cpp.write( f'    {rename.function_call(keep_fn)}(temp);\n')

    if construct_from_temp == 'address_of_value':
        out_cpp.write( f'    return {return_type}(&temp);\n')
    elif construct_from_temp == 'pointer':
        out_cpp.write( f'    return {return_type}(temp);\n')

    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')

def class_write_method(
        tu,
        register_fn_use,
        structname,
        classname,
        fnname,
        out_h,
        out_cpp,
        static=False,
        constructor=False,
        extras=None,
        struct=None,
        duplicate_type=None,
        out_swig_python=None,
        debug=None,
        ):
    '''
    Writes a method that calls <fnname>.

    Also appends python code to <out_swig_python> if specified.

        tu
            .
        register_fn_use
            Callback to keep track of what fz_*() fns have been used.
        structname
            E.g. fz_rect.
        classname
            E.g. Rect.
        fnname
            Name of fz_*() fn to wrap, e.g. fz_concat.
        out_h
        out_cpp
            Where to write generated code.
        static
            If true, we generate a static method.

            Otherwise we generate a normal class method, where first arg
            that is type <structname> is omitted from the generated method's
            prototype; in the implementation we use <this>.
        constructor
            If true, we write a constructor.
        extras
            None or ClassExtras instance.
            Only used if <constructor> is true.
        struct
            None or cursor for the struct definition.
            Only used if <constructor> is true.
        out_swig_python
            If specified and there are one or more out-params, we write python
            code to overwride the default SWIG-generated method, to call our
            *_outparams_fn() alternative.
        debug
            Show extra diagnostics.
    '''
    assert fnname not in omit_methods
    if debug:
        log( '{classname=} {fnname=}')
    assert fnname.startswith( ('fz_', 'pdf_'))
    fn_cursor = find_function( tu, fnname, method=True)
    if not fn_cursor:
        log( '*** ignoring {fnname=}')
        return

    # Construct prototype fnname(args).
    #
    methodname = rename.method( structname, fnname)
    if constructor:
        decl_h = f'{classname}('
        decl_cpp = f'{classname}('
    else:
        decl_h = f'{methodname}('
        decl_cpp = f'{methodname}('
    have_used_this = False
    num_out_params = 0
    comma = ''
    #debug = structname == 'pdf_document' and fnname == 'pdf_page_write'
    for arg in get_args( tu, fn_cursor):
        if debug:
            log( 'Looking at {structname=} {fnname=} {arg=}')
        decl_h += comma
        decl_cpp += comma
        if arg.out_param:
            num_out_params += 1
        if arg.alt:
            # This parameter is something like 'fz_foo* arg',
            # which we convert to 'mupdf_foo_s& arg' so that the caller can
            # use C++ class mupdf_foo_s.
            #
            if (1
                    and not static
                    and not constructor
                    and rename.class_(clip( arg.alt.type.spelling, 'struct ')) == classname
                    and not have_used_this
                    ):
                assert not arg.out_param
                # Omit this arg from the method's prototype - we'll use <this>
                # when calling the underlying fz_ function.
                have_used_this = True
                continue

            const = ''
            if not arg.out_param:
                extras2 = classextras.get( arg.alt.type.spelling)
                if not extras2:
                    log('cannot find {alt.spelling=} {arg.type.spelling=} {name=}')
            if not arg.out_param and not classextras.get( arg.alt.type.spelling).pod:
                const = 'const '
            decl_h +=   f'{const}{rename.class_(arg.alt.type.spelling)}& '
            decl_h += f'{arg.name}'
            decl_cpp += f'{const}{rename.class_(arg.alt.type.spelling)}& {arg.name}'
        else:
            logx( '{arg.spelling=}')
            decl_h += declaration_text( arg.cursor.type, arg.name)
            decl_cpp += declaration_text( arg.cursor.type, arg.name)
        comma = ', '

    decl_h += ')'
    decl_cpp += ')'

    fnname2 = rename.function( fnname)
    if constructor:
        comment = f'Constructor using {fnname}().'
    else:
        comment = f'Wrapper for {fnname}().'

    if not static and not constructor:
        assert have_used_this, f'error: wrapper for {structname}: {fnname}() is not useful - does not have a {structname} arg.'

    if not duplicate_type:
        register_fn_use( fnname)

    # If this is true, we explicitly construct a temporary from what the
    # wrapped function returns.
    #
    construct_from_temp = None

    warning_not_copyable = False
    warning_no_raw_constructor = False

    return_cursor = None
    return_type = None
    if constructor:
        fn_h = f'{decl_h}'
        fn_cpp = f'{classname}::{decl_cpp}'
    else:
        fn_h = declaration_text( fn_cursor.result_type, decl_h)
        fn_cpp = declaration_text( fn_cursor.result_type, f'{classname}::{decl_cpp}')

        # See whether we can convert return type to an instance of a wrapper
        # class.
        #
        if fn_cursor.result_type.kind == clang.cindex.TypeKind.POINTER:
            t = fn_cursor.result_type.get_pointee().get_canonical()
            return_cursor = find_struct( tu, t.spelling, require_definition=False)
            if return_cursor:
                return_extras = classextras.get( return_cursor.spelling)
                if return_extras:
                    # Change return type to be instance of class wrapper.
                    return_type = rename.class_(return_cursor.spelling)
                    if return_extras.copyable and return_extras.constructor_raw:
                        fn_h = f'{return_type} {decl_h}'
                        fn_cpp = f'{return_type} {classname}::{decl_cpp}'
                        construct_from_temp = 'pointer'
                    else:
                        if not return_extras.copyable:
                            warning_not_copyable = True
                        if not return_extras.constructor_raw:
                            warning_no_raw_constructor = True
        else:
            # The fz_*() function returns by value. See whether we can convert its
            # return type to an instance of a wrapping class.
            #
            # If so, we will use constructor that takes pointer to the fz_
            # struct. C++ doesn't allow us to use address of temporary, so we
            # generate code like this:
            #
            #   fz_quad_s ret = mupdf_snap_selection(...);
            #   return Quad(&ret);
            #
            t = fn_cursor.result_type.get_canonical()
            return_cursor = find_struct( tu, t.spelling)
            if return_cursor:
                tt = return_cursor.type.get_canonical()
                if tt.kind == clang.cindex.TypeKind.ENUM:
                    # For now, we return this type directly with no wrapping.
                    pass
                else:
                    return_extras = classextras.get( return_cursor.type.spelling)
                    return_type = rename.class_(return_cursor.type.spelling)
                    fn_h = f'{return_type} {decl_h}'
                    fn_cpp = f'{return_type} {classname}::{decl_cpp}'
                    construct_from_temp = 'address_of_value'

    if warning_not_copyable:
        log( '*** warning: {classname}::{decl_h}: Not able to return wrapping class {return_type} from {return_cursor.spelling} because {return_type} is not copyable.')
    if warning_no_raw_constructor:
        log( '*** warning: {classname}::{decl_h}: Not able to return wrapping class {return_type} from {return_cursor.spelling} because {return_type} has no raw constructor.')

    out_h.write( '\n')
    out_h.write( f'    /* {comment} */\n')

    # Copy any comment (indented) into class definition above method
    # declaration.
    if fn_cursor.raw_comment:
        for line in fn_cursor.raw_comment.split( '\n'):
            out_h.write( f'    {line}\n')

    if duplicate_type:
        out_h.write( f'    /* Disabled because same args as {duplicate_type}.\n')

    out_h.write( f'    FZ_FUNCTION {"static " if static else ""}{fn_h};\n')
    if duplicate_type:
        out_h.write( f'    */\n')

    out_cpp.write( f'/* {comment} */\n')
    if duplicate_type:
        out_cpp.write( f'/* Disabled because same args as {duplicate_type}.\n')

    out_cpp.write( f'FZ_FUNCTION {fn_cpp}\n')

    class_write_method_body(
            tu,
            structname,
            classname,
            fnname,
            out_cpp,
            static,
            constructor,
            extras,
            struct,
            fn_cursor,
            construct_from_temp,
            fnname2,
            return_cursor,
            return_type,
            )

    if duplicate_type:
        out_cpp.write( f'*/\n')

    if out_swig_python and num_out_params:
        make_python_class_method_outparam_override(
                tu,
                fn_cursor,
                fnname,
                out_swig_python,
                structname,
                classname,
                return_type,
                )


def class_custom_method( register_fn_use, classname, extramethod, out_h, out_cpp):
    '''
    Writes custom method as specified by <extramethod>.
    '''
    is_constructor = False
    is_destructor = False
    is_begin_end = False

    if extramethod.return_:
        name_args = extramethod.name_args
        return_space = f'{extramethod.return_} '
        comment = 'Custom method.'
        if name_args.startswith( 'begin(') or name_args.startswith( 'end('):
            is_begin_end = True
    elif extramethod.name_args == '~()':
        # Destructor.
        name_args = f'~{classname}{extramethod.name_args[1:]}'
        return_space = ''
        comment = 'Custom destructor.'
        is_destructor = True
    else:
        # Constructor.
        assert extramethod.name_args.startswith( '('), f'bad constructor/destructor in classname={classname}'
        name_args = f'{classname}{extramethod.name_args}'
        return_space = ''
        comment = 'Custom constructor.'
        is_constructor = True

    out_h.write( f'\n')
    if extramethod.comment:
        for line in extramethod.comment.strip().split('\n'):
            out_h.write( f'    {line}\n')
    else:
        out_h.write( f'    /* {comment} */\n')
    out_h.write( f'    FZ_FUNCTION {return_space}{name_args};\n')

    out_cpp.write( f'/* {comment} */\n')
    # Remove any default arg values from <name_args>.
    name_args_no_defaults = re.sub('= *[^(][^),]*', '', name_args)
    if name_args_no_defaults != name_args:
        log('have changed {name_args=} to {name_args_no_defaults=}')
    out_cpp.write( f'FZ_FUNCTION {return_space}{classname}::{name_args_no_defaults}')
    out_cpp.write( textwrap.dedent(extramethod.body))
    out_cpp.write( f'\n')

    if 1:   # lgtm [py/constant-conditional-expression]
        # Register calls of all fz_* functions. Not necessarily helpful - we
        # might only be interested in calls of fz_* functions that are directly
        # available to uses of class.
        #
        for fnname in re.findall( '(mupdf::[a-zA-Z0-9_]+) *[(]', extramethod.body):
            fnname = clip( fnname, 'mupdf::')
            if not fnname.startswith( 'pdf_'):
                fnname = 'fz_' + fnname
            #log( 'registering use of {fnname} in extramethod {classname}::{name_args}')
            register_fn_use( fnname)

    return is_constructor, is_destructor, is_begin_end


def class_raw_constructor(
        register_fn_use,
        classname,
        struct,
        structname,
        base_name,
        extras,
        constructor_fns,
        out_h,
        out_cpp,
        ):
    '''
    Create a raw constructor - a constructor taking pointer to underlying
    struct. If constructor_fns includes a fz_keep_*-style fn, we use
    that, otherwise we generate a constructor that simply copies the arg.
    '''
    #jlib.log( 'Creating raw constructor {classname=} {structname=} {extras.pod=} {extras.constructor_raw=} {fnname=}')
    comment = f'/* Constructor using raw copy of pre-existing {structname}. */'
    if extras.pod:
        constructor_decl = f'{classname}(const {structname}* internal)'
    else:
        constructor_decl = f'{classname}({structname}* internal)'
    out_h.write( '\n')
    out_h.write( f'    {comment}\n')
    if extras.constructor_raw == 'default':
        out_h.write( f'    FZ_FUNCTION {classname}({structname}* internal=NULL);\n')
    else:
        out_h.write( f'    FZ_FUNCTION {constructor_decl};\n')

    out_cpp.write( f'FZ_FUNCTION {classname}::{constructor_decl}\n')
    if extras.pod == 'inline':
        pass
    elif extras.pod:
        out_cpp.write( ': m_internal(*internal)\n')
    else:
        out_cpp.write( ': m_internal(internal)\n')
    out_cpp.write( '{\n')
    if extras.pod == 'inline':
        assert struct, f'cannot form raw constructor for inline pod {classname} without cursor for underlying {structname}'
        for c in struct.type.get_canonical().get_fields():
            if c.type.kind == clang.cindex.TypeKind.CONSTANTARRAY:
                out_cpp.write( f'    memcpy(this->{c.spelling}, internal->{c.spelling}, sizeof(this->{c.spelling}));\n')
            else:
                out_cpp.write( f'    this->{c.spelling} = internal->{c.spelling};\n')
    out_cpp.write( '}\n')
    out_cpp.write( '\n')

    if extras.pod == 'inline':
        # Write second constructor that takes underlying struct by value.
        #
        constructor_decl = f'{classname}(const {structname} internal)'
        out_h.write( '\n')
        out_h.write( f'    {comment}\n')
        out_h.write( f'    FZ_FUNCTION {constructor_decl};\n')

        out_cpp.write( f'FZ_FUNCTION {classname}::{constructor_decl}\n')
        out_cpp.write( '{\n')
        for c in struct.type.get_canonical().get_fields():
            if c.type.kind == clang.cindex.TypeKind.CONSTANTARRAY:
                out_cpp.write( f'    memcpy(this->{c.spelling}, &internal.{c.spelling}, sizeof(this->{c.spelling}));\n')
            else:
                out_cpp.write( f'    this->{c.spelling} = internal.{c.spelling};\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')

        # Write accessor for inline state.
        #
        for const in False, True:
            space_const = ' const' if const else ''
            const_space = 'const ' if const else ''
            out_h.write( '\n')
            out_h.write( f'    /* Access as underlying struct. */\n')
            out_h.write( f'    FZ_FUNCTION {const_space}{structname}* internal(){space_const};\n')
            out_cpp.write( f'{comment}\n')
            out_cpp.write( f'FZ_FUNCTION {const_space}{structname}* {classname}::internal(){space_const}\n')
            out_cpp.write( '{\n')
            field0 = get_field0(struct.type).spelling
            out_cpp.write( f'    return ({const_space}{structname}*) &this->{field0};\n')
            out_cpp.write( '}\n')
            out_cpp.write( '\n')



def class_accessors(
        tu,
        register_fn_use,
        classname,
        struct,
        structname,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes accessor functions for member data.
    '''
    if not extras.pod:
        logx( 'creating accessor for non-pod class {classname=} wrapping {structname}')
    for cursor in struct.type.get_canonical().get_fields():
        #jlib.log( 'accessors: {cursor.spelling=} {cursor.type.spelling=}')

        # We set this to fz_keep_<type>() function to call, if we return a
        # wrapper class constructed from raw pointer to underlying fz_* struct.
        keep_function = None

        # Set <decl> to be prototype with %s where the name is, e.g. 'int
        # %s()'; later on we use python's % operator to replace the '%s'
        # with the name we want.
        #
        if cursor.type.kind == clang.cindex.TypeKind.POINTER:
            decl = 'const ' + declaration_text( cursor.type, '%s()')
            pointee_type = cursor.type.get_pointee().get_canonical().spelling
            pointee_type = clip( pointee_type, 'const ')
            pointee_type = clip( pointee_type, 'struct ')
            #if 'fz_' in pointee_type:
            #    log( '{pointee_type=}')
            # We don't attempt to make accessors to function pointers.
            if cursor.type.get_pointee().get_canonical().kind == clang.cindex.TypeKind.FUNCTIONPROTO:
                logx( 'ignoring {cursor.spelling=} because pointer to FUNCTIONPROTO')
                continue
            elif pointee_type.startswith( ('fz_', 'pdf_')):
                extras2 = get_fz_extras( pointee_type)
                if extras2:
                    # Make this accessor return an instance of the wrapping
                    # class by value.
                    #
                    classname2 = rename.class_( pointee_type)
                    decl = f'{classname2} %s()'

                    # If there's a fz_keep_() function, we must call it on the
                    # internal data before returning the wrapping class.
                    pointee_type_base = clip( pointee_type, ('fz_', 'pdf_'))
                    keep_function = f'{prefix(pointee_type)}keep_{pointee_type_base}'
                    if find_function( tu, keep_function, method=False):
                        logx( 'using {keep_function=}')
                    else:
                        log( 'cannot find {keep_function=}')
                        keep_function = None
        elif cursor.type.kind == clang.cindex.TypeKind.FUNCTIONPROTO:
            log( 'ignoring {cursor.spelling=} because FUNCTIONPROTO')
            continue
        else:
            if 0 and extras.pod:    # lgtm [py/unreachable-statement]
                # Return reference so caller can modify. Unfortunately SWIG
                # converts non-const references to pointers, so generated
                # python isn't useful.
                fn_args = '& %s()'
            else:
                fn_args = '%s()'
            if cursor.type.get_array_size() >= 0:
                if 0:   # lgtm [py/unreachable-statement]
                    # Return reference to the array; we need to put fn name
                    # and args inside (...) to allow the declaration syntax
                    # to work - we end up with things like:
                    #
                    #   char (& media_class())[64];
                    #
                    # Unfortunately SWIG doesn't seem to be able to cope
                    # with this.
                    decl = declaration_text( cursor.type, '(%s)' % fn_args)
                else:
                    # Return pointer to the first element of the array, so
                    # that SWIG can cope.
                    fn_args = '* %s()'
                    type_ = cursor.type.get_array_element_type()
                    decl = declaration_text( type_, fn_args)
            else:
                if ( cursor.type.kind==clang.cindex.TypeKind.TYPEDEF
                        and cursor.type.get_typedef_name() in ('uint8_t', 'int8_t')
                        ):
                    # Don't let accessor return uint8_t because SWIG thinks it
                    # is a char*, leading to memory errors. Instead return int.
                    #
                    logx('Changing from {cursor.type.get_typedef_name()=} {cursor.type=} to int')
                    decl = f'int {fn_args}'
                else:
                    decl = declaration_text( cursor.type, fn_args)

        # todo: if return type is uint8_t or int8_t, maybe return as <int>
        # so SWIG doesn't think it is a string? This would fix errors witht
        # fz_image::n and fz_image::bpc.
        out_h.write( f'    FZ_FUNCTION {decl % cursor.spelling};\n')
        out_cpp.write( 'FZ_FUNCTION %s\n' % (decl % ( f'{classname}::{cursor.spelling}')))
        out_cpp.write( '{\n')
        if keep_function:
            out_cpp.write( f'    {rename.function_call(keep_function)}(m_internal->{cursor.spelling});\n')
        if extras.pod:
            out_cpp.write( f'    return m_internal.{cursor.spelling};\n')
        else:
            out_cpp.write( f'    return m_internal->{cursor.spelling};\n')
        out_cpp.write( '}\n')
        out_cpp.write( '\n')




def class_destructor(
        register_fn_use,
        classname,
        extras,
        destructor_fns,
        out_h,
        out_cpp,
        ):
    if len(destructor_fns) > 1:
        # Use function with shortest name.
        if 0:   # lgtm [py/unreachable-statement]
            jlib.log( 'Multiple possible destructor fns for {classname=}')
            for fnname, cursor in destructor_fns:
                jlib.log( '    {fnname=} {cursor.spelling=}')
        shortest = None
        for i in destructor_fns:
            if shortest is None or len(i[0]) < len(shortest[0]):
                shortest = i
        #jlib.log( 'Using: {shortest[0]=}')
        destructor_fns = [shortest]
    if len(destructor_fns):
        fnname, cursor = destructor_fns[0]
        register_fn_use( cursor.spelling)
        out_h.write( f'    /* Destructor using {cursor.spelling}(). */\n')
        out_h.write( f'    FZ_FUNCTION ~{classname}();\n');

        out_cpp.write( f'FZ_FUNCTION {classname}::~{classname}()\n')
        out_cpp.write(  '{\n')
        out_cpp.write( f'    {rename.function_call(fnname)}(m_internal);\n')
        out_cpp.write(  '}\n')
        out_cpp.write( '\n')
    else:
        out_h.write( '    /* We use default destructor. */\n')


def class_to_string_member(
        tu,
        classname,
        struct,
        structname,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes code for wrapper class's to_string() member function.
    '''
    out_h.write( f'\n')
    out_h.write( f'    /* Returns string containing our members, labelled and inside (...), using operator<<. */\n')
    out_h.write( f'    FZ_FUNCTION std::string to_string();\n')

    out_cpp.write( f'FZ_FUNCTION std::string {classname}::to_string()\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    std::ostringstream buffer;\n')
    out_cpp.write( f'    buffer << *this;\n')
    out_cpp.write( f'    return buffer.str();\n')
    out_cpp.write( f'}}\n')
    out_cpp.write( f'\n')


def struct_to_string_fns(
        tu,
        struct,
        structname,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes functions for text representation of struct/wrapper-class members.
    '''
    out_h.write( f'\n')
    out_h.write( f'/* Returns string containing a {structname}\'s members, labelled and inside (...), using operator<<. */\n')
    out_h.write( f'FZ_FUNCTION std::string to_string_{structname}(const {structname}& s);\n')

    out_h.write( f'\n')
    out_h.write( f'/* Returns string containing a {structname}\'s members, labelled and inside (...), using operator<<.\n')
    out_h.write( f'(Convenience overload). */\n')
    out_h.write( f'FZ_FUNCTION std::string to_string(const {structname}& s);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::string to_string_{structname}(const {structname}& s)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    std::ostringstream buffer;\n')
    out_cpp.write( f'    buffer << s;\n')
    out_cpp.write( f'    return buffer.str();\n')
    out_cpp.write( f'}}\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::string to_string(const {structname}& s)\n')
    out_cpp.write( f'{{\n')
    out_cpp.write( f'    return to_string_{structname}(s);\n')
    out_cpp.write( f'}}\n')


def struct_to_string_streaming_fns(
        tu,
        namespace,
        struct,
        structname,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes operator<< functions for streaming text representation of C struct
    members. Should be called at top-level (i.e. not inside 'namespace mupdf
    {...}') in out_h and out_cpp.
    '''
    out_h.write( f'\n')
    out_h.write( f'/* Writes {structname}\'s members, labelled and inside (...), to a stream. */\n')
    out_h.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {structname}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {structname}& rhs)\n')
    out_cpp.write( f'{{\n')
    i = 0
    out_cpp.write( f'    out\n')
    out_cpp.write( f'            << "("\n');
    for cursor in struct.type.get_canonical().get_fields():
        out_cpp.write( f'            << ');
        if i:
            out_cpp.write( f'" {cursor.spelling}="')
        else:
            out_cpp.write( f' "{cursor.spelling}="')
        out_cpp.write( f' << rhs.{cursor.spelling}\n')
        i += 1
    out_cpp.write( f'            << ")"\n');
    out_cpp.write( f'            ;\n')
    out_cpp.write( f'    return out;\n')
    out_cpp.write( f'}}\n')


def class_to_string_fns(
        tu,
        classname,
        struct,
        structname,
        extras,
        out_h,
        out_cpp,
        ):
    '''
    Writes functions for text representation of wrapper-class members. These
    functions make use of the corresponding struct functions created by
    struct_to_string_fns().
    '''
    out_h.write( f'\n')
    out_h.write( f'/* Writes a {classname}\'s underlying {structname}\'s members, labelled and inside (...), to a stream. */\n')
    out_h.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {classname}& rhs);\n')

    out_cpp.write( f'\n')
    out_cpp.write( f'FZ_FUNCTION std::ostream& operator<< (std::ostream& out, const {classname}& rhs)\n')
    out_cpp.write( f'{{\n')
    if extras.pod == 'inline':
        out_cpp.write( f'    return out << *rhs.internal();\n')
    elif extras.pod:
        out_cpp.write( f'    return out << rhs.m_internal;\n')
    else:
        out_cpp.write( f'    return out << " " << *rhs.m_internal;\n')
    out_cpp.write( f'}}\n')


def class_wrapper(
        tu,
        register_fn_use,
        struct,
        structname,
        classname,
        extras,
        out_h,
        out_cpp,
        out_h_end,
        out_swig_python,
        ):
    '''
    Creates source for a class called <classname> that wraps <struct>, with
    methods that call selected fz_*() functions. Writes to out_h and out_cpp.

    Created source is just the per-class code, e.g. it does not contain
    #include's.

    Args:
        tu:
            Clang translation unit.
        struct:
            Cursor for struct to wrap.
        structname:
            Name of struct to wrap.
        classname:
            Name of wrapper class to create.
        out_h:
            Stream to which we write class definition.
        out_cpp:
            Stream to which we write method implementations.
        out_h_end:
            Stream for text that should be put at the end of the generated
            header text.

    Returns (is_container, has_to_string). is_container is true if generated
    class has custom begin() and end() methods; has_to_string is true if we
    have created a to_string() method.
    '''
    assert extras, f'extras is None for {structname}'
    if extras.iterator_next:
        class_add_iterator( struct, structname, classname, extras)

    if extras.class_pre:
        out_h.write( textwrap.dedent( extras.class_pre))

    base_name = clip( structname, ('fz_', 'pdf_'))

    constructor_fns = class_find_constructor_fns( tu, classname, structname, base_name, extras)
    for fnname in extras.constructors_wrappers:
        cursor = find_function( tu, fnname, method=True)
        constructor_fns.append( (fnname, cursor, None))

    destructor_fns = class_find_destructor_fns( tu, structname, base_name)

    # Class definition beginning.
    #
    out_h.write( '\n')
    out_h.write( f'/* Wrapper class for struct {structname}. */\n')
    if struct.raw_comment:
        out_h.write( f'{struct.raw_comment}')
        if not struct.raw_comment.endswith( '\n'):
            out_h.write( '\n')
    out_h.write( f'struct {classname}\n{{')

    out_cpp.write( '\n')
    out_cpp.write( f'/* Implementation of methods for {classname} (wrapper for {structname}). */\n')
    out_cpp.write( '\n')

    # Trailing text in header, e.g. typedef for iterator.
    #
    if extras.class_top:
        # Strip leading blank line to avoid slightly odd-looking layout.
        text = clip( extras.class_top, '\n')
        text = textwrap.dedent( text)
        text = textwrap.indent( text, '    ')
        out_h.write( '\n')
        out_h.write( text)

    # Constructors
    #
    if constructor_fns:
        out_h.write( '\n')
        out_h.write( '    /* == Constructors. */\n')
    num_constructors = len(constructor_fns)
    for fnname, cursor, duplicate_type in constructor_fns:
        # clang-6 appears not to be able to handle fn args that are themselves
        # function pointers, so for now we allow make_function_wrapper() to
        # fail, so we need to use temporary buffers, otherwise out_functions_h
        # and out_functions_cpp can get partial text written.
        #
        temp_out_h = io.StringIO()
        temp_out_cpp = io.StringIO()
        try:
            class_write_method(
                    tu,
                    register_fn_use,
                    structname,
                    classname,
                    fnname,
                    temp_out_h,
                    temp_out_cpp,
                    static=False,
                    constructor=True,
                    extras=extras,
                    struct=struct,
                    duplicate_type=duplicate_type,
                    )
        except Clang6FnArgsBug as e:
            log( 'Unable to wrap function {fnname} becase: {e}')
        else:
            out_h.write( temp_out_h.getvalue())
            out_cpp.write( temp_out_cpp.getvalue())

    # Custom constructors.
    #
    for extra_constructor in extras.constructors_extra:
        class_custom_method(
                register_fn_use,
                classname,
                extra_constructor,
                out_h,
                out_cpp,
                )
        num_constructors += 1

    # Look for function that can be used by copy constructor and operator=.
    #
    if not extras.pod and extras.copyable and extras.copyable != 'default':
        class_copy_constructor(
                tu,
                register_fn_use,
                structname,
                base_name,
                classname,
                constructor_fns,
                out_h,
                out_cpp,
                )

    # Auto-add all methods that take <structname> as first param, but
    # skip methods that are already wrapped in extras.method_wrappers or
    # extras.methods_extra etc.
    #
    for fnname in find_wrappable_function_with_arg0_type( tu, structname):
        if g_show_details(fnname):
            log('{structname=}: looking at potential method wrapping {fnname=}')
        if fnname in extras.method_wrappers:
            #log( 'auto-detected fn already in {structname} method_wrappers: {fnname}')
            # Omit this function, because there is an extra method with the
            # same name. (We could probably include both as they will generally
            # have different args so overloading will destinguish them, but
            # extra methods are usually defined to be used in preference.)
            pass
        elif fnname.startswith( 'fz_new_draw_device'):
            # fz_new_draw_device*() functions take first arg fz_matrix, but
            # aren't really appropriate for the fz_matrix wrapper class.
            #
            pass
        else:
            for extramethod in extras.methods_extra:
                if extramethod.name_args.startswith( f'{clip(fnname, "fz_", "_s")}('):
                    if not extramethod.overload:
                        #log( 'fnname already in extras.methods_extra: {extramethod.name_args}')
                        break
            else:
                #log( 'adding to extras.method_wrappers: {fnname}')
                extras.method_wrappers.append( fnname)


    # Extra static methods.
    #
    if extras.method_wrappers_static:
        out_h.write( '\n')
        out_h.write( '    /* == Static methods. */\n')
    for fnname in extras.method_wrappers_static:
        class_write_method(
                tu,
                register_fn_use,
                structname,
                classname,
                fnname,
                out_h,
                out_cpp,
                static=True,
                struct=struct,
                out_swig_python=out_swig_python,
                )

    # Extra methods that wrap fz_*() fns.
    #
    if extras.method_wrappers or extras.methods_extra:
        out_h.write( '\n')
        out_h.write( '    /* == Methods. */')
        out_h.write( '\n')
    extras.method_wrappers.sort()
    for fnname in extras.method_wrappers:
        class_write_method(
                tu,
                register_fn_use,
                structname,
                classname,
                fnname,
                out_h,
                out_cpp,
                struct=struct,
                out_swig_python=out_swig_python,
                debug=g_show_details(fnname),
                )

    # Custom methods.
    #
    is_container = 0
    custom_destructor = False
    for extramethod in extras.methods_extra:
        is_constructor, is_destructor, is_begin_end = class_custom_method(
                register_fn_use,
                classname,
                extramethod,
                out_h,
                out_cpp,
                )
        if is_constructor:
            num_constructors += 1
        if is_destructor:
            custom_destructor = True
        if is_begin_end:
            is_container += 1

    assert is_container==0 or is_container==2   # Should be begin()+end() or neither.
    if is_container:
        pass
        #jlib.log( 'Generated class has begin() and end(): {classname=}')

    if num_constructors == 0 or extras.constructor_raw:
        class_raw_constructor(
                register_fn_use,
                classname,
                struct,
                structname,
                base_name,
                extras,
                constructor_fns,
                out_h,
                out_cpp,
                )

    # Accessor methods to POD data.
    #
    if extras.accessors and extras.pod == 'inline':
        log( 'ignoring {extras.accessors=} for {structname=} because {extras.pod=}.')
    elif extras.accessors:
        out_h.write( f'\n')
        out_h.write( f'    /* == Accessors to members of {structname} m_internal. */\n')
        out_h.write( '\n')
        class_accessors(
                tu,
                register_fn_use,
                classname,
                struct,
                structname,
                extras,
                out_h,
                out_cpp,
                )

    # Destructor.
    #
    if not custom_destructor:
        out_h.write( f'\n')
        class_destructor(
                register_fn_use,
                classname,
                extras,
                destructor_fns,
                out_h,
                out_cpp,
                )

    # Class members.
    #
    out_h.write( '\n')
    out_h.write( '    /* == Member data. */\n')
    out_h.write( '\n')
    if extras.pod == 'inline':
        out_h.write( f'    /* These members are the same as the members of {structname}. */\n')
        for c in struct.type.get_canonical().get_fields():
            out_h.write( f'    {declaration_text(c.type, c.spelling)};\n')
    elif extras.pod:
        out_h.write( f'    {struct.spelling}  m_internal; /* Wrapped data is held by value. */\n')
    else:
        out_h.write( f'    {structname}* m_internal; /* Pointer to wrapped data. */\n')

    # Make operator<< (std::ostream&, ...) for POD classes.
    #
    has_to_string = False
    if extras.pod:
        has_to_string = True
        class_to_string_member(
                tu,
                classname,
                struct,
                structname,
                extras,
                out_h,
                out_cpp,
                )

    # Trailing text in header, e.g. typedef for iterator.
    #
    if extras.class_bottom:
        out_h.write( textwrap.indent( textwrap.dedent( extras.class_bottom), '    '))

    # Private copy constructor if not copyable.
    #
    if not extras.copyable:
        out_h.write(  '\n')
        out_h.write(  '    private:\n')
        out_h.write(  '\n')
        out_h.write(  '    /* This class is not copyable or assignable. */\n')
        out_h.write( f'    {classname}(const {classname}& rhs);\n')
        out_h.write( f'    {classname}& operator=(const {classname}& rhs);\n')

    # Class definition end.
    #
    out_h.write( '};\n')

    # Make operator<< (std::ostream&, ...) for POD classes.
    #
    if extras.pod:
        class_to_string_fns(
                tu,
                classname,
                struct,
                structname,
                extras,
                out_h,
                out_cpp,
                )

    if extras.class_post:
        out_h_end.write( textwrap.dedent( extras.class_post))

    if extras.extra_cpp:
        out_cpp.write( textwrap.dedent( extras.extra_cpp))

    return is_container, has_to_string


def header_guard( name, out):
    '''
    Writes header guard for <name> to stream <out>.
    '''
    m = ''
    for c in name:
        m += c.upper() if c.isalnum() else '_'
    out.write( f'#ifndef {m}\n')
    out.write( f'#define {m}\n')
    out.write( '\n')

def tabify( filename, text):
    '''
    Returns <text> with leading multiples of 4 spaces converted to tab
    characters.
    '''
    ret = ''
    linenum = 0
    for line in text.split( '\n'):
        linenum += 1
        i = 0
        while 1:
            if i == len(line):
                break
            if line[i] != ' ':
                break
            i += 1
        if i % 4:
            if line[ int(i/4)*4:].startswith( ' *'):
                # This can happen in comments.
                pass
            else:
                log( '*** {filename}:{linenum}: {i=} {line!r=} indentation is not a multiple of 4')
        num_tabs = int(i / 4)
        ret += num_tabs * '\t' + line[ num_tabs*4:] + '\n'

    # We use [:-1] here because split() always returns extra last item '', so
    # we will have appended an extra '\n'.
    #
    return ret[:-1]


def cpp_source( dir_mupdf, namespace, base, header_git, out_swig_c, out_swig_python, doit=True):
    '''
    Generates all .h and .cpp files.

    dir_mupdf:
        Location of mupdf checkout.
    namespace:
        C++ namespace to use.
    base:
        Directory in which all generated files are placed.
    doit:
        For debugging only. If false, we don't actually write to any files.

    Returns (tu, hs, cpps, fn_usage_filename, container_classnames, to_string_structnames, fn_usage, output_param_fns, c_functions).
        tu:
            From clang.
        hs:
            List of generated .h files.
        cpps:
            List of generated .cpp files.
        fn_usage_filename:
            Name of output file containing information on function usage.
        container_classnames:
            List of generated classnames that have begin() and end() methods.
        to_string_structnames:
            List of generated structnames that have to_string() method.
        fn_usage:
            Dict mapping fz_* function names to number of usages in generated
            class code.
            Name of output file containing information on function usage.
        windows_def_filename:
            Name of created .def filename.
        output_param_fns:
            .
        c_functions:
            List of C function names.
        c_globals:
            List of global variables.
    '''
    assert not dir_mupdf.endswith( '/')
    assert not base.endswith( '/')
    clang_info()    # Ensure we have set up clang-python.

    index = clang.cindex.Index.create()
    #log( '{dir_mupdf=} {base=}')

    header = f'{dir_mupdf}/include/mupdf/fitz.h'
    assert os.path.isfile( header), f'header={header}'

    # Get clang to parse mupdf/fitz.h and mupdf/pdf.h.
    #
    # It might be possible to use index.parse()'s <unsaved_files> arg to
    # specify these multiple files, but i couldn't get that to work.
    #
    # So instead we write some #include's to a temporary file and ask clang to
    # parse it.
    #
    temp_h = f'_mupdfwrap_temp.h'
    try:
        with open( temp_h, 'w') as f:
            f.write( '#include "mupdf/fitz.h"\n')
            f.write( '#include "mupdf/pdf.h"\n')
        args = []
        args.append(['-I', f'{dir_mupdf}/include'])
        if g_windows:
            args = ('-I', f'{dir_mupdf}/include')
        else:
            args = ('-I', f'{dir_mupdf}/include', '-I', clang_info().include_path)
        tu = index.parse( temp_h, args=args)
    finally:
        if os.path.isfile( temp_h):
            os.remove( temp_h)

    os.makedirs( f'{base}/include/mupdf', exist_ok=True)
    os.makedirs( f'{base}/implementation', exist_ok=True)

    if doit:
        class File:
            def __init__( self, filename, tabify=True):
                self.filename = filename
                self.tabify = tabify
                self.file = io.StringIO()
                self.line_begin = True
            def write( self, text, fileline=False):
                if fileline:
                    # Generate #line <line> "<filename>" for our caller's
                    # location. This makes any compiler warnings refer to thei
                    # python code rather than the generated C++ code.
                    tb = traceback.extract_stack( None)
                    filename, line, function, source = tb[0]
                    if self.line_begin:
                        self.file.write( f'#line {line} "{filename}"\n')
                self.file.write( text)
                self.line_begin = text.endswith( '\n')
            def close( self):
                if self.filename:
                    # Overwrite if contents differ.
                    text = self.get()
                    if self.tabify:
                        text = tabify( self.filename, text)
                    jlib.update_file( text, self.filename)
            def get( self):
                return self.file.getvalue()
    else:
        class File:
            def __init__( self, filename):
                pass
            def write( self, text, fileline=False):
                pass
            def close( self):
                pass

    class Outputs:
        '''
        A set of output files.

        For convenience, after outputs.add( 'foo', 'foo.c'), outputs.foo is a
        python stream that writes to 'foo.c'.
        '''
        def __init__( self):
            self.items = []

        def add( self, name, filename):
            '''
            Sets self.<name> to file opened for writing on <filename>.
            '''
            file = File( filename)
            self.items.append( (name, filename, file))
            setattr( self, name, file)

        def get( self):
            '''
            Returns list of (name, filename, file) tuples.
            '''
            return self.items

        def close( self):
            for name, filename, file in self.items:
                file.close()

    out_cpps = Outputs()
    out_hs = Outputs()
    for name in (
            'classes',
            'exceptions',
            'functions',
            'internal',
            ):
        out_hs.add( name, f'{base}/include/mupdf/{name}.h')
        out_cpps.add( name, f'{base}/implementation/{name}.cpp')

    # Create extra File that writes to internal buffer rather than an actual
    # file, which we will append to out_h.
    #
    out_h_classes_end = File( None)

    # Make text of header comment for all generated file.
    #
    header_text = textwrap.dedent(
            f'''
            /*
            This file was auto-generated by mupdfwrap.py.
            ''')

    if header_git:
        git_id = jlib.get_git_id( dir_mupdf, allow_none=True)
        if git_id:
            git_id = git_id.split('\n', 1)
            header_text += textwrap.dedent(
                    f'''
                    mupdf checkout:
                        {git_id[0]}'
                    ''')

    header_text += '*/\n'
    header_text += '\n'
    header_text = header_text[1:]   # Strip leading \n.
    for _, _, file in out_cpps.get() + out_hs.get():
        file.write( header_text)

    # Write multiple-inclusion guards into headers:
    #
    for name, filename, file in out_hs.get():
        prefix = f'{base}/include/'
        assert filename.startswith( prefix)
        name = filename[ len(prefix):]
        header_guard( name, file)

    # Write required #includes into .h files:
    #
    out_hs.exceptions.write( textwrap.dedent(
            '''
            #include <stdexcept>
            #include <string>

            #include "mupdf/fitz.h"

            '''))

    out_hs.functions.write( textwrap.dedent(
            '''
            #include "mupdf/fitz.h"
            #include "mupdf/pdf.h"

            #include <iostream>
            #include <string>
            #include <vector>

            '''))

    out_hs.classes.write( textwrap.dedent(
            '''
            #include "mupdf/fitz.h"
            #include "mupdf/functions.h"
            #include "mupdf/pdf.h"

            #include <string>
            #include <vector>

            '''))

    # Write required #includes into .cpp files:
    #
    out_cpps.exceptions.write( textwrap.dedent(
            '''
            #include "mupdf/exceptions.h"
            #include "mupdf/fitz.h"

            '''))

    out_cpps.functions.write( textwrap.dedent(
            '''
            #include "mupdf/exceptions.h"
            #include "mupdf/functions.h"
            #include "mupdf/internal.h"

            #include <assert.h>
            #include <sstream>

            '''))

    out_cpps.classes.write(
            textwrap.dedent(
            '''
            #include "mupdf/classes.h"
            #include "mupdf/exceptions.h"
            #include "mupdf/internal.h"

            #include "mupdf/fitz/geometry.h"

            #include <sstream>
            #include <string.h>

            '''))

    namespace = 'mupdf'
    for _, _, file in out_cpps.get() + out_hs.get():
        if file == out_cpps.internal:
            continue
        make_namespace_open( namespace, file)

    # Write declataion and definition for metadata_keys.
    #
    out_hs.functions.write(
            textwrap.dedent(
            '''
            /*
            The keys that are defined for fz_lookup_metadata().
            */
            FZ_DATA extern const std::vector<std::string> metadata_keys;

            '''))
    out_cpps.functions.write(
            textwrap.dedent(
            '''
            FZ_FUNCTION const std::vector<std::string> metadata_keys = {
                    "format",
                    "encryption",
                    "info:Title",
                    "info:Author",
                    "info:Subject",
                    "info:Keywords",
                    "info:Creator",
                    "info:Producer",
                    "info:CreationDate",
                    "info:ModDate",
            };

            static bool s_trace = getenv("MUPDF_trace") ? true : false;

            '''))

    # Write source code for exceptions and wrapper functions.
    #
    log( 'Creating wrapper functions...')
    output_param_fns = make_function_wrappers(
            tu,
            namespace,
            out_hs.exceptions,
            out_cpps.exceptions,
            out_hs.functions,
            out_cpps.functions,
            out_hs.internal,
            out_cpps.internal,
            out_swig_c,
            out_swig_python,
            )

    fn_usage = dict()
    functions_unrecognised = set()
    c_functions = []

    for fnname, cursor in find_functions_starting_with( tu, '', method=True):
        fn_usage[ fnname] = [0, cursor]
        c_functions.append(fnname)

    c_globals = []
    windows_def = ''
    #windows_def += 'LIBRARY mupdfcpp\n'    # This breaks things.
    windows_def += 'EXPORTS\n'
    for name, cursor in find_global_data_starting_with( tu, ('fz_', 'pdf_')):
        if g_show_details(name):
            log('global: {name=}')
        c_globals.append(name)
        windows_def += f'    {name} DATA\n'

    jlib.update_file( windows_def, f'{base}/windows_mupdf.def')

    def register_fn_use( name):
        assert name.startswith( ('fz_', 'pdf_'))
        if name in fn_usage:
            fn_usage[ name][0] += 1
        else:
            functions_unrecognised.add( name)

    # Write source code for wrapper classes.
    #
    log( 'Creating wrapper classes...')

    # Find all classes that we can create.
    #
    classes = []
    for cursor in tu.cursor.get_children():
        if not cursor.spelling.startswith( ('fz_', 'pdf_')):
            continue
        if cursor.kind != clang.cindex.CursorKind.TYPEDEF_DECL:
            continue;
        type_ = cursor.underlying_typedef_type.get_canonical()
        if type_.kind != clang.cindex.TypeKind.RECORD:
            continue

        if not cursor.is_definition():
            extras = classextras.get( cursor.spelling)
            if extras and extras.opaque:
                pass
                #log( 'Creating wrapper for opaque struct: {cursor.spelling=}')
            else:
                continue

        structname = type_.spelling
        structname = clip( structname, 'struct ')
        if omit_class( structname):
            continue
        if structname in omit_class_names0:
            continue
        classname = rename.class_( structname)
        #log( 'Creating class wrapper. {classname=} {cursor.spelling=} {structname=}')

        # For some reason after updating mupdf 2020-04-13, clang-python is
        # returning two locations for struct fz_buffer_s, both STRUCT_DECL. One
        # is 'typedef struct fz_buffer_s fz_buffer;', the other is the full
        # struct definition.
        #
        # No idea why this is happening. Using .canonical doesn't seem to
        # affect things.
        #
        for cl, cu, s in classes:
            if cl == classname:
                logx( 'ignoring duplicate STRUCT_DECL for {structname=}')
                break
        else:
            classes.append( (classname, cursor, structname))

    classes.sort()

    if omit_class_names:
        log( '*** Some names to omit were left over: {omit_class_names=}')
        #assert 0

    # Write forward declarations - this is required because some class
    # methods take pointers to other classes.
    #
    out_hs.classes.write( '\n')
    out_hs.classes.write( '/* Forward declarations of all classes that we define. */\n')
    for classname, struct, structname in classes:
        out_hs.classes.write( f'struct {classname};\n')
    out_hs.classes.write( '\n')

    container_classnames = []
    to_string_structnames = []

    # Create each class.
    #
    for classname, struct, structname in classes:
        #log( 'creating wrapper {classname} for {cursor.spelling}')
        extras = classextras.get( structname)
        if extras.pod:
            struct_to_string_fns(
                    tu,
                    struct,
                    structname,
                    extras,
                    out_hs.functions,
                    out_cpps.functions,
                    )

        with jlib.LogPrefixScope( f'{structname}: '):
            is_container, has_to_string = class_wrapper(
                    tu,
                    register_fn_use,
                    struct,
                    structname,
                    classname,
                    extras,
                    out_hs.classes,
                    out_cpps.classes,
                    out_h_classes_end,
                    out_swig_python,
                    )
        if is_container:
            container_classnames.append( classname)
        if has_to_string:
            to_string_structnames.append( structname)

    # Write close of namespace.
    out_hs.classes.write( out_h_classes_end.get())
    for _, _, file in out_cpps.get() + out_hs.get():
        if file == out_cpps.internal:
            continue
        make_namespace_close( namespace, file)

    # Write operator<< functions - these need to be outside the namespace.
    #
    for classname, struct, structname in classes:
        extras = classextras.get( structname)
        if extras.pod:
            struct_to_string_streaming_fns(
                    tu,
                    namespace,
                    struct,
                    structname,
                    extras,
                    out_hs.functions,
                    out_cpps.functions,
                    )

    # Terminate multiple-inclusion guards in headers:
    #
    for _, _, file in out_hs.get():
        file.write( '\n#endif\n')

    out_hs.close()
    out_cpps.close()

    filenames_h = [filename for _, filename, _ in out_hs.get()]
    filenames_cpp = [filename for _, filename, _ in out_cpps.get()]
    if 0:   # lgtm [py/unreachable-statement]
        log( 'Have created:')
        for filename in filenames_h + filenames_cpp:
            log( '    {filename}')


    # Output usage information.
    #

    fn_usage_filename = f'{base}/fn_usage.txt'
    out_fn_usage = File( fn_usage_filename, tabify=False)
    functions_unused = 0
    functions_used = 0

    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        exclude_reasons = find_wrappable_function_with_arg0_type_excluded_cache.get( fnname, [])
        if n:
            functions_used += 1
        else:
            functions_unused += 1
        if n and not exclude_reasons:
            continue
        # 'cursor.displayname' doesn't include the return type, so we use our
        # declaration_text() fn to show the function details.
        if 0:   # lgtm [py/unreachable-statement]
            out_fn_usage.write( f'    {declaration_text( cursor.type, cursor.spelling, arg_names=1)}\n')
            out_fn_usage.write( f'        {n}\n')

    out_fn_usage.write( f'Functions not wrapped by class methods:\n')
    out_fn_usage.write( '\n')

    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        exclude_reasons = find_wrappable_function_with_arg0_type_excluded_cache.get( fnname, [])
        if not exclude_reasons:
            continue
        if n:
            continue
        num_interesting_reasons = 0
        for t, description in exclude_reasons:
            if t == MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
                continue
            if t == MethodExcludeReason_VARIADIC:
                continue
            num_interesting_reasons += 1
        if num_interesting_reasons:
            try:
                out_fn_usage.write( f'    {declaration_text( cursor.type, cursor.spelling, arg_names=1)}\n')
            except Clang6FnArgsBug as e:
                out_fn_usage.write( f'    {cursor.spelling} [full prototype not available due to known clang-6 issue]\n')
            for t, description in exclude_reasons:
                if t == MethodExcludeReason_FIRST_ARG_NOT_STRUCT:
                    continue
                out_fn_usage.write( f'        {description}\n')
            out_fn_usage.write( '\n')

    out_fn_usage.write( f'\n')
    out_fn_usage.write( f'Functions used more than once:\n')
    for fnname in sorted( fn_usage.keys()):
        n, cursor = fn_usage[ fnname]
        if n > 1:
            out_fn_usage.write( f'    n={n}: {declaration_text( cursor.type, cursor.spelling, arg_names=1)}\n')

    out_fn_usage.write( f'\n')
    out_fn_usage.write( f'Number of wrapped functions: {len(fn_usage)}\n')
    out_fn_usage.write( f'Number of wrapped functions used by wrapper classes: {functions_used}\n')
    out_fn_usage.write( f'Number of wrapped functions not used by wrapper classes: {functions_unused}\n')

    out_fn_usage.close()

    return (
            tu,
            base,
            filenames_h,
            filenames_cpp,
            fn_usage_filename,
            container_classnames,
            to_string_structnames,
            fn_usage,
            output_param_fns,
            c_functions,
            c_globals,
            )


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
    for cursor in tu.cursor.get_children():
        name = cursor.mangled_name
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
                type_ = type_.get_canonical()
                if type_.spelling.startswith( 'struct fz_'):
                    return True
            # Set uses_structs to true if fn returns a fz struct or any
            # argument is a fz struct.
            if uses_struct( cursor.result_type):
                uses_structs = True
            else:
                for arg in get_args( tu, cursor):
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
        log( '{filename!r=} {path=}')
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
            log( '    {fnname:40} {classes_n=} {directory_n=}')

    log( '{n_missing}')



def build_swig(
        build_dirs,
        container_classnames,
        to_string_structnames,
        swig_c,
        swig_python,
        c_functions,
        c_globals,
        language='python',
        swig='swig'
        ):
    '''
    Builds python wrapper for all mupdf_* functions and classes.
    '''

    # Find version of swig. (We use quotes around <swig> to make things work on
    # Windows.)
    t = jlib.system( f'"{swig}" -version', out='return')
    m = re.search( 'SWIG Version ([0-9]+)[.]([0-9]+)[.]([0-9]+)', t)
    assert m
    swig_major = int( m.group(1))

    # Create a .i file for SWIG.
    #

    common = f'''
            #include <stdexcept>
            #include "mupdf/functions.h"

            #include "mupdf/classes.h"

            /* Support for extracting buffer data into a Python bytes. */

            PyObject* buffer_extract_bytes(fz_buffer* buffer)
            {{
                unsigned char* c = NULL;
                /* We mimic the affects of fz_buffer_extract(), which leaves
                the buffer with zero capacity. */
                size_t len = mupdf::buffer_storage(buffer, &c);
                PyObject* ret = PyBytes_FromStringAndSize((const char*) c, (Py_ssize_t) len);
                if (ret) {{
                    mupdf::clear_buffer(buffer);
                    mupdf::trim_buffer(buffer);
                }}
                return ret;
            }}
            '''

    common += swig_c

    text = ''
    for fnname in c_functions:
        text += f'%ignore {fnname};\n'
    if 0 and g_windows:
        # Haven't yet figured out how to export global data from a DLL on
        # Windows. Probably needs mupdf_DATA in header only, but that's a
        # little tricky to do because we are using the raw mupdf headers.
        #
        for name in c_globals:
            text += f'%ignore {name};\n'

    text += textwrap.dedent(f'''
            %ignore fz_append_vprintf;
            %ignore fz_error_stack_slot;
            %ignore fz_format_string;
            %ignore fz_vsnprintf;
            %ignore fz_vthrow;
            %ignore fz_vwarn;
            %ignore fz_write_vprintf;

            // Not implemented in mupdf.so: fz_colorspace_name_process_colorants
            %ignore fz_colorspace_name_process_colorants;

            %ignore fz_open_file_w;

            %ignore {rename.function('fz_append_vprintf')};
            %ignore {rename.function('fz_error_stack_slot_s')};
            %ignore {rename.function('fz_format_string')};
            %ignore {rename.function('fz_vsnprintf')};
            %ignore {rename.function('fz_vthrow')};
            %ignore {rename.function('fz_vwarn')};
            %ignore {rename.function('fz_write_vprintf')};
            %ignore {rename.function('fz_vsnprintf')};
            %ignore {rename.function('fz_vthrow')};
            %ignore {rename.function('fz_vwarn')};
            %ignore {rename.function('fz_append_vprintf')};
            %ignore {rename.function('fz_write_vprintf')};
            %ignore {rename.function('fz_format_string')};
            %ignore {rename.function('fz_open_file_w')};

            // SWIG can't handle this because it uses a valist.
            %ignore {rename.function('Memento_vasprintf')};

            // asprintf() isn't available on windows, so exclude Memento_asprintf because
            // it is #define-d to asprintf.
            %ignore {rename.function('Memento_asprintf')};

            // Might prefer to #include mupdf/exceptions.h and make the
            // %exception block below handle all the different exception types,
            // but swig-3 cannot parse 'throw()' in mupdf/exceptions.h.
            //
            // So for now we just #include <stdexcept> and handle
            // std::exception only.

            %include "typemaps.i"
            %include "cpointer.i"

            %{{
            {common}
            %}}

            %include exception.i
            %include std_string.i
            %include carrays.i
            %include cdata.i
            %include std_vector.i
            %include argcargv.i

            namespace std
            {{
                %template(vectori) vector<int>;
                %template(vectors) vector<std::string>;
                %template(vectorq) vector<mupdf::{rename.class_("fz_quad")}>;
            }};

            // Make sure that operator++() gets converted to __next__().
            //
            // Note that swig already seems to do:
            //
            //     operator* => __ref__
            //     operator== => __eq__
            //     operator!= => __ne__
            //     operator-> => __deref__
            //
            // Just need to add this method to containers that already have
            // begin() and end():
            //     def __iter__( self):
            //         return CppIterator( self)
            //

            %rename(__increment__) *::operator++;


            %array_functions(unsigned char, bytes);

            %exception {{
              try {{
                $action
              }}
              catch(std::exception& e) {{
                SWIG_exception(SWIG_RuntimeError, e.what());
              }}
              catch(...) {{
                SWIG_exception(SWIG_RuntimeError, "Unknown exception");
              }}
            }}

            // Ensure SWIG handles OUTPUT params.
            //
            %include "cpointer.i"

            // Don't wrap raw fz_*() functions.
            %rename("$ignore", regexmatch$name="^fz_", %$isfunction, %$not %$ismember) "";

            {'%feature("autodoc", "3");' if swig_major < 4 else ''}

            // Get swig about pdf_clean_file()'s (int,argv)-style args:
            %apply (int ARGC, char **ARGV) {{ (int retainlen, char *retainlist[]) }}
            ''')

    text += common

    text += textwrap.dedent(f'''
            %pointer_functions(int, pint);

            %pythoncode %{{

            def Document_lookup_metadata(self, key):
                """
                Python implementation override of Document.lookup_metadata().

                Returns string or None if not found.
                """
                e = new_pint()
                ret = lookup_metadata(self.m_internal, key, e)
                e = pint_value(e)
                if e < 0:
                    return None
                return ret

            Document.lookup_metadata = Document_lookup_metadata

            ''')

    text += swig_python

    # Make some additions to the generated Python module.
    #
    # E.g. python wrappers for functions that take out-params should return
    # tuples.
    #
    text += textwrap.dedent('''

            import re

            # Wrap parse_page_range() to fix SWIG bug where a NULL return
            # value seems to mess up the returned list - we end up with ret
            # containing two elements rather than three, e.g. [0, 2]. This
            # occurs with SWIG-3.0; maybe fixed in SWIG-4?
            #
            w_parse_page_range = parse_page_range
            def parse_page_range(s, n):
                ret = w_parse_page_range(s, n)
                if len(ret) == 2:
                    return None, 0, 0
                else:
                    return ret[0], ret[1], ret[2]

            # Provide native python implementation of format_output_path() (->
            # fz_format_output_path).
            #
            def format_output_path( format, page):
                m = re.search( '(%[0-9]*d)', format)
                if m:
                    ret = format[ :m.start(1)] + str(page) + format[ m.end(1):]
                else:
                    dot = format.rfind( '.')
                    if dot < 0:
                        dot = len( format)
                    ret = format[:dot] + str(page) + format[dot:]
                return ret

            class IteratorWrap:
                """
                This is a Python iterator for containers that have C++-style
                begin() and end() methods that return iterators.

                Iterators must have the following methods:

                    __increment__(): move to next item in the container.
                    __ref__(): return reference to item in the container.

                Must also be able to compare two iterators for equality.

                """
                def __init__( self, container):
                    self.container = container
                    self.pos = None
                    self.end = container.end()
                def __iter__( self):
                    return self
                def __next__( self):    # for python2.
                    if self.pos is None:
                        self.pos = self.container.begin()
                    else:
                        self.pos.__increment__()
                    if self.pos == self.end:
                        raise StopIteration()
                    return self.pos.__ref__()
                def next( self):    # for python3.
                    return self.__next__()

            # The auto-generated Python class methd Buffer.buffer_extract()
            # returns (size, data).
            #
            # But these raw values aren't particularly useful to Python code so
            # we change the method to return a Python bytes instance instead,
            # using the special C function buffer_storage_bytes() defined
            # above.
            #
            # We make the original method available as
            # Buffer.buffer_extract_raw(); this can be used to create a
            # mupdf.Stream by passing the raw values back to C++ with:
            #
            #   data, size = buffer_.buffer_extract_raw()
            #   stream = mupdf.Stream(data, size))
            #
            # We don't provide a similar wrapper for Buffer.buffer_storage()
            # because we can't create a Python bytes object that
            # points into the buffer'a storage. We still provide
            # Buffer.buffer_storage_raw() just in case there is a need for
            # Python code that can pass the raw (data, size) back in to C.
            #

            Buffer.buffer_extract_raw = Buffer.buffer_extract

            def Buffer_buffer_extract(self):
                """
                Returns buffer data as a Python bytes instance, leaving the
                buffer empty. Note that this will make a copy of the underlying
                data.
                """
                return buffer_extract_bytes(self.m_internal)

            Buffer.buffer_extract = Buffer_buffer_extract

            Buffer.buffer_storage_raw = Buffer.buffer_storage
            delattr(Buffer, 'buffer_storage')


            ''')

    # Add __iter__() methods for all classes with begin() and end() methods.
    #
    for classname in container_classnames:
        text += f'{classname}.__iter__ = lambda self: IteratorWrap( self)\n'

    # For all wrapper classes with a to_string() method, add a __str__()
    # method to the underlying struct's Python class, which calls
    # to_string_<structname>().
    #
    # E.g. this allows Python code to print a mupdf.fz_rect instance.
    #
    # [We could instead call our generated to_string() and rely on overloading,
    # but this will end up switching on the type in the SWIG code.]
    #
    for structname in to_string_structnames:
        text += f'{structname}.__str__ = lambda s: to_string_{structname}(s)\n'

    # For all wrapper classes with a to_string() method, add a __str__() method
    # to the Python wrapper class, which calls the class's to_string() method.
    #
    # E.g. this allows Python code to print a mupdf.Rect instance.
    #
    for structname in to_string_structnames:
        text += f'{rename.class_(structname)}.__str__ = lambda self: self.to_string()\n'

    text += '%}\n'

    if 1:   # lgtm [py/constant-conditional-expression]
        # This is a horrible hack to avoid swig failing because
        # include/mupdf/pdf/object.h defines an enum which contains a #include.
        #
        # Would like to pre-process files in advance so that swig doesn't see
        # the #include, but this breaks swig in a different way - swig cannot
        # cope with some code in system headers.
        #
        # So instead we copy include/mupdf/pdf/object.h into
        # {build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf/object.h,
        # manually expanding the #include using a simpe .replace() call. The
        # we specify {build_dirs.dir_mupdf}/platform/python/include as the
        # first include path so that our modified mupdf/pdf/object.h will get
        # included in preference to the original.
        #
        os.makedirs(f'{build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf', exist_ok=True)
        with open( f'{build_dirs.dir_mupdf}/include/mupdf/pdf/object.h') as f:
            o = f.read()
        with open( f'{build_dirs.dir_mupdf}/include/mupdf/pdf/name-table.h') as f:
            name_table_h = f.read()
        oo = o.replace( '#include "mupdf/pdf/name-table.h"\n', name_table_h)
        assert oo != o
        jlib.update_file( oo, f'{build_dirs.dir_mupdf}/platform/python/include/mupdf/pdf/object.h')

    swig_i      = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp_swig.i'
    include1    = f'{build_dirs.dir_mupdf}/include/'
    include2    = f'{build_dirs.dir_mupdf}/platform/c++/include'
    swig_cpp    = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp_swig.cpp'
    swig_py     = f'{build_dirs.dir_so}/mupdf.py'

    os.makedirs( f'{build_dirs.dir_mupdf}/platform/python', exist_ok=True)
    os.makedirs( f'{build_dirs.dir_so}', exist_ok=True)
    jlib.update_file( text, swig_i)

    line_end = '^' if g_windows else '\\'
    command = (
            textwrap.dedent(
            f'''
            "{swig}"
                -Wall
                -c++
                {" -doxygen" if swig_major >= 4 else ""}
                -{language}
                -module mupdf
                -outdir {os.path.relpath(build_dirs.dir_so)}
                -o {os.path.relpath(swig_cpp)}
                -includeall
                -I{os.path.relpath(build_dirs.dir_mupdf)}/platform/python/include
                -I{os.path.relpath(include1)}
                -I{os.path.relpath(include2)}
                -ignoremissing
                {os.path.relpath(swig_i)}
            ''').strip().replace( '\n', "" if g_windows else "\\\n")
            )

    swig_cpp_old = None
    if os.path.isfile(swig_cpp):
        swig_cpp_old = swig_cpp + '-0'
        shutil.copy2(swig_cpp, swig_cpp_old)

    rebuilt = jlib.build(
            (swig_i, include1, include2),
            (swig_cpp, swig_py),
            command,
            )
    jlib.log('{rebuilt=}')
    if rebuilt:
        if g_openbsd:
            mupdf_py_prefix = textwrap.dedent(
                    f'''
                    # Explicitly load required .so's using absolute paths, so that we
                    # work without needing LD_LIBRARY_PATH to be defined.
                    #
                    import ctypes
                    import os
                    import importlib
                    for leaf in ('libmupdf.so', 'libmupdfcpp.so', '_mupdf.so'):
                        path = os.path.abspath(f'{{__file__}}/../{{leaf}}')
                        #print(f'path={{path}}')
                        #print(f'exists={{os.path.exists(path)}}')
                        ctypes.cdll.LoadLibrary( path)
                        #print(f'have loaded {{path}}')
                    ''')
            with open( swig_py) as f:
                mupdf_py_content = mupdf_py_prefix + f.read()
            with open( swig_py, 'w') as f:
                f.write( mupdf_py_content)

        elif g_windows:
            jlib.log('Adding prefix to {swig_cpp=}')
            prefix = ''
            postfix = ''
            with open( swig_cpp) as f:
                mupdf_py_content = prefix + f.read() + postfix
            with open( swig_cpp, 'w') as f:
                f.write( mupdf_py_content)

    if swig_cpp_old:
        def read_all(path):
            with open(path) as f:
                return f.read()
        swig_cpp_old_text = read_all(swig_cpp_old)
        swig_cpp_text = read_all(swig_cpp)
        if swig_cpp_text == swig_cpp_old_text:
            jlib.log('Preserving old file mtime etc because unchanged: {swig_cpp=}')
            jlib.rename(swig_cpp_old, swig_cpp)


def build_swig_java( container_classnames):
    return build_swig( container_classnames, 'java')


def test_swig():
    '''
    For testing different swig .i constructs.
    '''
    test_i = textwrap.dedent('''
            %include argcargv.i

            %apply (int ARGC, char **ARGV) { (int retainlen, const char **retainlist) }
            %apply (int ARGC, char **ARGV) { (const char **retainlist, int retainlen) }
            %apply (int ARGC, char **ARGV) { (const char *retainlist[], int retainlen) }

            %clear double a, int ARGC, char **ARGV;
            %clear double a, int argc, char *argv[];
            %clear int ARGC, char **ARGV;
            %clear (double a, int ARGC, char **ARGV);
            %clear (double a, int argc, char *argv[]);
            %clear (int ARGC, char **ARGV);
            %clear int retainlen, const char **retainlist;

            int bar( int argc, char* argv[]);
            int foo( double a, int argc, char* argv[]);

            int qwe( double a, int argc, const char** argv);

            void ppdf_clean_file( char *infile, char *outfile, char *password, pdf_write_options *opts, int retainlen, const char **retainlist);
            void ppdf_clean_file2(char *infile, char *outfile, char *password, pdf_write_options *opts, const char **retainlist, int retainlen);
            void ppdf_clean_file3(char *infile, char *outfile, char *password, pdf_write_options *opts, const char *retainlist[], int retainlen);

            ''')
    jlib.update_file( test_i, 'test.i')

    jlib.system( textwrap.dedent(
            '''
            swig
                -Wall
                -c++
                -python
                -module test
                -outdir .
                -o test.cpp
                test.i
            ''').replace( '\n', ' \\\n')
            )

class Cpu:
    '''
    For Windows only. Paths and names that depend on cpu.

    Members:
        .bits
            .
        .windows_subdir
            '' or 'x64/', e.g. platform/win32/x64/Release.
        .windows_name
            'x86' or 'x64'.
        .windows_config
            'x64' or 'Win32', e.g. /Build Release|x64
        .windows_suffix
            '64' or '', e.g. mupdfcpp64.dll
    '''
    def __init__(self, name):
        self.name = name
        if name == 'x32':
            self.bits = 32
            self.windows_subdir = ''
            self.windows_name = 'x86'
            self.windows_config = 'Win32'
            self.windows_suffix = ''
        elif name == 'x64':
            self.bits = 64
            self.windows_subdir = 'x64/'
            self.windows_name = 'x64'
            self.windows_config = 'x64'
            self.windows_suffix = '64'
        else:
            assert 0, f'Unrecognised cpu name: {name}'

    def __str__(self):
        return self.name

def python_version():
    '''
    Returns two-digit version number of Python as a string, e.g. '3.9'.
    '''
    return '.'.join(platform.python_version().split('.')[:2])

def cpu_name():
    '''
    Returns 'x32' or 'x64' depending on Python build.
    '''
    #log(f'sys.maxsize={hex(sys.maxsize)}')
    return f'x{32 if sys.maxsize == 2**31 else 64}'

def abspath(path):
    '''
    Like os.path.absath() but converts backslashes to forward slashes; this
    simplifies things on Windows - allows us to use '/' as directory separator
    when constructing paths, which is simpler than using os.sep everywhere.
    '''
    ret = os.path.abspath(path)
    ret = ret.replace('\\', '/')
    return ret

class BuildDirs:
    '''
    Locations of various generated files.
    '''
    def __init__( self):

        # Assume we are in mupdf/scripts/.
        file_ = abspath( __file__)
        assert file_.endswith( f'/scripts/mupdfwrap.py'), \
                'Unexpected __file__=%s file_=%s' % (__file__, file_)
        dir_mupdf = abspath( f'{file_}/../../')
        assert not dir_mupdf.endswith( '/')

        # Directories used with --build.
        self.dir_mupdf = dir_mupdf

        # Directory used with --ref.
        self.ref_dir = abspath( f'{self.dir_mupdf}/mupdfwrap_ref')
        assert not self.ref_dir.endswith( '/')

        if g_windows:
            # Default build depends on the Python that we are running under.
            #
            self.set_dir_so( f'{self.dir_mupdf}/build/shared-release-{cpu_name()}-py{python_version()}')
        else:
            self.set_dir_so( f'{self.dir_mupdf}/build/shared-release')

    def set_dir_so( self, dir_so):
        '''
        Sets self.dir_so and also updates self.cpp_flags etc.
        '''
        dir_so = abspath( dir_so)
        self.dir_so = dir_so

        if 0: pass  # lgtm [py/unreachable-statement]
        elif '-debug' in dir_so:    self.cpp_flags = '-g'
        elif '-release' in dir_so:  self.cpp_flags = '-O2 -DNDEBUG'
        elif '-memento' in dir_so:  self.cpp_flags = '-g -DMEMENTO'
        else:
            self.cpp_flags = None
            log( 'Warning: unrecognised {dir_so=}, so cannot determine cpp_flags')

        # Set self.cpu and self.python_version.
        if g_windows:
            # Infer from self.dir_so.
            m = re.match( 'shared-release(-(x[0-9]+))?(-py([0-9.]+))?$', os.path.basename(self.dir_so))
            #log(f'self.dir_so={self.dir_so} {os.path.basename(self.dir_so)} m={m}')
            assert m, f'Failed to parse dir_so={self.dir_so!r} - should be *-x32|x64-pyA.B'
            self.cpu = Cpu( m.group(2))
            self.python_version = m.group(4)
            #log('{self.cpu=} {self.python_version=} {dir_so=}')
        else:
            # Use Python we are running under.
            self.cpu = Cpu(cpu_name())
            self.python_version = python_version()


def to_pickle( obj, path):
    '''
    Pickles <obj> to file <path>.
    '''
    with open( path, 'wb') as f:
        pickle.dump( obj, f)

def from_pickle( path):
    '''
    Returns contents of file <path> unpickled.
    '''
    with open( path, 'rb') as f:
        return pickle.load( f)

def cmd_run_multiple(commands, prefix=None):
    '''
    Windows-only.

    Runs multiple commands joined by &&, using cmd.exe if we are running under
    Cygwin. We cope with commands that already contain double-quote characters.
    '''
    if g_cygwin:
        command = 'cmd.exe /V /C @ ' + ' "&&" '.join(commands)
    else:
        command = ' && '.join(commands)
    jlib.system(command, verbose=1, out='log', prefix=prefix)


def find_python( cpu, version=None):
    '''
    Windows only. Finds installed Python with specific word size and version.

    cpu:
        A Cpu instance. If None, we use whatever we are running on.
    version:
        Two-digit Python version as a string such as '3.8'. If None we use
        current Python's version.

    Returns (path, version, root, cpu):

        path:
            Path of python binary.
        version:
            Version as a string, e.g. '3.9'. Same as <version> if not None,
            otherwise the inferred version.
        root:
            The parent directory of <path>; allows
            Python headers to be found, for example
            <root>/include/Python.h.
        cpu:
            A Cpu instance, same as <cpu> if not None, otherwise the inferred
            cpu.

    We parse the output from 'py -0p' to find all available python
    installations.
    '''
    assert g_windows
    if cpu is None:
        cpu = Cpu(cpu_name())
    if version is None:
        version = python_version()
    command = 'py -0p'
    log('Running: {command}')
    text = jlib.system(command, out='return')
    version_list_highest = [0]
    ret = None
    for line in text.split('\n'):
        log( '    {line}')
        m = re.match( '^ *-([0-9.]+)-((64)|(32)) +([^\\r*]+)[\\r*]*$', line)
        if not m:
            continue
        version2 = m.group(1)
        bits = int(m.group(2))
        if bits != cpu.bits or version2 != version:
            continue
        path = m.group(5).strip()
        root = path[ :path.rfind('\\')]
        if not os.path.exists(path):
            # Sometimes it seems that the specified .../python.exe does not exist,
            # and we have to change it to .../python<version>.exe.
            #
            assert path.endswith('.exe'), f'path={path!r}'
            path2 = f'{path[:-4]}{version}.exe'
            log( 'Python {path!r} does not exist; changed to: {path2!r}')
            assert os.path.exists( path2)
            path = path2

        jlib.log('{cpu=} {version=}: returning {path=} {version=} {root=} {cpu=}')
        return path, version, root, cpu

    raise Exception( f'Failed to find python matching cpu={cpu}. Run "py -0p" to see available pythons')


def main():

    global g_show_details

    # Set default build directory. Can br overridden by '-d'.
    #
    build_dirs = BuildDirs()

    # Set default swig.
    #
    swig = 'swig'

    args = jlib.Args( sys.argv[1:])
    while 1:
        try:
            arg = args.next()
        except StopIteration:
            break
        #log( 'Handling {arg=}')

        with jlib.LogPrefixScope( f'{arg}: '):

            if arg == '-h' or arg == '--help':
                print( __doc__)

            elif arg == '--build' or arg == '-b':
                cpp_files   = [
                        f'{build_dirs.dir_mupdf}/platform/c++/implementation/classes.cpp',
                        f'{build_dirs.dir_mupdf}/platform/c++/implementation/exceptions.cpp',
                        f'{build_dirs.dir_mupdf}/platform/c++/implementation/functions.cpp',
                        f'{build_dirs.dir_mupdf}/platform/c++/implementation/internal.cpp',
                        ]
                h_files = [
                        f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/classes.h',
                        f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/exceptions.h',
                        f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/functions.h',
                        f'{build_dirs.dir_mupdf}/platform/c++/include/mupdf/internal.h',
                        ]
                container_classnames = None
                output_param_fns = None
                force_rebuild = False
                header_git = False
                swig_c = None
                swig_python = None
                g_show_details = lambda name: False
                jlib.log('{build_dirs.dir_so=}')

                while 1:
                    actions = args.next()
                    if actions == '-f':
                        force_rebuild = True
                    elif actions == '-d':
                        d = args.next()
                        g_show_details = lambda name: d in name
                    elif actions.startswith( '-'):
                        raise Exception( f'Unrecognised --build flag: {actions}')
                    else:
                        break

                if actions == 'all':
                    actions = '0123' if g_windows else 'm0123'

                for action in actions:
                    with jlib.LogPrefixScope( f'{action}: '):
                        jlib.log( '{action=}')
                        if 0:
                            pass

                        elif action == 'm':
                            # Build libmupdf.so.
                            jlib.log( 'Building libmupdf.so ...')
                            assert not g_windows, 'Cannot do "-b m" on Windows; C library is integrated into C++ library built by "-b 01"'
                            log( '{build_dirs.dir_mupdf=}')
                            make = 'make'
                            if g_openbsd:
                                # Need to run gmake, not make. Also for some
                                # reason gmake on OpenBSD sets CC to clang, but
                                # CXX to g++, so need to force CXX=clang++ too.
                                #
                                make = 'CXX=clang++ gmake'

                            command = f'cd {build_dirs.dir_mupdf} && {make} HAVE_GLUT=no HAVE_PTHREAD=yes shared=yes verbose=yes'
                            #command += ' USE_SYSTEM_FREETYPE=yes USE_SYSTEM_ZLIB=yes'
                            prefix = f'{build_dirs.dir_mupdf}/build/shared-'
                            assert build_dirs.dir_so.startswith(prefix), f'build_dirs.dir_so={build_dirs.dir_so} prefix={prefix}'
                            flags = build_dirs.dir_so[ len(prefix): ]
                            assert not flags.endswith('/')
                            #if flags.endswith('/'):    flags = flags[:-1]
                            flags = flags.split('-')
                            assert prefix.endswith('-')
                            actual_build_dir = prefix[:-1]
                            for flag in flags:
                                if flag in ('x32', 'x64') or flag.startswith('py'):
                                    # setup.py puts cpu and python version
                                    # elements into the build directory name
                                    # when creating wheels; we need to ignore
                                    # them.
                                    pass
                                else:
                                    if 0: pass  # lgtm [py/unreachable-statement]
                                    elif flag == 'debug':   command += ' build=debug'
                                    elif flag == 'release': command += ' build=release'
                                    elif flag == 'memento': command += ' build=memento'
                                    else:
                                        raise Exception(f'Unrecognised flag {flag!r} in {flags!r} in {build_dirs.dir_so!r}')
                                    actual_build_dir += f'-{flag}'

                            jlib.system( command, prefix=jlib.log_text(), out='log', verbose=1)

                            if actual_build_dir != build_dirs.dir_so:
                                # This happens when we are being run by
                                # setup.py - it it might specify '-d
                                # build/shared-release-x64-py3.8' (which
                                # will be put into build_dirs.dir_so) but
                                # the above 'make' command will create
                                # build/shared-release/libmupdf.so, so we need
                                # to copy into build/shared-release-x64-py3.8/.
                                #
                                jlib.copy( f'{actual_build_dir}/libmupdf.so', f'{build_dirs.dir_so}/libmupdf.so', verbose=1)

                        elif action == '0':
                            # Generate C++ code that wraps the fz_* API.
                            jlib.log( 'Generating C++ source code ...')
                            if not clang:
                                raise Exception('Cannot do "-b 0" because failed to import clang.')
                            namespace = 'mupdf'
                            swig_c = io.StringIO()
                            swig_python = io.StringIO()

                            (
                                    tu,
                                    base,
                                    h_files_actual,
                                    cpp_files_actual,
                                    fn_usage_filename,
                                    container_classnames,
                                    to_string_structnames,
                                    fn_usage,
                                    output_param_fns,
                                    c_functions,
                                    c_globals,

                            ) = cpp_source(
                                    build_dirs.dir_mupdf,
                                    namespace,
                                    f'{build_dirs.dir_mupdf}/platform/c++',
                                    header_git,
                                    swig_c,
                                    swig_python,
                                    )

                            to_pickle( container_classnames,    f'{build_dirs.dir_mupdf}/platform/c++/container_classnames.pickle')
                            to_pickle( to_string_structnames,   f'{build_dirs.dir_mupdf}/platform/c++/to_string_structnames.pickle')
                            to_pickle( swig_c.getvalue(),       f'{build_dirs.dir_mupdf}/platform/c++/swig_c.pickle')
                            to_pickle( swig_python.getvalue(),  f'{build_dirs.dir_mupdf}/platform/c++/swig_python.pickle')
                            to_pickle( c_functions,             f'{build_dirs.dir_mupdf}/platform/c++/c_functions.pickle')
                            to_pickle( c_globals,               f'{build_dirs.dir_mupdf}/platform/c++/c_globals.pickle')

                            def check_lists_equal(name, expected, actual):
                                expected.sort()
                                actual.sort()
                                if expected != actual:
                                    text = f'Generated {name} filenames differ from expected:\n'
                                    text += '    expected ({len(expected)}:\n'
                                    for i in expected:
                                        text += f'        {i}\n'
                                    text += '    generated ({len(actual)}:\n'
                                    for i in actual:
                                        text += f'        {i}\n'
                                    raise Exception(text)
                            check_lists_equal('C++ source', cpp_files, cpp_files_actual)
                            check_lists_equal('C++ headers', h_files, h_files_actual)

                            for dir_ in (
                                    f'{build_dirs.dir_mupdf}/platform/c++/implementation/',
                                    f'{build_dirs.dir_mupdf}/platform/c++/include/', '.h',
                                    ):
                                for path in jlib.get_filenames( dir_):
                                    path = path.replace('\\', '/')
                                    _, ext = os.path.splitext( path)
                                    if ext not in ('.h', '.cpp'):
                                        continue
                                    if path in h_files + cpp_files:
                                        continue
                                    log( 'Removing unknown C++ file: {path}')
                                    os.remove( path)

                            jlib.log( 'Wrapper classes that are containers: {container_classnames=}')

                            # Output info about fz_*() functions that we don't make use
                            # of in class methods.
                            #
                            # This is superceded by automatically finding fuctions to wrap.
                            #
                            if 0:   # lgtm [py/unreachable-statement]
                                log( 'functions that take struct args and are not used exactly once in methods:')
                                num = 0
                                for name in sorted( fn_usage.keys()):
                                    if name in omit_class_names:
                                        continue
                                    n, cursor = fn_usage[ name]
                                    if n == 1:
                                        continue
                                    if not fn_has_struct_args( tu, cursor):
                                        continue
                                    log( '    {n} {cursor.displayname} -> {cursor.result_type.spelling}')
                                    num += 1
                                log( 'number of functions that we should maybe add wrappers for: {num}')

                        elif action == '1':
                            # Compile and link generated C++ code to create libmupdfcpp.so.
                            if g_windows:
                                # We build mupdfcpp.dll using the .sln; it will
                                # contain all C functions internally - there is
                                # no mupdf.dll.
                                #
                                log(f'Building mupdfcpp.dll by running devenv ...')
                                command = (
                                        f'"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/Common7/IDE/devenv.com"'
                                        f' platform/win32/mupdf.sln'
                                        f' /Build "ReleasePython|{build_dirs.cpu.windows_config}"'
                                        f' /Project mupdfcpp'
                                        )
                                jlib.system(command, verbose=1, out='log')

                                jlib.copy(
                                        f'platform/win32/{build_dirs.cpu.windows_subdir}Release/mupdfcpp{build_dirs.cpu.windows_suffix}.dll',
                                        f'{build_dirs.dir_so}/',
                                        verbose=1,
                                        )

                            else:
                                jlib.log( 'Compiling generated C++ source code to create libmupdfcpp.so ...')
                                out_so = f'{build_dirs.dir_mupdf}/platform/c++/libmupdfcpp.so'
                                if build_dirs.dir_so:
                                    out_so = f'{build_dirs.dir_so}/libmupdfcpp.so'

                                mupdf_so = f'{build_dirs.dir_so}/libmupdf.so'

                                include1 = f'{build_dirs.dir_mupdf}/include'
                                include2 = f'{build_dirs.dir_mupdf}/platform/c++/include'
                                command = ( textwrap.dedent(
                                        f'''
                                        c++
                                            -o {out_so}
                                            {build_dirs.cpp_flags}
                                            -fPIC
                                            -shared
                                            -I {include1}
                                            -I {include2}
                                            {" ".join(cpp_files)}
                                            {jlib.link_l_flags(mupdf_so)}
                                        ''').strip().replace( '\n', ' \\\n')
                                        )
                                jlib.build(
                                        [include1, include2] + cpp_files,
                                        [out_so],
                                        command,
                                        force_rebuild,
                                        )

                        elif action == '2':
                            # Generate C++ code for python module using SWIG.
                            jlib.log( 'Generating Python module source code using SWIG ...')
                            if not os.path.isfile(f'{build_dirs.dir_mupdf}/platform/c++/container_classnames.pickle'):
                                raise Exception( 'action "0" required')
                            with jlib.LogPrefixScope( f'swig: '):
                                build_swig(
                                        build_dirs,
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/container_classnames.pickle'),
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/to_string_structnames.pickle'),
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/swig_c.pickle'),
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/swig_python.pickle'),
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/c_functions.pickle'),
                                        from_pickle( f'{build_dirs.dir_mupdf}/platform/c++/c_globals.pickle'),
                                        swig=swig,
                                        )

                        elif action == 'j':
                            # Just experimenting.
                            build_swig_java()

                        elif action == '3':
                            # Compile and link mupdfcpp_swig.cpp to create _mupdf.so.
                            #
                            jlib.log( 'Compiling/linking generated Python module source code to create _mupdf.so ...')

                            if g_windows:
                                python_path, python_version, python_root, cpu = find_python(
                                        build_dirs.cpu,
                                        build_dirs.python_version,
                                        )
                                log( 'best python for {build_dirs.cpu=}: {python_path=} {python_version=}')

                                py_root = python_root.replace('\\', '/')
                                env_extra = {
                                        'MUPDF_PYTHON_INCLUDE_PATH': f'{py_root}/include',
                                        'MUPDF_PYTHON_LIBRARY_PATH': f'{py_root}/libs',
                                        }
                                jlib.log('{env_extra=}')

                                # The swig-generated .cpp file must exist at
                                # this point.
                                #
                                cpp_path = 'platform/python/mupdfcpp_swig.cpp'
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

                                jlib.log('Building mupdfpyswig project')
                                command = (
                                        f'"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/Common7/IDE/devenv.com"'
                                        f' platform/win32/mupdfpyswig.sln'
                                        f' /Build "ReleasePython|{build_dirs.cpu.windows_config}"'
                                        f' /Project mupdfpyswig'
                                        )
                                jlib.system(command, verbose=1, out='log', env_extra=env_extra)

                                jlib.copy(
                                        f'platform/win32/{build_dirs.cpu.windows_subdir}Release/mupdfpyswig.dll',
                                        f'{build_dirs.dir_so}/_mupdf.pyd',
                                        verbose=1,
                                        )

                            elif g_windows:
                                # We run Windows compiler and linker manually;
                                # could probably add a project to mupdf.sln
                                # instead and build it with devenv.exe, but
                                # would have to hack things to pass in the
                                # appropriate python directory.
                                #
                                python_path, python_version, python_root, cpu = find_python(
                                        build_dirs.cpu,
                                        build_dirs.python_version,
                                        )
                                log( 'best python for {build_dirs.cpu=}: {python_path=} {python_version=}')

                                vcvars = f'C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars{build_dirs.cpu.bits}.bat'
                                vs_root = f'C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.28.29910/bin/Hostx64/{build_dirs.cpu.windows_name}'

                                command = (
                                        f'"{vs_root}/cl.exe"'
                                        #f' /nologo'
                                        f' /D "FZ_DLL_CLIENT"'  # Activates __declspec() in headers.
                                        f' /D "NDEBUG"'
                                        f' /D "UNICODE"'
                                        f' /D "WIN{build_dirs.cpu.bits}"'
                                        f' /D "_UNICODE"'
                                        f' /EHsc'               # Enable C++ exceptions.
                                        f' /FC'                 # Display full path of source code files passed to cl.exe in diagnostic text.
                                        f' /Faplatform/win32/{build_dirs.cpu.windows_subdir}Release/'       # Sets the listing file name.
                                        f' /Fdplatform/win32/{build_dirs.cpu.windows_subdir}Release/'       # Specifies a file name for the program database (PDB) file
                                        f' /Fo"platform/win32/{build_dirs.cpu.windows_subdir}Release/mupdfcpp_swig.obj"'        # Name of generated object file.
                                        f' /Fp"Release/_mupdf.pch"' # Specifies a precompiled header file name.
                                        f' /GL'                 # Enables whole program optimization.
                                        f' /GS'                 # Buffers security check.
                                        f' /Gd'                 # Uses the __cdecl calling convention (x86 only).
                                        f' /Gm-'                # Deprecated. Enables minimal rebuild.
                                        f' /Gy'                 # Enables function-level linking.
                                        f' /MD'                 # Creates a multithreaded DLL using MSVCRT.lib.
                                        f' /O2'
                                        f' /Oi'
                                        f' /Oy-'                # Omits frame pointer (x86 only).
                                        f' /W3'                 # Sets which warning level to output.
                                        f' /WX-'                # Treats all warnings as errors.
                                        f' /Zc:forScope'
                                        f' /Zc:inline'
                                        f' /Zc:wchar_t'
                                        f' /Zi'
                                        f' /analyze-'           # Enable code analysis.
                                        f' /c'                  # Compiles without linking.
                                        f' /diagnostics:column' # Controls the format of diagnostic messages.
                                        f' /errorReport:prompt' # Deprecated. Error reporting is controlled by Windows Error Reporting (WER) settings.
                                        f' /fp:precise'         # Specify floating-point behavior.
                                        f' /permissive-'        # Set standard-conformance mode.
                                        f' /sdl'                # Enables additional security features and warnings.
                                        rf' /I"{python_root}/include"'
                                        rf' /I"include"'
                                        rf' /I"platform/c++/include"'
                                        f' platform/python/mupdfcpp_swig.cpp'
                                        )
                                log('Compiling SWIG-generated code.')
                                cmd_run_multiple( [f'"{vcvars}"', command], prefix='cl.exe: ')

                                # Link.
                                command = (
                                        f'"{vs_root}/link.exe"'
                                        #f' /NOLOGO'
                                        f' /DLL'
                                        f' /DYNAMICBASE'        # Specifies whether to generate an executable image that's rebased at load time by using the address space layout randomization (ASLR) feature.
                                        f' /ERRORREPORT:PROMPT' # Deprecated. Error reporting is controlled by Windows Error Reporting (WER) settings.
                                        f' /IMPLIB:"platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.lib"'
                                        f' /INCREMENTAL:NO'     # Controls incremental linking.
                                        f' /LIBPATH:"platform/win32/{build_dirs.cpu.windows_subdir}Release"'
                                        f' /LIBPATH:"{python_root}/libs"'
                                        f' /LTCG:incremental'
                                        f' /LTCGOUT:"platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.iobj"'
                                        f' /MACHINE:{build_dirs.cpu.windows_name.upper()}'   # Specifies the target platform.
                                        f' /MANIFEST'           # Creates a side-by-side manifest file and optionally embeds it in the binary.
                                        f' /MANIFESTUAC:NO'     # Specifies whether User Account Control (UAC) information is embedded in the program manifest.
                                        f' /ManifestFile:"platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.dll.intermediate.manifest"'
                                        f' /NXCOMPAT'           # Marks an executable as verified to be compatible with the Windows Data Execution Prevention feature.
                                        f' /OPT:ICF'
                                        f' /OPT:REF'
                                        f' /OUT:"platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.dll"'
                                        f' /PDB:"platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.pdb"'
                                        f' {"/SAFESEH" if build_dirs.cpu.bits==32 else ""}'    # Not supported on x64.
                                        f' /SUBSYSTEM:WINDOWS'  # Tells the operating system how to run the .exe file.
                                        f' /TLBID:1'            # A user-specified value for a linker-created type library. It overrides the default resource ID of 1.
                                        f' "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib"'
                                        f' "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib"'
                                        #f' python3.lib'         # not needed because on Windows Python.h has info about library.
                                        f' platform/win32/{build_dirs.cpu.windows_subdir}Release/mupdfcpp_swig.obj'
                                        f' mupdfcpp.lib'
                                        )
                                log('Linking SWIG-generated code.')
                                cmd_run_multiple( [f'"{vcvars}"', command], prefix='link.exe: ')

                                # To be found at runtime, we need to rename _mupdf.dll to _mupdf.pyd. As usual, totally
                                # undocumented. Discovered this at:
                                # https://blog.schuetze.link/2018/07/21/a-dive-into-packaging-native-python-extensions.html
                                #
                                jlib.copy(
                                        f'platform/win32/{build_dirs.cpu.windows_subdir}Release/_mupdf.dll',
                                        f'{build_dirs.dir_so}/_mupdf.pyd',
                                        verbose=1,
                                        )

                            else:
                                # We use g++ debug/release flags as implied by
                                # --dir-so, but all builds output the same file
                                # mupdf:platform/python/_mupdf.so. We could instead
                                # generate mupdf.py and _mupdf.so in the --dir-so
                                # directory?
                                #
                                # [While libmupdfcpp.so requires matching
                                # debug/release build of libmupdf.so, it looks
                                # like _mupdf.so does not require a matching
                                # libmupdfcpp.so and libmupdf.sp.]
                                #

                                if 1:
                                    # We use python-config which appears to
                                    # work better than pkg-config because
                                    # it copes with multiple installed
                                    # python's, e.g. manylinux_2014's
                                    # /opt/python/cp*-cp*/bin/python*.
                                    #
                                    # But... it seems that we should not
                                    # attempt to specify libpython on the link
                                    # command. The manylinkux docker containers
                                    # don't actually contain libpython.so, and
                                    # it seems that this deliberate. And the
                                    # link command runs ok.
                                    #
                                    python_exe = os.path.realpath( sys.executable)
                                    python_config = f'{python_exe}-config'
                                    # --cflags gives things like
                                    # -Wno-unused-result -g etc, so we just use
                                    # --includes.
                                    python_includes = jlib.system( f'{python_config} --includes', out='return')
                                    #python_link     = jlib.system( f'{python_config} --ldflags', out='return')
                                    python_link = ''
                                    libpython_so    = None
                                else:
                                    # Use pkg-config to find compile/link flags for building with python.
                                    python_includes = jlib.system( 'pkg-config --cflags python3', out='return')
                                    python_link     = jlib.system( 'pkg-config --libs python3', out='return')
                                    libpython_so    = None

                                # These are the input files to our g++ command:
                                #
                                mupdfcpp_swig_cpp   = f'{build_dirs.dir_mupdf}/platform/python/mupdfcpp_swig.cpp'
                                include1            = f'{build_dirs.dir_mupdf}/include'
                                include2            = f'{build_dirs.dir_mupdf}/platform/c++/include'

                                mupdf_so            = f'{build_dirs.dir_so}/libmupdf.so'
                                mupdfcpp_so         = f'{build_dirs.dir_so}/libmupdfcpp.so'

                                # Python expects _mupdf.so to be in same directory as mupdf.py.
                                #
                                out_so              = f'{build_dirs.dir_so}/_mupdf.so'

                                # We use jlib.link_l_flags() to add -L options
                                # to search parent directories of each .so that
                                # we need, and -l with the .so leafname without
                                # leading 'lib' or trailing '.so'. This ensures
                                # that at runtime one can set LD_LIBRARY_PATH to
                                # parent directories and have everything work.
                                #
                                command = ( textwrap.dedent(
                                        f'''
                                        c++
                                            -o {out_so}
                                            {build_dirs.cpp_flags}
                                            -fPIC
                                            --shared
                                            -I {include1}
                                            -I {include2}
                                            {python_includes}
                                            {mupdfcpp_swig_cpp}
                                            {jlib.link_l_flags( [mupdf_so, mupdfcpp_so, libpython_so])}
                                            {python_link}
                                        ''').strip().replace( '\n', ' \\\n').strip()
                                        )
                                jlib.build(
                                        ( include1, include2, mupdfcpp_swig_cpp, mupdf_so, mupdfcpp_so,),
                                        ( out_so,),
                                        command,
                                        force_rebuild,
                                        )
                        else:
                            raise Exception( 'unrecognised --build action %r' % action)

            elif arg == '--compare-fz_usage':
                directory = args.next()
                compare_fz_usage( tu, directory, fn_usage)

            elif arg == '--diff':
                for path in jlib.get_filenames( build_dirs.ref_dir):
                    #log( '{path=}')
                    assert path.startswith( build_dirs.ref_dir)
                    if not path.endswith( '.h') and not path.endswith( '.cpp'):
                        continue
                    tail = path[ len( build_dirs.ref_dir):]
                    path2 = f'{build_dirs.dir_mupdf}/platform/c++/{tail}'
                    command = f'diff -u {path} {path2}'
                    log( 'running: {command}')
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

                def do_doxygen( name, outdir, path):
                    '''
                    name:
                        Doxygen PROJECT_NAME of generated documentation
                    outdir:
                        Directory in which we run doxygen, so root of generated
                        documentation will be in <outdir>/html/index.html
                    path:
                        Doxygen INPUT setting; this is the path relative to
                        <outdir> of the directory which contains the API to
                        document.
                    '''
                    # We generate a blank doxygen configuration file, make
                    # some minimal changes, then run doxygen on the modified
                    # configuration.
                    #
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
                    log( 'running: {command}')
                    jlib.system( command, out='return')
                    log( 'have created: {outdir}/html/index.html')

                langs = args.next()
                if langs == 'all':
                    langs = 'c,c++,python'

                langs = langs.split( ',')

                for lang in langs:
                    if lang == 'c':
                        do_doxygen( 'mupdf', 'include', 'mupdf')

                    elif lang == 'c++':
                        do_doxygen( 'mupdfcpp', 'platform/c++/include', 'mupdf')

                    elif lang == 'python':
                        ld_library_path = os.path.abspath( f'{build_dirs.dir_so}')
                        pythonpath = build_dirs.dir_so
                        jlib.system(
                                f'cd {build_dirs.dir_so}; LD_LIBRARY_PATH={ld_library_path} PYTHONPATH={pythonpath} pydoc3 -w ./mupdf.py',
                                out='log',
                                )
                        path = f'{build_dirs.dir_so}/mupdf.html'
                        assert os.path.isfile( path)
                        log( 'have created: {path}')

                    else:
                        raise Exception( f'unrecognised language param: {lang}')

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
                log('Have set {build_dirs=}')

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
                log( 'running: {command}')
                e = jlib.system(
                        command,
                        raise_errors=False,
                        verbose=False,
                        out='log',
                        )
                sys.exit(e)

            elif arg == '--swig':
                swig = args.next()

            elif arg == '--swig-windows-auto':
                import stat
                import urllib.request
                import zipfile
                name = 'swigwin-4.0.2'

                # Download swig .zip file if not already present.
                #
                if not os.path.exists(f'{name}.zip'):
                    url = f'http://prdownloads.sourceforge.net/swig/{name}.zip'
                    log(f'Downloading Windows SWIG from: {url}')
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
                    jlib.ensure_empty_dir(f'{name}-0')
                    z.extractall(f'{name}-0')
                    os.rename(f'{name}-0/{name}', name)
                    os.rmdir(f'{name}-0')

                    # Need to make swig.exe executable.
                    swig_local_stat = os.stat(swig_local)
                    os.chmod(swig_local, swig_local_stat.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

                # Set our <swig> to be the local windows swig.exe.
                #
                swig = swig_local

            elif arg == '--sync':
                sync_docs = False
                destination = args.next()
                if destination == '-d':
                    sync_docs = True
                    destination = args.next()
                log( 'Syncing to {destination=}')
                files = list( hs) + list( cpps) + [
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
                    log( 'converting {i} to {o}')
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

                if sync_docs:
                    files += [
                            f'{build_dirs.dir_mupdf}/include/html/',
                            f'{build_dirs.dir_mupdf}/platform/c++/include/html/',
                            f'{build_dirs.dir_so}/mupdf.html',
                            ]

                # Insert extra './' into each path so that rsync -R uses the
                # 'mupdf/...' tail of each local path for the remote path.
                #
                for i in range( len( files)):
                    files[i] = files[i].replace( '/mupdf/', '/./mupdf/')

                jlib.system( f'rsync -aiRz {" ".join( files)} {destination}', verbose=1, out='log')

            elif arg == '--test' or arg == '-t':

                # We need to set LD_LIBRARY_PATH and PYTHONPATH so that our
                # test .py programme can load mupdf.py and _mupdf.so.
                env_extra = {}
                command_prefix = ''
                log('{build_dirs=}')
                if g_windows:
                    # On Windows, it seems that 'py' runs the default
                    # python. Also, Windows appears to be able to find
                    # _mupdf.pyd in same directory as mupdf.py.
                    #
                    python_path, python_version, python_root, cpu = find_python( build_dirs.cpu, build_dirs.python_version)
                    python_path = python_path.replace('\\', '/')    # Allows use on Cygwin.
                    env_extra = {
                            'PYTHONPATH': os.path.relpath(build_dirs.dir_so),
                            }
                    command_prefix = f'"{python_path}"'
                elif g_openbsd:
                    # We have special support to not require LD_LIBRARY_PATH.
                    #command_prefix = f'PYTHONPATH={os.path.relpath(build_dirs.dir_so)}'
                    env_extra = {
                            'PYTHONPATH': os.path.relpath(build_dirs.dir_so)
                            }
                else:
                    # On Linux it looks like we need to specify
                    # LD_LIBRARY_PATH. fixme: revisit this because these days
                    # jlib.y uses rpath when constructing link commands.
                    #
                    env_extra = {
                            'LD_LIBRARY_PATH': os.path.abspath(build_dirs.dir_so),
                            'PYTHONPATH': os.path.relpath(build_dirs.dir_so),
                            }
                    #command_prefix = f'LD_LIBRARY_PATH={os.path.abspath(build_dirs.dir_so)} PYTHONPATH={os.path.relpath(build_dirs.dir_so)}'

                log( 'running mupdf_test.py...')
                command = f'{command_prefix} ./scripts/mupdfwrap_test.py'
                with open( f'{build_dirs.dir_mupdf}/platform/python/mupdf_test.py.out.txt', 'w') as f:
                    jlib.system( command, env_extra=env_extra, out='log', verbose=1)

                # Run mutool.py.
                #
                mutool_py = os.path.relpath( f'{__file__}/../mutool.py')
                zlib_pdf = os.path.relpath(f'{build_dirs.dir_mupdf}/thirdparty/zlib/zlib.3.pdf')
                for args2 in (
                        f'trace {zlib_pdf}',
                        f'convert -o zlib.3.pdf-%d.png {zlib_pdf}',
                        f'draw -o zlib.3.pdf-%d.png -s tmf -v -y l -w 150 -R 30 -h 200 {zlib_pdf}',
                        f'draw -o zlib.png -R 10 {zlib_pdf}',
                        f'clean -gggg -l {zlib_pdf} zlib.clean.pdf',
                        ):
                    command = f'{command_prefix} {mutool_py} {args2}'
                    log( 'running: {command}')
                    jlib.system( f'{command}', env_extra=env_extra, out='log', verbose=1)

                log( 'Tests ran ok.')

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
                test_swig()

            elif arg == '--venv':
                venv = args.next()
                args_tail = ''
                while 1:
                    try:
                        args_tail += ' ' + args.next()
                    except StopIteration:
                        break
                commands = (
                        f'"{sys.executable}" -m venv {venv}',
                        f'{venv}\\Scripts\\activate.bat',
                        # Upgrading pip seems to fail on some Windows systems,
                        # even when retrying after first failure.
                        #f'(pip install --upgrade pip || pip install --upgrade pip)',
                        f'pip install libclang',
                        f'python {sys.argv[0]} {args_tail}',
                        f'deactivate',
                        )
                command = '&&'.join(commands)
                jlib.system(command, out='log', verbose=1)

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


if __name__ == '__main__':
    jlib.force_line_buffering()
    try:
        main()
    except Exception:
        sys.stderr.write(jlib.exception_info())
        sys.exit(1)
