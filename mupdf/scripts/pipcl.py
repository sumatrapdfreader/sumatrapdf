'''
Support for Python packaging operations.
'''

import base64
import distutils.util
import hashlib
import io
import os
import shutil
import site
import subprocess
import sys
import tarfile
import time
import zipfile


class Package:
    '''
    Helper for Python packaging operations.

    Our constructor takes a definition of a Python package similar to that
    passed to distutils.core.setup() or setuptools.setup() - name, version,
    summary etc, plus callbacks for build, clean and getting list of sdist filenames.

    We then provide methods that can be used to implement a Python package's
    PEP-517 backend and/or minimal setup.py support for use with a legacy
    (pre-PEP-517) pip.

    A PEP-517 backend can do:

        import pipcl
        p = pipcl.Package('foo', '1.2.3', ...)

        def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
            return p.build_wheel(wheel_directory, config_settings, metadata_directory)

        def build_sdist(sdist_directory, config_settings=None):
            return p.build_sdist(sdist_directory, config_settings)

    A setup.py script can do:

        import sys
        import pipcl

        p = pipcl.Package('foo', '1.2.3', ...)
        p.handle_argv(sys.argv)
    '''
    def __init__(self,
            name,
            version,
            summary = None,
            description = None,
            classifiers = None,
            author = None,
            author_email = None,
            url_docs = None,
            url_home = None,
            url_source = None,
            url_tracker = None,
            keywords = None,
            platform = None,
            license_files = None,
            fn_build = None,
            fn_clean = None,
            fn_sdist = None,
            ):
        '''
        name
            A string, the name of the Python package.
        version
            A string containing only 0-9 and '.'.
        summary
            A string.
        description
            A string.
        classifiers
            A list of strings.
        url_home
        url_source
        url_docs
        utl_tracker
            A string containing a URL.
        keywords
            A string containing space-separated keywords.
        platform
            A string, used in metainfo.
        license_files
            List of string names of license files.
        fn_build
            A function taking no args that builds the package.

            Should return a list of items, each item being a tuple of two
            strings (from_, to_), or a single string <path> which is treated as
            a tuple (path, path).

            <from_> should be the path to a file. <to_> identifies what the
            file should be called when generating a wheel or installing.

            If we are building a wheel (e.g. 'bdist_wheel' in the argv passed
            to self.handle_argv() or PEP-517 pip calls self.build_wheel()), we
            copy file <from_> to <to_> inside the wheel.

            If we are installing (e.g. 'install' command in the argv passed to
            self.handle_argv()), we copy <from_> to <sitepackages>/<to_>, where
            <sitepackages> is the first item in site.getsitepackages()[] that
            exists.
        fn_clean
            A function taking a single arg <all_> that cleans generated files.
            <all_> is true iff --all is in argv.
        fn_sdist
            A function taking no args that returns a list of paths, e.g. from
            git_items(), for files that should be copied into the sdist.
        '''
        self.name = name
        self.version = version
        self.summary = summary
        self.description = description
        self.classifiers = classifiers
        self.author = author
        self.author_email  = author_email
        self.url_docs = url_docs
        self.url_home  = url_home
        self.url_source = url_source
        self.url_tracker = url_tracker
        self.keywords = keywords
        self.platform = platform
        self.license_files = license_files
        self.fn_build = fn_build
        self.fn_clean = fn_clean
        self.fn_sdist = fn_sdist


    def build_wheel(self, wheel_directory, config_settings=None, metadata_directory=None):
        '''
        Helper for implementing a PEP-517 backend's build_wheel() function.

        Also called by handle_argv() to handle the 'bdist_wheel' command.
        '''
        _log('build_wheel():'
                f' wheel_directory={wheel_directory}'
                f' config_settings={config_settings}'
                f' metadata_directory={metadata_directory}'
                )

        # Find platform tag used in wheel filename, as described in PEP-0425.
        #
        tag_platform = distutils.util.get_platform().replace('-', '_').replace('.', '_')
        tag = f'py3-none-{tag_platform}'

        path = f'{wheel_directory}/{self.name}-{self.version}-{tag}.whl'

        # Do build and get list of files to copy into the wheel.
        #
        items = []
        if self.fn_build:
            _log(f'calling self.fn_build={self.fn_build}')
            items = self.fn_build()

        _log(f'creating wheel: {path}')
        os.makedirs(wheel_directory, exist_ok=True)
        record = _Record()
        with zipfile.ZipFile(path, 'w') as z:

            def add_file(from_, to_):
                z.write(from_, to_)
                record.add_file(from_, to_)

            def add_str(content, to_):
                z.writestr(to_, content)
                record.add_content(content, to_)

            # Add the files returned by fn_build().
            #
            for item in items:
                from_, to_ = _fromto(item)
                add_file(from_, to_)

            dist_info_path = f'{self.name}-{self.version}.dist-info'
            # Add <name>-<version>.dist-info/WHEEL.
            #
            add_str(
                    f'Wheel-Version: 1.0\n'
                    f'Generator: bdist_wheel\n'
                    f'Root-Is-Purelib: false\n'
                    f'Tag: {tag}\n'
                    ,
                    f'{dist_info_path}/WHEEL',
                    )
            # Add <name>-<version>.dist-info/METADATA.
            #
            add_str(self._metainfo(), f'{dist_info_path}/METADATA')
            if self.license_files:
                for license_file in self.license_files:
                    from_, to_ = _fromto(license_file)
                    add_file(from_, f'{dist_info_path}/{to_}')

            # Update <name>-<version>.dist-info/RECORD. This must be last.
            #
            z.writestr(f'{dist_info_path}/RECORD', record.get())

        _log( f'build_wheel(): Have created wheel: {path}')
        return os.path.basename(path)


    def build_sdist(self, sdist_directory, config_settings=None):
        '''
        Helper for implementing a PEP-517 backend's build_sdist() function.

        [Though as of 2021-03-24 pip doesn't actually seem to ever call the
        backend's build_sdist() function?]

        Also called by handle_argv() to handle the 'sdist' command.
        '''
        paths = []
        if self.fn_sdist:
            paths = self.fn_sdist()

        manifest = []
        def add(tar, name, contents):
            '''
            Adds item called <name> to tarfile.TarInfo <tar>, containing
            <contents> (which must be bytes, not str).
            '''
            if isinstance(contents, str):
                contents = contents.encode('utf8')
            ti = tarfile.TarInfo(name)
            ti.size = len(contents)
            ti.mtime = time.time()
            tar.addfile(ti, io.BytesIO(contents))

        os.makedirs(sdist_directory, exist_ok=True)
        tarpath = f'{sdist_directory}/{self.name}-{self.version}.tar.gz'
        with tarfile.open(tarpath, 'w:gz') as tar:
            for path in paths:
                if os.path.abspath(path).startswith(f'{os.path.abspath(sdist_directory)}/'):
                    # Ignore files inside <opt_dist_dir>, to save fn_sdist()
                    # from worrying about this.
                    continue
                if not os.path.exists(path):
                    # This appears to happen for sub-submodules.
                    _log(f'Ignoring non-existant path: {path}')
                    continue
                #log(f'path={path}')
                tar.add( path, f'{self.name}-{self.version}/{path}', recursive=False)
                manifest.append(path)
            add(tar, f'{self.name}-{self.version}/PKG-INFO', self._metainfo().encode('utf8'))

            # It doesn't look like MANIFEST or setup.cfg are required.
            #
            if 0:
                # Add manifest:
                add(tar, f'{self.name}-{self.version}/MANIFEST', '\n'.join(manifest))

            if 0:
                # add setup.cfg
                setup_cfg = ''
                setup_cfg += '[bdist_wheel]\n'
                setup_cfg += 'universal = 1\n'
                setup_cfg += '\n'
                setup_cfg += '[flake8]\n'
                setup_cfg += 'max-line-length = 100\n'
                setup_cfg += 'ignore = F821\n'
                setup_cfg += '\n'
                setup_cfg += '[metadata]\n'
                setup_cfg += 'license_file = LICENSE\n'
                setup_cfg += '\n'
                setup_cfg += '[tool:pytest]\n'
                setup_cfg += 'minversion = 2.2.0\n'
                setup_cfg += '\n'
                setup_cfg += '[egg_info]\n'
                setup_cfg += 'tag_build = \n'
                setup_cfg += 'tag_date = 0\n'
                add(tar, f'{self.name}-{self.version}/setup.cfg', setup_cfg)

        _log( f'build_sdist(): Have created sdist: {tarpath}')
        return os.path.basename(tarpath)


    def argv_clean(self, all_):
        '''
        Called by handle_argv().
        '''
        if self.fn_clean:
            self.fn_clean(all_)


    def argv_install(self, record_path):
        '''
        Called by handle_argv().
        '''
        items = []
        if self.fn_build:
            items = self.fn_build()

        # We install to the first item in site.getsitepackages()[] that exists.
        #
        sitepackages_all = site.getsitepackages()
        for p in sitepackages_all:
            if os.path.exists(p):
                sitepackages = p
                break
        else:
            text = 'No item exists in site.getsitepackages():\n'
            for i in sitepackages_all:
                text += f'    {i}\n'
            raise Exception(text)

        record = _Record() if record_path else None
        for item in items:
            from_, to_ = _fromto(item)
            to_path = f'{sitepackages}/{to_}'
            _log(f'copying from {from_} to {to_path}')
            shutil.copy2( from_, f'{to_path}')
            if record:
                # Could maybe use relative path of to_path from sitepackages/.
                record.add_file(from_, to_)

        if record:
            with open(record_path, 'w') as f:
                f.write(record.get())

        _log(f'argv_install(): Finished.')


    def argv_dist_info(self, egg_base):
        '''
        Called by handle_argv(). There doesn't seem to be any documentation for
        setup.py dist_info, but it appears to be like egg_info except it writes
        to a slightly different directory.
        '''
        self._write_info(f'{egg_base}/{self.name}.dist-info')


    def argv_egg_info(self, egg_base):
        '''
        Called by handle_argv().
        '''
        if egg_base is None:
            egg_base = '.'
        self._write_info(f'{egg_base}/.egg-info')


    def _write_info(self, root):
        '''
        Writes egg/dist info to files in <root>/.
        '''
        _log(f'_write_info(): creating files in root={root}')
        os.mkdir(root)
        with open(f'{root}/PKG-INFO', 'w') as f:
            f.write(self._metainfo())

        # These don't seem to be required?
        #
        #with open(f'{root}/SOURCES.txt', 'w') as f:
        #    pass
        #with open(f'{root}/dependency_links.txt', 'w') as f:
        #    pass
        #with open(f'{root}/top_level.txt', 'w') as f:
        #    f.write(f'{self.name}\n')
        #with open(f'{root}/METADATA', 'w') as f:
        #    f.write(self._metainfo())


    def handle_argv(self, argv):
        '''
        Handles old-style (pre PEP-517) command line passed by old releases of pip to a
        setup.py script.

        We only handle those args that seem to be used by pre-PEP-517 pip.
        '''
        _log(f'handle_argv(): argv: {argv}')

        class ArgsRaise:
            pass

        class Args:
            '''
            Iterates over argv items.
            '''
            def __init__( self, argv):
                self.items = iter( argv)
            def next( self, eof=ArgsRaise):
                '''
                Returns next arg. If no more args, we return <eof> or raise an
                exception if <eof> is ArgsRaise.
                '''
                try:
                    return next( self.items)
                except StopIteration:
                    if eof is ArgsRaise:
                        raise Exception('Not enough args')
                    return eof

        command = None
        opt_all = None
        opt_dist_dir = 'dist'
        opt_egg_base = None
        opt_install_headers = None
        opt_python_tag = None
        opt_record = None

        args = Args(argv[1:])

        while 1:
            arg = args.next(None)
            if arg is None:
                break

            elif arg in ('-h', '--help'):
                _log(
                        'Options:\n'
                        '    bdist_wheel\n'
                        '        Creates a wheel called\n'
                        '        <dist-dir>/<name>-<version>-<details>.whl, where\n'
                        '        <dist-dir> is "dist" or as specified by --dist-dir,\n'
                        '        and <details> encodes ABI and platform etc.\n'
                        '    clean\n'
                        '        Cleans build files.\n'
                        '    egg_info\n'
                        '        Creates files in <egg-base>/.egg-info/, where\n'
                        '        <egg-base> is as specified with --egg-base.\n'
                        '    install\n'
                        '        Installs into location from Python\'s\n'
                        '        site.getsitepackages() array. Writes installation\n'
                        '        information to <record> if --record\n'
                        '        was specified.\n'
                        '    sdist\n'
                        '        Make a source distribution:\n'
                        '            <dist-dir>/<name>-<version>.tar.gz\n'
                        '    dist_info\n'
                        '        Like <egg_info> but creates files in\n'
                        '        <egg-base>/<name>.dist-info/\n'
                        '    --dist-dir | -d <dist-dir>\n'
                        '        Default is "dist".\n'
                        '    --egg-base <egg-base>\n'
                        '        Used by "egg_info".\n'
                        '    --record <record>\n'
                        '        Used by "install".\n'
                        '    --single-version-externally-managed\n'
                        '        Ignored.\n'
                        '    --compile\n'
                        '        Ignored.\n'
                        '    --install-headers <directory>\n'
                        '        Ignored.\n'
                        )
                return

            elif arg in ('bdist_wheel', 'clean', 'egg_info', 'install', 'sdist', 'dist_info'):
                assert command is None, 'Two commands specified: {command} and {arg}.'
                command = arg

            elif arg == '--all':
                opt_all = True

            elif arg == '--compile':
                pass

            elif arg == '--dist-dir' or arg == '-d':
                opt_dist_dir = args.next()

            elif arg == '--egg-base':
                opt_egg_base = args.next()

            elif arg == '--help-commands':
                print('help')
                return

            elif arg == '--install-headers':
                opt_install_headers = args.next()

            elif arg == '--python-tag':
                opt_python_tag = args.next()

            elif arg == '--record':
                opt_record = args.next()

            elif arg == '--single-version-externally-managed':
                pass

            else:
               raise Exception(f'Unrecognised arg: {arg}')

        if not command:
            return

        _log(f'handle_argv(): Handling command={command}')
        if 0:   pass
        elif command == 'bdist_wheel':  self.build_wheel(opt_dist_dir)
        elif command == 'clean':        self.argv_clean(opt_all)
        elif command == 'dist_info':    self.argv_dist_info(opt_egg_base)
        elif command == 'egg_info':     self.argv_egg_info(opt_egg_base)
        elif command == 'install':      self.argv_install(opt_record)
        elif command == 'sdist':        self.build_sdist(opt_dist_dir)
        else:
            assert 0, f'Unrecognised command: {command}'

        _log(f'handle_argv(): Finished handling command={command}')


    def __str__(self):
        return ('{'
            f'name={self.name!r}'
            f' version={self.version!r}'
            f' summary={self.summary!r}'
            f' description={self.description!r}'
            f' classifiers={self.classifiers!r}'
            f' author={self.author!r}'
            f' author_email ={self.author_email!r}'
            f' url_docs={self.url_docs!r}'
            f' url_home ={self.url_home!r}'
            f' url_source={self.url_source!r}'
            f' url_tracker={self.url_tracker!r}'
            f' keywords={self.keywords!r}'
            f' platform={self.platform!r}'
            f' license_files={self.license_files!r}'
            f' fn_clean={self.fn_clean!r}'
            f' fn_sdist={self.fn_sdist!r}'
            f' fn_build={self.fn_build!r}'
            '}'
            )


    def _metainfo(self):
        '''
        Returns text for .egg-info/PKG-INFO file, or PKG-INFO in an sdist
        tar.gz file, or ...dist-info/METADATA in a wheel.
        '''
        ret = []
        def add(key, value):
            if value is not None:
                ret.append(f'{key}: {value}')
        add('Metadata-Version', '1.0')
        add('Name', self.name)
        add('Version', self.version)
        add('Summary', self.summary)
        add('Home-page', self.url_home)
        add('Platform', self.platform)
        add('Author', self.author)
        add('Author-email', self.author_email)
        if self.url_source:  add('Home-page', f'Source, {self.url_source}')
        if self.url_docs:    add('Home-page', f'Source, {self.url_docs}')
        if self.url_tracker: add('Home-page', f'Source, {self.url_tracker}')
        if self.keywords:
            add('Keywords', self.keywords)
        if self.classifiers:
            classifiers2 = self.classifiers
            if isinstance(classifiers2, str):
                classifiers2 = classifiers2.split('\n')
            for c in classifiers2:
                add('Classifier', c)
        return '\n'.join(ret)


