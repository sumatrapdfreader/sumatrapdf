"""
Builds sumatra and uploads results to s3 for easy analysis, viewable at:
http://kjkpub.s3.amazonaws.com/sumatrapdf/buildbot/index.html
"""
import os, shutil, time, datetime, cPickle, traceback
import s3, util
from util import file_remove_try_hard, run_cmd_throw
from util import parse_svnlog_out, Serializable, create_dir
from util import load_config, run_cmd, strip_empty_lines, build_installer_data
from util import verify_path_exists, verify_started_in_right_directory
from buildbot_html import gen_analyze_html, build_index_html, rebuild_trans_src_path_cache
from buildbot_html import build_sizes_json, g_first_analyze_build

"""
TODO:
 - at some point the index.html page will get too big, so split it into N-item chunks
   (100? 300?)
 - should also do pre-release builds if there was a new checkin since the last uploaded
   build but is different that than build and there was no checkin for at least 4hr
   (all those rules are to ensure we don't create pre-release builds too frequently)
"""

class Stats(Serializable):
	fields = {
		"analyze_sumatra_warnings_count" : 0,
		"analyze_mupdf_warnings_count" : 0,
		"analyze_ext_warnings_count" : 0,
		"rel_sumatrapdf_exe_size" : 0,
		"rel_sumatrapdf_no_mupdf_exe_size" : 0,
		"rel_installer_exe_size" : 0,
		"rel_libmupdf_dll_size" : 0,
		"rel_nppdfviewer_dll_size" : 0,
		"rel_pdffilter_dll_size" : 0,
		"rel_pdfpreview_dll_size" : 0,
		"rel_failed" : False,
		"rel_build_log": "",
		"analyze_out" : "",
	}
	fields_no_serialize = ["rel_build_log", "analyze_out"]

	def __init__(self, read_from_file=None):
		Serializable.__init__(self, Stats.fields, Stats.fields_no_serialize, read_from_file)

def file_size(p):
  return os.path.getsize(p)

def str2bool(s):
	if s.lower() in ("true", "1"): return True
	if s.lower() in ("false", "0"): return False
	assert(False)

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

	stats.rel_build_log = ""
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
	html = build_index_html(stats_for_ver, checkin_comment_for_ver)
	json_s = build_sizes_json(get_stats_cache_dir, stats_for_ver)

	s3.upload_data_public_with_content_type(stats_txt, s3dir + "stats.txt", silent=True)
	s3.upload_data_public_with_content_type(json_s, "sumatrapdf/buildbot/sizes.js", silent=True)
	s3.upload_data_public_with_content_type(html, "sumatrapdf/buildbot/index.html", silent=True)

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

def main():
	verify_started_in_right_directory()
	# to avoid problems, we build a separate source tree, just for the buildbot
	src_path = os.path.join("..", "sumatrapdf_buildbot")
	verify_path_exists(src_path)
	conf = load_config()
	s3.set_secrets(conf.aws_access, conf.aws_secret)
	s3.set_bucket("kjkpub")
	os.chdir(src_path)

	#build_version("6698", skip_release=True)
	#build_index_html()
	#build_sizes_json()
	#build_curr(force=True)
	buildbot_loop()

if __name__ == "__main__":
	main()
