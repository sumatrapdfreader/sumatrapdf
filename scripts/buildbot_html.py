import os, string, json, cgi, util, s3
from util import formatInt

g_first_analyze_build = 6000

g_index_html_css = """
<style type="text/css">
#table-5 {
    background-color: #f5f5f5;
    padding: 5px;
    border-radius: 5px;
    -moz-border-radius: 5px;
    -webkit-border-radius: 5px;
    border: 1px solid #ebebeb;
}
#table-5 td, #table-5 th {
    padding: 1px 5px;
}
#table-5 thead {
    font: normal 15px Helvetica Neue,Helvetica,sans-serif;
    text-shadow: 0 1px 0 white;
    color: #999;
}
#table-5 th {
    text-align: left;
    border-bottom: 1px solid #fff;
}
#table-5 td {
    font-size: 14px;
}
#table-5 td:hover {
    background-color: #fff;
}
</style>"""

def a(url, txt): return '<a href="' + url + '">' + txt + '</a>'
def pre(s): return '<pre style="white-space: pre-wrap;">' + s + '</pre>'
def td(s, off=0): return " " * off + '<td>%s</td>' % s
def th(s): return '<th style="font-size:80%%">%s</th>' % s

def size_diff_html(n):
    if n > 0:   return ' (<font color=red>+' + str(n) + '</font>)'
    elif n < 0: return ' (<font color=green>' + str(n) + '</font>)'
    else:       return ''

# given a list of files from s3 in the form ${ver}/${name}, group them
# into a list of lists, [[${ver}, [${name1}, ${name2}]], ${ver2}, [${name1}]] etc.
# we rely that the files are already sorted by ${ver}
def group_by_ver(files):
    res = []
    curr_ver = None
    curr_ver_names = []
    for f in files:
        (ver, name) = f.split("/", 1)
        if ver == curr_ver:
            curr_ver_names.append(name)
        else:
            if curr_ver != None:
                assert(len(curr_ver_names) > 0)
                res.append([curr_ver, curr_ver_names])
            curr_ver = ver
            curr_ver_names = [name]
    if curr_ver != None:
        assert(len(curr_ver_names) > 0)
        res.append([curr_ver, curr_ver_names])
    return res

# TODO: would be nicer if "sumatrapdf_buildbot" wasn't hard-coded, but don't know
# how to get this reliably
g_buildbot_src_path = "sumatrapdf_buildbot\\"

# Turn:
# c:\users\kkowalczyk\src\sumatrapdf\src\utils\allocator.h(156) : warning C6011: Dereferencing NULL pointer 'node'. : Lines: 149, 150, 151, 153, 154, 156
# Into:
# <a href="https://code.google.com/p/sumatrapdf/source/browse/trunk/src/utils/allocator.h#156">src\utils\allocator.h(156)</a>:<br>
# warning C6011: Dereferencing NULL pointer 'node'. : Lines: 149, 150, 151, 153, 154, 156
def htmlize_error_lines(lines, ver):
    if len(lines) == 0: return ([],[],[])
    sumatra_errors = []
    mupdf_errors = []
    ext_errors = []
    for l in lines:
        if g_buildbot_src_path not in l:
            ext_errors.append(l) # system includes
            continue
        rel_path_start = l.find(g_buildbot_src_path) + len(g_buildbot_src_path)
        l = l[rel_path_start:]
        err_start = l.find(" : ")
        src = l[:err_start]
        msg = l[err_start + 3:]
        a = htmlize_src_link(src, ver)
        s = a + " " + msg
        if l.startswith("src\\"):     sumatra_errors.append(s)
        elif l.startswith("mupdf\\"): mupdf_errors.append(s)
        elif l.startswith("ext\\"):   ext_errors.append(s)
        else: ext_errors.append(s) # shouldn't really happen (handled earlier), but...
    return (sumatra_errors, mupdf_errors, ext_errors)

def stats_for_previous_successful_build(ver, stats_for_ver):
    ver = int(ver) - 1
    while True:
        stats = stats_for_ver(str(ver))
        if None == stats: return None
        if not stats.rel_failed: return stats
        ver -= 1

