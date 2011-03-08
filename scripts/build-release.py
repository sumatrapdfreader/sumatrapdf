"""
Builds a (pre)release build of SumatraPDF, including the installer,
and optionally uploads it to s3.
"""

import os
import os.path
import shutil
import sys
import time

from util import log, run_cmd_throw, test_for_flag, s3UploadFilePublic, s3UploadDataPublic, ensure_s3_doesnt_exist, ensure_path_exists, zip_file, extract_sumatra_version, verify_started_in_right_directory, build_installer_native, parse_svninfo_out

args = sys.argv
upload               = test_for_flag(args, "-upload")
upload_tmp           = test_for_flag(args, "-uploadtmp")
testing              = test_for_flag(args, "-test") or test_for_flag(args, "-testing")
build_test_installer = test_for_flag(args, "-test-installer") or test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")
build_prerelease     = test_for_flag(args, "-prerelease")

def usage():
  print("build-release.py [-upload][-uploadtmp][-test][-test-installer][-prerelease]")
  sys.exit(1)

# What: build release version of sumatra and (optionally) upload it to s3
# 
# How:
#   * extract version from Version.h
#   * build with nmake, sending version as argument
#   * compress with mpress
#   * build an installer
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/rel/SumatraPDF-<ver>.exe
#       sumatrapdf/rel/SumatraPDF-<ver>.zip
#       sumatrapdf/rel/SumatraPDF-<ver>.pdb
#       sumatrapdf/rel/SumatraPDF-<ver>-install.exe
#
#   * file sumatrapdf/sumpdf-latest.txt must be manually updated

# What: build a pre-release version of sumatra and
#       upload it to s3
# How:
#   * get svn version
#   * build with nmake, sending svn version as argument
#   * compress with mpress
#   * build an installer
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.exe
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.pdb.zip
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-dbg.exe
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-dbg.pdb.zip
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-install.exe
#       sumatrapdf/sumatralatest.js
#       sumatrapdf/sumpdf-prerelease-latest.txt

