#!/usr/bin/env python3

'''
Installation script for MuPDF Python bindings, using scripts/pipcl.py.

Notes:

    When building an sdist (e.g. with 'pip sdist'), we use clang-python to
    generate C++ source which is then included in the sdist.

    Thus when we are in an sdist and are installing or building a wheel, we
    don't need clang-python .


Internal testing only - environmental variables:

    MUPDF_SETUP_BUILD_DIR
        Overrides the default build directory.

    MUPDF_SETUP_USE_CLANG_PYTHON
        Affects whether we use clang-python when building.

        If set, must be '0' or '1', and we override the default and do not
        ('0') / do ('1') use clang-python to generate C++ source code from
        MuPDF headers.

        If we are an sdist we default to not re-generating C++ - the generated
        files will be already available in platform/c++/. Otherwise we default
        to generating C++ source code.

    MUPDF_SETUP_USE_SWIG
        If set, must be '0' or '1', and we do not ('0') / do ('1') attempt to
        run swig.
'''

import os
import platform
import re
import subprocess
import sys
import sys
import time


def log(text=''):
    for line in text.split('\n'):
        print(f'mupdf:setup.py: {line}')
    sys.stdout.flush()

def cache(function):
    '''
    Simple (and probably unnecessary) caching decorator.
    '''
    cache = {}
    def wrapper(*args):
        if not args in cache:
            cache[args] = function()
        return cache[args]
    return wrapper

@cache
def root_dir():
    return os.path.dirname(os.path.abspath(__file__))

@cache
def windows():
    s = platform.system()
    return s == 'Windows' or s.startswith('CYGWIN')

@cache
def build_dir():
    # This is x86/x64-specific.
    #
    # We generate 32 or 64-bit binaries to match whatever Python we
    # are running under.
    #
    ret = os.environ.get('MUPDF_SETUP_BUILD_DIR')
    if ret is None:
        cpu = 'x32' if sys.maxsize == 2**31 - 1 else 'x64'
        python_version = '.'.join(platform.python_version().split('.')[:2])
        ret = f'{root_dir()}/build/shared-release-{cpu}-py{python_version}'
    return ret

@cache
def in_sdist():
    return os.path.exists(f'{root_dir()}/PKG-INFO')

sys.path.append(f'{root_dir()}/scripts')
import pipcl


@cache
def mupdf_version():
    '''
    Returns version string.

    If $MUPDF_SETUP_VERSION is set we use it directly, asserting that it starts
    with the version string defined in include/mupdf/fitz/version.h.

    Otherwise if we are in an sdist ('PKG-INFO' exists) we use its
    version. We assert that this starts with the base version in
    include/mupdf/fitz/version.h.

    Otherwise we generate a version string by appending the current date and
    time to the base version in include/mupdf/fitz/version.h. For example
    '1.18.0.20210330.1800'.
    '''
    with open(f'{root_dir()}/include/mupdf/fitz/version.h') as f:
        text = f.read()
    m = re.search('\n#define FZ_VERSION "([^"]+)"\n', text)
    assert m
    base_version = m.group(1)

    # If MUPDF_SETUP_VERSION exists, use it.
    #
    ret = os.environ.get('MUPDF_SETUP_VERSION')
    if ret:
        log(f'Using version from $MUPDF_SETUP_VERSION: {ret}')
        assert ret.startswith(base_version)
        return ret

    # If we are in an sdist, so use the version from the PKG-INFO file.
    #
    if in_sdist():
        items = pipcl.parse_pkg_info('PKG-INFO')
        assert items['Name'] == 'mupdf'
        ret = items['Version']
        #log(f'Using version from PKG-INFO: {ret}')
        assert ret.startswith(base_version)
        return ret

    # If we get here, we are in a source tree.
    #
    # We use the MuPDF version with a unique(ish) suffix based on the current
    # date and time, so we can make multiple Python releases without requiring
    # an increment to the MuPDF version.
    #
    # This also allows us to easily experiment on test.pypi.org.
    #
    ret = base_version + time.strftime(".%Y%m%d.%H%M")
    #log(f'Have created version number: {ret}')
    return ret


# Set MUPDF_SETUP_MINIMAL=1 to create minimal package for testing
# upload/download etc, avoiding compilation when installing.
#
g_test_minimal = int(os.environ.get('MUPDF_SETUP_MINIMAL', '0'))


def git_info():
    '''
    Returns (current, origin, diff):
        current: git id from 'git show'.
        origin: git id from 'git show origin'.
        diff: diff relative to current.
    '''
    def get_id(command):
        text = subprocess.check_output(command, shell=True, cwd=root_dir())
        text = text.decode('utf8')
        text = text.split('\n', 1)[0]
        text = text.split(' ', 1)[0]
        return text
    current = get_id('git show --pretty=oneline')
    origin = get_id('git show --pretty=oneline origin')
    command = ''
    diff = subprocess.check_output(f'cd {root_dir()} && git diff', shell=True).decode('utf8')
    return current, origin, diff


