#!/usr/bin/env python3

'''
Support for creating Python wheels and uploading to pypi.org.

Overview:

    Support for building sdist, building wheels locally/remotely, uploading
    sdist and wheels to pypi.org, testing that 'pip install <package-name>'
    works.

    When building wheels, on Unix we use a manylinux docker container, on
    Windows we use the 'py' command to find different installed Pythons.

    May require installation of python-venv or similar package.
'''


# Interactive session inside docker instance:
#        docker exec -it pypackage bash


import argparse
import distutils.util
import glob
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import time

import jlib


log = jlib.log


def system(
        command,
        raise_errors=True,
        return_output=False,
        prefix=None,
        caller=1,
        bufsize=-1,
        ):
    '''
    Runs a command. See jlib.system()'s docs for details.
    '''
    return jlib.system(
            command,
            verbose=not return_output,
            raise_errors=raise_errors,
            out='return' if return_output else 'log',
            prefix=prefix,
            caller=caller+1,
            bufsize=bufsize,
            )


def windows():
    s = platform.system()
    return s == 'Windows' or s.startswith('CYGWIN')

def unix():
    return not windows()


def make_tag(py_version=None):
    '''
    py_version:
        None or E.g. '3.9.4'. If None we use native Python version.
    cpu:
    '''
    # Find platform tag used in wheel filename, as described in
    # PEP-0425. E.g. 'openbsd_6_8_amd64', 'win_amd64' or 'win32'.
    #
    tag_platform = distutils.util.get_platform().replace('-', '_').replace('.', '_')

    if py_version is None:
        py_version = platform.python_version()
    # Get two-digit python version, e.g. 3.8 for python-3.8.6.
    #
    tag_python = ''.join(py_version.split('.')[:2])

    # Final tag is, for example, 'py39-none-win32', 'py39-none-win_amd64'
    # or 'py38-none-openbsd_6_8_amd64'.
    #
    tag = f'cp{tag_python}-none-{tag_platform}'
    return tag


# We keep track of all the venv's we have created so that we can avoid
# unnecessarily running pip --upgrade. todo: check this is actually necessary.
venv_installed = set()

def venv_run(
        commands,
        venv='pypackage-venv',
        py=None,
        clean=False,
        return_output=False,
        directory=None,
        prefix=None,
        pip_upgrade=True,
        bufsize=-1,
        ):
    '''
    Runs commands inside Python venv, joined by &&.

    commands:
        List of commands to run.
    venv:
        Name of venv to create and use. Relative to <directory> if <directory>
        is not None.
    py:
        Python to run. If None we use sys.executable.
    clean:
        If true we first delete any existing <venv>. For safety we assert that
        <venv> starts with 'pypackage-venv'.
    directory:
        Directory in which to create venv and run <commands>. If None we use
        the current directory.
    prefix:
        Prefix to prepend to each output line.
    pip_upgrade:
        If true (the default) we do 'pip install --upgrade pip' before running
        <commands>.
    bufsize:
        Use 0 if we expect interactive prompts.
    '''
    if isinstance(commands, str):
        commands = [commands]
    if py is None:
        py = 'py' if windows() else sys.executable
    if windows():
        # Run under cmd.exe with all commands inside "...".
        pre = [
                f'cmd.exe /c "'
                f'{"cd "+directory if directory else "true"}',
                f'{py} -m venv {venv}',
                f'{venv}\\Scripts\\activate.bat',
                ]
        post = [f'deactivate"']
    else:
        pre = [
                f'{"cd "+directory if directory else "true"}',
                f'{py} -m venv {venv}',
                f'. {venv}/bin/activate',
                ]
        post = [f'deactivate']

    if clean or venv not in venv_installed:
        if clean:
            assert venv.startswith('pypackage-venv')
            log('Removing directory to get clean venv: {venv}')
            shutil.rmtree(venv, ignore_errors=1)
        venv_installed.add(venv)

        if pip_upgrade:
            pre += [
                    'pip install --upgrade pip',
                    ]
            if windows():
                # It looks like first attempt to upgrade pip can fail with
                # 'EnvironmentError: [WinError 5] Access is denied'. So we make an
                # extra first attempt.
                #
                commands0 = pre + post
                command0 = '&&'.join(commands0)
                log(f'Running pre-install of pip: {command0}')
                e = system(command0, raise_errors=False, prefix=prefix)
                if e:
                    log(f'[Ignoring error from dummy run of pip install on Windows.]')

    commands = pre + commands + post

    if windows():
        # Spaces mess things up on Windows.
        command = '&&'.join(commands)
    else:
        command = ' && '.join(commands)

    return system(
            command,
            return_output=return_output,
            prefix=prefix,
            bufsize=bufsize,
            )


