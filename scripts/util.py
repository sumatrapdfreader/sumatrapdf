import os.path
import re
import shutil
import struct
import subprocess
import sys
import zipfile
import zlib

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

def test_for_flag(args, arg):
  if arg not in args:
    return False
  args.remove(arg)
  return True

S3_BUCKET = "kjkpub"
g_s3conn = None

def s3connection():
  global g_s3conn
  if g_s3conn is None:
    import_boto()
    g_s3conn = S3Connection(awscreds.access, awscreds.secret, True)
  return g_s3conn

def s3PubBucket(): return s3connection().get_bucket(S3_BUCKET)

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
  revision_num = None
  for l in txt.split("\n"):
    l = l.strip()
    if 0 == len(l): continue
    (name, val) = l.split(": ")
    if name == "Last Changed Rev":
      return int(val)
    if name == "Revision":
      revision_num = int(val)
  if revision_num is not None:
    return revision_num
  raise Exception("parse_svn_info_out() failed to parse '%s'" % txt)

# version line is in the format:
# #define CURR_VERSION 1.1
def extract_sumatra_version(file_path):
  fo = open(file_path, "r")
  d = fo.read()
  fo.close()
  m = re.search('CURR_VERSION (\\d+(?:\\.\\d+)*)', d)
  ver = m.group(1)
  return ver

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

INSTALLER_HEADER_FILE      = "kifi"
INSTALLER_HEADER_FILE_ZLIB = "kifz"
INSTALLER_HEADER_END       = "kien"

def installer_append_file(fo, path, name_in_installer):
  fi = open(path, "rb")
  data = fi.read()
  fi.close()
  assert len(data) == os.path.getsize(path)
  write_with_size(fo, data, name_in_installer)
  write_with_size(fo, name_in_installer)
  write_no_size(fo, INSTALLER_HEADER_FILE)

def installer_append_file_zlib(fo, path, name_in_installer):
  fi = open(path, "rb")
  data = fi.read()
  fi.close()
  assert len(data) == os.path.getsize(path)
  data2 = zlib.compress(data, 9)
  assert len(data2) < os.path.getsize(path)
  write_with_size(fo, data2, name_in_installer)
  write_with_size(fo, name_in_installer)
  write_no_size(fo, INSTALLER_HEADER_FILE_ZLIB)
  
  """
  data3 = bz2.compress(data, 9)  
  print("")
  print("uncompressed: %d" % len(data))
  print("zlib        : %d" % len(data2))
  print("bz2         : %d" % len(data3))
  print("")
  """

def installer_mark_end(fo):
  write_no_size(fo, INSTALLER_HEADER_END)

# doesn't really belong here, but have no better place
def zip_file(dst_zip_file, src, src_name=None):
  zf = zipfile.ZipFile(dst_zip_file, "w", zipfile.ZIP_DEFLATED)
  if not src_name:
    src_name = os.path.basename(src)
  zf.write(src, src_name)
  zf.close()

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
def build_installer_native(dir, nameprefix):
  installer_template_exe = os.path.join(dir, "Installer.exe")
  if nameprefix is not None:
    installer_exe = os.path.join(dir, "%s-install.exe" % nameprefix)
    exe = os.path.join(dir, "%s.exe" % nameprefix)
  else:
    installer_exe = os.path.join(dir, "SumatraPDF-install.exe")
    exe = os.path.join(dir, "SumatraPDF.exe")
  plugin = os.path.join(dir, "npPdfViewer.dll")
  shutil.copy(installer_template_exe, installer_exe)

  fo = open(installer_exe, "ab")
  # append installer data to installer exe
  installer_mark_end(fo) # this are read backwards so end marker is written first
  installer_append_file(fo, exe, "SumatraPDF.exe")
  installer_append_file(fo, plugin, "npPdfViewer.dll")
  font_name =  "DroidSansFallback.ttf"
  font_path = os.path.join("mupdf", "fonts", "droid", font_name)
  installer_append_file_zlib(fo, font_path, font_name)
  fo.close()
  return installer_exe