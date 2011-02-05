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

import bz2
import installer
import os
import os.path
import re
import shutil
import subprocess
import sys
import time

def test_for_flag(args, arg):
  if arg not in args:
    return False
  args.remove(arg)
  return True

args = sys.argv
upload = test_for_flag(args, "-upload")
upload_tmp = test_for_flag(args, "-uploadtmp")
testing = test_for_flag(args, "-test") or test_for_flag(args, "-testing")
build_test_installer = test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")

if upload or upload_tmp:
  try:
    import boto.s3
    from boto.s3.key import Key
  except:
    print("You need boto library (http://code.google.com/p/boto/)")
    print("svn checkout http://boto.googlecode.com/svn/trunk/ boto")
    print("cd boto; python setup.py install")
    raise

  try:
    import awscreds
  except:
    print "awscreds.py file needed with access and secret globals for aws access"
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(__file__)
if SCRIPT_DIR:
  SCRIPT_DIR = os.path.split(SCRIPT_DIR)[0]
else:
  SCRIPT_DIR = os.getcwd()

S3_BUCKET = "kjkpub"
g_s3conn = None

def log(s):
  print(s)
  sys.stdout.flush()

def s3connection():
  global g_s3conn
  if g_s3conn is None:
    g_s3conn = boto.s3.connection.S3Connection(awscreds.access, awscreds.secret, True)
  return g_s3conn

def s3PubBucket(): return s3connection().get_bucket(S3_BUCKET)

def ul_cb(sofar, total):
  log("So far: %d, total: %d" % (sofar , total))

def s3UploadFilePublic(local_file_name, remote_file_name):
  log("Uploading %s as %s" % (local_file_name, remote_file_name))
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_filename(local_file_name, cb=ul_cb)
  k.make_public()

def ensure_s3_doesnt_exist(remote_file_path):
  bucket = s3PubBucket()
  if not bucket.get_key(remote_file_path):
    return
  print("'%s' already exists on s3" % remote_file_path)
  sys.exit(1)

def ensure_path_exists(path):
  if not os.path.exists(path):
    print("path '%s' doesn't exist" % path)
    sys.exit(1)

# version line is in the format:
# #define CURR_VERSION 1.1
def extract_sumatra_version(file_path):
  fo = open(file_path, "r")
  d = fo.read()
  fo.close()
  m = re.search('CURR_VERSION (\\d+(?:\\.\\d+)*)', d)
  ver = m.group(1)
  return ver

# like cmdrun() but throws an exception on failure
def run_cmd_throw(*args):
  cmd = " ".join(args)
  log("\nrun_cmd_throw: '%s'" % cmd)
  cmdproc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  res = cmdproc.communicate()
  errcode = cmdproc.returncode
  if 0 != errcode:
    print("Failed with error code %d" % errcode)
    print("Stdout:")
    print(res[0])
    print("Stderr:")
    print(res[1])
    raise Exception("'%s' failed with error code %d" % (cmd, errcode))
  return (res[0], res[1])

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
  installer.mark_end(fo) # this are read backwards so end marker is written first
  installer.append_file(fo, exe, "SumatraPDF.exe")
  font_name =  "DroidSansFallback.ttf"
  font_path = os.path.join(SCRIPT_DIR, "mupdf", "fonts", "droid", font_name)
  installer.append_file_zlib(fo, font_path, font_name)
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
    ensure_s3_doesnt_exist(remote_exe)
    ensure_s3_doesnt_exist(remote_pdb)
    ensure_s3_doesnt_exist(remote_zip)
    ensure_s3_doesnt_exist(remote_installer_exe)

  objdir = "obj-rel"
  if not testing and os.path.exists(objdir):
    shutil.rmtree(objdir, ignore_errors=True)

  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  tmp_exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  tmp_pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  tmp_installer = os.path.join(srcdir, objdir, "Installer.exe")
  tmp_installer_pdb = os.path.join(srcdir, objdir, "Installer.pdb")

  ensure_path_exists(tmp_exe)
  ensure_path_exists(tmp_pdb)
  ensure_path_exists(tmp_installer)
  ensure_path_exists(tmp_installer_pdb)

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

  installer.zip_file("SumatraPDF-%s.zip" % ver, "SumatraPDF-%s.exe" % ver, "SumatraPDF.exe")

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
