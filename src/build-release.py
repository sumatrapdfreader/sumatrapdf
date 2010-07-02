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
# #define CURR_VERSION "1.1"
def extract_sumatra_version(file_path):
  fo = open(file_path, "r")
  d = fo.read()
  fo.close()
  m = re.search('CURR_VERSION "([^"]+)"', d)
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

  log("compiling started")
  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")
  log("compiling finished")

  tmp_exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  tmp_pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  ensure_path_exists(tmp_exe)
  ensure_path_exists(tmp_pdb)

  builds_dir = os.path.join(SCRIPT_DIR, "builds", ver)

  if TESTING and os.path.exists(builds_dir):
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
  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_pdb, local_pdb)

  os.chdir(builds_dir)

  if TESTING:
    compression_type = "-1"
  else:
    compression_type = "--ultra-brute"
  log("upx started")
  run_cmd_throw("upx", compression_type, "--compress-icons=0", "SumatraPDF-%s.exe" % ver)
  log("upx finished")

  log("zip started")
  shutil.copy("SumatraPDF-%s.exe" % ver, "SumatraPDF.exe")
  run_cmd_throw("zip", "-0", "SumatraPDF-%s.zip" % ver, "SumatraPDF.exe")
  log("zip finished")

  local_zip = os.path.join(builds_dir, "SumatraPDF-%s.zip" % ver)
  ensure_path_exists(local_zip)

  os.chdir(SCRIPT_DIR)
  run_cmd_throw("makensis", "/DSUMVER=%s" % ver, "installer")
  local_installer_exe = os.path.join(builds_dir, "SumatraPDF-%s-install.exe" % ver)
  shutil.move("SumatraPDF-%s-install.exe" % ver, local_installer_exe)
  ensure_path_exists(local_installer_exe)

  if upload or upload_tmp:
    s3UploadFilePublic(local_exe_uncompr, remote_exe)
    s3UploadFilePublic(local_pdb, remote_pdb)
    s3UploadFilePublic(local_zip, remote_zip)
    s3UploadFilePublic(local_installer_exe, remote_installer_exe)

if __name__ == "__main__":
  main()