def check_sdist(sdist):
    '''
    Checks sdist with 'twine check'.
    '''
    venv_run([
            f'pip install twine',
            f'twine check {sdist}',
            ])


def check_wheel(wheel):
    '''
    Checks wheel with 'twine check'.
    '''
    # We don't install and use check-wheel-contents, because it thinks
    # top-level .dll files are incorrect.
    venv_run([
            f'pip install twine',
            #f'check-wheel-contents {wheel}',
            f'twine check {wheel}',
            ])


def find_new_file(pattern, t):
    '''
    Finds file matching <pattern> whose mtime is >= t. Raises exception if
    there isn't exactly one such file.
    '''
    paths = glob.glob(pattern)
    paths_new = []
    for path in paths:
        tt = os.path.getmtime(path)
        if tt >= t:
            paths_new.append(path)

    if len(paths_new) == 0:
        raise Exception(f'No new file found matching glob: {pattern}')
    elif len(paths_new) > 1:
        text = 'More than one file found matching glob: {pattern}\n'
        for path in paths_new:
            text += f'    {path}\n'
        raise Exception(text)

    return paths_new[0]


def make_sdist(package_directory, out_directory):
    '''
    Creates sdist from source tree <package_directory>, in <out_directory>. Returns
    the path of the generated sdist.
    '''
    os.makedirs(out_directory, exist_ok=True)
    t = time.time()
    command = (
            f'cd {os.path.relpath(package_directory)}'
            f' && ./setup.py sdist -d "{os.path.relpath(out_directory, package_directory)}"'
            )
    system(command)

    # Find new file in <sdist_directory>.
    sdist = find_new_file(f'{out_directory}/*.tar.gz', t)
    check_sdist(sdist)

    return sdist


