# What: build release version of sumatra and (optionally) upload it to s3

# How:
#   * extract version from Version.h
#   * build with nmake, sending svn version as argument
#   * compress with upx
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
import re
import shutil
import subprocess
import sys
import struct
import time

def test_for_flag(args, arg):
  try:
    pos = args.index(arg)
  except:
    return False
  del args[pos]
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

TESTING = False

SCRIPT_DIR = os.path.dirname(__file__)
if SCRIPT_DIR == "":
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

def direxists(path):
  if not os.path.exists(path):
    #print("%s path doesn't exist" % path)
    return False
  if os.path.isdir(path):
    #print("%s path exists and is a dir" % path)
    return True
  else:
    #print("%s path exists but is not a dir" % path)
    return False

def build_installer_nsis(builds_dir, ver):
  os.chdir(SCRIPT_DIR)
  run_cmd_throw("makensis", "/DSUMVER=%s" % ver, "installer")
  local_installer_exe = os.path.join(builds_dir, "SumatraPDF-%s-install.exe" % ver)
  shutil.move("SumatraPDF-%s-install.exe" % ver, local_installer_exe)
  ensure_path_exists(local_installer_exe)
  return local_installer_exe

def write_no_size(fo, data):
  log("Writing %d bytes at %d '%s'" % (len(data), fo.tell(), data))
  fo.write(data)
  
def write_with_size(fo, data, name=None):
  if name:
    log("Writing %d bytes at %d (data of name %s)" % (len(data), fo.tell(), name))
  else:
    log("Writing %d bytes at %d (data)" % (len(data), fo.tell()))
  fo.write(data)
  tmp = struct.pack("<I", len(data))
  log("Writing %d bytes at %d (data size)" % (len(tmp), fo.tell()))
  fo.write(tmp)

INSTALLER_HEADER_FILE = "kifi"
INSTALLER_HEADER_END  = "kien"

def append_installer_file(fo, path, name_in_installer):
  fi = open(path, "rb")
  data = fi.read()
  fi.close()
  assert len(data) == os.path.getsize(path)
  write_with_size(fo, data, name_in_installer)
  write_with_size(fo, name_in_installer)
  write_no_size(fo, INSTALLER_HEADER_FILE)

def mark_installer_end(fo):
  write_no_size(fo, INSTALLER_HEADER_END)

def copy_installer_manifest(src, dst):
  fo = open(src, "r")
  d = fo.read()
  fo.close()
  d = d.replace("asInvoker", "requireAdministrator")
  fo = open(dst, "w")
  fo.write(d)
  fo.close()

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
  installer_exe = os.path.join(builds_dir, "SumatraPDF-%s-install-native.exe" % ver)
  exe = os.path.join(builds_dir, "SumatraPDF-%s.exe" % ver)

  shutil.copy(installer_template_exe, installer_exe)

  fo = open(installer_exe, "ab")
  # append installer data to installer exe
  mark_installer_end(fo) # this are read backwards so end marker is written first
  append_installer_file(fo, exe, "SumatraPDF.exe")
  # TOD: write compressed
  font_name =  "DroidSansFallback.ttf"
  font_path = os.path.join(SCRIPT_DIR, "..", "mupdf", "fonts", "droid", font_name)
  append_installer_file(fo, font_path, font_name)
  fo.close()
  return installer_exe

def build_installer_for_testing():
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
  objdir = os.path.join(os.getcwd(), "obj-dbg")
  installer_template_exe = os.path.join(objdir, "Installer.exe")
  installer_exe = os.path.join(objdir, "SumatraPDF-installer.exe")
  shutil.copy(installer_template_exe, installer_exe)
  copy_installer_manifest(os.path.join(objdir, "Installer.exe.manifest"), os.path.join(objdir, "SumatraPDF-installer.exe.manifest"))

  exe = os.path.join(objdir, "SumatraPDF.exe")
  fo = open(installer_exe, "ab")
  # append installer data to installer exe
  mark_installer_end(fo) # this are read backwards so end marker is written first
  append_installer_file(fo, exe, "SumatraPDF.exe")
  # TODO: write compressed
  font_name =  "DroidSansFallback.ttf"
  font_path = os.path.join(os.getcwd(), "mupdf", "fonts", "droid", font_name)
  append_installer_file(fo, font_path, font_name)
  fo.close()
  return installer_exe

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

  os.chdir("..")
  srcdir = os.getcwd()

  if build_test_installer:
    build_installer_for_testing()
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
  if not TESTING and os.path.exists(objdir):
    shutil.rmtree(objdir, ignore_errors=True)

  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  tmp_exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  tmp_pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  tmp_installer = os.path.join(srcdir, objdir, "Installer.exe")

  ensure_path_exists(tmp_exe)
  ensure_path_exists(tmp_pdb)

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
  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_pdb, local_pdb)
  shutil.copy(tmp_installer, local_installer)

  os.chdir(builds_dir)

  if testing:
    compression_type = "-1"
  else:
    compression_type = "--ultra-brute"
  run_cmd_throw("upx", compression_type, "--compress-icons=0", "SumatraPDF-%s.exe" % ver)

  shutil.copy("SumatraPDF-%s.exe" % ver, "SumatraPDF.exe")
  run_cmd_throw("zip", "-0", "SumatraPDF-%s.zip" % ver, "SumatraPDF.exe")

  local_zip = os.path.join(builds_dir, "SumatraPDF-%s.zip" % ver)
  ensure_path_exists(local_zip)

  local_installer_exe = build_installer_nsis(builds_dir, ver)
  local_installer_native_exe = build_installer_native(builds_dir, ver)

  if upload or upload_tmp:
    s3UploadFilePublic(local_exe_uncompr, remote_exe)
    s3UploadFilePublic(local_pdb, remote_pdb)
    s3UploadFilePublic(local_zip, remote_zip)
    s3UploadFilePublic(local_installer_exe, remote_installer_exe)

if __name__ == "__main__":
  main()
