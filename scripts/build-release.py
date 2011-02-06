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

from util import log, run_cmd_throw, test_for_flag, s3UploadFilePublic, s3UploadDataPublic, ensure_s3_doesnt_exist, ensure_path_exists, installer_mark_end, installer_append_file, installer_append_file_zlib, zip_file, extract_sumatra_version

args = sys.argv
upload = test_for_flag(args, "-upload")
upload_tmp = test_for_flag(args, "-uploadtmp")
testing = test_for_flag(args, "-test") or test_for_flag(args, "-testing")
build_test_installer = test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")

SCRIPT_DIR = os.path.dirname(__file__)
if SCRIPT_DIR:
  SCRIPT_DIR = os.path.split(SCRIPT_DIR)[0]
else:
  SCRIPT_DIR = os.getcwd()

def usage():
  print("sumatra-build-release.py [-upload] [sumatra-source-dir]")
  sys.exit(1)

# construct a full installer by appending data at the end of installer executable.
# appended data is in the format:
#  $data - data as binary. In our case it's Sumatra's binary
#  $data_size - as 32-bit integer
#  $data-name - name of the data. In our case it's name of the file to be written out
#  $data-name-len - length of $data-name, as 32-bit integer
#  $header - 4 byte, unique header of this section ('kifi' - kjk installer file info)
# this format is designed to be read backwards (because it's easier for the installer to
# seek to the end of itself than parse pe header to figure out where the executable ends
# and data starts)
def build_installer_native(builds_dir, ver):
  installer_template_exe = os.path.join(builds_dir, "Installer.exe")
  if ver is not None:
    installer_exe = os.path.join(builds_dir, "SumatraPDF-%s-install.exe" % ver)
    exe = os.path.join(builds_dir, "SumatraPDF-%s.exe" % ver)
  else:
    installer_exe = os.path.join(builds_dir, "SumatraPDF-install.exe")
    exe = os.path.join(builds_dir, "SumatraPDF.exe")

  shutil.copy(installer_template_exe, installer_exe)

  fo = open(installer_exe, "ab")
  # append installer data to installer exe
  installer_mark_end(fo) # this are read backwards so end marker is written first
  installer_append_file(fo, exe, "SumatraPDF.exe")
  font_name =  "DroidSansFallback.ttf"
  font_path = os.path.join(SCRIPT_DIR, "mupdf", "fonts", "droid", font_name)
  installer_append_file_zlib(fo, font_path, font_name)
  fo.close()
  return installer_exe

def verify_started_in_right_directory():
  p1 = os.path.join("scripts", "build-release.py")
  p2 = os.path.join(os.getcwd(), "scripts", "build-release.py")
  if not (os.path.exists(p1) and os.path.exists(p2)):
    print("This script must be run from top of the source tree")
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

  srcdir = os.getcwd()
  verify_started_in_right_directory()

  if build_test_installer:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
    objdir = os.path.join(SCRIPT_DIR, "obj-dbg")
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

  objdir = "obj-rel"
  if not testing and os.path.exists(objdir):
    shutil.rmtree(objdir, ignore_errors=True)

  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  tmp_exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  tmp_pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  tmp_installer = os.path.join(srcdir, objdir, "Installer.exe")
  tmp_installer_pdb = os.path.join(srcdir, objdir, "Installer.pdb")

  for t in [tmp_exe, tmp_pdb, tmp_installer, tmp_installer_pdb]:
    ensure_path_exists(t)

  builds_dir = os.path.join(SCRIPT_DIR, "builds", ver)

  if testing and os.path.exists(builds_dir):
    shutil.rmtree(builds_dir)

  if not os.path.exists(builds_dir):
    log("Creating dir '%s'" % builds_dir)
    os.makedirs(builds_dir)
  else:
    log("Dir '%s' already exists" % builds_dir)
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

  stripreloc = os.path.join(SCRIPT_DIR, "bin", "StripReloc")
  builds_dir_rel = os.path.join("builds", ver)
  run_cmd_throw(stripreloc, os.path.join(builds_dir_rel, "Installer.exe"))

  # mpress and zip expect to be run from inside builds_dir
  cwd = os.getcwd()
  os.chdir(builds_dir)

  mpress = os.path.join(SCRIPT_DIR, "bin", "mpress")
  if not os.path.isfile(mpress): mpress = "mpress"
  run_cmd_throw(mpress, "-s", "-r", "SumatraPDF-%s.exe" % ver)
  #run_cmd_throw("upx", compression_type, "--compress-icons=0", "SumatraPDF-%s.exe" % ver)

  zip_file("SumatraPDF-%s.zip" % ver, "SumatraPDF-%s.exe" % ver, "SumatraPDF.exe")

  os.chdir(cwd)

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
