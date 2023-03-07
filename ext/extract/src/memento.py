#!/usr/bin/env python3

'''
Post-processor for Memento.

Usage:
    memento.py <args> [<command> ...]

Args:
    -q <quiet>
        Controls how often we output 'Memory squeezing @ ...' lines. E.g. '-q
        10' outputs for multiples of 10.

If <command> is specified we run it and look at the output. Otherwise we assume
that Memento output is available on our stdin.
'''

import os
import re
import subprocess
import sys


def main():
    quiet = 1
    quiet_next = 0
    out_raw = None
    command = None
    args = iter(sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except StopIteration:
            break
        if arg == '-h':
            print(__doc__)
        elif arg == '-o':
            out_raw = open(next(args), 'w')
        elif arg == '-q':
            quiet = int(next(args))
        elif arg.startswith('-'):
            raise Exception(f'unrecognised arg: {arg}')
        else:
            command = arg
            for a in args:
                command += f' {a}'

    if command:
        print(f'Running: {command}')
        child = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                shell=True,
                text=True,
                )
        stdin = child.stdout
    else:
        stdin = sys.stdin

    openbsd = os.uname()[0] == 'OpenBSD'
    n = None
    segv = 0
    leaks = 0
    lines = []
    for line in stdin:
        if out_raw:
            out_raw.write(line)
        m = re.match('^Memory squeezing @ ([0-9]+)( complete)?', line)
        if m:
            if not m.group(2):
                # Start of squeeze.

                if 0 and not openbsd:
                    # Looks like memento's forked processes might terminate
                    # before they get to output the 'Memory squeezing @ <N>
                    # complete' line.
                    #
                    assert n is None, f'n={n} line={line!r}'

                n = int(m.group(1))
                if n >= quiet_next:
                    sys.stdout.write(f'quiet_next={quiet_next!r} n={n!r}: {line}')
                    sys.stdout.flush()
                    quiet_next = (n + quiet) // quiet * quiet
            else:
                # End of squeeze.
                assert n == int(m.group(1))
                # Output info about any failure:
                if segv or leaks:
                    print(f'Failure at squeeze {n}: segv={segv} leaks={leaks}:')
                    for l in lines:
                        if l.endswith('\n'):
                            l = l[:-1]
                        print(f'    {l}')
                    if command:
                        print(f'Examine with: MEMENTO_FAILAT={n} {command}')
                lines = []
                segv = 0
                leaks = 0
                n = None
        else:
            if n is not None:
                lines.append(line)
                if line.startswith('SEGV at:'):
                    segv = 1
                if line.startswith('Allocated blocks'):
                    leaks = 1


if __name__ == '__main__':
    main()
