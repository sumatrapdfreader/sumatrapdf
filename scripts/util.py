import os.path, re, subprocess, sys, tempfile, hashlib, string, time
import zipfile2 as zipfile

g_config = None
def import_boto():
  global S3Connection, bucket_lister, g_config
  try:
    from boto.s3.connection import S3Connection
    from boto.s3.bucketlistresultset import bucket_lister
  except:
    print("You need boto library (http://code.google.com/p/boto/)")
    print("svn checkout http://boto.googlecode.com/svn/trunk/ boto")
    print("cd boto; python setup.py install")
    raise

  g_config =  load_config()
  if  not g_config.HasAwsCreds():
    print "config.py doesn't have aws creds"
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

def strip_empty_lines(s):
  s = s.replace("\r\n", "\n")
  lines = [l.strip() for l in s.split("\n") if len(l.strip()) > 0]
  return string.join(lines, "\n")

def test_for_flag(args, arg, has_data=False):
  if arg not in args:
    if not has_data:
      return False
    for argx in args:
      if argx.startswith(arg + "="):
        args.remove(argx)
        return argx[len(arg) + 1:]
    return None

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
    g_s3conn = S3Connection(g_config.aws_access, g_config.aws_secret, True)
  return g_s3conn

def s3PubBucket():
  return s3connection().get_bucket(S3_BUCKET)

def ul_cb(sofar, total):
  print("So far: %d, total: %d" % (sofar , total))

def s3UploadFilePublic(local_path, remote_path, silent=False):
  size = os.path.getsize(local_path)
  log("s3 upload %d bytes of '%s' as '%s'" % (size, local_path, remote_path))
  k = s3PubBucket().new_key(remote_path)
  if silent:
    k.set_contents_from_filename(local_path)
  else:
    k.set_contents_from_filename(local_path, cb=ul_cb)
  k.make_public()

def s3UploadDataPublic(data, remote_path):
  log("s3 upload %d bytes of data as '%s'" % (len(data), remote_path))
  k = s3PubBucket().new_key(remote_path)
  k.set_contents_from_string(data)
  k.make_public()

def s3UploadDataPublicWithContentType(data, remote_path, silent=False):
  # writing to a file to force boto to set Content-Type based on file extension.
  # TODO: there must be a simpler way
  tmp_name = os.path.basename(remote_path)
  tmp_path = os.path.join(tempfile.gettempdir(), tmp_name)
  open(tmp_path, "w").write(data.encode("utf-8"))
  s3UploadFilePublic(tmp_path, remote_path, silent)
  os.remove(tmp_path)

def s3DownloadToFile(remote_path, local_path):
  log("s3 download '%s' as '%s'" % (remote_path, local_path))
  k = s3PubBucket().new_key(remote_path)
  k.get_contents_to_filename(local_path)

def s3List(s3dir):
  b = s3PubBucket()
  return bucket_lister(b, s3dir)

def s3Delete(remote_path):
  log("s3 delete '%s'" % remote_path)
  s3PubBucket().new_key(remote_path).delete()

def s3Exists(remote_path):
  return s3PubBucket().get_key(remote_path)

def verify_s3_doesnt_exist(remote_path):
  if not s3Exists(remote_path):
    return
  print("'%s' already exists in s3" % remote_path)
  sys.exit(1)

def file_sha1(fp):
  data = open(fp, "rb").read()
  m = hashlib.sha1()
  m.update(data)
  return m.hexdigest()

def verify_path_exists(path):
  if not os.path.exists(path):
    print("path '%s' doesn't exist" % path)
    sys.exit(1)

def verify_started_in_right_directory():
  if os.path.exists(os.path.join("scripts", "build-release.py")): return
  if os.path.exists(os.path.join(os.getcwd(), "scripts", "build-release.py")): return
  print("This script must be run from top of the source tree")
  sys.exit(1)