def get_flag(name, default):
    '''
    name:
        Name of environmental variable.
    default:
        Value to return if <name> undefined.
    Returns False if name is '0', True if name is '1', <default> if
    undefined. Otherwise assert fails.
    '''
    value = os.environ.get(name)
    if value is None:
        ret = default
    elif value == '0':
        ret = False
    elif value == '1':
        ret = True
    else:
        assert 0, f'If set, ${name} must be "0" or "1", but is: {value!r}'
    log(f'name={name} default={default} value={value} ret={ret}')
    return ret


# pipcl Callbacks.
#

def sdist():
    '''
    pipcl callback. If we are a git checkout, return all files known to
    git. Otherwise return all files except for those in build/.
    '''
    # Create 'git-info' file containing git ids that identify this tree. For
    # the moment this is a simple text format, but we could possibly use pickle
    # instead, depending on whether we want to include more information, e.g.
    # diff relative to origin.
    #
    git_id, git_id_origin, git_diff = git_info()
    with open(f'{root_dir()}/git-info', 'w') as f:
        f.write(f'git-id: {git_id}\n')
        f.write(f'git-id-origin: {git_id_origin}\n')
        f.write(f'git-diff:\n{git_diff}\n')

    if g_test_minimal:
        # Cut-down for testing.
        return [
                'setup.py',
                'pyproject.toml',
                'scripts/pipcl.py',
                'include/mupdf/fitz/version.h',
                'COPYING',
                'git-info',
                ]

    if not os.path.exists(f'{root_dir()}/.git'):
        raise Exception(f'Cannot make sdist because not a git checkout')
    paths = pipcl.git_items( root_dir(), submodules=True)

    # Build C++ files and SWIG C code for inclusion in sdist, so that it can be
    # used on systems without clang-python or SWIG.
    #
    use_clang_python = get_flag('MUPDF_SETUP_USE_CLANG_PYTHON', True)
    use_swig = get_flag('MUPDF_SETUP_USE_SWIG', True)
    b = ''
    if use_clang_python:
        b += '0'
    if use_swig:
        b += '2'
    extra = ' --swig-windows-auto' if windows() else ''
    command = '' if os.getcwd() == root_dir() else f'cd {os.path.relpath(root_dir())} && '
    command += f'{sys.executable} ./scripts/mupdfwrap.py{extra} -d {build_dir()} -b "{b}"'
    log(f'Running: {command}')
    subprocess.check_call(command, shell=True)
    paths += [
            'build/shared-release/mupdf.py',
            'git-info',
            'platform/c++/c_functions.pickle',
            'platform/c++/c_globals.pickle',
            'platform/c++/container_classnames.pickle',
            'platform/c++/implementation/classes.cpp',
            'platform/c++/implementation/exceptions.cpp',
            'platform/c++/implementation/functions.cpp',
            'platform/c++/implementation/internal.cpp',
            'platform/c++/include/mupdf/classes.h',
            'platform/c++/include/mupdf/exceptions.h',
            'platform/c++/include/mupdf/functions.h',
            'platform/c++/include/mupdf/internal.h',
            'platform/c++/swig_c.pickle',
            'platform/c++/swig_python.pickle',
            'platform/c++/to_string_structnames.pickle',
            'platform/c++/windows_mupdf.def',
            'platform/python/mupdfcpp_swig.cpp',
            ]
    return paths


def build():
    '''
    pipcl callback. Build MuPDF C, C++ and Python libraries.
    '''
    if g_test_minimal:
        # Cut-down for testing.
        log(f'g_test_minimal set so doing minimal build.')
        os.makedirs(f'{root_dir()}/{build_dir()}', exist_ok=True)
        paths = (
                f'{root_dir()}/{build_dir()}/mupdf.py',
                f'{root_dir()}/{build_dir()}/_mupdf.pyd',
                f'{root_dir()}/{build_dir()}/mupdfcpp.dll',
                )
        ret = []
        for path in paths:
            with open(path, 'w') as f:
                # Need to make files non-identical, otherwise
                # check-wheel-contents complains.
                f.write(f'{path}\n')
            ret.append( (path, os.path.basename(path)))
        return ret

    # If we are an sdist, default to not trying to run clang-python - the
    # generated files will already exist, and installing/using clang-python
    # might be tricky.
    #
    use_clang_python = get_flag('MUPDF_SETUP_USE_CLANG_PYTHON', not in_sdist())
    use_swig = get_flag('MUPDF_SETUP_USE_SWIG', True)

    b = ''
    if not windows():
        b = 'm'     # Build C library.
    if use_clang_python:
        b += '0'    # Build C++ source.
    b += '1'        # Build C++ library (also contains C library on Windows).
    if use_swig:
        b += '2'    # Build SWIG-generated source.
    b += '3'        # Build SWIG library _mupdf.so.

    command = '' if root_dir() == os.getcwd() else f'cd {os.path.relpath(root_dir())} && '
    command += (
            f'"{sys.executable}" ./scripts/mupdfwrap.py'
            f'{" --swig-windows-auto" if windows() else ""}'
            f' -d {build_dir()}'
            f' -b {b}'
            )

    do_build = os.environ.get('MUPDF_SETUP_DO_BUILD')
    if do_build == '0':
        # This is a hack for testing.
        log(f'Not doing build because $MUPDF_SETUP_DO_BUILD={do_build}')
    else:
        log(f'build(): Building MuPDF C, C++ and Python libraries with: {command}')
        subprocess.check_call(command, shell=True)

    # Return generated files to install or copy into wheel.
    #
    if windows():
        infix = '' if sys.maxsize == 2**31 - 1 else '64'
        names = [
                f'mupdfcpp{infix}.dll', # C and C++.
                '_mupdf.pyd',           # Python internals.
                'mupdf.py',             # Python.
                ]
    else:
        names = [
                'libmupdf.so',      # C.
                'libmupdfcpp.so',   # C++.
                '_mupdf.so',        # Python internals.
                'mupdf.py',         # Python.
                ]
    paths = []
    for name in names:
        paths.append((f'{build_dir()}/{name}', name))

    log(f'build(): returning: {paths}')
    return paths


