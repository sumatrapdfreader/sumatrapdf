"""
Builds sumatra and uploads results to s3 for easy analysis, viewable at:
http://kjkpub.s3.amazonaws.com/sumatrapdf/buildbot/index.html
"""
import os, shutil, sys, time, string, datetime, json, cPickle, cgi, traceback
import s3, util
from util import file_remove_try_hard, run_cmd_throw
from util import parse_svnlog_out, formatInt
from util import load_config, run_cmd, strip_empty_lines, build_installer_data
from util import verify_path_exists, verify_started_in_right_directory

"""
TODO:
 - at some point the index.html page will get too big, so split it into N-item chunks
   (100? 300?)
 - should also do pre-release builds if there was a new checkin since the last uploaded
   build but is different that than build and there was no checkin for at least 4hr
   (all those rules are to ensure we don't create pre-release builds too frequently)
"""

g_first_analyze_build = 6000

def file_size(p):
  return os.path.getsize(p)

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
		print("%s not in g_src_trans_map" % s)
		print(g_src_trans_map.keys())
	return g_src_trans_map[s]

g_spaces = "                              "
def a(url, txt): return '<a href="' + url + '">' + txt + '</a>'
def pre(s): return '<pre style="white-space: pre-wrap;">' + s + '</pre>'
def td(s, off=0): return g_spaces[:off] + '<td>%s</td>' % s

# Turn:
# src\utils\allocator.h(156)
# Into:
# <a href="https://code.google.com/p/sumatrapdf/source/browse/trunk/src/utils/allocator.h#156">src\utils\allocator.h(156)</a>
def htmlize_src_link(s, ver):
	parts = s.split("(")
	src_path = parts[0] # src\utils\allocator.h
	src_path = trans_src_path(src_path) # src\utils\Allocator.h
	src_path_in_url = src_path.replace("\\", "/")
	src_line = parts[1][:-1] # "156)"" => "156"
	base = "https://code.google.com/p/sumatrapdf/source/browse/trunk/"
	#url = base + src_path_in_url + "#" + src_line
	url =  base + src_path_in_url + "?r=%s#%s" % (str(ver), str(src_line))
	return a(url, src_path + "(" + src_line + ")")

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
	# TODO: would be nicer if "sumatrapdf_buildbot" wasn't hard-coded, but don't know
	# how to get this reliably
	rel_path_start = lines[0].find("sumatrapdf_buildbot\\") + len("sumatrapdf_buildbot\\")
	for l in lines:
		l = l[rel_path_start:]
		err_start = l.find(" : ")
		src = l[:err_start]
		msg = l[err_start + 3:]
		a = htmlize_src_link(src, ver)
		s = a + " " + msg
		if l.startswith("src\\"):     sumatra_errors.append(s)
		elif l.startswith("mupdf\\"): mupdf_errors.append(s)
		elif l.startswith("ext\\"):   ext_errors.append(s)
		else: assert(False)
	return (sumatra_errors, mupdf_errors, ext_errors)

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

def str2bool(s):
	if s.lower() in ("true", "1"): return True
	if s.lower() in ("false", "0"): return False
	assert(False)

def test_ser():
	stats = Stats()
	print stats.to_s()

