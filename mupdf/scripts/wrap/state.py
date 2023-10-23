'''
Misc state.
'''

import glob
import os
import platform
import re
import sys

import jlib

from . import parse

try:
    import clang.cindex
except Exception as e:
    jlib.log('Warning: failed to import clang.cindex: {e=}\n'
            f'We need Clang Python to build MuPDF python.\n'
            f'Install with `pip install libclang` (typically inside a Python venv),\n'
            f'or (OpenBSD only) `pkg_add py3-llvm.`\n'
            )
    clang = None

omit_fns = [
        'fz_open_file_w',
        'fz_colorspace_name_process_colorants', # Not implemented in mupdf.so?
        'fz_clone_context_internal',            # Not implemented in mupdf?
        'fz_assert_lock_held',      # Is a macro if NDEBUG defined.
        'fz_assert_lock_not_held',  # Is a macro if NDEBUG defined.
        'fz_lock_debug_lock',       # Is a macro if NDEBUG defined.
        'fz_lock_debug_unlock',     # Is a macro if NDEBUG defined.
        'fz_argv_from_wargv',       # Only defined on Windows. Breaks our out-param wrapper code.

        # Only defined on Windows, so breaks building Windows wheels from
        # sdist, because the C++ source in sdist (usually generated on Unix)
        # does not contain these functions, but SWIG-generated code will try to
        # call them.
        'fz_utf8_from_wchar',
        'fz_wchar_from_utf8',
        'fz_fopen_utf8',
        'fz_remove_utf8',
        'fz_argv_from_wargv',
        'fz_free_argv',
        'fz_stdods',
        ]

omit_methods = []


def get_name_canonical( type_):
    '''
    Wrap Clang's clang.cindex.Type.get_canonical() to avoid returning anonymous
    struct that clang spells as 'struct (unnamed at ...)'.
    '''
    if type_.spelling == 'size_t':
        #jlib.log( 'Not canonicalising {self.spelling=}')
        return type_
    ret = type_.get_canonical()
    if 'struct (unnamed' in ret.spelling:
        jlib.log( 'Not canonicalising {type_.spelling=}')
        ret = type_
    return ret


class State:
    def __init__( self):
        self.os_name = platform.system()
        self.windows = (self.os_name == 'Windows' or self.os_name.startswith('CYGWIN'))
        self.cygwin = self.os_name.startswith('CYGWIN')
        self.openbsd = self.os_name == 'OpenBSD'
        self.linux = self.os_name == 'Linux'
        self.macos = self.os_name == 'Darwin'
        self.pyodide = os.environ.get('OS') == 'pyodide'
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

        for cursor in parse.get_children(tu.cursor):
            verbose = state_.show_details( cursor.spelling)
            if verbose:
                jlib.log('Looking at {cursor.spelling=} {cursor.kind=} {cursor.location=}')
            if cursor.kind==clang.cindex.CursorKind.ENUM_DECL:
                #jlib.log('ENUM_DECL: {cursor.spelling=}')
                enum_values = list()
                for cursor2 in cursor.get_children():
                    #jlib.log('    {cursor2.spelling=}')
                    name = cursor2.spelling
                    enum_values.append(name)
                enums[ get_name_canonical( cursor.type).spelling] = enum_values
            if cursor.kind==clang.cindex.CursorKind.TYPEDEF_DECL:
                name = cursor.spelling
                if name.startswith( ( 'fz_', 'pdf_')):
                    structs[ name] = cursor
            if cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL:
                fnname = cursor.spelling
                if self.show_details( fnname):
                    jlib.log( 'Looking at {fnname=}')
                if fnname in omit_fns:
                    jlib.log('{fnname=} is in omit_fns')
                else:
                    fns[ fnname] = cursor
            if (cursor.kind == clang.cindex.CursorKind.VAR_DECL
                    and cursor.linkage == clang.cindex.LinkageKind.EXTERNAL
                    ):
                global_data[ cursor.spelling] = cursor

        self.functions_cache[ tu] = fns
        self.global_data[ tu] = global_data
        self.enums[ tu] = enums
        self.structs[ tu] = structs
        jlib.log('Have populated fns and global_data. {len(enums)=} {len(self.structs)} {len(fns)=}')

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
            verbose = state_.show_details( fnname)
            if method and fnname in omit_methods:
                if verbose:
                    jlib.log('{fnname=} is in {omit_methods=}')
                continue
            if not fnname.startswith( name_prefix):
                if 0 and verbose:
                    jlib.log('{fnname=} does not start with {name_prefix=}')
                continue
            if verbose:
                jlib.log('{name_prefix=} yielding {fnname=}')
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
            assert 0, f'method={method} fnname={fnname} omit_methods={omit_methods}'
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
    def __init__(self, name=None):
        if name is None:
            name = cpu_name()
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
    def __repr__(self):
        return f'Cpu:{self.name}'

