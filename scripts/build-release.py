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
  log("Starting build-release.py")

  s3dir = "rel"
  if upload:
    log("Will upload to s3")
  if upload_tmp:
    s3dir = "tmp"
    log("Will upload to tmp s3")

  if len(args) != 1:
    usage()

  verify_started_in_right_directory()
  topdir = os.getcwd()

  if build_test_installer:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
    objdir = os.path.join("obj-dbg")
    build_installer_native(objdir, None)
    sys.exit(0)

  ver = extract_sumatra_version(os.path.join("src", "Version.h"))
  log("Version: '%s'" % ver)

  remote_exe           = "sumatrapdf/%s/SumatraPDF-%s.exe" % (s3dir, ver)
  remote_pdb           = "sumatrapdf/%s/SumatraPDF-%s.pdb" % (s3dir, ver)
  remote_zip           = "sumatrapdf/%s/SumatraPDF-%s.zip" % (s3dir, ver)
  remote_installer_exe = "sumatrapdf/%s/SumatraPDF-%s-install.exe" % (s3dir, ver)

  if upload:
    for t in [remote_exe, remote_pdb, remote_zip, remote_installer_exe]:
      ensure_s3_doesnt_exist(t)

  if not testing and os.path.exists("obj-rel"):
    shutil.rmtree("obj-rel", ignore_errors=True)

  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  tmp_exe = os.path.join("obj-rel", "SumatraPDF.exe")
  tmp_pdb = os.path.join("obj-rel", "SumatraPDF.pdb")
  tmp_installer = os.path.join("obj-rel", "Installer.exe")
  tmp_installer_pdb = os.path.join("obj-rel", "Installer.pdb")

  for t in [tmp_exe, tmp_pdb, tmp_installer, tmp_installer_pdb]:
    ensure_path_exists(t)

  builds_dir = os.path.join("builds", ver)

  # ensure clean build
  if os.path.exists(builds_dir) and not testing:
    shutil.rmtree(builds_dir)

  if not os.path.exists(builds_dir):
    os.makedirs(builds_dir)
  ensure_path_exists(builds_dir)

  local_exe = os.path.join(builds_dir, "SumatraPDF-%s.exe" % ver)
  local_exe_uncompr = os.path.join(builds_dir, "SumatraPDF-uncompr.exe")
  local_pdb = os.path.join(builds_dir, "SumatraPDF-%s.pdb" % ver)
  local_installer = os.path.join(builds_dir, "Installer.exe")
  local_installer_pdb = os.path.join(builds_dir, "Installer.pdb")

  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_pdb, local_pdb)
  shutil.copy(tmp_installer, local_installer)
  shutil.copy(tmp_installer_pdb, local_installer_pdb)

  stripreloc = os.path.join("bin", "StripReloc")
  run_cmd_throw(stripreloc, os.path.join(builds_dir, "Installer.exe"))

  # mpress and zip expect to be run from inside builds_dir
  os.chdir(builds_dir)
  mpress = os.path.join("..", "bin", "mpress")
  if not os.path.isfile(mpress): mpress = "mpress"
  run_cmd_throw(mpress, "-s", "-r", "SumatraPDF-%s.exe" % ver)
  zip_file("SumatraPDF-%s.zip" % ver, "SumatraPDF-%s.exe" % ver, "SumatraPDF.exe")
  os.chdir(topdir)

  local_zip = os.path.join(builds_dir, "SumatraPDF-%s.zip" % ver)
  ensure_path_exists(local_zip)
  local_installer_native_exe = build_installer_native(builds_dir, ver)

  os.remove(local_exe)
  os.remove(local_installer)
  os.remove(local_installer + ".bak")

  if upload or upload_tmp:
    s3UploadFilePublic(local_exe_uncompr, remote_exe)
    s3UploadFilePublic(local_pdb, remote_pdb)
    s3UploadFilePublic(local_zip, remote_zip)
    s3UploadFilePublic(local_installer_native_exe, remote_installer_exe)

if __name__ == "__main__":
  main()
