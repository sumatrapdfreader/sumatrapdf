import sys
import os
import subprocess
import util2


def usage_and_exit():
    print("Usage: test-unrar.py dir")
    sys.exit(1)


@util2.memoize
def detect_unarr_exe():
    p = os.path.join("obj-rel", "unarr.exe")
    if os.path.exists(p):
        return p
    p = os.path.join("obj-dbg", "unarr.exe")
    if os.path.exists(p):
        return p
    print("didn't find unarr.exe in obj-rel or obj-dbg. Run scripts/build-unarr.bat first.")
    sys.exit(1)


def should_test_file(f):
    exts = [".rar", ".zip", ".cbz", ".cbr", ".epub", ".xps"]
    f = f.lower()
    for ext in exts:
        if f.endswith(ext):
            return True
    return False


files_tested = 0
files_failed = []
fo = None

# Apparently shell argument to Popen it must be False on unix/mac and True
# on windows
def shell_arg():
    if os.name == "nt":
        return True
    return False


def subprocess_flags():
    # this magic disables the modal dialog that windows shows if the process crashes
    # TODO: it doesn't seem to work, maybe because it was actually a crash in a process
    # sub-launched from the process I'm launching. I had to manually disable this in
    # registry, as per http://stackoverflow.com/questions/396369/how-do-i-disable-the-debug-close-application-dialog-on-windows-vista:
    # DWORD HKLM or HKCU\Software\Microsoft\Windows\Windows Error Reporting\DontShowUI = "1"
    # DWORD HKLM or HKCU\Software\Microsoft\Windows\Windows Error Reporting\Disabled = "1"
    # see: http://msdn.microsoft.com/en-us/library/bb513638.aspx
    if sys.platform.startswith("win"):
        import ctypes
        SEM_NOGPFAULTERRORBOX = 0x0002  # From MSDN
        ctypes.windll.kernel32.SetErrorMode(SEM_NOGPFAULTERRORBOX)
        return 0x8000000  # win32con.CREATE_NO_WINDOW?
    return 0


# will throw an exception if a command doesn't exist
# otherwise returns a tuple:
# (stdout, stderr, errcode)
def run_cmd(*args):
    cmd = " ".join(args)
    cmdproc = subprocess.Popen(args, shell=shell_arg(), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, creationflags=subprocess_flags())
    res = cmdproc.communicate()
    return (res[0], res[1], cmdproc.returncode)


def strip_empty_lines_and_dedup(s):
    lines = []
    for l in s.splitlines():
        l = l.strip()
        if len(l) == 0:
            continue
        if l not in lines:
            lines.append(l)
    return "\n".join(lines)


def get_file_size(p):
    try:
        si = os.stat(p)
        return si.st_size
    except:
        return 0


def test_unarr(dir):
    global files_tested, files_failed, fo
    #print("Directory: %s" % dir)
    unarr = detect_unarr_exe()
    files = os.listdir(dir)
    for f in files:
        p = os.path.join(dir, f)
        if os.path.isdir(p):
            test_unarr(p)
            continue
        if not should_test_file(f):
            continue
        (out, err, errcode) = run_cmd(unarr, p)
        if errcode != 0:
            out = strip_empty_lines_and_dedup(out)
            err = strip_empty_lines_and_dedup(err)
            files_failed.append(p)
            files_failed.append(out)
            files_failed.append(err)
            file_size = get_file_size(p)
            print("%s of %d failed with out: '%s' err: '%s'" % (p, file_size, out, err))
            fo.write("%s of %d failed with out: '%s' err: '%s'\n" % (p, file_size, out, err))
        files_tested += 1
        if files_tested % 100 == 0:
            print("tested %d files" % files_tested)


def dump_failures():
    global files_failed, files_tested, fo
    # print just the names of files failed
    fo.write("\n-------------------------------------------\n")
    n = len(files_failed) / 3
    i = 0
    while i < n:
        p = files_failed[i*3]
        size = get_file_size(p)
        fo.write("%s of %d failed\n" % (p, size))
        i += 1
    print("Failed %d out of %d" % (n, files_tested))
    fo.write("Failed %d out of %d\n" % (n, files_tested))


def main():
    global fo
    detect_unarr_exe()  # detect early if doesn't exist
    if len(sys.argv) != 2:
        usage_and_exit()
    fo = open("unarr_failed.txt", "w")
    test_unarr(sys.argv[1])
    dump_failures()
    fo.close()


if __name__ == "__main__":
    main()