class Stats(object):
	int_fields = ("analyze_sumatra_warnings_count", "analyze_mupdf_warnings_count", "analyze_ext_warnings_count",
		"rel_sumatrapdf_exe_size", "rel_sumatrapdf_no_mupdf_exe_size",
		"rel_installer_exe_size", "rel_libmupdf_dll_size", "rel_nppdfviewer_dll_size",
		"rel_pdffilter_dll_size", "rel_pdfpreview_dll_size")
	bool_fields = ("rel_failed")
	str_fields = ()

	def __init__(self, serialized_file=None):
		# serialized to stats.txt
		self.analyze_sumatra_warnings_count = 0
		self.analyze_mupdf_warnings_count = 0
		self.analyze_ext_warnings_count = 0

		self.rel_failed = False
		self.rel_sumatrapdf_exe_size = 0
		self.rel_sumatrapdf_no_mupdf_exe_size = 0
		self.rel_installer_exe_size = 0
		self.rel_libmupdf_dll_size = 0
		self.rel_nppdfviewer_dll_size = 0
		self.rel_pdffilter_dll_size = 0
		self.rel_pdfpreview_dll_size = 0

		# just for passing data aroun
		self.analyze_out = None
		self.rel_build_log = None

		if serialized_file != None:
			self.from_s(open(serialized_file, "r").read())

	def add_field(self, name):
		val = self.__getattribute__ (name)
		self.fields.append("%s: %s" % (name, str(val)))

	def add_rel_fields(self):
		for f in ("rel_sumatrapdf_exe_size", "rel_sumatrapdf_no_mupdf_exe_size",
			"rel_installer_exe_size", "rel_libmupdf_dll_size", "rel_nppdfviewer_dll_size",
			"rel_pdffilter_dll_size", "rel_pdfpreview_dll_size"):
			self.add_field(f)

	def to_s(self):
		self.fields = []
		self.add_field("analyze_sumatra_warnings_count")
		self.add_field("analyze_mupdf_warnings_count")
		self.add_field("analyze_ext_warnings_count")
		self.add_field("rel_failed")
		if not self.rel_failed:
			self.add_rel_fields()
		return string.join(self.fields, "\n")

	def set_field(self, name, val, tp):
		if tp in ("str", "string"):
			self.__setattr__(name, val)
		elif tp == "bool":
			self.__setattr__(name, str2bool(val))
		elif tp in ("int", "num"):
			self.__setattr__(name, int(val))
		else:
			assert(False)

	def from_s(self, s):
		lines = s.split("\n")
		for l in lines:
			(name, val) = l.split(": ", 1)
			name = name.replace("release_", "rel_")
			if name in self.int_fields: self.set_field(name, val, "int")
			elif name in self.bool_fields: self.set_field(name, val, "bool")
			elif name in self.str_fields: self.set_field(name, val, "str")
			else: print(name); assert(False)

def create_dir(d):
	if not os.path.exists(d): os.makedirs(d)
	return d

def get_cache_dir(): return create_dir(os.path.join("..", "sumatrapdfcache", "buildbot"))
def get_stats_cache_dir(): return create_dir(os.path.join(get_cache_dir(), "stats"))
def get_logs_cache_dir(): return create_dir(os.path.join(get_cache_dir(), "logs"))

# logs are only kept for potential troubleshooting and they're quite big,
# so we delete old files (we keep logs for the last $to_keep revisions)
def delete_old_logs(to_keep=10):
	files = os.listdir(get_logs_cache_dir())
	versions = []
	for f in files:
		ver = int(f.split("_")[0])
		if ver not in versions: versions.append(ver)
	versions.sort(reverse=True)
	if len(versions) <= to_keep :
		return
	to_delete = versions[to_keep:]
	for f in files:
		ver = int(f.split("_")[0])
		if ver in to_delete:
			p = os.path.join(get_logs_cache_dir(), f)
			os.remove(p)

# return Stats object or None if we don't have it for this version
def stats_for_ver(ver):
	local_path = os.path.join(get_stats_cache_dir(), ver + ".txt")
	if not os.path.exists(local_path):
		s3_path = "sumatrapdf/buildbot/%s/stats.txt" % ver
		if not s3.exists(s3_path): return None
		s3.download_to_file(s3_path, local_path)
		assert(os.path.exists(local_path))
	return Stats(local_path)

# We cache results of running svn log in a dict mapping
# version to string returned by svn log
g_svn_log_per_ver = None

def load_svn_log_data():
	try:
		path = os.path.join(get_cache_dir(), "snv_log.dat")
		fo = open(path, "rb")
	except IOError:
		# it's ok if doesn't exist
		return {}
	try:
		res = cPickle.load(fo)
		fo.close()
		return res
	except:
		fo.close()
		file_remove_try_hard(path)
		return {}

def save_svn_log_data(data):
	p = os.path.join(get_cache_dir(), "snv_log.dat")
	fo = open(p, "wb")
	cPickle.dump(data, fo, protocol=cPickle.HIGHEST_PROTOCOL)
	fo.close()

