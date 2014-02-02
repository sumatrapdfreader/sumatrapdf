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
  * file sumatrapdf/sumpdf-latest.txt must be manually updated

Building pre-release version:
  * get svn version
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
      sumatrapdf/sumpdf-prerelease-latest.txt
"""

import os
import shutil
import sys
import time
import re
import struct
import types
import s3
import util
from util import test_for_flag, run_cmd_throw
from util import verify_started_in_right_directory, parse_svninfo_out, log
from util import extract_sumatra_version, zip_file
from util import load_config, verify_path_exists
import trans_upload
import trans_download
from binascii import crc32


def usage():
    print(
        "build.py [-upload][-uploadtmp][-test][-test-installer][-prerelease][-platform=X64]")
    sys.exit(1)


def lzma_compress(src, dst):
    d = os.path.dirname(__file__)
    lzma = os.path.realpath(os.path.join(d, "..", "bin", "lzma.exe"))
    run_cmd_throw(lzma, "e", src, dst, "-f86")


def copy_to_dst_dir(src_path, dst_dir):
    name_in_obj_rel = os.path.basename(src_path)
    dst_path = os.path.join(dst_dir, name_in_obj_rel)
    shutil.copy(src_path, dst_path)


def is_more_recent(src_path, dst_path):
    return os.path.getmtime(src_path) > os.path.getmtime(dst_path)


def get_real_name(f):
    if type(f) in [types.ListType, types.TupleType]:
        assert len(f) == 2
        return f[0]
    return f


def get_in_archive_name(f):
    if type(f) in [types.ListType, types.TupleType]:
        assert len(f) == 2
        return f[1]
    return f

g_lzma_archive_magic_id = 0x4c7a5341


def get_file_crc32(path):
    with open(path, "rb") as fo:
        d = fo.read()
    checksum = crc32(d, 0)
    return checksum & 0xFFFFFFFF

"""
Create a simple lzma archive in format:

