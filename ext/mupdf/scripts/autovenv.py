'''
Automatic creation/use of a venv.

Example usage:

    import autovenv
    autovenv.enter(create=3, packages=['foopackage', 'barpackage'])
    import foomodule
    import barmodule
    ...
'''

import os
import platform
import shlex
import shutil
import subprocess
import sys
import sysconfig
import time


def log(text, verbose, t0):
    debug = False
    if debug or verbose:
        text = f'autovenv [+{time.time()-t0:.1f}]: {text}'
    if verbose:
        print(text, flush=1)
    if debug:
        with open('autoenv-env-log.txt', 'a') as f:
            print(text, file=f)


def run(command, verbose, t0, check=1, batch=True):
    log(f'Running {batch=}: {command}', verbose, t0)
    if batch:
        def write_out(text):
            for line in text.split('\n'):
                log(line, verbose=True, t0=t0)
        try:    # pylint: disable=no-else-raise
            cp = subprocess.run(command, shell=1, check=check, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except subprocess.SubprocessError as e:
            text = e.stdout # pylint: disable=no-member
            # Docs say that e.stdout is always bytes, but doesn't seem to be the case?
            if isinstance(text, bytes):
                text = text.decode('utf8', errors='replace')
            write_out(text)
            raise
        else:
            text = cp.stdout
            write_out(text)
            return cp
    else:
        cp = subprocess.run(command, shell=1, check=check)
    return cp


def gil():
    Py_GIL_DISABLED = sysconfig.get_config_var('Py_GIL_DISABLED')
    #print(f'{Py_GIL_DISABLED=}')
    if Py_GIL_DISABLED==1:
        gil_enabled = sys._is_gil_enabled() # pylint:disable=protected-access
        #print(f'{gil_enabled=}')
        if not gil_enabled:
            return False
    return True


def bits():
    return int.bit_length(sys.maxsize+1)


def shlex_quote(arg):
    '''
    On Windows we simply enclose <arg> within "..." or raise an exception if
    <arg> contains double-quotes.

    Otherwise we simply use shlex.quote().
    '''
    if platform.system() == 'Windows':
        if '"' in arg:
            raise Exception(f'Cannot quote {arg=}.')
        return f'"{arg}"'
    else:
        return shlex.quote(arg)


def shlex_join(argv):
    '''
    Like shlex.join(), but on Windows we use shlex_quote().
    '''
    if platform.system() == 'Windows':
        argv = [shlex_quote(arg) for arg in argv]
        return ' '.join(argv)
    else:
        return shlex.join(argv)


def enter(*,
        venv_name=None,
        venv_prefix='autovenv',
        create=None,
        packages=None,
        verbose=False,
        ):
    '''
    Creates and re-runs inside a venv if we are not already in a venv.

    If we are already in a venv, we run `pip install --upgrade` for each item
    in <packages> and return.

    Otherwise we do the following:

    * Create the venv if required.
    * In the venv, run `pip install --upgrade` for each item in <packages>.
    * Create a child process that runs `python <sys.args>` inside the venv.
    * Call `sys.exit()` with termination code of child process.
    * We do not return.

    Args:

        venv_name:
            Name of venv. If false, we use:
                <venv_prefix>-<python-version><T>-<bits>
            Where <T> is '-t' if free-thread else ''.
        venv_prefix:
            Used if <venv_name> is false.
        create:
            One of:
                1: Only run `python -m venv <venv_name>` and install packages
                   if the <venv_name> directory does not exist.
                2: Always run `python -m venv <venv_name>`.
                3: Delete any existing venv and then run `python -m venv <venv_name>`.
            If None, we use create=2.
        packages:
            String or list of packages, passed directly to `pip install`.
        verbose:
            If true we output diagnostics when running commands.
    '''
    t0 = time.time()

    if isinstance(packages, str):
        packages = (packages,)
    packages = packages or list()

    if create is None:
        create = 2
    assert create in (1, 2, 3), f'Unrecognised {create=}, should be 1, 2 or 3.'

    if sys.prefix != sys.base_prefix:
        log(f'Already in a venv, {sys.prefix=}.', verbose, t0)
        for package in packages:
            if package:
                run(f'pip install --upgrade {shlex_quote(package)}', verbose, t0)
        return

    # We are not in a venv.
    log(f'Not in a venv.', verbose, t0)

    # Set venv name.
    if not venv_name:
        if not venv_prefix:
            venv_prefix = f'autovenv'
        venv_name = f'{venv_prefix}-{platform.python_version()}{"" if gil() else "-t"}-{bits()}'

    # Create venv.
    if create == 3:
        # Delete any existing venv.
        if os.path.isdir(venv_name):
            shutil.rmtree(venv_name, ignore_errors=1)
        assert not os.path.exists(venv_name)

    if create == 1 and os.path.isdir(venv_name):
        # Don't recreate existing venv or install packages.
        packages = list()
    else:
        # Create venv.
        run(f'{shlex_quote(sys.executable)} -m venv {venv_name}', verbose, t0)

    # Get command to enter venv.
    if platform.system() == 'Windows':
        venv_enter = f'{venv_name}\\Scripts\\activate'
    else:
        venv_enter = f'. {venv_name}/bin/activate'

    # Install packages.
    for package in (packages or list()):
        if package:
            run(f'{venv_enter} && pip install --upgrade {shlex_quote(package)}', verbose, t0)

    # Rerun ourselves in the venv.
    command = f'{venv_enter} && python {shlex_join(sys.argv)}'

    cp = run(command, verbose, t0, check=0, batch=0)
    sys.exit(cp.returncode)