def make_unix(
        sdist,
        abis=None,
        out_dir=None,
        test_direct_install=False,
        install_docker=None,
        docker_image=None,
        pull_docker_image=None,
        container_name=None,
        ):
    '''
    Builds Python wheels using a manylinux container.

        sdist:
            Pathname of sdist file; we copy into container and extract there.
        abis
            List of string python versions for which we build
            wheels. Any '.' are removed and we then take the first two
            characters. E.g. ['3.8.4', '39'] is same as ['38', '39'].
        out_dir
            If not None, the directory into which we generated wheels.
        test_direct_install
            If true we run 'pip install <sdist>' in the container.
            If None we default to false.
        install_docker
            If true we attempt to install docker (currently specific to
            Debian/Devuan).
            If None we default to false.
        docker_image
            Name of docker image to use.
            If None we default to: quay.io/pypa/manylinux2014_x86_64
        pull_docker_image
            If true we run 'docker pull ...' to update the image.
            If None we default to true.
        container_name
            Name to use for the container.
            If None we default to 'pypackage'.

    Returns wheels, a list of wheel pathnames.
    '''
    if abis is None:
        abis = ['37', '38', '39']
    if test_direct_install is None:
        test_direct_install = False
    if install_docker is None:
        install_docker = False
    if docker_image is None:
        docker_image='quay.io/pypa/manylinux2014_x86_64'
    if pull_docker_image is None:
        pull_docker_image = True
    if container_name is None:
        container_name = 'pypackage'

    io_directory = 'pypackage-io'

    assert sdist.endswith('.tar.gz')
    sdist_leaf = os.path.basename(sdist)    # e.g. mupdf-1.18.0.20210504.1544.tar.gz
    package_root = sdist_leaf[ : -len('.tar.gz')]   # e.g. mupdf-1.18.0.20210504.1544

    check_sdist(sdist)

    if install_docker:
        # Note that if docker is not already installed, we will
        # need to add user to docker group, e.g. with:
        #   sudo usermod -a -G docker $USER
        system('sudo apt install docker.io')

    # Copy sdist into what will be the container's /io/ directory.
    #
    os.makedirs( io_directory, exist_ok=True)
    system( f'rsync -ai {sdist} {io_directory}/')

    if pull_docker_image:
        # Ensure we have latest version of the docker image.
        #
        system(f'docker pull {docker_image}')

    if 1:
        # Ensure docker instance is created and running. Also give it a name
        # <container_name> so we can refer to it below.
        #
        # '-itd' ensures docker instance runs in background.
        #
        # With '-v HOST:CONTAINER', HOST must be absolute path.
        #
        # It's ok for this to fail - container is already created and running.
        #
        e = system(
            f' docker run'
            f' -itd'
            f' --name {container_name}'
            f' -v {os.path.abspath(io_directory)}:/io'
            f' {docker_image}'
            ,
            raise_errors=False,
            )
        log(f'docker run: ignoring any error. e={e}')

        # Start docker instance if not already started.
        #
        e = system(f'docker start {container_name}', raise_errors=False,)
        log(f'docker start: e={e}')

    def docker_system(command, prefix='', return_output=False):
        '''
        Runs specified command inside container. Runs via bash so <command> can
        contain shell constructs. If out is 'capture', we return the output text.
        '''
        command = command.replace('"', '\\"')
        command = f'docker exec {container_name} bash -c "{command}"'
        log(f'Running: {command}')
        return system(
                command,
                return_output=return_output,
                prefix=None if return_output else f'container: {prefix}',
                caller=2,
                )

    if test_direct_install:
        # Test direct intall from sdist.
        #
        docker_system( f'pip3 -vvv install /io/{sdist_leaf}')

    container_pythons = []
    for abi in abis:
        vv = abi.replace('.', '')[:2] # E.g. '38' for Python-3.8.
        pattern = f'/opt/python/cp{vv}-cp{vv}*'
        paths = docker_system(f'ls -d {pattern}', return_output=1)
        log(f'vv={vv}: paths={paths!r}')
        paths = paths.strip().split('\n')
        assert len(paths) == 1, f'No single match for glob pattern in container. pattern={pattern}: {paths}'
        container_pythons.append( (vv, paths[0]))

    # In the container, extract <sdist>; we will use the extracted sdist to
    # build wheels.
    #
    docker_system( f'tar -xzf /io/{sdist_leaf}')

    # Build wheels inside container.
    wheels = []
    for vv, container_python in container_pythons:
        log(f'Building wheel with python {vv}: {container_python}')

        # Build wheel.
        t = time.time()
        docker_system(
                f'cd {package_root} && {container_python}/bin/python ./setup.py --dist-dir /io bdist_wheel',
                prefix=f'{container_python}: '
                )

        # Find new wheel.
        wheel = find_new_file(f'{io_directory}/{package_root}-cp{vv}-none-*.whl', t)
        wheels.append(wheel)
        check_wheel(wheel)
        if out_dir:
            # Rename the wheel to be a manylinux wheel so it can be uploaded to
            # pypi.org.
            #
            leaf = os.path.basename(wheel)
            m = re.match('^([^-]+-[^-]+-[^-]+-[^-]+-)linux(_[^.]+.whl)$', leaf)
            assert m, f'Cannot parse wheel: {wheel!r}'
            leaf2 = f'{m.group(1)}manylinux2014{m.group(2)}'
            log(f'changing leaf from {leaf} to {leaf2}')
            os.makedirs(out_dir, exist_ok=1)
            shutil.copy2(wheel, f'{out_dir}/{leaf2}')

    log( f'wheels are ({len(wheels)}):')
    for wheel in wheels:
        log( f'    {wheel}')

    return wheels


def windows_python_from_abi(abi):
    '''
    Returns (cpu, python_version, py).
    '''
    cpu, python_version = abi.split('-')
    python_version = '.'.join(python_version)
    assert cpu in ('x32', 'x64')
    py = f'py -{python_version}-{cpu[1:]}'
    return cpu, python_version, py


