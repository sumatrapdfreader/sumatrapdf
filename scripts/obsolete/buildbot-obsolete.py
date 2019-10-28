"""
Builds sumatra and uploads results to s3 for easy analysis, viewable at:
https://kjkpub.s3.amazonaws.com/sumatrapdf/buildbot/index.html
"""
import sys
import os
# assumes is being run as ./scripts/buildbot.py
efi_scripts_dir = os.path.join("tools", "efi")
sys.path.append(efi_scripts_dir)

import shutil
import time
import datetime
import cPickle
import traceback
import s3
import util
import efiparse
import build
from util import file_remove_try_hard, run_cmd_throw, pretty_print_secs
from util import Serializable, create_dir
from util import load_config, run_cmd, strip_empty_lines
from util import verify_path_exists, verify_started_in_right_directory
from buildbot_html import gen_analyze_html, build_index_html, rebuild_trans_src_path_cache
from buildbot_html import build_sizes_json, g_first_analyze_build
import runtests

"""
TODO:
 - diff for symbols in html format
 - upload efi html diff as part of buildbot

 MAYBE:
 - aggressive optimization cause symbol churn which makes reading efi output
   hard. One option would be to run efi on an executable compiled with less
   aggressive optimization. Another would be to post-process the result
   and use heuristic to suppress bogus changes
"""


class Stats(Serializable):
    fields = {
        "analyze_sumatra_warnings_count": 0,
        "analyze_mupdf_warnings_count": 0,
        "analyze_ext_warnings_count": 0,
        "rel_sumatrapdf_exe_size": 0,
        "rel_sumatrapdf_no_mupdf_exe_size": 0,
        "rel_installer_exe_size": 0,
        "rel_libmupdf_dll_size": 0,
        "rel_nppdfviewer_dll_size": 0,
        "rel_pdffilter_dll_size": 0,
        "rel_pdfpreview_dll_size": 0,
        "rel_failed": False,
        "rel_build_log": "",
        "analyze_out": "",
    }
    fields_no_serialize = ["rel_build_log", "analyze_out"]

    def __init__(self, read_from_file=None):
        Serializable.__init__(self, Stats.fields,
                              Stats.fields_no_serialize, read_from_file)


def file_size(p):
    return os.path.getsize(p)


def str2bool(s):
    if s.lower() in ("true", "1"):
        return True
    if s.lower() in ("false", "0"):
        return False
    assert(False)


TIME_BETWEEN_PRE_RELEASE_BUILDS_IN_SECS = 60 * 60 * 8  # 8hrs
g_time_of_last_build = None
g_cache_dir = create_dir(
    os.path.realpath(os.path.join("..", "sumatrapdfcache", "buildbot")))
g_stats_cache_dir = create_dir(os.path.join(g_cache_dir, "stats"))
g_logs_cache_dir = create_dir(os.path.join(g_cache_dir, "logs"))


def get_cache_dir():
    return g_cache_dir


def get_stats_cache_dir():
    return g_stats_cache_dir


def get_logs_cache_dir():
    return g_logs_cache_dir


@util.memoize
def cert_path():
    scripts_dir = os.path.realpath(os.path.dirname(__file__))
    cert_path = os.path.join(scripts_dir, "cert.pfx")
    return verify_path_exists(cert_path)


def logs_efi_out_path(ver):
    return os.path.join(get_logs_cache_dir(), str(ver) + "_efi.txt.bz2")


# logs are only kept for potential troubleshooting and they're quite big,
# so we delete old files (we keep logs for the last $to_keep revisions)
def delete_old_logs(to_keep=10):
    files = os.listdir(get_logs_cache_dir())
    versions = []
    for f in files:
        ver = int(f.split("_")[0])
        if ver not in versions:
            versions.append(ver)
    versions.sort(reverse=True)
    if len(versions) <= to_keep:
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
        if not s3.exists(s3_path):
            return None
        s3.download_to_file(s3_path, local_path)
        assert(os.path.exists(local_path))
    return Stats(local_path)


def previous_successful_build_ver(ver):
    ver = int(ver) - 1
    while True:
        stats = stats_for_ver(str(ver))
        if None == stats:
            return 0
        if not stats.rel_failed:
            return ver
        ver -= 1


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
    raise BaseException("NYI for git")
    ver = str(ver)
    if g_svn_log_per_ver is None:
        g_svn_log_per_ver = load_svn_log_data()
    if ver not in g_svn_log_per_ver:
        # TODO: retry few times to make it robust against temporary network
        # failures
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


def verify_efi_present():
    try:
        (out, err, errcode) = util.run_cmd("efi.exe")
    except:
        print("Must have efi.exe in the %PATH%!!!")
        sys.exit(1)
    if "Usage:" not in out:
        print("efi.exe created unexpected output:\n%s" % out)
        sys.exit(1)


