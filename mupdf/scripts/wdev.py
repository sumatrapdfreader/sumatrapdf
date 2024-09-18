'''
Finds locations of Windows command-line development tools.
'''

import os
import platform
import glob
import re
import subprocess
import sys
import sysconfig
import textwrap


class WindowsVS:
    r'''
    Windows only. Finds locations of Visual Studio command-line tools. Assumes
    VS2019-style paths.

    Members and example values::

        .year:      2019
        .grade:     Community
        .version:   14.28.29910
        .directory: C:\Program Files (x86)\Microsoft Visual Studio\2019\Community
        .vcvars:    C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat
        .cl:        C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\bin\Hostx64\x64\cl.exe
        .link:      C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.28.29910\bin\Hostx64\x64\link.exe
        .csc:       C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\Roslyn\csc.exe
        .msbuild:   C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe
        .devenv:    C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com

    `.csc` is C# compiler; will be None if not found.
    '''
    def __init__( self, year=None, grade=None, version=None, cpu=None, verbose=False):
        '''
        Args:
            year:
                None or, for example, `2019`. If None we use environment
                variable WDEV_VS_YEAR if set.
            grade:
                None or, for example, one of:

                * `Community`
                * `Professional`
                * `Enterprise`

                If None we use environment variable WDEV_VS_GRADE if set.
            version:
                None or, for example: `14.28.29910`. If None we use environment
                variable WDEV_VS_VERSION if set.
            cpu:
                None or a `WindowsCpu` instance.
        '''
        def default(value, name):
            if value is None:
                name2 = f'WDEV_VS_{name.upper()}'
                value = os.environ.get(name2)
                if value is not None:
                    _log(f'Setting {name} from environment variable {name2}: {value!r}')
            return value
        try:
            year = default(year, 'year')
            grade = default(grade, 'grade')
            version = default(version, 'version')

            if not cpu:
                cpu = WindowsCpu()

            # Find `directory`.
            #
            pattern = f'C:\\Program Files*\\Microsoft Visual Studio\\{year if year else "2*"}\\{grade if grade else "*"}'
            directories = glob.glob( pattern)
            if verbose:
                _log( f'Matches for: {pattern=}')
                _log( f'{directories=}')
            assert directories, f'No match found for: {pattern}'
            directories.sort()
            directory = directories[-1]

            # Find `devenv`.
            #
            devenv = f'{directory}\\Common7\\IDE\\devenv.com'
            assert os.path.isfile( devenv), f'Does not exist: {devenv}'

            # Extract `year` and `grade` from `directory`.
            #
            # We use r'...' for regex strings because an extra level of escaping is
            # required for backslashes.
            #
            regex = rf'^C:\\Program Files.*\\Microsoft Visual Studio\\([^\\]+)\\([^\\]+)'
            m = re.match( regex, directory)
            assert m, f'No match: {regex=} {directory=}'
            year2 = m.group(1)
            grade2 = m.group(2)
            if year:
                assert year2 == year
            else:
                year = year2
            if grade:
                assert grade2 == grade
            else:
                grade = grade2

            # Find vcvars.bat.
            #
            vcvars = f'{directory}\\VC\\Auxiliary\\Build\\vcvars{cpu.bits}.bat'
            assert os.path.isfile( vcvars), f'No match for: {vcvars}'

            # Find cl.exe.
            #
            cl_pattern = f'{directory}\\VC\\Tools\\MSVC\\{version if version else "*"}\\bin\\Host{cpu.windows_name}\\{cpu.windows_name}\\cl.exe'
            cl_s = glob.glob( cl_pattern)
            assert cl_s, f'No match for: {cl_pattern}'
            cl_s.sort()
            cl = cl_s[ -1]

            # Extract `version` from cl.exe's path.
            #
            m = re.search( rf'\\VC\\Tools\\MSVC\\([^\\]+)\\bin\\Host{cpu.windows_name}\\{cpu.windows_name}\\cl.exe$', cl)
            assert m
            version2 = m.group(1)
            if version:
                assert version2 == version
            else:
                version = version2
            assert version

            # Find link.exe.
            #
            link_pattern = f'{directory}\\VC\\Tools\\MSVC\\{version}\\bin\\Host{cpu.windows_name}\\{cpu.windows_name}\\link.exe'
            link_s = glob.glob( link_pattern)
            assert link_s, f'No match for: {link_pattern}'
            link_s.sort()
            link = link_s[ -1]

            # Find csc.exe.
            #
            csc = None
            for dirpath, dirnames, filenames in os.walk(directory):
                for filename in filenames:
                    if filename == 'csc.exe':
                        csc = os.path.join(dirpath, filename)
                        #_log(f'{csc=}')
                        #break

            # Find MSBuild.exe.
            #
            msbuild = None
            for dirpath, dirnames, filenames in os.walk(directory):
                for filename in filenames:
                    if filename == 'MSBuild.exe':
                        msbuild = os.path.join(dirpath, filename)
                        #_log(f'{csc=}')
                        #break

            self.cl = cl
            self.devenv = devenv
            self.directory = directory
            self.grade = grade
            self.link = link
            self.csc = csc
            self.msbuild = msbuild
            self.vcvars = vcvars
            self.version = version
            self.year = year
        except Exception as e:
            raise Exception( f'Unable to find Visual Studio') from e

    def description_ml( self, indent=''):
        '''
        Return multiline description of `self`.
        '''
        ret = textwrap.dedent(f'''
                year:         {self.year}
                grade:        {self.grade}
                version:      {self.version}
                directory:    {self.directory}
                vcvars:       {self.vcvars}
                cl:           {self.cl}
                link:         {self.link}
                csc:          {self.csc}
                msbuild:      {self.msbuild}
                devenv:       {self.devenv}
                ''')
        return textwrap.indent( ret, indent)

    def __str__( self):
        return ' '.join( self._description())