def checkin_comment_for_ver(ver):
	global g_svn_log_per_ver
	ver = str(ver)
	if g_svn_log_per_ver is None:
		g_svn_log_per_ver = load_svn_log_data()
	if ver not in g_svn_log_per_ver:
		# TODO: retry few times to make it robust against temporary network failures
		(out, err) = run_cmd_throw("svn", "log", "-r%s" % ver, "-v")
		g_svn_log_per_ver[ver] = out
		save_svn_log_data(g_svn_log_per_ver)
	s = g_svn_log_per_ver[ver]
	res = parse_svnlog_out(s)
	if res is None:
		return "not a source code change"
	return res[1]

# return true if we already have results for a given build number in s3
def has_already_been_built(ver):
	s3_dir = "sumatrapdf/buildbot/"
	n1 = s3_dir + ver + "/analyze.html"
	n2 = s3_dir + ver + "/release_build_log.txt"
	keys = s3.list(s3_dir)
	for k in keys:
		if k.name in [n1, n2]:
			return True
	return False

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

# return stats for the first successful build before $ver
def stats_for_previous_build(ver):
	ver = int(ver) - 1
	while True:
		stats = stats_for_ver(str(ver))
		# we assume that we have
		if None == stats: return None
		if not stats.rel_failed: return stats
		ver -= 1

def size_diff_html(n):
	if n > 0:   return ' (<font color=red>+' + str(n) + '</font>)'
	elif n < 0: return ' (<font color=green>' + str(n) + '</font>)'
	else:       return ''

def build_files_changed(ver):
	# TODO: get files changed in a revision and check if any source files changed
	return True

def build_sizes_json():
	files = os.listdir(get_stats_cache_dir())
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
	s3.upload_data_public_with_content_type(s, "sumatrapdf/buildbot/sizes.js", silent=True)

# build sumatrapdf/buildbot/index.html summary page that links to each
# sumatrapdf/buildbot/${ver}/analyze.html
def build_index_html():
	s3_dir = "sumatrapdf/buildbot/"
	html = '<html><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>%s</head><body>\n' % g_index_html_css
	html += "<p>SumatraPDF buildbot results:</p>\n"
	names = [n.name for n in s3.list(s3_dir)]
	# filter out top-level files like index.html and sizes.js
	names = [n[len(s3_dir):] for n in names if len(n.split("/")) == 4]
	names.sort(reverse=True, key=lambda name: int(name.split("/")[0]))

	html += '<table id="table-5"><tr><th>build</th><th>/analyze</th><th>release</th>'
	html += '<th style="font-size:80%">SumatraPDF.exe size</th><th style="font-size:80%"">Installer.exe size</th><th>checkin comment</th></tr>\n'
	files_by_ver = group_by_ver(names)
	for arr in files_by_ver:
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

		# TODO: this must group several revisions on one line to actually be shorter
		if not build_files_changed(ver):
			html += '<td colspan=4>unchanged</td>'
			continue

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

		# SumatraPDF.exe, Installer.exe size
		if stats.rel_failed:
			html += td("", 4) + "\n" + td("", 4) + "\n"
		else:
			prev_stats = stats_for_previous_build(ver)
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

		# checkin comment
		(comment, trimmed) = util.trim_str(checkin_comment_for_ver(ver))
		comment = comment.decode('utf-8')
		comment = cgi.escape(comment)
		if trimmed: comment += a(src_url, "...")
		html += td(comment, 4) + "\n"
		html += "  </tr>\n"
	html += "</table>"
	html += "</body></html>\n"
	#print(html)
	s3.upload_data_public_with_content_type(html, "sumatrapdf/buildbot/index.html", silent=True)

g_cert_pwd = None
def get_cert_pwd():
	global g_cert_pwd
	if g_cert_pwd == None:
		cert_path = os.path.join("scripts", "cert.pfx")
		if not os.path.exists(cert_path):
			print("%s missing" % cert_path)
			sys.exit(1)
		conf = load_config()
		g_cert_pwd = conf.GetCertPwdMustExist()
	return g_cert_pwd

