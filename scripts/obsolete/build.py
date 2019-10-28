#!/usr/bin/env python

"""
Builds a (pre)release build of SumatraPDF, including the installer,
and optionally uploads it to s3.

Terms:
 static build  - SumatraPDF.exe single executable with mupdf code statically
                 linked in
 library build - SumatraPDF.exe executable that uses libmupdf.dll

Building release version:
  * extract version from Version.h
  * build with nmake, sending version as argument
  * build an installer
  * upload to s3 kjkpub bucket. Uploaded files:
      sumatrapdf/rel/SumatraPDF-<ver>.exe
         uncompressed portable executable, for archival
      sumatrapdf/rel/SumatraPDF-<ver>.pdb.zip
         pdb symbols for libmupdf.dll, and Sumatra's static and library builds
      sumatrapdf/rel/SumatraPDF-<ver>-install.exe
         installer for library build
      sumatrapdf/sumpdf-update.txt
         updates Latest version, keeps Stable version
  * files sumatrapdf/sumpdf-update.txt and sumatrapdf/sumpdf-latest.txt
    must be manually updated using update_auto_update_ver.py in order to
    make automated update checks find the latest version

Building pre-release version:
  * get git version
  * build with nmake, sending svn version as argument
  * build an installer
  * upload to s3 kjkpub bucket. Uploaded files:
      sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.exe
        static, portable executable
      sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.pdb.zip
         pdb symbols for libmupdf.dll and Sumatra's static and library builds
      sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-install.exe
         installer for library build
      sumatrapdf/sumatralatest.js
      sumatrapdf/sumpdf-prerelease-update.txt
      sumatrapdf/sumpdf-prerelease-latest.txt
"""

import os
import shutil
import sys
import time
import re
import struct
import types
import subprocess
import s3
import util
from util import test_for_flag, run_cmd_throw, run_cmd
from util import verify_started_in_right_directory, log
from util import extract_sumatra_version, zip_file, get_git_linear_version
from util import load_config, verify_path_exists, strip_empty_lines
import trans_upload
import trans_download
import upload_sources


def usage():
    print(
        "build.py [-upload][-uploadtmp][-test][-test-installer][-prerelease][-platform=X64]")
    sys.exit(1)


@util.memoize
def get_top_dir():
    scripts_dir = os.path.realpath(os.path.dirname(__file__))
    return os.path.realpath(os.path.join(scripts_dir, ".."))


def copy_to_dst_dir(src_path, dst_dir):
    name_in_obj_rel = os.path.basename(src_path)
    dst_path = os.path.join(dst_dir, name_in_obj_rel)
    shutil.copy(src_path, dst_path)


def create_pdb_lzsa_archive(dir, archive_name):
    archive_path = os.path.join(dir, archive_name)
    MakeLzsa = os.path.join(dir, "MakeLZSA.exe")
    files = ["libmupdf.pdb", "Installer.pdb",
             "SumatraPDF-mupdf-dll.pdb", "SumatraPDF.pdb"]
    files = [os.path.join(dir, file) + ":" + file for file in files]
    run_cmd_throw(MakeLzsa, archive_path, *files)
    return archive_path


def create_pdb_zip_archive(dir, archive_name):
    archive_path = os.path.join(dir, archive_name)
    files = ["libmupdf.pdb", "Installer.pdb",
             "SumatraPDF-mupdf-dll.pdb", "SumatraPDF.pdb"]
    for file_name in files:
        file_path = os.path.join(dir, file_name)
        zip_file(archive_path, file_path, file_name, compress=True, append=True)
    print("Created zip archive: %s" % archive_path)
    return archive_path


# delete all but the last 3 pre-release builds in order to use less s3 storage
def delete_old_pre_release_builds():
    s3Dir = "sumatrapdf/prerel/"
    keys = s3.list(s3Dir)
    files_by_ver = {}
    for k in keys:
        # print(k.name)
        # sumatrapdf/prerel/SumatraPDF-prerelease-4819.pdb.zip
        ver = re.findall(
            r'sumatrapdf/prerel/SumatraPDF-prerelease-(\d+)*', k.name)
        ver = int(ver[0])
        # print(ver)
        val = files_by_ver.get(ver, [])
        # print(val)
        val.append(k.name)
        # print(val)
        files_by_ver[ver] = val
    versions = files_by_ver.keys()
    versions.sort()
    # print(versions)
    todelete = versions[:-3]
    # print(todelete)
    for vertodelete in todelete:
        for f in files_by_ver[vertodelete]:
            #print("Deleting %s" % f)
            s3.delete(f)


