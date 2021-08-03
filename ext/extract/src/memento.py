#!/usr/bin/env python3

'''
Post-processor for Memento.

Args:
    -q <quiet>
        Controls how often we output 'Memory squeezing @ ...' lines. E.g. '-q
        10' outputs for multiples of 10.
'''

import os
import re
import sys


def main():
    quiet = 1
    out_raw = None
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
        else:
            raise Exception(f'unrecognised arg: {arg}')
    
    openbsd = os.uname()[0] == 'OpenBSD'
    n = None
    segv = 0
    leaks = 0
    lines = []
    for line in sys.stdin:
        if out_raw:
            out_raw.write(line)
        m = re.match('^Memory squeezing @ ([0-9]+)( complete)?', line)
        if m:
            if not m.group(2):
                # Start of squeeze.
                
                if not openbsd:
                    # Looks like memento's forked processes might terminate
                    # before they get to output the 'Memory squeezing @ <N>
                    # complete' line.
                    #
                    assert n is None, f'n={n} line={line!r}'
                
                n = int(m.group(1))
                if n % quiet == 0:
                    sys.stdout.write(line)
                    sys.stdout.flush()
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