u32   magic_id 0x4c7a5341 ("LzSA' for "Lzma Simple Archive")
u32   number of files
for each file:
  u32        file size uncompressed
  u32        file size compressed
  u32        crc32 checksum of compressed data
  FILETIME   last modification time in Windows's FILETIME format
  char[...]  file name, 0-terminated
u32   crc32 checksum of the header (i.e. data so far)
for each file:
  compressed file data

Integers are little-endian.

You can over-write the file name in the archive by using list instead of
a string in files: ["foo.txt", "bar.txt"] will add file "foo.txt" under name
"bar.txt"
"""


def create_lzma_archive(dir, archiveName, files):
    for f in files:
        f = get_real_name(f)
        src = os.path.join(dir, f)
        dst = src + ".lzma"
        if not os.path.exists(dst) or is_more_recent(src, dst):
            lzma_compress(src, dst)

    d = struct.pack("<II", g_lzma_archive_magic_id, len(files))
    for f in files:
        real_name = get_real_name(f)
        path = os.path.join(dir, real_name)
        d += struct.pack("<I", os.path.getsize(path))
        d += struct.pack("<I", os.path.getsize(path + ".lzma"))
        d += struct.pack("<I", get_file_crc32(path + ".lzma"))
        d += struct.pack("<Q",
                         int((os.path.getmtime(path) + 11644473600L) * 10000000))
        f = get_in_archive_name(f)
        d += f + "\0"
    checksum = crc32(d, 0) & 0xFFFFFFFF
    d += struct.pack("<I", checksum)

    archive_path = os.path.join(dir, archiveName)
    with open(archive_path, "wb") as fo:
        fo.write(d)
        for f in files:
            f = get_real_name(f)
            path = os.path.join(dir, f) + ".lzma"
            with open(path, "rb") as fi:
                d = fi.read()
                fo.write(d)
    print("Created archive: %s" % archive_path)
    return archive_path

# build installer data, will be included as part of Installer.exe resources


def build_installer_data(dir):
    src = os.path.join("mupdf", "resources", "fonts",
                       "droid", "DroidSansFallback.ttf")
    if not os.path.exists(src):
        # location before
        # https://code.google.com/p/sumatrapdf/source/detail?r=8266
        src = os.path.join("mupdf", "fonts", "droid", "DroidSansFallback.ttf")
    assert os.path.exists(src)
    dst = os.path.join(dir, "DroidSansFallback.ttf")
    if not os.path.exists(dst) or is_more_recent(src, dst):
        copy_to_dst_dir(src, dir)

    files = [
        ["SumatraPDF-no-MuPDF.exe", "SumatraPDF.exe"], "DroidSansFallback.ttf",
        "libmupdf.dll", "npPdfViewer.dll", "PdfFilter.dll", "PdfPreview.dll",
        "uninstall.exe"]
    create_lzma_archive(dir, "InstallerData.dat", files)
    installer_res = os.path.join(dir, "sumatrapdf", "Installer.res")
    util.delete_file(installer_res)


def create_pdb_archive(dir, archive_name):
    files = ["libmupdf.pdb", "Installer.pdb",
             "SumatraPDF-no-MuPDF.pdb", "SumatraPDF.pdb"]
    return create_lzma_archive(dir, archive_name, files)

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
        # we don't control the name of the file pigz will create
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


def build(upload, upload_tmp, testing, build_test_installer, build_rel_installer, build_prerelease, skip_transl_update, svn_revision, target_platform):

    verify_started_in_right_directory()
    if build_prerelease:
        if svn_revision is None:
            run_cmd_throw("svn", "update")
            (out, err) = run_cmd_throw("svn", "info")
            ver = str(parse_svninfo_out(out))
        else:
            # allow to pass in an SVN revision, in case SVN itself isn't
            # available
            ver = svn_revision
    else:
        ver = extract_sumatra_version(os.path.join("src", "Version.h"))
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
                "\nNew translations have been downloaded from apptranslator.og")
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
    s3_pdb_zip = s3_prefix + ".pdb.zip"
    s3_exe_zip = s3_prefix + ".zip"

    s3_files = [s3_exe, s3_installer, s3_pdb_zip]
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

    (out, err) = run_cmd_throw("nmake", "-f", "makefile.msvc",
                               config, extcflags, platform, "all_sumatrapdf")
    if build_test_installer:
        print_run_resp(out, err)

    exe = os.path.join(obj_dir, "SumatraPDF.exe")
    if upload:
        sign(exe, cert_pwd)
        sign(os.path.join(obj_dir, "uninstall.exe"), cert_pwd)

    build_installer_data(obj_dir)
    (out, err) = run_cmd_throw("nmake", "-f", "makefile.msvc",
                               "Installer", config, platform, extcflags)
    if build_test_installer:
        print_run_resp(out, err)

    if build_test_installer or build_rel_installer:
        sys.exit(0)

    installer = os.path.join(obj_dir, "Installer.exe")
    if upload:
        sign(installer, cert_pwd)

    pdb_archive = create_pdb_archive(obj_dir, "%s.pdb.lzma" % filename_base)

    builds_dir = os.path.join("builds", ver)
    if os.path.exists(builds_dir):
        shutil.rmtree(builds_dir)
    os.makedirs(builds_dir)

    copy_to_dst_dir(exe, builds_dir)
    copy_to_dst_dir(installer, builds_dir)
    copy_to_dst_dir(pdb_archive, builds_dir)

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
        jstxt += 'var sumLatestExe = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_exe
        jstxt += 'var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_pdb_zip
        jstxt += 'var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_installer

    s3.upload_file_public(installer, s3_installer)
    s3.upload_file_public(pdb_archive, s3_pdb_zip)
    s3.upload_file_public(exe, s3_exe)

    if build_prerelease:
        s3.upload_data_public(jstxt, "sumatrapdf/sumatralatest.js")
        txt = "%s\n" % ver
        s3.upload_data_public(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")
        delete_old_pre_release_builds()
    else:
        s3.upload_file_public(exe_zip_path, s3_exe_zip)

    # Note: for release builds, must update sumatrapdf/sumpdf-latest.txt in s3
    # manually to: "%s\n" % ver


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
    main()