def git_items( directory, submodules=False):
    '''
    Helper for Package's fn_sdist() callback.

    Returns list of paths for all files known to git within <directory>. Each
    path is relative to <directory>.

    <directory> must be somewhere within a git checkout.

    We run a 'git ls-files' command internally.
    '''
    command = 'cd ' + directory + ' && git ls-files'
    if submodules:
        command += ' --recurse-submodules'
    text = subprocess.check_output( command, shell=True)
    ret = []
    for path in text.decode('utf8').strip().split( '\n'):
        path2 = os.path.join(directory, path)
        if os.path.isdir(path2):
            _log(f'*** Ignoring git ls-files item that is actually a directory: {path2}')
        else:
            ret.append(path)
    return ret


# Implementation helpers.
#

def _log(text=''):
    '''
    Logs lines with prefix.
    '''
    for line in text.split('\n'):
        print(f'pipcl.py: {line}')
    sys.stdout.flush()

def _fromto(p):
    '''
    If <p> is a string we return (p, p). Otherwise we assert that <p> is a
    tuple of two strings and return <p> itself.
    '''
    if isinstance(p, str):
        return p, p
    elif isinstance(p, tuple) and len(p) == 2:
        from_, to_ = p
        if isinstance(from_, str) and isinstance(to_, str):
            return from_, to_
    assert 0, 'p should be str or (str, str), but is: {p}'

class _Record:
    '''
    Internal - builds up text suitable for writing to a RECORD item, e.g.
    within a wheel.
    '''
    def __init__(self):
        self.text = ''

    def add_content(self, content, to_):
        if isinstance(content, str):
            content = content.encode('latin1')
        h = hashlib.sha256(content)
        digest = h.digest()
        digest = base64.urlsafe_b64encode(digest)
        self.text += f'{to_},sha256={digest},{len(content)}\n'

    def add_file(self, from_, to_):
        with open(from_, 'rb') as f:
            content = f.read()
        self.add_content(content, to_)

    def get(self):
        return self.text

def _parse_pkg_info(path):
    '''
    Parses a PKJG-INFO file, each line is '<key>: <value>\n'. Returns a dict.
    '''
    ret = dict()
    with open(path) as f:
        for line in f:
            s = line.find(': ')
            if s >= 0 and line.endswith('\n'):
                k = line[:s]
                v = line[s+2:-1]
                ret[k] = v
    return ret