class WindowsCpu:
    '''
    For Windows only. Paths and names that depend on cpu.

    Members:
        .bits
            32 or 64.
        .windows_subdir
            Empty string or `x64/`.
        .windows_name
            `x86` or `x64`.
        .windows_config
            `x64` or `Win32`, e.g. for use in `/Build Release|x64`.
        .windows_suffix
            `64` or empty string.
    '''
    def __init__(self, name=None):
        if not name:
            name = _cpu_name()
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


class WindowsPython:
    '''
    Windows only. Information about installed Python with specific word size
    and version. Defaults to the currently-running Python.

    Members:

        .path:
            Path of python binary.
        .version:
            `{major}.{minor}`, e.g. `3.9` or `3.11`. Same as `version` passed
            to `__init__()` if not None, otherwise the inferred version.
        .include:
            Python include path.
        .cpu:
            A `WindowsCpu` instance, same as `cpu` passed to `__init__()` if
            not None, otherwise the inferred cpu.
        .libs:
            Python libs directory.

    We parse the output from `py -0p` to find all available python
    installations.
    '''

    def __init__( self, cpu=None, version=None, verbose=True):
        '''
        Args:

            cpu:
                A WindowsCpu instance. If None, we use whatever we are running
                on.
            version:
                Two-digit Python version as a string such as `3.8`. If None we
                use current Python's version.
            verbose:
                If true we show diagnostics.
        '''
        if cpu is None:
            cpu = WindowsCpu(_cpu_name())
        if version is None:
            version = '.'.join(platform.python_version().split('.')[:2])
        _log(f'Looking for Python {version=} {cpu.bits=}.')

        if '.'.join(platform.python_version().split('.')[:2]) == version:
            # Current python matches, so use it directly. This avoids problems
            # on Github where experimental python-3.13 is not available via
            # `py`.
            _log(f'{cpu=} {version=}: using {sys.executable=}.')
            self.path = sys.executable
            self.version = version
            self.cpu = cpu
            self.include = sysconfig.get_path('include')

        else:
            command = 'py -0p'
            if verbose:
                _log(f'{cpu=} {version=}: Running: {command}')
            text = subprocess.check_output( command, shell=True, text=True)
            for line in text.split('\n'):
                #_log( f'    {line}')
                if m := re.match( '^ *-V:([0-9.]+)(-32)? ([*])? +(.+)$', line):
                    version2 = m.group(1)
                    bits = 32 if m.group(2) else 64
                    current = m.group(3)
                    path = m.group(4).strip()
                elif m := re.match( '^ *-([0-9.]+)-((32)|(64)) +(.+)$', line):
                    version2 = m.group(1)
                    bits = int(m.group(2))
                    path = m.group(5).strip()
                else:
                    if verbose:
                        _log( f'No match for {line=}')
                    continue
                if verbose:
                    _log( f'{version2=} {bits=} {path=} from {line=}.')
                if bits != cpu.bits or version2 != version:
                    continue
                root = os.path.dirname(path)
                if not os.path.exists(path):
                    # Sometimes it seems that the specified .../python.exe does not exist,
                    # and we have to change it to .../python<version>.exe.
                    #
                    assert path.endswith('.exe'), f'path={path!r}'
                    path2 = f'{path[:-4]}{version}.exe'
                    _log( f'Python {path!r} does not exist; changed to: {path2!r}')
                    assert os.path.exists( path2)
                    path = path2

                self.path = path
                self.version = version
                self.cpu = cpu
                command = f'{self.path} -c "import sysconfig; print(sysconfig.get_path(\'include\'))"'
                _log(f'Finding Python include path by running {command=}.')
                self.include = subprocess.check_output(command, shell=True, text=True).strip()
                _log(f'Python include path is {self.include=}.')
                #_log( f'pipcl.py:WindowsPython():\n{self.description_ml("    ")}')
                break
            else:
                _log(f'Failed to find python matching cpu={cpu}.')
                _log(f'Output from {command!r} was:\n{text}')
                raise Exception( f'Failed to find python matching cpu={cpu} {version=}.')

        # Oddly there doesn't seem to be a
        # `sysconfig.get_path('libs')`, but it seems to be next
        # to `includes`:
        self.libs = os.path.abspath(f'{self.include}/../libs')

        _log( f'WindowsPython:\n{self.description_ml("    ")}')

    def description_ml(self, indent=''):
        ret = textwrap.dedent(f'''
                path:       {self.path}
                version:    {self.version}
                cpu:        {self.cpu}
                include:    {self.include}
                libs:       {self.libs}
                ''')
        return textwrap.indent( ret, indent)

    def __repr__(self):
        return f'path={self.path!r} version={self.version!r} cpu={self.cpu!r} include={self.include!r} libs={self.libs!r}'


# Internal helpers.
#

def _cpu_name():
    '''
    Returns `x32` or `x64` depending on Python build.
    '''
    #log(f'sys.maxsize={hex(sys.maxsize)}')
    return f'x{32 if sys.maxsize == 2**31 - 1 else 64}'



def _log(text=''):
    '''
    Logs lines with prefix.
    '''
    for line in text.split('\n'):
        print(f'{__file__}: {line}')
    sys.stdout.flush()