def main():
  if len(args) != 1:
    usage()
  verify_started_in_right_directory()

  if build_test_installer:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
    build_installer_native("obj-dbg", None)
    sys.exit(0)

  if build_prerelease:
    run_cmd_throw("svn", "update")
    (out, err) = run_cmd_throw("svn", "info")
    ver = str(parse_svninfo_out(out))
  else:
    ver = extract_sumatra_version(os.path.join("src", "Version.h"))
  log("Version: '%s'" % ver)

  filename_base = "SumatraPDF-%s" % ver
  if build_prerelease:
    filename_base = "SumatraPDF-prerelease-%s" % ver

  s3dir = "sumatrapdf/rel"
  if upload:
    log("Will upload to s3")
  if build_prerelease:
    s3dir = "sumatrapdf/prerel"
  elif upload_tmp:
    s3dir = "sumatrapdf/tmp"
    log("Will upload to tmp s3")

  tmp = "%s/%s" % (s3dir, filename_base)
  remote_exe           = "%s.exe" % tmp
  remote_pdb           = "%s.pdb" % tmp
  remote_zip           = "%s.zip" % tmp
  remote_installer_exe = "%s-install.exe" % tmp
  if build_prerelease:
    remote_pdb_zip     = remote_pdb + ".zip"
    remote_exe_dbg     = "%s-dbg.exe" % tmp
    remote_pdb_dbg_zip = "%s-dbg.pdb.zip" % tmp

  if upload:
    remote_files = [remote_exe, remote_installer_exe]
    if build_prerelease:
      remote_files += [remote_pdb_zip, remote_exe_dbg, remote_pdb_dbg_zip]
    else:
      remote_files += [remote_pdb, remote_zip]
    map(ensure_s3_doesnt_exist, remote_files)

  if not testing:
    shutil.rmtree("obj-rel", ignore_errors=True)
    if build_prerelease:
      shutil.rmtree("obj-dbg", ignore_errors=True)

  builds_dir = os.path.join("builds", ver)
  if not testing and os.path.exists(builds_dir):
    shutil.rmtree(builds_dir)
  if not os.path.exists(builds_dir):
    os.makedirs(builds_dir)

  if build_prerelease:
    extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", extcflags)
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg", extcflags)
  else:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  files = ["SumatraPDF.exe", "SumatraPDF.pdb", "npPdfViewer.dll", "Installer.exe",
           "SumatraPDF-no-MuPDF.exe", "SumatraPDF-no-MuPDF.pdb", "libmupdf.dll", "libmupdf.pdb", "PdfFilter.dll"]
  [tmp_exe, tmp_pdb, tmp_plugin, tmp_installer, tmp_exe2, tmp_pdb2, tmp_lib, tmp_lib_pdb, tmp_filter] = [os.path.join("obj-rel", t) for t in files]

  local_exe = os.path.join(builds_dir, "%s.exe" % filename_base)
  local_exe_uncompr = os.path.join(builds_dir, "%s-uncompr.exe" % filename_base)
  local_pdb = os.path.join(builds_dir, "%s.pdb" % filename_base)
  local_zip = os.path.join(builds_dir, "%s.zip" % filename_base)
  [_, _, local_plugin, local_installer, local_exe2, _, local_lib, _, local_filter] = [os.path.join(builds_dir, t) for t in files]

  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_exe2, local_exe2)
  shutil.copy(tmp_lib, local_lib)
  shutil.copy(tmp_pdb, local_pdb)
  shutil.copy(tmp_plugin, local_plugin)
  shutil.copy(tmp_filter, local_filter)
  shutil.copy(tmp_installer, local_installer)

  if build_prerelease:
    local_exe_dbg = os.path.join(builds_dir, "%s-dbg.exe" % filename_base)
    local_pdb_zip = local_pdb + ".zip"
    local_pdb_dbg_zip = os.path.join(builds_dir, "%s-dbg.pdb.zip" % filename_base)

    [tmp_exe_dbg, tmp_pdb_dbg, _, _, _, tmp_pdb2_dbg, _, tmp_lib_pdb_dbg, _] = [os.path.join("obj-dbg", t) for t in files]
    shutil.copy(tmp_exe_dbg, local_exe_dbg)

    zip_file(local_pdb_zip, local_pdb)
    zip_file(local_pdb_zip, tmp_pdb2, append=True)
    zip_file(local_pdb_zip, tmp_lib_pdb, append=True)
    zip_file(local_pdb_dbg_zip, tmp_pdb_dbg, "%s.pdb" % filename_base)
    zip_file(local_pdb_dbg_zip, tmp_pdb2_dbg, append=True)
    zip_file(local_pdb_dbg_zip, tmp_lib_pdb_dbg, append=True)

  # run mpress and StripReloc from inside builds_dir for better
  # compat across python version
  prevdir = os.getcwd(); os.chdir(builds_dir)
  run_cmd_throw("StripReloc", "Installer.exe")
  run_cmd_throw("mpress", "-s", "-r", "%s.exe" % filename_base)
  if build_prerelease:
    run_cmd_throw("mpress", "-s", "-r", "%s-dbg.exe" % filename_base)
  os.chdir(prevdir)

  local_installer_native_exe = build_installer_native(builds_dir, filename_base)
  if not build_prerelease:
    zip_file(local_zip, local_exe, "SumatraPDF.exe", False)
    ensure_path_exists(local_zip)

  if upload and build_prerelease:
    s3UploadFilePublic(local_exe, remote_exe)
    s3UploadFilePublic(local_exe_dbg, remote_exe_dbg)
    s3UploadFilePublic(local_pdb_zip, remote_pdb_zip)
    s3UploadFilePublic(local_pdb_dbg_zip, remote_pdb_dbg_zip)
    s3UploadFilePublic(local_installer_native_exe, remote_installer_exe)

    jstxt  = 'var sumLatestVer = %s;\n' % ver
    jstxt  = 'var sumBuiltOn = "%s";\n' % time.strftime("%Y-%m-%d")
    jstxt += 'var sumLatestName = "%s";\n' % remote_exe.split("/")[-1]
    jstxt += 'var sumLatestExe = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_exe
    jstxt += 'var sumLatestExeDbg = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_exe_dbg
    jstxt += 'var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_pdb_zip
    jstxt += 'var sumLatestPdbDbg = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_pdb_dbg_zip
    jstxt += 'var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_installer_exe
    s3UploadDataPublic(jstxt, "sumatrapdf/sumatralatest.js")
    txt = "%s\n" % ver
    s3UploadDataPublic(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")

  elif upload or upload_tmp and not build_prerelease:
    s3UploadFilePublic(local_exe_uncompr, remote_exe)
    # TODO: also upload PDBs for libmupdf.dll, SumatraPDF-no-mupdf.exe, npPdfViewer.dll, etc.?
    s3UploadFilePublic(local_pdb, remote_pdb)
    s3UploadFilePublic(local_zip, remote_zip)
    s3UploadFilePublic(local_installer_native_exe, remote_installer_exe)
    txt = "%s\n" % ver
    # s3UploadDataPublic(txt, "sumatrapdf/sumpdf-latest.txt")

  files = [local_installer, local_installer + ".bak", local_plugin, local_exe2, local_lib, local_filter]
  if build_prerelease:
    files += [local_pdb]
  else:
    files += [local_exe]
  map(os.remove, files)

if __name__ == "__main__":
  main()
