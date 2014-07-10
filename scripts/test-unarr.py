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



def test_unarr(dir):
    global files_tested, files_failed
    print("Directory: %s" % dir)
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
            files_failed.append(p)
            files_failed.append(out)
            files_failed.append(err)
            print("%s failed with out: '%s' err: '%s'" % (p, out, err))
        files_tested += 1
        if files_tested % 100 == 0:
            print("tested %d files" % files_tested)


def dump_failures():
    global files_failed, files_tested
    i = 0
    while i < len(files_failed):
        p = files_failed[i]
        out = files_failed[i+1]
        err = files_failed[i+2]
        i += 3
        print("%s failed with out: '%s' err: '%s'" % (p, out, err))
    print("Failed %d out of %d" % (len(files_failed)/3, files_tested))


def main():
    detect_unarr_exe()  # detect early if doesn't exist
    if len(sys.argv) != 2:
        usage_and_exit()
    test_unarr(sys.argv[1])
    dump_failures()


if __name__ == "__main__":
    main()