def run_cmd(*args):
  cmd = " ".join(args)
  print("run_cmd_throw: '%s'" % cmd)
  # this magic disables the modal dialog that windows shows if the process crashes
  # TODO: it doesn't seem to work, maybe because it was actually a crash in a process
  # sub-launched from the process I'm launching. I had to manually disable this in
  # registry, as per http://stackoverflow.com/questions/396369/how-do-i-disable-the-debug-close-application-dialog-on-windows-vista:
  # DWORD HKLM or HKCU\Software\Microsoft\Windows\Windows Error Reporting\DontShowUI = "1"
  # DWORD HKLM or HKCU\Software\Microsoft\Windows\Windows Error Reporting\Disabled = "1"
  # see: http://msdn.microsoft.com/en-us/library/bb513638.aspx
  if sys.platform.startswith("win"):
    import ctypes
    SEM_NOGPFAULTERRORBOX = 0x0002 # From MSDN
    ctypes.windll.kernel32.SetErrorMode(SEM_NOGPFAULTERRORBOX);
    subprocess_flags = 0x8000000 #win32con.CREATE_NO_WINDOW?
  else:
    subprocess_flags = 0
  cmdproc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess_flags)
  res = cmdproc.communicate()
  return (res[0], res[1], cmdproc.returncode)

# like run_cmd() but throws an exception on failure
def run_cmd_throw(*args):
  cmd = " ".join(args)
  print("run_cmd_throw: '%s'" % cmd)

  # see comment in run_cmd()
  if sys.platform.startswith("win"):
    import ctypes
    SEM_NOGPFAULTERRORBOX = 0x0002 # From MSDN
    ctypes.windll.kernel32.SetErrorMode(SEM_NOGPFAULTERRORBOX);
    subprocess_flags = 0x8000000 #win32con.CREATE_NO_WINDOW?
  else:
    subprocess_flags = 0
  cmdproc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess_flags)
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

# Parse output of "svn log -r${rev} -v", which looks sth. like this:
#------------------------------------------------------------------------
#r6667 | kkowalczyk | 2012-09-25 22:52:34 -0700 (Tue, 25 Sep 2012) | 1 line
#Changed paths:
#   M /trunk/installer-vc2008.vcproj
#   D /trunk/src/utils/Http.h
#   A /trunk/src/utils/HttpUtil.cpp (from /trunk/src/utils/Http.cpp:6665)
#
#rename Http.[h|cpp] => HttpUtil.[h|cpp]
#------------------------------------------------------------------------
# Returns a tuple:
# (user, comment, modified, added, deleted)
# or None in case this is not a source checkin (but e.g. a wiki page edit)
def parse_svnlog_out(txt):
  lines = [l.strip() for l in txt.split("\n")]
  # remove empty line at the end
  if len(lines) > 1 and len(lines[-1]) == 0:
    lines = lines[:-1]
  if 1 == len(lines): return None
  if not lines[0].startswith("---"):
    print(txt)
    print("l: '%s'" % lines[0])
    assert lines[0].startswith("----")
  if not lines[-1].startswith("---"):
    print(txt)
    print("l: '%s'" % lines[-1])
    assert lines[-1].startswith("----")
  user = lines[1].split(" | ")[1]
  assert "Changed paths:" == lines[2]
  modified = []
  added = []
  deleted = []
  lines = lines[3:]
  n = 0
  while True:
    if 0 == len(lines[n]): break
    s = lines[n]
    #print("s: %s" % s)
    typ = s[0]
    name = s[2:]
    assert name[0] == '/'
    if typ == 'M':
      modified.append(name)
    elif typ == 'D':
      deleted.append(name)
    elif typ == 'A':
      added.append(name)
    else:
      print("typ: %s\n" % typ)
      assert False
    n += 1
  lines = lines[n+1:-1] # skip the last ----
  comment = string.join(lines, "\n")
  return (user, comment, modified, added, deleted)