def sign(file_path, cert_pwd):
    # not everyone has a certificate, in which case don't sign
    if cert_pwd is None:
        print("Skipping signing %s" % file_path)
        return
    # the sign tool is finicky, so copy it and cert to the same dir as
    # exe we're signing
    file_dir = os.path.dirname(file_path)
    file_name = os.path.basename(file_path)
    cert_src = os.path.join("scripts", "cert.pfx")
    cert_dest = os.path.join(file_dir, "cert.pfx")
    if not os.path.exists(cert_dest):
        shutil.copy(cert_src, cert_dest)
    curr_dir = os.getcwd()
    os.chdir(file_dir)
    run_cmd_throw(
        "signtool.exe", "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
        "/du", "http://blog.kowalczyk.info/software/sumatrapdf/", "/f", "cert.pfx", "/p", cert_pwd, file_name)
    os.chdir(curr_dir)


# sometimes sign() fails, probably because of time-stamping, so we retry 3 times,
# 1 minute apart
def sign_retry(file_path, cert_pwd):
    nRetries = 3
    while nRetries > 1:  # the last one will rethrow
        try:
            sign(file_path, cert_pwd)
            return
        except:
            time.sleep(60)  # 1 min
        nRetries -= 1
    sign(file_path, cert_pwd)


def print_run_resp(out, err):
    if len(out) > 0:
        print(out)
    if len(err) > 0:
        print(err)


def zip_one_file(dir, to_pack, zip_name):
    verify_path_exists(dir)
    # for the benefit of pigz, we have to cd to the directory, because
    # we don't control the name of the file inside created zip file - it's
    # the same as path of the file we're compressing
    curr_dir = os.getcwd()
    os.chdir(dir)
    verify_path_exists(to_pack)
    util.delete_file(zip_name)  # ensure destination doesn't exist
    try:
        # -11 for zopfil compression
        # --keep to not delete the source file
        # --zip to create a single-file zip archive
        # we can't control the name of the file pigz will create, so rename
        # to desired name after it's created
        pigz_dst = to_pack + ".zip"
        util.delete_file(pigz_dst)
        run_cmd_throw("pigz", "-11", "--keep", "--zip", to_pack)
        print("Compressed using pigz.exe")
        if pigz_dst != zip_name:
            print("moving %s => %s" % (pigz_dst, zip_name))
            shutil.move(pigz_dst, zip_name)
    except:
        # if pigz.exe is not in path, use regular zip compression
        zip_file(zip_name, to_pack, to_pack, compress=True)
        print("Compressed using regular zip")
    verify_path_exists(zip_name)
    os.chdir(curr_dir)


# returns a ver up to first decimal point i.e. "2.3.1" => "2.3"
def get_short_ver(ver):
    parts = ver.split(".")
    if len(parts) <= 2:
        return ver
    return parts[0] + "." + parts[1]


# when doing a release build, we must be on /svn/branches/${ver_short}working
# branch
def verify_correct_branch(ver):
    # TODO: update for git
    raise BaseException("implement for git")
    #short_ver = get_short_ver(ver)
    #branch = get_svn_branch()
    #expected = "/branches/%sworking" % short_ver
    #assert branch == expected, "svn branch is '%s' and should be '%s' for version %s (%s)" % (branch, expected, ver, short_ver)


# if we haven't tagged this release in svn yet, svn info for the /tags/${ver}rel
# must fail
def verify_not_tagged_yet(ver):
    # TODO: update for git
    raise BaseException("implement for git")
    #out, err, errcode = run_cmd("svn", "info", "https://sumatrapdf.googlecode.com/svn/tags/%srel" % ver)
    #assert errcode == 1, "out: '%s'\nerr:'%s'\nerrcode:%d" % (out, err, errcode)


def svn_tag_release(ver):
    raise BaseException("implement for git")
    #working = "https://sumatrapdf.googlecode.com/svn/branches/%sworking" % get_short_ver(ver)
    #rel = "https://sumatrapdf.googlecode.com/svn/tags/%srel" % ver
    #msg = "tag %s release" % ver
    #run_cmd_throw("svn", "copy", working, rel, "-m", msg)


def try_find_scripts_file(file_name):
    top_dir = get_top_dir()
    dst = os.path.join(top_dir, "scripts", file_name)
    src = os.path.join(top_dir, "..", "sumatrapdf", "scripts", file_name)
    if not os.path.exists(dst) and os.path.exists(src):
        shutil.copyfile(src, dst)


# returns the version marked as Stable at the given url;
# returns the Latest version or the provided fallback if it's missing
def get_stable_version(url, fallback):
    import urllib2
    import SquareTree
    try:
        data = urllib2.urlopen(url).read()
        root = SquareTree.Parse(data)
        node = root.GetChild("SumatraPDF")
        return node.GetValue("Stable") or node.GetValue("Latest") or fallback
    except:
        return fallback


