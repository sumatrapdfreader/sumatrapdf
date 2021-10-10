import sys
import os
import subprocess
import shutil
import util

g_get_files = False
g_show_files = False

def usage_and_exit():
    print("Usage: test-unrar.py dir")
    print("       test-unrar.py summary [file]")
    sys.exit(1)


@util.memoize
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
    exts = [
        ".rar", ".cbr",
        ".zip", ".cbz", ".epub", ".xps", ".fb2z",
        ".7z", ".cb7",
        ".tar", ".cbt"
    ]
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


def strip_empty_lines_and_dedup(s, only_last=-1):
    lines = []
    for l in s.splitlines():
        l = l.strip()
        if len(l) == 0:
            continue
        if l not in lines:
            lines.append(l)
    if only_last != -1:
        lines = lines[-only_last:]
    return "\n".join(lines)


def shorten_err(s):
    return strip_empty_lines_and_dedup(s)


def shorten_out(s):
    return strip_empty_lines_and_dedup(s, 3)


def get_file_size(p):
    try:
        si = os.stat(p)
        return si.st_size
    except:
        return 0


# some errors we don't want to fix
def err_whitelisted(s):
    if "Splitting files isn't really supported" in s:
        return True
    if "Unsupported compression version: 15" in s:
        return True
    if "Encrypted entries will fail to uncompress" in s:
        return True
    return False


def test_unarr(dir):
    global files_tested, files_failed, fo
    #print("Directory: %s" % dir)
    unarr = detect_unarr_exe()
    try:
        files = os.listdir(dir)
    except:
        return
    for f in files:
        p = os.path.join(dir, f)
        if os.path.isdir(p):
            test_unarr(p)
            continue
        if not should_test_file(f):
            continue
        (out, err, errcode) = run_cmd(unarr, p)
        if errcode != 0:
            out = shorten_out(out)
            err = shorten_err(err)
            if err_whitelisted(err):
                continue
            files_failed.append(p)
            files_failed.append(out)
            files_failed.append(err)
            file_size = get_file_size(p)
            print("%s of %d failed with out:\n%s\nerr:\n%s\n" % (p, file_size, out, err))
            fo.write("%s of %d failed with out:\n%s\nerr:\n%s\n\n" % (p, file_size, out, err))
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


def errors_to_sorted_array(errors):
    a = []
    for (err, count) in errors.items():
        a.append([count, err])
    return sorted(a, cmp=lambda x,y: cmp(y[0], x[0]))


def get_files_for_error(error_to_files, err):
    res = []
    files = error_to_files[err]
    for f in files:
        try:
            size = os.path.getsize(f)
            res.append([size, f])
        except:
            pass
    return sorted(res, cmp=lambda x,y: cmp(x[0], y[0]))


def copy_file_here(f, n, m):
    fn, ext = os.path.splitext(f)
    dst = "%2d-%2d%s" % (n, m, ext)
    dst = os.path.join("files", dst)
    shutil.copyfile(f, dst)


def get_all_files(files, n):
    m = 1
    for f in files:
        print(" %6d %s" % (f[0], f[1]))
        copy_file_here(f[1], n, m)
        m += 1


def show_files(files):
    for f in files:
        print(" %6d %s" % (f[0], f[1]))
    print("")
    print("")


def print_errors(arr, error_to_files):
    global g_get_files, g_show_files
    n = 1
    total = 0
    for el in arr:
        print("%s: %d" % (el[1], el[0]))
        files = get_files_for_error(error_to_files, el[1])
        total += el[0]
        if g_get_files or g_show_files:
            show_files(files)
        if g_get_files:
            get_all_files(files, n)
        n += 1
    print("\nTotal: %d" % total)


def extract_file_path(l):
    idx = l.find(" of ")
    if idx == -1:
        return None
    return l[:idx]


def do_summary_on_file(path):
    fo = open(path, "r")
    errors = {}  # map error string to number of failures
    error_to_files = {}
    seen_error = False
    file_path = None
    for l in fo:
        l = l.strip()
        if "failed with out" in l:
            file_path = extract_file_path(l)
        if l == "err:":
            seen_error = False
            continue
        if seen_error:
            continue
        if not l.startswith("!"):
            continue
        seen_error = True
        if file_path is not None and os.path.exists(file_path):
            errors[l] = errors.get(l, 0) + 1
            a = error_to_files.get(l, [])
            a.append(file_path)
            error_to_files[l] = a
    fo.close()
    arr = errors_to_sorted_array(errors)
    print_errors(arr, error_to_files)


def do_summary():
    fn = "unarr_failed.txt"
    if len(sys.argv) > 2:
        fn = sys.argv[2]
    do_summary_on_file(fn)


def do_getfiles():
    global g_get_files
    g_get_files = True
    if not os.path.exists("files"):
        os.makedirs("files")
    do_summary()


def main():
    global fo
    detect_unarr_exe()  # detect early if doesn't exist
    if len(sys.argv) < 2:
        usage_and_exit()
    if sys.argv[1] == "summary":
        do_summary()
        return
    if sys.argv[1] == "getfiles":
        do_getfiles()
        return

    if len(sys.argv) != 2:
        usage_and_exit()
    fo = open("unarr_failed.txt", "w")
    test_unarr(sys.argv[1])
    dump_failures()
    fo.close()


if __name__ == "__main__":
    main()