def make_windows(
        sdist,
        abis=None,
        out_dir=None,
        ):
    '''
    Builds Python wheels on Windows.

    sdist:
        Pathname of sdist; we extract it and generate wheels inside the
        extracted tree. Note that this means that running a second time with
        the same sdist will not be a clean build.

    abis:
        List of <cpu>-<pythonversion> strings.
            cpu:
                'x32' or 'x64'
            pythonversion:
                String version number, may contain '.' characters, e.g. '3.8'
                or '39'.
    out_dir:
        Directory into which we copy the generated wheels. Defaults to
        'pypackage-out'.

    Returns wheels, a list of wheel pathnames.
    '''
    log(f'sdist={sdist} abis={abis} out_dir={out_dir}')
    assert windows()
    if abis is None:
        abis = ['x32-38', 'x32-39', 'x64-38', 'x64-39']
    if out_dir is None:
        out_dir = 'pypackage-out'

    # Extact sdist and run setup.py to build wheel inside the extracted
    # tree. We make basic checks that extraction will extract to a single
    # subdirectory.
    #
    log(f'Extracting sdist={sdist}')
    with tarfile.open(sdist, 'r:gz') as t:
        items = t.getnames()
        assert items
        item = items[0]
        #log(f'item={item}')
        assert not item.startswith('.')
        s = item.find('/')
        assert s > 0
        prefix = item[:s+1]
        for item in items:
            assert item.startswith(prefix)
        t.extractall()
        directory = prefix

    os.makedirs(out_dir, exist_ok=True)
    out_dir2 = os.path.relpath(os.path.abspath(out_dir), directory)

    wheels = []
    for abi in abis:
        cpu, python_version, py = windows_python_from_abi(abi)
        t = time.time()
        log(f'*** Running venv_run() with directory={directory} os.getcwd()={os.getcwd()}')
        venv_run(
                [
                f'pwd',
                f'python setup.py -d {out_dir2} bdist_wheel',
                ],
                py=py,
                directory=directory,
                prefix=f'{python_version}-{cpu} wheel build: ',
                )
        wheel = find_new_file(f'{out_dir}/*.whl', t)
        check_wheel(wheel)
        wheels.append(wheel)

    return wheels


def test_internal(test_command, package_name, pip_install_arg, py):
    if test_command == '.':
        test_command = ''

    jlib.log('Testing {package_name=} {pip_install_arg=} {py=}')
    if not test_command:
        test_command = 'pypackage_test.py'
        code = (
                f'import {package_name}\n'
                f'print("Successfully imported {package_name}")\n'
                )
        log(f'Testing default code for package_name={package_name}:\n{code}')
        with open(test_command, 'w') as f:
            f.write(code)

    venv_run(
            [
            f'pip install {pip_install_arg}',
            f'python {test_command}',
            ],
            venv='pypackage-venv-test',
            py=py,
            clean=True,
            )

def test_local(test_command, wheels, py):
    # Find wheel matching tag output by running <py> ourselves with --tag
    # inside a venv running <py>:
    log(f'Finding wheel tag for {py!r}')
    text = venv_run(
            f'python {sys.argv[0]} tag',
            return_output=True,
            venv='pypackage-venv-test',
            py=py,
            clean=True,
            pip_upgrade=False,  # Saves a little time.
            )
    m = re.search('tag: (.+)', text)
    assert m, f'Failed to find expected tag: ... in output text: {text!r}'
    tag = m.group(1).strip()    # Sometimes we get \r at end, so remove it here.
    log(f'Looking for wheel matching tag {tag!r}')
    for wheel in wheels:
        name, version, py_, none, cpu = parse_wheel(wheel)
        if '-'.join((py_, none, cpu)) == tag:
            log( f'Matching wheel: {wheel}')
            package_name = name
            pip_install_arg = wheel
            break
    else:
        text = f'Cannot find wheel matching: tag={tag!r}\n'
        text += f'Wheels are ({len(wheels)}):\n'
        for wheel in wheels:
            text += f'    {wheel}\n'
        assert 0, text
    test_internal(test_command, package_name, pip_install_arg, py)


