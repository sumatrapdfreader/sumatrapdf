# What: build release version of sumatra and (optionally) upload it to s3

# How:
#   * extract version from Version.h
#   * build with nmake, sending svn version as argument
#   * compress with mpress
#   * build an installer
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/rel/SumatraPDF-<ver>.exe
#       sumatrapdf/rel/SumatraPDF-<ver>.zip
#       sumatrapdf/rel/SumatraPDF-<ver>.pdb
#       sumatrapdf/rel/SumatraPDF-<ver>-install.exe
#
#   * file sumatrapdf/sumpdf-latest.txt must be manually updated

import os
import os.path
import shutil
import sys
import time

from util import log, run_cmd_throw, test_for_flag, s3UploadFilePublic, s3UploadDataPublic, ensure_s3_doesnt_exist, ensure_path_exists, installer_mark_end, installer_append_file, installer_append_file_zlib, zip_file, extract_sumatra_version, verify_started_in_right_directory, build_installer_native

args = sys.argv
upload = test_for_flag(args, "-upload")
upload_tmp = test_for_flag(args, "-uploadtmp")
testing = test_for_flag(args, "-test") or test_for_flag(args, "-testing")
build_test_installer = test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")

def usage():
  print("sumatra-build-release.py [-upload] [sumatra-source-dir]")
  sys.exit(1)

def main():
  if len(args) != 1:
    usage()
  verify_started_in_right_directory()

  s3dir = "sumatrapdf/rel"
  if upload:
    log("Will upload to s3")
  if upload_tmp:
    s3dir = "sumatrapdf/tmp"
    log("Will upload to tmp s3")

  if build_test_installer:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
    objdir = os.path.join("obj-dbg")
    build_installer_native(objdir, None)
    sys.exit(0)

  ver = extract_sumatra_version(os.path.join("src", "Version.h"))
  log("Version: '%s'" % ver)

  tmp = "%s/SumatraPDF-%s" % (s3dir, ver)
  remote_exe           = "%s.exe" % tmp
  remote_pdb           = "%s.pdb" % tmp
  remote_zip           = "%s.zip" % tmp
  remote_installer_exe = "%s-install.exe" % tmp

  if upload:
    map(ensure_s3_doesnt_exist, [remote_exe, remote_pdb, remote_zip, remote_installer_exe])

  if not testing and os.path.exists("obj-rel"):
    shutil.rmtree("obj-rel", ignore_errors=True)

  builds_dir = os.path.join("builds", ver)
  if not testing and os.path.exists(builds_dir):
    shutil.rmtree(builds_dir)
  if not os.path.exists(builds_dir):
    os.makedirs(builds_dir)

  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  files = ["SumatraPDF.exe", "SumatraPDF.pdb", "npPdfViewer.dll", "Installer.exe"]
  [tmp_exe, tmp_pdb, tmp_plugin, tmp_installer] = [os.path.join("obj-rel", t) for t in files]

  local_exe = os.path.join(builds_dir, "SumatraPDF-%s.exe" % ver)
  local_exe_uncompr = os.path.join(builds_dir, "SumatraPDF-uncompr.exe")
  local_pdb = os.path.join(builds_dir, "SumatraPDF-%s.pdb" % ver)
  local_plugin = os.path.join(builds_dir, "npPdfViewer.dll")
  local_installer = os.path.join(builds_dir, "Installer.exe")
  local_zip = os.path.join(builds_dir, "SumatraPDF-%s.zip" % ver)

  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_pdb, local_pdb)
  shutil.copy(tmp_plugin, local_plugin)
  shutil.copy(tmp_installer, local_installer)

  # run mpress and StripReloc from inside builds_dir for better
  # compat across python version
  prevdir = os.getcwd(); os.chdir(builds_dir)
  run_cmd_throw("StripReloc", "Installer.exe")
  run_cmd_throw("mpress", "-s", "-r", "SumatraPDF-%s.exe" % ver)
  os.chdir(prevdir)

  zip_file(local_zip, local_exe, "SumatraPDF.exe")
  ensure_path_exists(local_zip)

  nameprefix = "SumatraPDF-%s" % ver
  local_installer_native_exe = build_installer_native(builds_dir, nameprefix)

  if upload or upload_tmp:
    s3UploadFilePublic(local_exe_uncompr, remote_exe)
    s3UploadFilePublic(local_pdb, remote_pdb)
    s3UploadFilePublic(local_zip, remote_zip)
    s3UploadFilePublic(local_installer_native_exe, remote_installer_exe)

  map(os.remove, [local_exe, local_installer, local_installer + ".bak"])

if __name__ == "__main__":
  main()