def python_version():
    '''
    Returns two-digit version number of Python as a string, e.g. '3.9'.
    '''
    ret = '.'.join(platform.python_version().split('.')[:2])
    #jlib.log(f'returning ret={ret!r}')
    return ret

def cpu_name():
    '''
    Returns 'x32' or 'x64' depending on Python build.
    '''
    ret = f'x{32 if sys.maxsize == 2**31 - 1 else 64}'
    #jlib.log(f'returning ret={ret!r}')
    return ret

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
        #jlib.log( f'platform.platform(): {platform.platform()}')
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

        self.set_dir_so( f'{self.dir_mupdf}/build/shared-release')

    def set_dir_so( self, dir_so):
        '''
        Sets self.dir_so and also updates self.cpp_flags etc. Special case
        `dir_so='-'` sets to None.
        '''
        if dir_so == '-':
            self.dir_so = None
            self.cpp_flags = None
            return

        dir_so = abspath( dir_so)
        self.dir_so = dir_so

        if state_.windows:
            # debug builds have:
            # /Od
            # /D _DEBUG
            # /RTC1
            # /MDd
            #
            if 0: pass  # lgtm [py/unreachable-statement]
            elif '-release' in dir_so:
                self.cpp_flags = '/O2 /DNDEBUG'
            elif '-debug' in dir_so:
                # `/MDd` forces use of debug runtime and (i think via
                # it setting `/D _DEBUG`) debug versions of things like
                # `std::string` (incompatible with release builds). We also set
                # `/Od` (no optimisation) and `/RTC1` (extra runtime checks)
                # because these seem to be conventionally set in VS.
                #
                self.cpp_flags = '/MDd /Od /RTC1'
            elif '-memento' in dir_so:
                self.cpp_flags = '/MDd /Od /RTC1 /DMEMENTO'
            else:
                self.cpp_flags = None
                jlib.log( 'Warning: unrecognised {dir_so=}, so cannot determine cpp_flags')
        else:
            if 0: pass  # lgtm [py/unreachable-statement]
            elif '-debug' in dir_so:    self.cpp_flags = '-g'
            elif '-release' in dir_so:  self.cpp_flags = '-O2 -DNDEBUG'
            elif '-memento' in dir_so:  self.cpp_flags = '-g -DMEMENTO'
            else:
                self.cpp_flags = None
                jlib.log( 'Warning: unrecognised {dir_so=}, so cannot determine cpp_flags')

        # Set self.cpu and self.python_version.
        if state_.windows:
            # Infer cpu and python version from self.dir_so. And append current
            # cpu and python version if not already present.
            leaf = os.path.basename(self.dir_so)
            m = re.match( 'shared-([a-z]+)$', leaf)
            if m:
                suffix = f'-{Cpu(cpu_name())}-py{python_version()}'
                jlib.log('Adding suffix to {leaf!r}: {suffix!r}')
                self.dir_so += suffix
                leaf = os.path.basename(self.dir_so)
            m = re.search( '-(x[0-9]+)-py([0-9.]+)$', leaf)
            #log(f'self.dir_so={self.dir_so} {os.path.basename(self.dir_so)} m={m}')
            assert m, f'Failed to parse dir_so={self.dir_so!r} - should be *-x32|x64-pyA.B'
            self.cpu = Cpu( m.group(1))
            self.python_version = m.group(2)
            #jlib.log('{self.cpu=} {self.python_version=} {dir_so=}')
        else:
            # Use Python we are running under.
            self.cpu = Cpu(cpu_name())
            self.python_version = python_version()

    def windows_build_type(self):
        dir_so_flags = os.path.basename( self.dir_so).split( '-')
        if 'debug' in dir_so_flags:
            return 'Debug'
        elif 'release' in dir_so_flags:
            return 'Release'
        else:
            assert 0, f'Expecting "-release-" or "-debug-" in build_dirs.dir_so={self.dir_so}'