# if scripts/cert.pfx and scripts/config.py don't exist, try to copy them from
# ../../sumatrapdf/scripts directory
def try_find_config_files():
    try_find_scripts_file("config.py")
    try_find_scripts_file("cert.pfx")


def append_to_file(path, s):
    with open(path, "a") as fo:
        fo.write(s)
        fo.write("\n---------------------------------------------\n\n")


def build(upload, upload_tmp, testing, build_test_installer, build_rel_installer, build_prerelease, skip_transl_update, svn_revision, target_platform):

    verify_started_in_right_directory()
    try_find_config_files()
    if build_prerelease:
        if svn_revision is None:
            ver = str(get_git_linear_version())
        else:
            # allow to pass in an SVN revision, in case SVN itself isn't
            # available
            ver = svn_revision
    else:
        ver = extract_sumatra_version(os.path.join("src", "Version.h"))
        if upload:
            verify_correct_branch(ver)
            verify_not_tagged_yet(ver)

    log("Version: '%s'" % ver)

    # don't update translations for release versions to prevent Trunk changes
    # from messing up the compilation of a point release on a branch
    if build_prerelease and not skip_transl_update:
        trans_upload.uploadStringsIfChanged()
        changed = trans_download.downloadAndUpdateTranslationsIfChanged()
        # Note: this is not a perfect check since re-running the script will
        # proceed
        if changed:
            print(
                "\nNew translations have been downloaded from apptranslator.org")
            print(
                "Please verify and checkin src/Translations_txt.cpp and strings/translations.txt")
            sys.exit(1)

    filename_base = "SumatraPDF-%s" % ver
    if build_prerelease:
        filename_base = "SumatraPDF-prerelease-%s" % ver

    s3_dir = "sumatrapdf/rel"
    if build_prerelease:
        s3_dir = "sumatrapdf/prerel"
    if upload_tmp:
        upload = True
        s3_dir += "tmp"

    if upload:
        log("Will upload to s3 at %s" % s3_dir)
        conf = load_config()
        s3.set_secrets(conf.aws_access, conf.aws_secret)
        s3.set_bucket("kjkpub")

    s3_prefix = "%s/%s" % (s3_dir, filename_base)
    s3_exe = s3_prefix + ".exe"
    s3_installer = s3_prefix + "-install.exe"
    s3_pdb_lzsa = s3_prefix + ".pdb.lzsa"
    s3_pdb_zip = s3_prefix + ".pdb.zip"
    s3_exe_zip = s3_prefix + ".zip"

    s3_files = [s3_exe, s3_installer, s3_pdb_lzsa, s3_pdb_zip]
    if not build_prerelease:
        s3_files.append(s3_exe_zip)

    cert_pwd = None
    cert_path = os.path.join("scripts", "cert.pfx")
    if upload:
        map(s3.verify_doesnt_exist, s3_files)
        verify_path_exists(cert_path)
        conf = load_config()
        cert_pwd = conf.GetCertPwdMustExist()

    obj_dir = "obj-rel"
    if target_platform == "X64":
        obj_dir += "64"

    log_file_path = os.path.join(obj_dir, "build_log.txt")
    if not testing and not build_test_installer and not build_rel_installer:
        shutil.rmtree(obj_dir, ignore_errors=True)
        shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)

    config = "CFG=rel"
    if build_test_installer and not build_prerelease:
        obj_dir = "obj-dbg"
        config = "CFG=dbg"
    extcflags = ""
    if build_prerelease:
        extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
    platform = "PLATFORM=%s" % (target_platform or "X86")

    # build executables for signing (building the installer will build the rest)
    (out, err) = run_cmd_throw("nmake", "-f", "makefile.msvc",
                               config, extcflags, platform,
                               "SumatraPDF", "Uninstaller")
    if build_test_installer:
        print_run_resp(out, err)

    append_to_file(log_file_path, strip_empty_lines(out))

    exe = os.path.join(obj_dir, "SumatraPDF.exe")
    sign_retry(exe, cert_pwd)
    sign_retry(os.path.join(obj_dir, "SumatraPDF-mupdf-dll.exe"), cert_pwd)
    sign_retry(os.path.join(obj_dir, "uninstall.exe"), cert_pwd)

    (out, err) = run_cmd_throw("nmake", "-f", "makefile.msvc",
                               "Installer", config, platform, extcflags)
    if build_test_installer:
        print_run_resp(out, err)

    append_to_file(log_file_path, strip_empty_lines(out))

    if build_test_installer or build_rel_installer:
        sys.exit(0)

    installer = os.path.join(obj_dir, "Installer.exe")
    sign_retry(installer, cert_pwd)

    pdb_lzsa_archive = create_pdb_lzsa_archive(obj_dir, "%s.pdb.lzsa" % filename_base)
    pdb_zip_archive = create_pdb_zip_archive(obj_dir, "%s.pdb.zip" % filename_base)

    builds_dir = os.path.join("builds", ver)
    if os.path.exists(builds_dir):
        shutil.rmtree(builds_dir)
    os.makedirs(builds_dir)

    copy_to_dst_dir(exe, builds_dir)
    copy_to_dst_dir(installer, builds_dir)
    copy_to_dst_dir(pdb_lzsa_archive, builds_dir)
    copy_to_dst_dir(pdb_zip_archive, builds_dir)

    # package portable version in a .zip file
    if not build_prerelease:
        exe_zip_name = "%s.zip" % filename_base
        zip_one_file(obj_dir, "SumatraPDF.exe", exe_zip_name)
        exe_zip_path = os.path.join(obj_dir, exe_zip_name)
        copy_to_dst_dir(exe_zip_path, builds_dir)

    if not upload:
        return

    if build_prerelease:
        jstxt = 'var sumLatestVer = %s;\n' % ver
        jstxt += 'var sumBuiltOn = "%s";\n' % time.strftime("%Y-%m-%d")
        jstxt += 'var sumLatestName = "%s";\n' % s3_exe.split("/")[-1]
        jstxt += 'var sumLatestExe = "https://kjkpub.s3.amazonaws.com/%s";\n' % s3_exe
        jstxt += 'var sumLatestPdb = "https://kjkpub.s3.amazonaws.com/%s";\n' % s3_pdb_zip
        jstxt += 'var sumLatestInstaller = "https://kjkpub.s3.amazonaws.com/%s";\n' % s3_installer

    s3.upload_file_public(installer, s3_installer)
    s3.upload_file_public(pdb_lzsa_archive, s3_pdb_lzsa)
    s3.upload_file_public(pdb_zip_archive, s3_pdb_zip)
    s3.upload_file_public(exe, s3_exe)

    if build_prerelease:
        s3.upload_data_public(jstxt, "sumatrapdf/sumatralatest.js")
        # don't set a Stable version for prerelease builds
        txt = "[SumatraPDF]\nLatest %s\n" % ver
        s3.upload_data_public(txt, "sumatrapdf/sumpdf-prerelease-update.txt")
        # keep updating the legacy file for now
        txt = "%s\n" % ver
        s3.upload_data_public(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")
        delete_old_pre_release_builds()
    else:
        # update the Latest version for manual update checks but
        # leave the Stable version for automated update checks
        update_url = "https://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-update.txt"
        ver_stable = get_stable_version(update_url, "2.5.2")
        s3.upload_file_public(exe_zip_path, s3_exe_zip)
        s3.upload_data_public("[SumatraPDF]\nLatest %s\nStable %s\n" % (ver, ver_stable), "sumatrapdf/sumpdf-update.txt")

    if not build_prerelease:
        svn_tag_release(ver)
        upload_sources.upload(ver)

    # Note: for release builds, must run scripts/update_auto_update_ver.py


def build_pre_release():
    build(
        upload=True, upload_tmp=False, testing=False, build_test_installer=False,
        build_rel_installer=False, build_prerelease=True, skip_transl_update=True,
        svn_revision=None, target_platform=None)


def main():
    args = sys.argv[1:]
    upload = test_for_flag(args, "-upload")
    upload_tmp = test_for_flag(args, "-uploadtmp")
    testing = test_for_flag(
        args, "-test") or test_for_flag(args, "-testing")
    build_test_installer = test_for_flag(
        args, "-test-installer") or test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")
    build_rel_installer = test_for_flag(args, "-testrelinst")
    build_prerelease = test_for_flag(args, "-prerelease")
    skip_transl_update = test_for_flag(args, "-noapptrans")
    svn_revision = test_for_flag(args, "-svn-revision", True)
    target_platform = test_for_flag(args, "-platform", True)

    if len(args) != 0:
        usage()
    build(
        upload, upload_tmp, testing, build_test_installer, build_rel_installer,
        build_prerelease, skip_transl_update, svn_revision, target_platform)


def test_zip():
    dir = "obj-rel"
    to_pack = "SumatraPDF.exe"
    zip_name = "SumatraPDF-2.3.zip"
    zip_one_file(dir, to_pack, zip_name)
    sys.exit(0)

if __name__ == "__main__":
    #print("svn ver: %d"  % get_git_linear_version())
    main()