def sign(file_path, cert_pwd):
  # the sign tool is finicky, so copy it and cert to the same dir as
  # exe we're signing
  file_dir = os.path.dirname(file_path)
  file_name = os.path.basename(file_path)
  cert_src = os.path.join("scripts", "cert.pfx")
  sign_tool_src = os.path.join("bin", "ksigncmd.exe")
  cert_dest = os.path.join(file_dir, "cert.pfx")
  sign_tool_dest = os.path.join(file_dir, "ksigncmd.exe")
  if not os.path.exists(cert_dest): shutil.copy(cert_src, cert_dest)
  if not os.path.exists(sign_tool_dest): shutil.copy(sign_tool_src, sign_tool_dest)
  curr_dir = os.getcwd()
  os.chdir(file_dir)
  run_cmd_throw("ksigncmd.exe", "/f", "cert.pfx", "/p", cert_pwd, file_name)
  os.chdir(curr_dir)

def try_sign(obj_dir):
	try:
		exe = os.path.join(obj_dir, "SumatraPDF.exe")
		sign(exe, get_cert_pwd())
		sign(os.path.join(obj_dir, "uninstall.exe"), get_cert_pwd())
	except:
		return False
	return True

# sometimes ksigncmd.exe fails, so we try 3 times
def sign_try_hard(obj_dir):
	tries = 3
	while tries > 0:
		if try_sign(obj_dir): return
		tries -= 1
	assert(False)

def file_size_in_obj(file_name):
	return file_size(os.path.join("obj-rel", file_name))

def build_release(stats, ver):
	config = "CFG=rel"
	obj_dir = "obj-rel"
	extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
	platform = "PLATFORM=X86"

	shutil.rmtree(obj_dir, ignore_errors=True)
	shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)
	(out, err, errcode) = run_cmd("nmake", "-f", "makefile.msvc", config, extcflags, platform, "all_sumatrapdf")

	log_path = os.path.join(get_logs_cache_dir(), ver + "_rel_log.txt")
	build_log = out + "\n====STDERR:\n" + err
	build_log = strip_empty_lines(build_log)
	open(log_path, "w").write(build_log)

	stats.rel_build_log = None
	stats.rel_failed = False
	if errcode != 0:
		stats.rel_build_log = build_log
		stats.rel_failed = True
		return

	stats.rel_sumatrapdf_exe_size = file_size_in_obj("SumatraPDF.exe")
	stats.rel_sumatrapdf_no_mupdf_exe_size = file_size_in_obj("SumatraPDF-no-MuPDF.exe")
	stats.rel_libmupdf_dll_size = file_size_in_obj("libmupdf.dll")
	stats.rel_nppdfviewer_dll_size = file_size_in_obj("npPdfViewer.dll")
	stats.rel_pdffilter_dll_size = file_size_in_obj("PdfFilter.dll")
	stats.rel_pdfpreview_dll_size = file_size_in_obj("PdfPreview.dll")

	build_installer_data(obj_dir)
	run_cmd_throw("nmake", "-f", "makefile.msvc", "Installer", config, platform, extcflags)
	p = os.path.join(obj_dir, "Installer.exe")
	stats.rel_installer_exe_size = file_size(p)

def build_analyze(stats, ver):
	config = "CFG=rel"
	obj_dir = "obj-rel"
	extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
	platform = "PLATFORM=X86"

	shutil.rmtree(obj_dir, ignore_errors=True)
	shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)
	(out, err, errcode) = run_cmd("nmake", "-f", "makefile.msvc", "WITH_ANALYZE=yes", config, extcflags, platform, "all_sumatrapdf")
	stats.analyze_out = out

	log_path = os.path.join(get_logs_cache_dir(), ver + "_analyze_log.txt")
	s = out + "\n====STDERR:\n" + err
	open(log_path, "w").write(strip_empty_lines(s))

def svn_update_to_ver(ver):
	run_cmd_throw("svn", "update", "-r" + ver)
	rebuild_trans_src_path_cache()

