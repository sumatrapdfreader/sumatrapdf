#!/usr/bin/env python

import sys
import urllib2
from util import load_config
import s3


def getch_unix():
    import sys, tty, termios
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch


def getch_win():
    import msvcrt
    return msvcrt.getch()


def discover_getch():
    try:
        import msvcrt
        return getch_win
    except ImportError:
        return getch_unix


getch = discover_getch()


def usage_and_exit():
    print("usage: update_auto_update_ver.py $ver")
    sys.exit(1)


def report_invalid_ver(ver):
    print("'%s' is not a valid program version" % ver)
    sys.exit(1)

def is_num(s):
    try:
        n = int(s)
        if str(n) != s:
            return False
    except:
        return False
    return True


def validate_ver(ver):
    parts = ver.split(".")
    for p in parts:
        if not is_num(p):
            report_invalid_ver(ver)


def get_url(url):
    try:
        s = urllib2.urlopen(url).read()
        return s.strip()
    except:
        return None


def v2fhelper(v, suff, version, weight):
    parts = v.split(suff)
    if 2 != len(parts):
        return v
    version[4] = weight
    version[5] = parts[1]
    return parts[0]

# Convert a Mozilla-style version string into a floating-point number
#   1.2.3.4, 1.2a5, 2.3.4b1pre, 3.0rc2, etc
def version2float(v):
    version = [
        0, 0, 0, 0, # 4-part numerical revision
        4, # Alpha, beta, RC or (default) final
        0, # Alpha, beta, or RC version revision
        1  # Pre or (default) final
    ]
    parts = v.split("pre")
    if 2 == len(parts):
        version[6] = 0
        v = parts[0]

    v = v2fhelper(v, "a",  version, 1)
    v = v2fhelper(v, "b",  version, 2)
    v = v2fhelper(v, "rc", version, 3)

    parts = v.split(".")[:4]
    for (p, i) in zip(parts, range(len(parts))):
        version[i] = p
    ver = float(version[0])
    ver += float(version[1]) / 100.
    ver += float(version[2]) / 10000.
    ver += float(version[3]) / 1000000.
    ver += float(version[4]) / 100000000.
    ver += float(version[5]) / 10000000000.
    ver += float(version[6]) / 1000000000000.
    return ver


# Return True if ver1 > ver2 using semantics of comparing version
# numbers
def ProgramVersionGreater(ver1, ver2):
    v1f = version2float(ver1)
    v2f = version2float(ver2)
    return v1f > v2f


def verify_version_not_lower(myver, curr1, curr2):
    if curr1 != None and ProgramVersionGreater(curr1, myver):
        print("version you gave is less than sumpdf-latest.txt (%s < %s)" % (myver, curr1))
        sys.exit(1)
    if curr2 != None and ProgramVersionGreater(curr2, myver):
        print("version you gave is less than sumpdf-latest.txt (%s < %s)" % (myver, curr2))
        sys.exit(1)

def main():
    args = sys.argv[1:]
    if len(args) == 0:
        usage_and_exit()
    ver = args[0]
    validate_ver(ver)
    url1 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-latest.txt"
    url2 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-latest-manual.txt"

    conf = load_config()
    assert conf.aws_access != "" and conf.aws_secret != ""
    s3.set_secrets(conf.aws_access, conf.aws_secret)
    s3.set_bucket("kjkpub")

    v1 = get_url(url1)
    v2 = get_url(url2)
    verify_version_not_lower(ver, v1, v2)
    sys.stdout.write("Going to update auto-update version to %s. Are you sure? [y/N] " % ver)
    sys.stdout.flush()
    ch = getch()
    print()
    if ch not in ['y', 'Y']:
        print("Didn't update because you didn't press 'y'")
        sys.exit(1)

    s = "%s\n" % ver
    s3.upload_data_public(s, "sumatrapdf/sumpdf-latest-manual.txt")
    s3.upload_data_public(s, "sumatrapdf/sumpdf-latest.txt")
    v1 = get_url(url1)
    v2 = get_url(url2)
    if v1 != ver or v2 != ver:
        print("Upload failed because v1 or v2 != ver ('%s' or '%s' != '%s'" % (v1, v2, ver))
        sys.exit(1)
    print("Successfully update auto-update version to '%s'" % ver)


if __name__ == "__main__":
    main()
