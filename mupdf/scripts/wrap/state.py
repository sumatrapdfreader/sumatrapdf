'''
Misc state.
'''

import glob
import os
import platform
import re
import sys

import jlib


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
            jlib.log( 'Retrying import of clang using info from python2 {clang_path=}')
            sys.path.append( os.path.dirname( clang_path))
            import clang.cindex
        else:
            raise

except Exception as e:
    jlib.log('Warning: failed to import clang.cindex: {e=}\n'
            f'We need Clang Python to build MuPDF python.\n'
            f'Install with "pip install libclang" or use the --venv option, or:\n'
            f'    OpenBSD: pkg_add py3-llvm\n'
            f'    Linux:debian/devuan: apt install python-clang\n'
            )
    clang = None


omit_fns = [
        'fz_open_file_w',
        'fz_colorspace_name_process_colorants', # Not implemented in mupdf.so?
        'fz_clone_context_internal',            # Not implemented in mupdf?
        'fz_arc4_final',
        'fz_assert_lock_held',      # Is a macro if NDEBUG defined.
        'fz_assert_lock_not_held',  # Is a macro if NDEBUG defined.
        'fz_lock_debug_lock',       # Is a macro if NDEBUG defined.
        'fz_lock_debug_unlock',     # Is a macro if NDEBUG defined.
        'fz_argv_from_wargv',       # Only defined on Windows. Breaks our out-param wrapper code.
        ]

omit_methods = []

class ClangInfo:
    '''
    Sets things up so we can import and use clang.

    Members:
        .libclang_so
        .resource_dir
        .include_path
        .clang_version
    '''
    def __init__( self, verbose):
        '''
        We look for different versions of clang until one works.

        Searches for libclang.so and registers with
        clang.cindex.Config.set_library_file(). This appears to be necessary
        even when clang is installed as a standard package.
        '''
        if state_.windows:
            # We require 'pip install libclang' which avoids the need to look
            # for libclang.
            return
        # As of 2022-09-16, max libclang version is 14.
        for version in range( 20, 5, -1):
            ok = self._try_init_clang( version, verbose)
            if ok:
                break
        else:
            raise Exception( 'cannot find libclang.so')

    def _try_init_clang( self, version, verbose):
        if verbose:
            jlib.log( 'Looking for libclang.so, {version=}.')
        if state_.openbsd:
            clang_bin = glob.glob( f'/usr/local/bin/clang-{version}')
            if not clang_bin:
                if verbose:
                    jlib.log('Cannot find {clang_bin=}', 1)
                return
            if verbose:
                jlib.log( '{clang_bin=}')
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
            if verbose:
                jlib.log('{self.libclang_so=} {self.resource_dir=} {self.include_path=}')
            if os.environ.get('VIRTUAL_ENV'):
                clang.cindex.Config.set_library_file( self.libclang_so)
            return True

        if verbose:
            jlib.log( '{os.environ.get( "PATH")=}')
        for p in os.environ.get( 'PATH').split( ':'):
            pp = os.path.join( p, f'clang-{version}*')
            clang_bins = glob.glob( pp)
            if not clang_bins:
                if verbose:
                    jlib.log( 'No match for: {pp=}')
                continue
            if verbose:
                jlib.log( '{clang_bins=}')
            clang_bins.sort()
            for clang_bin in clang_bins:
                if verbose:
                    jlib.log( '{clang_bin=}')
                e, clang_search_dirs = jlib.system(
                        f'{clang_bin} -print-search-dirs',
                        #verbose=log,
                        out='return',
                        raise_errors=False,
                        )
                if e:
                    if verbose:
                        jlib.log( '[could not find {clang_bin}: {e=}]')
                    return
                if verbose:
                    jlib.log( '{clang_search_dirs=}')
                if version == 10:
                    m = re.search( '\nlibraries: =(.+)\n', clang_search_dirs)
                    assert m
                    clang_search_dirs = m.group(1)
                clang_search_dirs = clang_search_dirs.strip().split(':')
                if verbose:
                    jlib.log( '{clang_search_dirs=}')
                for i in ['/usr/lib', '/usr/local/lib'] + clang_search_dirs:
                    for leaf in f'libclang-{version}.*so*', f'libclang.so.{version}.*':
                        p = os.path.join( i, leaf)
                        p = os.path.abspath( p)
                        if verbose:
                            jlib.log( '{p=}')
                        libclang_so = glob.glob( p)
                        if not libclang_so:
                            continue

                        # We have found libclang.so.
                        self.libclang_so = libclang_so[0]
                        jlib.log( 'Using {self.libclang_so=}')
                        clang.cindex.Config.set_library_file( self.libclang_so)
                        self.resource_dir = jlib.system(
                                f'{clang_bin} -print-resource-dir',
                                out='return',
                                ).strip()
                        self.include_path = os.path.join( self.resource_dir, 'include')
                        self.clang_version = version
                        return True
        if verbose:
            jlib.log( 'Failed to find libclang, {version=}.')


clang_info_cache = None

def clang_info( verbose=False):
    global clang_info_cache
    if not clang_info_cache:
        clang_info_cache = ClangInfo( verbose)
    return clang_info_cache