def test_pypi(test_command, package_name, pypi_test, py):
    assert package_name, f'Cannot test installation from pypi.org because no package name specified.'
    pip_install_arg = ''
    if pypi_test:
        pip_install_arg += '-i https://test.pypi.org/simple '
    pip_install_arg += package_name
    test_internal(test_command, package_name, pip_install_arg, py)


def test(test_command, package_name, wheels, abis, pypi, pypi_test, py):
    '''
    If on Windows and <py> is false, we run test() with a python for
    each wheel in <wheels>.

    test_command:
        .
    package_name:
        Can be None if we are using <wheels>.
    wheels:
        List of wheels, used if pypi is false.
    pypi:
        If true, install from pypi.org or test.pypi.org if pypi_test is true.
    pypi_test:
        If true, pypi installs from test.pypi.org.
    py:
        Python command. If None, on Windows we test ABI from each wheel,
        otherwise we use native python.
    '''
    if not py and windows():
        if wheels:
            # Test with each wheel.
            log('Testing for python implied by each wheel.')
            for wheel in wheels:
                name, version, py, none, cpu = parse_wheel(wheel)
                pyv = f'{py[2]}.{py[3]}'
                cpu_bits = 64 if cpu == 'win_amd64' else 32
                py = f'py -{pyv}-{cpu_bits}'
                if package_name is None:
                    package_name = name
                if pypi:
                    test_pypi(test_command, package_name, pypi_test, py)
                else:
                    test_local(test_command, wheels, py)
        else:
            assert pypi, f'No wheels specified; need to use *pypi.org.'
            assert package_name, f'No wheels specified; need package_name.'
            # Test with each ABI.
            for abi in abis:
                cpu, python_version, py = windows_python_from_abi(abi)
                jlib.log('Testing with {package_name=} {pypi_test=} {py=}')
                test_pypi(test_command, package_name, pypi_test, py)
    else:
        if not py:
            if windows():
                log('Using default python "py".')
                py = 'py'
            else:
                log('Using default python {sys.executable=}.')
                py = sys.executable
        if pypi:
            test_pypi(test_command, package_name, pypi_test, py)
        else:
            test_local(test_command, wheels, py)


def parse_remote(remote):
    '''
    remote:
        [<user>@]<host>:[<directory>]

    Returns <user>@, <host>, <directory>.

    directory will have '/' appended if
    not already present.

    user: if not '', will end with '@'.
    host>: does not end with ':'.
    directory: if not '', will end with '/'. Does not start with ':'.
    '''
    m = re.match('^(([^@]+@))?([^:]+):(.*)$', remote)
    assert m, f'Expected [<user>@]<hostname>:[<directory>] but: {remote!r}'
    user, host, directory = m.group(1), m.group(3), m.group(4)
    if user is None:
        user = ''
    if directory is None:
        directory = ''
    if directory and not directory.endswith('/'):
        directory += '/'
    return user, host, directory


def wheels_for_sdist(sdist, outdir):
    '''
    Returns list of paths of wheels in <outdir> that match <sdist>.
    '''
    m = re.match('^([^-]+)-([^-]+)[.]tar.gz$', os.path.basename(sdist))
    assert m, f'Bad sdist name, expected .../<name>-<version>.tar.gz: {sdist!r}'
    name_version = f'{m.group(1)}-{m.group(2)}-'
    ret = []
    for i in os.listdir(outdir):
        if i.startswith(name_version) and i.endswith('.whl'):
            ret.append(os.path.join(outdir, i))
    return ret


def parse_sdist(sdist):
    '''
    Parses leafname of sdist, returning (name, version).
    '''
    m = re.match('^([^-]+)-([^-]+)[.]tar.gz$', os.path.basename(sdist))
    assert m, f'Unable to parse sdist: {sdist!r}'
    return m.group(1), m.group(2)


def parse_wheel(wheel):
    m = re.match('^([^-]+)-([^-]+)-([^-]+)-([^-]+)-([^-]+)[.]whl$', os.path.basename(wheel))
    assert m, f'Cannot parse wheel path: {wheel}'
    name, version, py, none, cpu = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
    return name, version, py, none, cpu