def parse_svnlog_out_test():
  s = """------------------------------------------------------------------------
r6667 | kkowalczyk | 2012-09-25 22:52:34 -0700 (Tue, 25 Sep 2012) | 1 line
Changed paths:
   M /trunk/src/SumatraPDF.cpp
   D /trunk/src/utils/Http.h
   A /trunk/src/utils/HttpUtil.h (from /trunk/src/utils/Http.h:6665)
   M /trunk/sumatrapdf-vc2012.vcxproj
   M /trunk/sumatrapdf-vc2012.vcxproj.filters

rename Http.[h|cpp] => HttpUtil.[h|cpp]
------------------------------------------------------------------------"""
  res = parse_svnlog_out(s)
  (user, comment, modified, added, deleted) = res
  print("User: %s\nComment: %s\nModified: %s\nAdded: %s\nDeleted: %s\n" % (user, comment, str(modified), str(added), str(deleted)))
  assert user == "kkowalczyk"
  assert comment == "rename Http.[h|cpp] => HttpUtil.[h|cpp]"

def parse_svnlog_out_test2(startrev=1, endrev=6700):
  rev = endrev
  while rev >= startrev:
    (out, err) = run_cmd_throw("svn", "log", "-r%s" % str(rev), "-v")
    res = parse_svnlog_out(out)
    print("\nRev: %s" % str(rev))
    if None == res:
      print("Not svn checkin")
    else:
      (user, comment, modified, added, deleted) = res
      print("User: %s\nComment: %s\nModified: %s\nAdded: %s\nDeleted: %s\n" % (user, comment, str(modified), str(added), str(deleted)))
    rev -= 1

# version line is in the format:
# #define CURR_VERSION 1.1
def extract_sumatra_version(file_path):
  content = open(file_path).read()
  ver = re.findall(r'CURR_VERSION (\d+(?:\.\d+)*)', content)[0]
  return ver

def file_remove_try_hard(path):
  removeRetryCount = 0
  while removeRetryCount < 3:
    try:
      os.remove(path)
      return
    except:
      time.sleep(1) # try to sleep to make the time for the file not be used anymore
      print "exception: n  %s, n  %s, n  %s n  when trying to remove file %s" % (sys.exc_info()[0], sys.exc_info()[1], sys.exc_info()[2], path)
    removeRetryCount += 1

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

def formatInt(x):
    if x < 0:
        return '-' + formatInt(-x)
    result = ''
    while x >= 1000:
        x, r = divmod(x, 1000)
        result = ".%03d%s" % (r, result)
    return "%d%s" % (x, result)

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

# Some operations, like uploading to s3, require knowing s3 credential
# We store all such information that cannot be publicly known in a file
# config.py. This object is just a wrapper to documents the fields
# and given default values if config.py doesn't exist

class Config(object):
  def __init__(self):
    self.aws_access = None
    self.aws_secret = None
    self.cert_pwd = None
    self.trans_ul_secret = None

  def GetCertPwdMustExist(self):
    assert(None != self.cert_pwd)
    return self.cert_pwd

  def GetTransUploadSecret(self):
    assert(None != self.trans_ul_secret)
    return self.trans_ul_secret

  # TODO: could verify aws creds better i.e. check the lengths
  def GetAwsCredsMustExist(self):
    assert(None != self.aws_access)
    assert(None != self.aws_secret)
    return (self.aws_access, self.aws_secret)

  def HasAwsCreds(self):
    if None is self.aws_access: return False
    if None is self.aws_secret: return False
    return True

def load_config():
  c = Config()
  try:
    import config
    c.aws_access = config.aws_access
    c.aws_secret = config.aws_secret
    c.cert_pwd = config.cert_pwd
    c.trans_ul_secret = config.trans_ul_secret
  except:
    # it's ok if doesn't exist, we just won't have the config data
    print("no config.py!")
  return c

def test_load_config():
  c = load_config()
  vals = (c.aws_access, c.aws_secret, c.cert_pwd, c.trans_ul_secret)
  print("aws_secret: %s\naws_secret: %s\ncert_pwd: %s\ntrans_ul_secret: %s" % vals)

if __name__ == "__main__":
  #parse_svnlog_out_test2()
  #test_load_config()
  pass