def clean(all_):
    if all_:
        return [
                'build',
                'platform/win32/Release',
                'platform/win32/ReleaseDLL',
                'platform/win32/Win32',
                'platform/win32/x64',
                ]
    else:
        # Ideally we would return selected directories in platform/win32/ if on
        # Windows, but that would get a little involved.
        #
        return build_dir()


# Setup pipcl.
#
description = """
Summary
-------

* Python bindings for the MuPDF PDF library.
* A python module called ``mupdf``.
* Generated from the MuPDF C++ API, which is itself generated from the MuPDF C API.
* Provides Python functions that wrap most ``fz_`` and ``pdf_`` functions.
* Provides Python classes that wrap most ``fz_`` and ``pdf_`` structs.

  * Class methods provide access to most of the underlying C API functions (except for functions that don't take struct args such as ``fz_strlcpy()``).
* MuPDF's ``setjmp``/``longjmp`` exceptions are converted to Python exceptions.
* Functions and methods do not take ``fz_context`` arguments. (Automatically-generated per-thread contexts are used internally.)
* Wrapper classes automatically handle reference counting of the underlying structs (with internal calls to ``fz_keep_*()`` and ``fz_drop_*()``).
* Provides a small number of extensions beyond the basic C API:

  * Some generated classes have extra support for iteration.
  * Some custom class methods and constructors.
  * Simple 'POD' structs have ``__str__()`` methods, for example ``mupdf.Rect`` is represented like: ``(x0=90.51 y0=160.65 x1=501.39 y1=215.6)``.

Example usage
-------------

Minimal Python code that uses the ``mupdf`` module:

::

    import mupdf
    document = mupdf.Document('foo.pdf')


A simple example Python test script (run by ``scripts/mupdfwrap.py -t``) is:

* ``scripts/mupdfwrap_test.py``

More detailed usage of the Python API can be found in:

* ``scripts/mutool.py``
* ``scripts/mutool_draw.py``

Here is some example code that shows all available information about document's Stext blocks, lines and characters:

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

More information
----------------

https://twiki.ghostscript.com/do/view/Main/MuPDFWrap

"""

mupdf_package = pipcl.Package(
        name = 'mupdf',
        version = mupdf_version(),
        root = root_dir(),
        summary = 'Python bindings for MuPDF library.',
        description = description,
        classifiers = [
                'Development Status :: 4 - Beta',
                'Intended Audience :: Developers',
                'License :: OSI Approved :: GNU Affero General Public License v3',
                'Programming Language :: Python :: 3',
                ],
        author = 'Artifex Software, Inc.',
        author_email = 'support@artifex.com',
        url_docs = 'https://twiki.ghostscript.com/do/view/Main/MuPDFWrap/',
        url_home = 'https://mupdf.com/',
        url_source = 'https://git.ghostscript.com/?p=mupdf.git',
        url_tracker = 'https://bugs.ghostscript.com/',
        keywords = 'PDF',
        platform = None,
        license_files = ['COPYING'],
        fn_build = build,
        fn_clean = clean,
        fn_sdist = sdist,
        )


# Things to allow us to function as a PIP-517 backend:
#
def build_wheel( wheel_directory, config_settings=None, metadata_directory=None):
    return mupdf_package.build_wheel(
            wheel_directory,
            config_settings,
            metadata_directory,
            )

def build_sdist( sdist_directory, config_settings=None):
    return mupdf_package.build_sdist(
            sdist_directory,
            config_settings,
            )

# Allow us to be used as a pre-PIP-517 setup.py script.
#
if __name__ == '__main__':
    mupdf_package.handle_argv(sys.argv)