def file_size_in_obj(file_name, defSize=None):
    p = os.path.join("obj-rel", file_name)
    if not os.path.exists(p) and defSize is not None:
        return defSize
    return file_size(p)


def clean_release():
    shutil.rmtree("obj-rel", ignore_errors=True)
    shutil.rmtree("vs-premake", ignore_errors=True)
    shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)


def build_release(stats, ver):
    config = "CFG=rel"
    obj_dir = "obj-rel"
    extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
    platform = "PLATFORM=X86"

    clean_release()
    (out, err, errcode) = run_cmd("nmake", "-f", "makefile.msvc",
                                  config, extcflags, platform,
                                  "all_sumatrapdf")

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
    stats.rel_sumatrapdf_no_mupdf_exe_size = file_size_in_obj(
        "SumatraPDF-mupdf-dll.exe")
    stats.rel_libmupdf_dll_size = file_size_in_obj("libmupdf.dll")
    stats.rel_nppdfviewer_dll_size = file_size_in_obj("npPdfViewer.dll", 0)
    stats.rel_pdffilter_dll_size = file_size_in_obj("PdfFilter.dll")
    stats.rel_pdfpreview_dll_size = file_size_in_obj("PdfPreview.dll")
    stats.rel_installer_exe_size = file_size_in_obj("Installer.exe")


def build_analyze(stats, ver):
    config = "CFG=rel"
    obj_dir = "obj-rel"
    extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
    platform = "PLATFORM=X86"

    shutil.rmtree(obj_dir, ignore_errors=True)
    shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)
    (out, err, errcode) = run_cmd("nmake", "-f", "makefile.msvc",
                                  "WITH_ANALYZE=yes", config, extcflags, platform, "all_sumatrapdf")
    stats.analyze_out = out

    log_path = os.path.join(get_logs_cache_dir(), ver + "_analyze_log.txt")
    s = out + "\n====STDERR:\n" + err
    open(log_path, "w").write(strip_empty_lines(s))


def svn_update_to_ver(ver):
    run_cmd_throw("svn", "update", "-r" + ver)
    rebuild_trans_src_path_cache()


# runs efi.exe on obj-rel/SumatraPDF.exe, stores the data in obj-rel/efi.txt.bz2
# and uploads to s3 as efi.txt.bz2
def build_and_upload_efi_out(ver):
    obj_dir = "obj-rel"
    s3dir = "sumatrapdf/buildbot/%s/" % ver
    os.chdir(obj_dir)
    util.run_cmd_throw("efi", "SumatraPDF.exe", ">efi.txt")
    util.bz_file_compress("efi.txt", "efi.txt.bz2")
    s3.upload_file_public("efi.txt.bz2", s3dir + "efi.txt.bz2", silent=True)
    shutil.copyfile("efi.txt.bz2", logs_efi_out_path(ver))
    os.chdir("..")


def get_efi_out(ver):
    ver = str(ver)
    p = logs_efi_out_path(ver)
    if os.path.exists(p):
        return p
    # TODO: try download from s3 if doesn't exist? For now we rely on the fact
    # that it was build on this machine, so the results should still be in logs
    # cache
    return None


def efi_diff_as_txt(diff, max=-1):
    lines = []
    diff.added.sort(key=lambda sym: sym.size, reverse=True)
    diff.removed.sort(key=lambda sym: sym.size, reverse=True)
    diff.changed.sort(key=lambda sym: sym.size_diff, reverse=True)

    added = diff.added
    if len(added) > 0:
        lines.append("\nAdded symbols:")
        if max != -1:
            added = added[:max]
        for sym in added:
            #sym = diff.syms2.name_to_sym[sym_name]
            size = sym.size
            s = "%4d : %s" % (size, sym.full_name())
            lines.append(s)

    removed = diff.removed
    if len(removed) > 0:
        lines.append("\nRemoved symbols:")
        if max != -1:
            removed = removed[:max]
        for sym in removed:
            #sym = diff.syms2.name_to_sym[sym_name]
            size = sym.size
            s = "%4d : %s" % (size, sym.full_name())
            lines.append(s)

    changed = diff.changed
    if len(changed) > 0:
        lines.append("\nChanged symbols:")
        if max != -1:
            changed = changed[:max]
        for sym in changed:
            size = sym.size_diff
            lines.append("%4d : %s" % (size, sym.full_name()))
    return "\n".join(lines)