# build sumatrapdf/buildbot/index.html summary page that links to each
# sumatrapdf/buildbot/${ver}/analyze.html
def build_index_html(stats_for_ver, checkin_comment_for_ver):
    s3_dir = "sumatrapdf/buildbot/"
    html = '<html><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>%s</head><body>\n' % g_index_html_css
    html += "<p>SumatraPDF buildbot results:</p>\n"
    names = [n.name for n in s3.list(s3_dir)]
    # filter out top-level files like index.html and sizes.js
    names = [n[len(s3_dir):] for n in names if len(n.split("/")) == 4]
    names.sort(reverse=True, key=lambda name: int(name.split("/")[0]))

    html += '<table id="table-5"><tr>' + th("svn") + th("/analyze")
    html += th("build") + th("tests") + th("SumatraPDF.exe")
    html += th("Installer.exe") + th("efi") + th("checkin comment") + '</tr>\n'
    files_by_ver = group_by_ver(names)
    for arr in files_by_ver[:512]:
        (ver, files) = arr
        if "stats.txt" not in files:
            print("stats.txt missing in %s (%s)" % (ver, str(files)))
            assert("stats.txt" in files)
        try:
            stats = stats_for_ver(ver)
        except:
            print("names: %s" % str(names))
            print("ver:   %s" % str(ver))
            print("files: %s" % str(files))
            raise
        total_warnings = stats.analyze_sumatra_warnings_count + stats.analyze_mupdf_warnings_count + stats.analyze_ext_warnings_count
        if int(ver) >= g_first_analyze_build and total_warnings > 0 and not stats.rel_failed:
            assert("analyze.html" in files)

        s3_ver_url = "http://kjkpub.s3.amazonaws.com/" + s3_dir + ver + "/"
        html += "  <tr>\n"

        # build number
        src_url = "https://code.google.com/p/sumatrapdf/source/detail?r=" + ver
        html += td(a(src_url, ver), 4) + "\n"

        # /analyze warnings count
        if int(ver) >= g_first_analyze_build and total_warnings > 0:
            url = s3_ver_url + "analyze.html"
            s = "%d %d %d" % (stats.analyze_sumatra_warnings_count, stats.analyze_mupdf_warnings_count, stats.analyze_ext_warnings_count)
            html += td(a(url, s), 4)
        else:
            html += td("", 4)

        # release build status
        if stats.rel_failed:
            url =  s3_ver_url + "release_build_log.txt"
            s = '<b>' + a(url, "fail") + '</b>'
        else:
            s = '<font color="green"<b>ok!</b></font>'
        html += td(s, 4) + "\n"

        # tests status
        if "tests_error.txt" in files:
            url =  s3_ver_url + "tests_error.txt"
            s = '<b>' + a(url, "fail") + '</b>'
        else:
            s = '<font color="green"<b>ok!</b></font>'
        html += td(s, 4) + "\n"

        # SumatraPDF.exe, Installer.exe size
        if stats.rel_failed:
            html += td("", 4) + "\n" + td("", 4) + "\n"
        else:
            prev_stats = stats_for_previous_successful_build(ver, stats_for_ver)
            if None == prev_stats:
                num_s = formatInt(stats.rel_sumatrapdf_exe_size)
                html += td(num_s, 4) + "\n"
                num_s = formatInt(stats.rel_installer_exe_size)
                html += td(num_s, 4) + "\n"
            else:
                s = size_diff_html(stats.rel_sumatrapdf_exe_size - prev_stats.rel_sumatrapdf_exe_size)
                num_s = formatInt(stats.rel_sumatrapdf_exe_size)
                s = num_s + s
                html += td(s, 4) + "\n"
                s = size_diff_html(stats.rel_installer_exe_size - prev_stats.rel_installer_exe_size)
                num_s = formatInt(stats.rel_installer_exe_size)
                s = num_s + s
                html += td(s, 4) + "\n"

        # efi diff
        if "efi_diff.txt" in files:
            url = s3_ver_url + "efi_diff.txt"
            html += td(a(url, "diff"), 4)
        else:
            html += td("")

        # checkin comment
        (comment, trimmed) = util.trim_str(checkin_comment_for_ver(ver))
        comment = comment.decode('utf-8')
        comment = cgi.escape(comment)
        if trimmed: comment += a(src_url, "...")
        html += td(comment, 4) + "\n"
        html += "  </tr>\n"
    html += "</table>"
    html += "</body></html>\n"
    return html

