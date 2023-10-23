#! /usr/bin/env python3

'''
Builds MuPDF docs.

* Takes care of using a venv and installing required packages and running
  `sphinx-build`.
* Works on Unix and Windows.
* Currently only builds HTML docs.
* Arguments are handled in the order in which they occur on the command line.
* Generated docs start at: build/docs/html/index.html

Args:
    build:
        Build docs:
        * Create and enter a Python venv.
        * Install packages as specified in file `docs/src/requirements.txt`.
        * Run `sphinx-build`.
    clean:
        Removes generated directory.
    -h
    --help
        Show this help.
    --pip 0|1
        If 0, we do not create the venv or install packages into it. Instead we
        assume that the venv has already been created and contains the required
        Python packages. This runs slightly faster.

If no args are given, we do `build`.

Example timings:
    60s     Clean build (including creating venv and installing packages).
    10s     Rebuild with no changes.
     3.3s   Rebuild with no changes and --pip 0.
'''

import shutil
import os
import platform
import subprocess
import sys


def main():

    pip = True

    if len(sys.argv) == 1:
        sys.argv.append('build')

    args = iter(sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except StopIteration:
            break
        if arg in ('-h', '--help'):
            print(__doc__.strip())
        elif arg == 'clean':
            print(f'Removing: {dir_out}')
            shutil.rmtree(dir_out, ignore_errors=True)
        elif arg == 'build':
            build(pip)
        elif arg == '--pip':
            pip = int(next(args))


def build(pip):

    windows = platform.system() == 'Windows'
    openbsd = platform.system() == 'OpenBSD'
    macos = platform.system() == 'Darwin'

    root = os.path.relpath(os.path.abspath(f'{__file__}/../..'))
    dir_in = f'{root}/docs/src'
    dir_out = f'{root}/build/docs'
    dir_venv = f'{root}/build/docs/venv'

    # We construct a command that does everything we need to do.
    #
    command = 'true'

    # Create venv.
    #
    print(f'Using Python venv: {dir_venv}')
    if pip:
        command += f' && {sys.executable} -m venv {dir_venv}'
    else:
        assert os.path.isdir(dir_venv), f'Python venv directory does not exist: {dir_venv}'

    # Activate the venv.
    #
    if windows:
        # Most of the time we can use `/` on Windows, but for activating a venv
        # we need to use `os.sep`.
        command += f' && ./{dir_venv}/Scripts/activate'.replace('/', os.sep)
    else:
        command += f' && . {dir_venv}/bin/activate'

    # Install required packages.
    #
    if pip:
        command += f' && python -m pip install --upgrade pip'
        command += f' && python -m pip install'
        with open(f'{dir_in}/requirements.txt') as f:
            for line in f:
                package = line.strip()
                if package.startswith('#') or not package:
                    continue
                if (openbsd or macos) and package == 'rst2pdf':
                    print(f'Not installing on OpenBSD/MacOS because install fails: {package}')
                    continue
                command += f' {package}'

    # Run sphinx-build.
    #
    command += f' && sphinx-build -M html {dir_in} {dir_out}'

    # Run command.
    #
    print(f'Running command: {command}')
    sys.stdout.flush()
    subprocess.run(command, shell=True, check=True)

    print(f'')
    print(f'sphinx-build succeeded.')
    print(f'Have built documentation:')
    print(f'    {dir_out}/html/index.html')


if __name__ == '__main__':
    main()