# builds efi diff between this version and previous succesful version
# and uploads as efi_diff.txt
def build_and_upload_efi_txt_diff(ver):
    prev_ver = previous_successful_build_ver(ver)
    if 0 == prev_ver:
        return
    efi_path_curr = get_efi_out(ver)
    if not efi_path_curr:
        print("didn't find efi output for %s" % str(ver))
        return
    efi_path_prev = get_efi_out(prev_ver)
    if not efi_path_prev:
        print("didn't find efi output for %s" % str(prev_ver))
        return
    obj_file_splitters = ["obj-rel\\", "INTEL\\"]
    efi1 = efiparse.parse_file(efi_path_prev, obj_file_splitters)
    efi2 = efiparse.parse_file(efi_path_curr, obj_file_splitters)
    diff = efiparse.diff(efi1, efi2)
    s = str(diff)
    s = s + "\n" + efi_diff_as_txt(diff)
    s = ""
    s3dir = "sumatrapdf/buildbot/%s/" % str(ver)
    s3.upload_data_public_with_content_type(
        s, s3dir + "efi_diff.txt", silent=True)


# TODO: maybe add debug build and 64bit release?
# skip_release is just for testing
def build_version(ver, skip_release=False):
    print("Building version %s" % ver)

    clean_release()
    # a hack: checkin_comment_for_ver() might call svn log, which doesn't like
    # unversioning directories (like obj-rel or vs-premake), so we call it here,
    # after clean, to cache the result
    checkin_comment_for_ver(ver)

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
            # don't bother running analyze if release failed
            run_analyze = False
            s3.upload_data_public_with_content_type(
                stats.rel_build_log, s3dir + "release_build_log.txt", silent=True)

    if not stats.rel_failed:
        build_and_upload_efi_out(ver)

    if run_analyze:
        start_time = datetime.datetime.now()
        build_analyze(stats, ver)
        dur = datetime.datetime.now() - start_time
        print("%s for analyze build" % str(dur))
        html = gen_analyze_html(stats, ver)
        p = os.path.join(get_logs_cache_dir(), "%s_analyze.html" % str(ver))
        open(p, "w").write(html)
        s3.upload_data_public_with_content_type(
            html, s3dir + "analyze.html", silent=True)

    if not stats.rel_failed:
        build_and_upload_efi_txt_diff(ver)

    # TODO: it appears we might throw an exception after uploading analyze.html but
    # before/dufing uploading stats.txt. Would have to implement transactional
    # multi-upload to be robust aginst that, so will just let it be
    stats_txt = stats.to_s()
    s3.upload_data_public_with_content_type(
        stats_txt, s3dir + "stats.txt", silent=True)
    html = build_index_html(stats_for_ver, checkin_comment_for_ver)
    s3.upload_data_public_with_content_type(
        html, "sumatrapdf/buildbot/index.html", silent=True)
    json_s = build_sizes_json(get_stats_cache_dir, stats_for_ver)
    s3.upload_data_public_with_content_type(
        json_s, "sumatrapdf/buildbot/sizes.js", silent=True)
    if stats.rel_failed:
        email_build_failed(ver)
        return  # don't run tests if build fails

    # TODO: can't run tests anymore because premake4 only generates
    # vs 2010 solution, which can't be executed by vs 2013
    #err = runtests.run_tests()
    err = None
    if err != None:
        s3.upload_data_public_with_content_type(
            err, s3dir + "tests_error.txt", silent=True)
        email_tests_failed(ver, err)
        print("Tests failed. Error message:\n" + err)
    else:
        print("Tests passed!")


def test_build_html_index():
    print("test_build_html_index()")
    html = build_index_html(stats_for_ver, checkin_comment_for_ver)
    print("after build_index_html()")
    import codecs
    codecs.open("index.html", "w", "utf8").write(html)
    print("after write")
    sys.exit(1)


g_email_to = ["kkowalczyk@gmail.com", "zeniko@gmail.com"]


def email_tests_failed(ver, err):
    s3_url_start = "https://kjkpub.s3.amazonaws.com/sumatrapdf/buildbot/"
    c = load_config()
    if not c.HasNotifierEmail():
        print("email_tests_failed() not ran because not c.HasNotifierEmail()")
        return
    sender, senderpwd = c.GetNotifierEmailAndPwdMustExist()
    subject = "SumatraPDF tests failed for build %s" % str(ver)
    checkin_url = "https://code.google.com/p/sumatrapdf/source/detail?r=%s" % str(ver)
    body = "Checkin: %s\n\n" % checkin_url
    log_url = s3_url_start + str(ver) + "/tests_error.txt"
    body += "Build log: %s\n\n" % log_url
    buildbot_index_url = s3_url_start + "index.html"
    body += "Buildbot: %s\n\n" % buildbot_index_url
    body += "Error: %s\n\n" % err
    util.sendmail(sender, senderpwd, g_email_to, subject, body)