g_src_trans_map = None
def rebuild_trans_src_path_cache():
    global g_src_trans_map
    g_src_trans_map = {}
    for root, dirs, files in os.walk("src"):
        for file in files:
            file_path = os.path.join(root, file)
            g_src_trans_map[file_path.lower()] = file_path
    for root, dirs, files in os.walk("ext"):
        for file in files:
            file_path = os.path.join(root, file)
            g_src_trans_map[file_path.lower()] = file_path
    for root, dirs, files in os.walk("mupdf"):
        for file in files:
            file_path = os.path.join(root, file)
            g_src_trans_map[file_path.lower()] = file_path

# for some reason file names are lower-cased and the url has exact case
# we need to convert src_path to have the exact case for urls to work
# i.e. given "src\doc.h" we need to return "src\Doc.h"
def trans_src_path(s):
    if s not in g_src_trans_map:
        #print("%s not in g_src_trans_map" % s)
        #print(g_src_trans_map.keys())
        # can happen for system includes e.g. objbase.h
        return s
    return g_src_trans_map[s]

# Turn:
# src\utils\allocator.h(156)
# Into:
# <a href="https://code.google.com/p/sumatrapdf/source/browse/trunk/src/utils/allocator.h#156">src\utils\allocator.h(156)</a>
def htmlize_src_link(s, ver):
    try:
        parts = s.split("(")
        src_path = parts[0] # src\utils\allocator.h
        src_path = trans_src_path(src_path) # src\utils\Allocator.h
        src_path_in_url = src_path.replace("\\", "/")
        src_line = parts[1][:-1] # "156)"" => "156"
        base = "https://code.google.com/p/sumatrapdf/source/browse/trunk/"
        #url = base + src_path_in_url + "#" + src_line
        url =  base + src_path_in_url + "?r=%s#%s" % (str(ver), str(src_line))
    except:
        print("htmlize_src_link: s: '%s', ver: '%s'" % (str(s), str(ver)))
        return s
    return a(url, src_path + "(" + src_line + ")")

def skip_error(s):
    # C2220 - warning treated as error
    # LNK2019 - linker error unresolved external symbol
    for err in ("C2220", "LNK2019"):
        if err in s: return True
    return False

# given a text generated with /analyze, extract the lines that contain
# error information
def extract_analyze_errors(s):
    errors = []
    for l in s.split('\n'):
        if ": error " in l or ": warning " in l:
            if not skip_error(l) and l not in errors:
                errors.append(l)
    return errors

def gen_analyze_html(stats, ver):
    (sumatra_errors, mupdf_errors, ext_errors) = htmlize_error_lines(extract_analyze_errors(stats.analyze_out), ver)
    stats.analyze_sumatra_warnings_count = len(sumatra_errors)
    stats.analyze_mupdf_warnings_count = len(mupdf_errors)
    stats.analyze_ext_warnings_count = len(ext_errors)
    s = "<html><body>"
    s += a("../index.html", "Home")
    s += ": build %s, %d warnings in sumatra code, %d in mupdf, %d in ext:" % (str(ver), stats.analyze_sumatra_warnings_count, stats.analyze_mupdf_warnings_count, stats.analyze_ext_warnings_count)
    s += pre(string.join(sumatra_errors, ""))
    s += "<p>Warnings in mupdf code:</p>"
    s += pre(string.join(mupdf_errors, ""))
    s += "<p>Warnings in ext code:</p>"
    s += pre(string.join(ext_errors, ""))
    s += "</pre>"
    s += "</body></html>"
    return s

def build_sizes_json(stats_cache_dir, stats_for_ver):
    files = os.listdir(stats_cache_dir())
    versions = [int(f.split(".")[0]) for f in files]
    versions.sort()
    #print(versions)
    sumatra_sizes = []
    installer_sizes = []
    prev_sumatra_size = 0
    prev_installer_size = 0
    for ver in versions:
        stats = stats_for_ver(str(ver))
        sumatra_size = stats.rel_sumatrapdf_exe_size
        installer_size = stats.rel_installer_exe_size
        if sumatra_size == prev_sumatra_size and installer_size == prev_installer_size:
            continue
        sumatra_sizes.append(sumatra_size)
        installer_sizes.append(installer_size)
        prev_sumatra_size = sumatra_size
        prev_installer_size = installer_size
    sumatra_json = json.dumps(sumatra_sizes)
    installer_json = json.dumps(installer_sizes)
    s = "var g_sumatra_sizes = %s;\nvar g_installer_sizes = %s;\n" % (sumatra_json, installer_json)
    return s