class State:
    def __init__( self):
        self.os_name = platform.system()
        self.windows = (self.os_name == 'Windows' or self.os_name.startswith('CYGWIN'))
        self.cygwin = self.os_name.startswith('CYGWIN')
        self.openbsd = self.os_name == 'OpenBSD'
        self.linux = self.os_name == 'Linux'
        self.macos = self.os_name == 'Darwin'
        self.have_done_build_0 = False

        # Maps from <tu> to dict of fnname: cursor.
        self.functions_cache = dict()

        # Maps from <tu> to dict of dataname: cursor.
        self.global_data = dict()

        self.enums = dict()
        self.structs = dict()

        # Code should show extra information if state_.show_details(name)
        # returns true.
        #
        self.show_details = lambda name: False

    def functions_cache_populate( self, tu):
        if tu in self.functions_cache:
            return
        fns = dict()
        global_data = dict()
        enums = dict()
        structs = dict()

        for cursor in tu.cursor.get_children():
            if cursor.kind==clang.cindex.CursorKind.ENUM_DECL:
                #jlib.log('ENUM_DECL: {cursor.spelling=}')
                enum_values = list()
                for cursor2 in cursor.get_children():
                    #jlib.log('    {cursor2.spelling=}')
                    name = cursor2.spelling
                    enum_values.append(name)
                enums[ cursor.type.get_canonical().spelling] = enum_values
            if cursor.kind==clang.cindex.CursorKind.TYPEDEF_DECL:
                name = cursor.spelling
                if name.startswith( ( 'fz_', 'pdf_')):
                    structs[ name] = cursor
            if (cursor.linkage == clang.cindex.LinkageKind.EXTERNAL
                    or cursor.is_definition()  # Picks up static inline functions.
                    ):
                if cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL:
                    fnname = cursor.mangled_name
                    if self.show_details( fnname):
                        jlib.log( 'Looking at {fnname=}')
                    if fnname not in omit_fns:
                        fns[ fnname] = cursor
                else:
                    global_data[ cursor.mangled_name] = cursor

        self.functions_cache[ tu] = fns
        self.global_data[ tu] = global_data
        self.enums[ tu] = enums
        self.structs[ tu] = structs
        jlib.log('Have populated fns and global_data. {len(enums)=} {len(self.structs)}')

    def find_functions_starting_with( self, tu, name_prefix, method):
        '''
        Yields (name, cursor) for all functions in <tu> whose names start with
        <name_prefix>.

        method:
            If true, we omit names that are in omit_methods
        '''
        self.functions_cache_populate( tu)
        fn_to_cursor = self.functions_cache[ tu]
        for fnname, cursor in fn_to_cursor.items():
            if method and fnname in omit_methods:
                continue
            if not fnname.startswith( name_prefix):
                continue
            yield fnname, cursor

    def find_global_data_starting_with( self, tu, prefix):
        for name, cursor in self.global_data[tu].items():
            if name.startswith( prefix):
                yield name, cursor

    def find_function( self, tu, fnname, method):
        '''
        Returns cursor for function called <fnname> in <tu>, or None if not found.
        '''
        assert ' ' not in fnname, f'fnname={fnname}'
        if method and fnname in omit_methods:
            return
        self.functions_cache_populate( tu)
        return self.functions_cache[ tu].get( fnname)



state_ = State()


def abspath(path):
    '''
    Like os.path.absath() but converts backslashes to forward slashes; this
    simplifies things on Windows - allows us to use '/' as directory separator
    when constructing paths, which is simpler than using os.sep everywhere.
    '''
    ret = os.path.abspath(path)
    ret = ret.replace('\\', '/')
    return ret


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

def cmd_run_multiple(commands, prefix=None):
    '''
    Windows-only.

    Runs multiple commands joined by &&, using cmd.exe if we are running under
    Cygwin. We cope with commands that already contain double-quote characters.
    '''
    if state_.cygwin:
        command = 'cmd.exe /V /C @ ' + ' "&&" '.join(commands)
    else:
        command = ' && '.join(commands)
    jlib.system(command, verbose=1, out='log', prefix=prefix)


class BuildDirs:
    '''
    Locations of various generated files.
    '''
    def __init__( self):

        # Assume we are in mupdf/scripts/.
        file_ = abspath( __file__)
        assert file_.endswith( f'/scripts/wrap/state.py'), \
                'Unexpected __file__=%s file_=%s' % (__file__, file_)
        dir_mupdf = abspath( f'{file_}/../../../')
        assert not dir_mupdf.endswith( '/')

        # Directories used with --build.
        self.dir_mupdf = dir_mupdf

        # Directory used with --ref.
        self.ref_dir = abspath( f'{self.dir_mupdf}/mupdfwrap_ref')
        assert not self.ref_dir.endswith( '/')

        if state_.windows:
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
            jlib.log( 'Warning: unrecognised {dir_so=}, so cannot determine cpp_flags')

        # Set self.cpu and self.python_version.
        if state_.windows:
            # Infer from self.dir_so.
            m = re.match( 'shared-([a-z]+)(-(x[0-9]+))?(-py([0-9.]+))?$', os.path.basename(self.dir_so))
            #log(f'self.dir_so={self.dir_so} {os.path.basename(self.dir_so)} m={m}')
            assert m, f'Failed to parse dir_so={self.dir_so!r} - should be *-x32|x64-pyA.B'
            assert m.group(3), f'No cpu in self.dir_so: {self.dir_so}'
            self.cpu = Cpu( m.group(3))
            self.python_version = m.group(5)
            #log('{self.cpu=} {self.python_version=} {dir_so=}')
        else:
            # Use Python we are running under.
            self.cpu = Cpu(cpu_name())
            self.python_version = python_version()