def email_msg(msg):
    c = load_config()
    if not c.HasNotifierEmail():
        print("email_build_failed() not ran because not c.HasNotifierEmail()")
        return
    sender, senderpwd = c.GetNotifierEmailAndPwdMustExist()
    subject = "SumatraPDF buildbot failed"
    util.sendmail(sender, senderpwd, ["kkowalczyk@gmail.com"], subject, msg)


def email_build_failed(ver):
    s3_url_start = "https://kjkpub.s3.amazonaws.com/sumatrapdf/buildbot/"
    c = load_config()
    if not c.HasNotifierEmail():
        print("email_build_failed() not ran because not c.HasNotifierEmail()")
        return
    sender, senderpwd = c.GetNotifierEmailAndPwdMustExist()
    subject = "SumatraPDF build %s failed" % str(ver)
    checkin_url = "https://code.google.com/p/sumatrapdf/source/detail?r=%s" % str(ver)
    body = "Checkin: %s\n\n" % checkin_url
    build_log_url = s3_url_start + str(ver) + "/release_build_log.txt"
    body += "Build log: %s\n\n" % build_log_url
    buildbot_index_url = s3_url_start + "index.html"
    body += "Buildbot: %s\n\n" % buildbot_index_url
    util.sendmail(sender, senderpwd, g_email_to, subject, body)


# for testing
def build_curr(force=False):
    raise BaseException("NYI for git")
    (local_ver, latest_ver) = util.get_svn_versions()
    print("local ver: %s, latest ver: %s" % (local_ver, latest_ver))
    if not has_already_been_built(local_ver) or force:
        build_version(local_ver)
    else:
        print("We have already built revision %s" % local_ver)


def build_version_retry(ver, try_count=2):
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
    global g_time_of_last_build
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
        revs_built = 0
        while int(local_ver) <= int(latest_ver):
            if not has_already_been_built(local_ver):
                build_version_retry(local_ver)
                revs_built += 1
            else:
                print("We have already built revision %s" % local_ver)
            local_ver = str(int(local_ver) + 1)
        delete_old_logs()
        # don't sleep if we built something in this cycle. a new checkin might
        # have happened while we were working
        if revs_built > 0:
            g_time_of_last_build = datetime.datetime.now()
            continue

        secs_until_prerelease = None
        if g_time_of_last_build is not None:
            td = datetime.datetime.now() - g_time_of_last_build
            secs_until_prerelease = TIME_BETWEEN_PRE_RELEASE_BUILDS_IN_SECS - \
                int(td.total_seconds())
            if secs_until_prerelease < 0:
                build_pre_release()
                g_time_of_last_build = None

        if secs_until_prerelease is None:
            print("Sleeping for 15 minutes to wait for new checkin")
        else:
            print("Sleeping for 15 minutes, %s until pre-release" %
                  pretty_print_secs(secs_until_prerelease))
        time.sleep(60 * 15)  # 15 mins


def ignore_pre_release_build_error(s):
    # it's possible we did a pre-release build outside of buildbot and that
    # shouldn't be a fatal error
    if "already exists in s3" in s:
        return True
    return False


def build_pre_release():
    try:
        cert_dst_path = os.path.join("scripts", "cert.pfx")
        if not os.path.exists(cert_dst_path):
            shutil.copyfile(cert_path(), cert_dst_path)
        print("Building pre-release")
        build.build_pre_release()
    except BaseException, e:
        s = str(e)
        print(s)
        # a bit of a hack. not every kind of failure should stop the buildbot
        if not ignore_pre_release_build_error(s):
            traceback.print_exc()
            raise


def test_email_tests_failed():
    email_tests_failed("200", "hello")
    sys.exit(1)


def verify_can_send_email():
    c = load_config()
    if not c.HasNotifierEmail():
        print("can't run. scripts/config.py missing notifier_email and/or notifier_email_pwd")
        sys.exit(1)


def main():
    verify_can_send_email()
    cert_path()  # early check and ensures value is memoized
    verify_efi_present()
    verify_started_in_right_directory()
    # to avoid problems, we build a separate source tree, just for the buildbot
    src_path = os.path.join("..", "sumatrapdf_buildbot")
    verify_path_exists(src_path)
    conf = load_config()
    s3.set_secrets(conf.aws_access, conf.aws_secret)
    s3.set_bucket("kjkpub")
    os.chdir(src_path)

    # test_email_tests_failed()
    #build_version("8190", skip_release=True)
    # test_build_html_index()
    # build_sizes_json()
    # build_curr(force=True)

    # TODO: add a try/catch and e-mail if failed for unexpected reasons
    buildbot_loop()

if __name__ == "__main__":
    try:
        main()
    except Exception, e:
        msg = "buildbot failed\nException: " + str(e) + "\n"
        email_msg(msg)