# TODO: maybe add debug build and 64bit release?
# skip_release is just for testing
def build_version(ver, skip_release=False):
	print("Building version %s" % ver)
	svn_update_to_ver(ver)
	s3dir = "sumatrapdf/buildbot/%s/" % ver

	stats = Stats()
	# only run /analyze on newer builds since we didn't have the necessary
	# makefile logic before
	run_analyze = int(ver) >= g_first_analyze_build

	if not skip_release:
		start_time = datetime.datetime.now()
		build_release(stats, ver)
		dur = datetime.datetime.now() - start_time
		print("%s for release build" % str(dur))
		if stats.rel_failed:
			run_analyze = False # don't bother running analyze if release failed
			s3.upload_data_public_with_content_type(stats.rel_build_log, s3dir + "release_build_log.txt", silent=True)

	if run_analyze:
		start_time = datetime.datetime.now()
		build_analyze(stats, ver)
		dur = datetime.datetime.now() - start_time
		print("%s for analyze build" % str(dur))
		html = gen_analyze_html(stats, ver)
		p = os.path.join(get_logs_cache_dir(), "%s_analyze.html" % str(ver))
		open(p, "w").write(html)
		s3.upload_data_public_with_content_type(html, s3dir + "analyze.html", silent=True)

	# TODO: it appears we might throw an exception after uploading analyze.html but
	# before/dufing uploading stats.txt. Would have to implement transactional
	# multi-upload to be robust aginst that, so will just let it be
	stats_txt = stats.to_s()
	s3.upload_data_public_with_content_type(stats_txt, s3dir + "stats.txt", silent=True)
	build_sizes_json()
	build_index_html()

# for testing
def build_curr(force=False):
	(local_ver, latest_ver) = util.get_svn_versions()
	print("local ver: %s, latest ver: %s" % (local_ver, latest_ver))
	if not has_already_been_built(local_ver) or force:
			build_version(local_ver)
	else:
		print("We have already built revision %s" % local_ver)

def build_version_try(ver, try_count = 2):
	# it can happen we get a valid but intermitten exception e.g.
	# due to svn command failing due to server hiccup
	# in that case we'll retry, waiting 1 min in between,
	# but only up to try_count times
	while True:
		try:
			build_version(ver)
		except Exception, e:
			# rethrow assert() exceptions, they come from our code
			# and we should stop
			if isinstance(e, AssertionError):
				print("assert happened:")
				print(str(e))
				traceback.print_exc()
				raise e
			print(str(e))
			traceback.print_exc()
			try_count -= 1
			if 0 == try_count:
				raise
			time.sleep(60)
		return

def buildbot_loop():
	while True:
		# util.get_svn_versions() might throw an exception due to
		# temporary network problems, so retry
		try:
			(local_ver, latest_ver) = util.get_svn_versions()
		except:
			print("get_svn_versions() threw an exception")
			time.sleep(120)
			continue

		print("local ver: %s, latest ver: %s" % (local_ver, latest_ver))
		while int(local_ver) <= int(latest_ver):
			if not has_already_been_built(local_ver):
				build_version_try(local_ver)
			else:
				print("We have already built revision %s" % local_ver)
			local_ver = str(int(local_ver)+1)
		delete_old_logs()
		print("Sleeping for 15 minutes")
		time.sleep(60*15) # 15 mins

def copy_secrets(dst_path):
	src = os.path.join("scripts", "config.py")
	shutil.copyfile(src, os.path.join(dst_path, src))
	src = os.path.join("scripts", "cert.pfx")
	shutil.copyfile(src, os.path.join(dst_path, src))

def main():
	verify_started_in_right_directory()
	# to avoid problems, we build a separate source tree, just for the buildbot
	src_path = os.path.join("..", "sumatrapdf_buildbot")
	verify_path_exists(src_path)
	copy_secrets(src_path)
	os.chdir(src_path)
	get_cert_pwd() # early exit if problems

	#build_version("6698", skip_release=True)
	#build_index_html()
	#build_sizes_json()
	#build_curr(force=True)
	buildbot_loop()

if __name__ == "__main__":
	main()
