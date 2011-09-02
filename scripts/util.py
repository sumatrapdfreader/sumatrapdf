import os.path
import re
import shutil
import struct
import subprocess
import sys
import zipfile2 as zipfile
import bz2

def import_boto():
  global Key, S3Connection, awscreds
  try:
    from boto.s3.key import Key
    from boto.s3.connection import S3Connection
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

def log(s):
  print(s)
  sys.stdout.flush()

def group(list, size):
  i = 0
  while list[i:]:
    yield list[i:i + size]
    i += size

def uniquify(array):
  return list(set(array))

def test_for_flag(args, arg, has_data=False):
  if arg not in args:
    return None if has_data else False
  if not has_data:
    args.remove(arg)
    return True

  ix = args.index(arg)
  if ix == len(args) - 1:
    return None
  data = args[ix + 1]
  args.pop(ix + 1)
  args.pop(ix)
  return data

S3_BUCKET = "kjkpub"
g_s3conn = None

def s3connection():
  global g_s3conn
  if g_s3conn is None:
    import_boto()
    g_s3conn = S3Connection(awscreds.access, awscreds.secret, True)
  return g_s3conn

def s3PubBucket():
  return s3connection().get_bucket(S3_BUCKET)

def ul_cb(sofar, total):
  print("So far: %d, total: %d" % (sofar , total))

def s3UploadFilePublic(local_file_name, remote_file_name):
  log("s3 upload '%s' as '%s'" % (local_file_name, remote_file_name))
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_filename(local_file_name, cb=ul_cb)
  k.make_public()

def s3UploadDataPublic(data, remote_file_name):
  log("s3 upload data as '%s'" % remote_file_name)
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_string(data)
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

def verify_started_in_right_directory():
  p1 = os.path.join("scripts", "build-release.py")
  p2 = os.path.join(os.getcwd(), "scripts", "build-release.py")
  if not (os.path.exists(p1) and os.path.exists(p2)):
    print("This script must be run from top of the source tree")
    sys.exit(1)

# like cmdrun() but throws an exception on failure
def run_cmd_throw(*args):
  cmd = " ".join(args)
  print("\nrun_cmd_throw: '%s'" % cmd)
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

# Parse output of svn info and return revision number indicated by
# "Last Changed Rev" field or, if that doesn't exist, by "Revision" field
def parse_svninfo_out(txt):
  ver = re.findall(r'(?m)^Last Changed Rev: (\d+)', txt)
  if ver: return ver[0]
  ver = re.findall(r'(?m)^Revision: (\d+)', txt)
  if ver: return ver[0]
  raise Exception("parse_svn_info_out() failed to parse '%s'" % txt)

# version line is in the format:
# #define CURR_VERSION 1.1
def extract_sumatra_version(file_path):
  content = open(file_path).read()
  ver = re.findall(r'CURR_VERSION (\d+(?:\.\d+)*)', content)[0]
  return ver

def zip_file(dst_zip_file, src, src_name=None, compress=True, append=False):
  mode = "w"
  if append: mode = "a"
  if compress:
    zf = zipfile.ZipFile(dst_zip_file, mode, zipfile.ZIP_DEFLATED)
  else:
    zf = zipfile.ZipFile(dst_zip_file, mode, zipfile.ZIP_STORED)
  if src_name is None:
    src_name = os.path.basename(src)
  zf.write(src, src_name)
  zf.close()

# build the .zip with with installer data, will be included as part of 
# Installer.exe resources
def build_installer_data(dir):
  zf = zipfile.ZipFile(os.path.join(dir, "InstallerData.zip"), "w", zipfile.ZIP_BZIP2)
  exe = os.path.join(dir, "SumatraPDF-no-MuPDF.exe")
  zf.write(exe, "SumatraPDF.exe")
  for f in ["libmupdf.dll", "npPdfViewer.dll", "PdfFilter.dll", "PdfPreview.dll", "uninstall.exe"]:
    zf.write(os.path.join(dir, f), f)
  font_path = os.path.join("mupdf", "fonts", "droid", "DroidSansFallback.ttf")
  zf.write(font_path, "DroidSansFallback.ttf")
  zf.close()