def main():

    sdist = None
    wheels = []
    pypi_test = True
    abis = None
    outdir = 'pypackage-out'
    remote = None
    if windows():
        abis = ['x32-38', 'x32-39', 'x64-38', 'x64-39']
    else:
        abis = ['37', '38', '39']
        manylinux_container_name = None
        manylinux_install_docker = False
        manylinux_docker_image = None
        manylinux_pull_docker_image = None

    parser = jlib.Arg('', required=1, help=__doc__,
            subargs=[
                jlib.Arg('abis <abis>',
                        help=f'''
                        Set ABIs to build, comma-separated. Default is {abis}.
                        ''',
                        ),
                jlib.Arg('build', help='Build wheels.', multi=True,
                        subargs = [
                            jlib.Arg('-r <uri>',
                                    help='''
                                    Build on specified remote machine
                                    [<user>@]<host>:[<directory>] and copy them
                                    back to local machine.
                                    ''',
                                    ),
                            jlib.Arg('-a <abis>',
                                    help='Set ABIs to build remotely, comma-separated.'
                                    ),
                            jlib.Arg('-t',
                                    help='''
                                    Run basic "test ." import test. On Windows
                                    this is is done for each wheel we have
                                    built by running "py <abi>"; otherwise we
                                    only test with native python.
                                    ''',
                                    ),
                            ],
                        ),
                jlib.Arg('pypi-test <t:int>',
                        help='Whether to use test.pypi.org.',
                        ),
                jlib.Arg('remote', multi=True,
                        help='''
                        Sync to and run pypackage.py on remote machine.
                        ''',
                        subargs=[
                            jlib.Arg('-s <files>',
                                    help='''
                                    Comma-separated files to sync to remote
                                    machine. (pypackage.py is always synced.)
                                    ''',
                                    ),
                            jlib.Arg('-a <args>',
                                    help='''
                                    Args to pass to pypackage.py on remote
                                    machine.
                                    ''',
                                    ),
                            jlib.Arg('<uri>', required=1,
                                    help='''
                                    The remote machine:
                                    [<user>@]<host>:[<directory>]
                                    ''',
                                    ),
                            ],
                        ),
                jlib.Arg('sdist <sdist|package-dir>',
                        help='''
                        Name of preexisting sdist file to use or Python package
                        directory (e.g. containing setup.py) in which to build
                        a new sdist.
                        ''',
                        ),
                jlib.Arg('tag', help='Internal use only.'),
                jlib.Arg('test',
                        help='''
                        Run test programme. If <command> is '.' or '', we
                        instead run a temporary test programme that imports the
                        package. Uses list of wheels from "wheels <pattern>"
                        or "build ...", otherwise uses ABIs (default or as
                        specified with 'abis ...').
                        ''',
                        subargs = [
                            jlib.Arg('-p <python>',
                                    help='''
                                    Set python to run. If not specified, on
                                    Windows we test with each wheel/abi's
                                    matching python, otherwise we use default
                                    python.
                                    ''',
                                    ),
                            jlib.Arg('--pypi <package-name>',
                                    help='''
                                    Install specified package from pypi before
                                    running test; otherwise we install from
                                    local wheels.
                                    ''',
                                    ),
                            jlib.Arg('<command>', required=1,
                                    help='The test command to run.',
                                    ),
                            ],
                        ),
                jlib.Arg('upload', help='Upload sdist and wheels to pypi.'),
                jlib.Arg('wheels <pattern>',
                        help='''
                        Specify pre-existing wheels using glob pattern. A "*"
                        is appended if does not end with ".whl".
                        ''',
                        ),
                ],
            )

    args = parser.parse(sys.argv[1:])

    # Need to handle args in particular order because some depend on others.
    #
    if args.abis:
        abis_prev = abis
        abis = args.abis.split(',')
        log(f'Changing abis from {abis_prev} to {abis}')

    if args.sdist:
        if os.path.isfile(args.sdist):
            parse_sdist(args.sdist)
            sdist = args.sdist
        else:
            package_directory = args.sdist
            sdist = make_sdist(package_directory, outdir)

    if args.pypi_test:
        pypi_test = int(args.pypi_test)

    for build in args.build:
        assert sdist, f'build requires sdist'
        if build.r:
            # Remote build.
            user, host, directory = parse_remote(build.r)
            local_dir = os.path.dirname(__file__)
            command = (''
                    f'rsync -aP {local_dir}/pypackage.py {local_dir}/jlib.py {sdist} {user}{host}:{directory}'
                    f' && echo rsync finished'
                    f' && ssh {user}{host} '
                    f'"'
                    f'{"cd "+directory if directory else "true"}'
                    f' && ./pypackage.py sdist {os.path.basename(sdist)}'
                    )
            if build.a:
                command += f' abis {build.a}'

            command += (
                    f' build'
                    )

            if build.t:
                # Also run basic import test.
                command += " -t"

            command += (
                    f'"'
                    f' && rsync -ai {user}{host}:{directory}pypackage-out/ {outdir}/'
                    )
            system(command, prefix=f'{user}{host}:{directory}: ')

        else:
            # Local build.
            if windows():
                wheels = make_windows(sdist, abis, outdir)
            else:
                wheels = make_unix(
                        sdist,
                        abis,
                        outdir,
                        test_direct_install = False,
                        install_docker = manylinux_install_docker,
                        docker_image = manylinux_docker_image,
                        pull_docker_image = manylinux_pull_docker_image,
                        container_name = manylinux_container_name,
                        )
            if build.t:
                # Run basic import test.
                package_name, _ = parse_sdist(sdist)
                test('', package_name, wheels, abis, pypi=False, pypi_test=None, py=None)

            log(f'sdist: {sdist}')
            for wheel in wheels:
                log(f'    wheel: {wheel}')

    if args.tag:
        print(f'tag: {make_tag()}')

    if args.wheels:
        pattern = args.wheels
        if not pattern.endswith('.whl'):
            pattern += '*'
            log(f'Have appended "*" to get pattern={pattern!r}')
        wheels_raw = glob.glob(pattern)
        if not wheels_raw:
            log(f'Warning: no matches found for wheels pattern {pattern!r}.')
        wheels = []
        for wheel in wheels_raw:
            if wheel.endswith('.whl'):
                wheels.append(wheel)
        log(f'Found {len(wheels)} wheels with pattern {pattern}:')
        for wheel in wheels:
            log(f'    {wheel}')

    if args.test:
        pypi = False
        package_name = None
        if args.test.pypi:
            pypi = True
            package_name = args.test.pypi
        test(args.test.command, package_name, wheels, abis, pypi, pypi_test, py=args.test.p)

    if args.upload:
        assert sdist, f'Cannot upload because no sdist specified; use "sdist ...".'
        wheels = wheels_for_sdist(sdist, outdir)
        log(f'Uploading wheels ({len(wheels)} for sdist: {sdist!r}')
        for wheel in wheels:
            log(f'    {wheel}')
        venv_run([
                f'pip install twine',
                f'python -m twine upload --disable-progress-bar {"--repository testpypi" if pypi_test else ""} {sdist} {" ".join(wheels)}',
                ],
                bufsize=0,  # So we see login/password prompts.
                )

    for remote in args.remote:
        user, host, directory = parse_remote(remote.uri)
        local_dir = os.path.dirname(__file__)
        sync_files = f'{local_dir}/pypackage.py,{local_dir}/jlib.py'
        if remote.s:
            sync_files += f',{remote.s}'
        system(
                f'rsync -ai {sync_files.replace(",", " ")} {sdist if sdist else ""} {user}{host}:{directory}',
                prefix=f'rsync to {user}{host}:{directory}: ',
                )
        if remote.a:
            remote_args = remote.a
            system(
                    f'ssh {user}{host} '
                    f'"'
                    f'{"cd "+directory+" && " if directory else ""}'
                    f'./pypackage.py {remote_args}'
                    f'"'
                    ,
                    prefix=f'{user}{host}:{directory}: ',
                    )


if __name__ == '__main__':
    try:
        main()
    except Exception:
        sys.stderr.write(jlib.exception_info())
        sys.exit(1)
