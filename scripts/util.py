import os, re, subprocess, sys, hashlib, string, time, types, zipfile

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

def trim_str(s):
  if len(s) < 75: return (s, False)
  # we don't want to trim if adding "..." would make it bigger than original
  if len(s) < 78: return (s, False)
  return (s[:75], True)

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

  idx = args.index(arg)
  if idx == len(args) - 1:
    return None
  data = args[idx + 1]
  args.pop(idx + 1)
  args.pop(idx)
  return data

def file_sha1(fp):
  data = open(fp, "rb").read()
  m = hashlib.sha1()
  m.update(data)
  return m.hexdigest()

def delete_file(path):
  if os.path.exists(path):
    os.remove(path)

def create_dir(d):
  if not os.path.exists(d): os.makedirs(d)
  return d

def verify_path_exists(path):
  if not os.path.exists(path):
    print("path '%s' doesn't exist" % path)
    sys.exit(1)
  return path

def verify_started_in_right_directory():
  if os.path.exists(os.path.join("scripts", "build.py")): return
  if os.path.exists(os.path.join(os.getcwd(), "scripts", "build.py")): return
  print("This script must be run from top of the source tree")
  sys.exit(1)

def subprocess_flags():
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
    return 0x8000000 #win32con.CREATE_NO_WINDOW?
  return 0

# Apparently shell argument to Popen it must be False on unix/mac and True on windows
def shell_arg():
  if os.name == "nt":
    return True
  return False

def run_cmd(*args):
  cmd = " ".join(args)
  print("run_cmd_throw: '%s'" % cmd)
  cmdproc = subprocess.Popen(args, shell=shell_arg(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess_flags())
  res = cmdproc.communicate()
  return (res[0], res[1], cmdproc.returncode)

# like run_cmd() but throws an exception on failure
def run_cmd_throw(*args):
  cmd = " ".join(args)
  print("run_cmd_throw: '%s'" % cmd)

  cmdproc = subprocess.Popen(args, shell=shell_arg(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess_flags())
  res = cmdproc.communicate()
  errcode = cmdproc.returncode
  if 0 != errcode:
    print("Failed with error code %d" % errcode)
    if len(res[0]) > 0: print("Stdout:\n%s" % res[0])
    if len(res[1]) > 0: print("Stderr:\n%s" % res[1])
    raise Exception("'%s' failed with error code %d" % (cmd, errcode))
  return (res[0], res[1])

# work-around a problem with running devenv from command-line:
# http://social.msdn.microsoft.com/Forums/en-US/msbuild/thread/9d8b9d4a-c453-4f17-8dc6-838681af90f4
def kill_msbuild():
  (stdout, stderr, err) = run_cmd("taskkill", "/F", "/IM", "msbuild.exe")
  if err not in (0, 128): # 0 is no error, 128 is 'process not found'
    print("err: %d\n%s%s" % (err, stdout, stderr))
    print("exiting")
    sys.exit(1)

# Parse output of svn info and return revision number indicated by
# "Last Changed Rev" field or, if that doesn't exist, by "Revision" field
def parse_svninfo_out(txt):
  ver = re.findall(r'(?m)^Last Changed Rev: (\d+)', txt)
  if ver: return ver[0]
  ver = re.findall(r'(?m)^Revision: (\d+)', txt)
  if ver: return ver[0]
  raise Exception("parse_svn_info_out() failed to parse '%s'" % txt)

# returns local and latest (on the server) svn versions
def get_svn_versions():
  (out, err) = run_cmd_throw("svn", "info")
  ver_local = str(parse_svninfo_out(out))
  (out, err) = run_cmd_throw("svn", "info", "https://sumatrapdf.googlecode.com/svn/trunk")
  ver_latest = str(parse_svninfo_out(out))
  return ver_local, ver_latest

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

def str2bool(s):
  if s.lower() in ("true", "1"): return True
  if s.lower() in ("false", "0"): return False
  assert(False)

class Serializable(object):
  def __init__(self, fields, fields_no_serialize, read_from_file=None):
    self.fields = fields
    self.fields_no_serialize = fields_no_serialize
    self.vals = {}

    if read_from_file != None:
      self.from_s(open(read_from_file, "r").read())

  def type_of_field(self, name):
    return type(self.fields[name])

  def from_s(self, s):
    #print(s)
    lines = s.split("\n")
    for l in lines:
      (name, val) = l.split(": ", 1)
      tp = self.type_of_field(name)
      if tp == types.IntType:
        self.vals[name] = int(val)
      elif tp == types.LongType:
        self.vals[name] = long(val)
      elif tp == types.BooleanType:
        self.vals[name] = str2bool(val)
      elif tp in (types.StringType, types.UnicodeType):
        self.vals[name] = val
      else: print(name); assert(False)

  def to_s(self):
    res = []
    for k, v in self.vals.items():
      if k in self.fields_no_serialize:
        continue
      res.append("%s: %s" % (k, str(v)))
    return string.join(res, "\n")

  def write_to_file(self, filename):
    open(filename, "w").write(self.to_s())

  def compat_types(self, tp1, tp2):
    if tp1 == tp2: return True
    num_types = (types.IntType, types.LongType)
    if tp1 in num_types and tp2 in num_types: return True
    return False

  def __setattr__(self, k, v):
    if k in self.fields:
      if not self.compat_types(type(v), type(self.fields[k])):
        print("k='%s', %s != %s (type(v) != type(self.fields[k]))" % (k, type(v), type(self.fields[k])))
        assert type(v) == type(self.fields[k])
      self.vals[k] = v
    else:
      super(Serializable, self).__setattr__(k, v)

  def __getattr__(self, k):
    if k in self.vals:
      return self.vals[k]
    if k in self.fields:
      return self.fields[k]
    return super(Serializable, self).__getattribute__(k)

import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText

def sendmail(sender, senderpwd, to, subject, body):
    #print("sendmail is disabled"); return
    mail = MIMEMultipart()
    mail['From'] = sender
    toHdr = to
    if isinstance(toHdr, list):
      toHdr = ", ".join(toHdr)
    mail['To'] = toHdr
    mail['Subject'] = subject
    mail.attach(MIMEText(body))
    msg = mail.as_string()
    #print(msg)
    mailServer = smtplib.SMTP("smtp.gmail.com", 587)
    mailServer.ehlo()
    mailServer.starttls()
    mailServer.ehlo()
    mailServer.login(sender, senderpwd)
    mailServer.sendmail(sender, to, msg)
    mailServer.close()

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
    self.notifier_email = None
    self.notifier_email_pwd = None

  def GetNotifierEmailAndPwdMustExist(self):
    assert(None != self.notifier_email and None != self.notifier_email_pwd)
    return (self.notifier_email, self.notifier_email_pwd)

  def HasNotifierEmail(self):
    return self.notifier_email != None and self.notifier_email_pwd != None

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

def gob_uvarint_encode(i):
  assert i >= 0
  if i <= 0x7f:
    return chr(i)
  res = ""
  while i > 0:
    b = i & 0xff
    res += chr(b)
    i = i >> 8
  l = 256 - len(res)
  res = res[::-1] # reverse string
  return chr(l) + res

def gob_varint_encode(i):
  if i < 0:
    i = (~i << 1) | 1
  else:
    i = i << 1
  return gob_uvarint_encode(i)

# data generated with UtilTests.cpp (define GEN_PYTHON_TESTS to 1)
def test_gob():
  assert gob_varint_encode(0) == chr(0)
  assert gob_varint_encode(1) == chr(2)
  assert gob_varint_encode(127) == chr(255) + chr(254)
  assert gob_varint_encode(128) == chr(254) + chr(1) + chr(0)
  assert gob_varint_encode(129) == chr(254) + chr(1) + chr(2)
  assert gob_varint_encode(254) == chr(254) + chr(1) + chr(252)
  assert gob_varint_encode(255) == chr(254) + chr(1) + chr(254)
  assert gob_varint_encode(256) == chr(254) + chr(2) + chr(0)
  assert gob_varint_encode(4660) == chr(254) + chr(36) + chr(104)
  assert gob_varint_encode(74565) == chr(253) + chr(2) + chr(70) + chr(138)
  assert gob_varint_encode(1193046) == chr(253) + chr(36) + chr(104) + chr(172)
  assert gob_varint_encode(19088743) == chr(252) + chr(2) + chr(70) + chr(138) + chr(206)
  assert gob_varint_encode(305419896) == chr(252) + chr(36) + chr(104) + chr(172) + chr(240)
  assert gob_varint_encode(2147483647) == chr(252) + chr(255) + chr(255) + chr(255) + chr(254)
  assert gob_varint_encode(-1) == chr(1)
  assert gob_varint_encode(-2) == chr(3)
  assert gob_varint_encode(-255) == chr(254) + chr(1) + chr(253)
  assert gob_varint_encode(-256) == chr(254) + chr(1) + chr(255)
  assert gob_varint_encode(-257) == chr(254) + chr(2) + chr(1)
  assert gob_varint_encode(-4660) == chr(254) + chr(36) + chr(103)
  assert gob_varint_encode(-74565) == chr(253) + chr(2) + chr(70) + chr(137)
  assert gob_varint_encode(-1193046) == chr(253) + chr(36) + chr(104) + chr(171)
  assert gob_varint_encode(-1197415) == chr(253) + chr(36) + chr(138) + chr(205)
  assert gob_varint_encode(-19158648) == chr(252) + chr(2) + chr(72) + chr(172) + chr(239)
  assert gob_uvarint_encode(0) == chr(0)
  assert gob_uvarint_encode(1) == chr(1)
  assert gob_uvarint_encode(127) == chr(127)
  assert gob_uvarint_encode(128) == chr(255) + chr(128)
  assert gob_uvarint_encode(129) == chr(255) + chr(129)
  assert gob_uvarint_encode(254) == chr(255) + chr(254)
  assert gob_uvarint_encode(255) == chr(255) + chr(255)
  assert gob_uvarint_encode(256) == chr(254) + chr(1) + chr(0)
  assert gob_uvarint_encode(4660) == chr(254) + chr(18) + chr(52)
  assert gob_uvarint_encode(74565) == chr(253) + chr(1) + chr(35) + chr(69)
  assert gob_uvarint_encode(1193046) == chr(253) + chr(18) + chr(52) + chr(86)
  assert gob_uvarint_encode(19088743) == chr(252) + chr(1) + chr(35) + chr(69) + chr(103)
  assert gob_uvarint_encode(305419896) == chr(252) + chr(18) + chr(52) + chr(86) + chr(120)
  assert gob_uvarint_encode(2147483647) == chr(252) + chr(127) + chr(255) + chr(255) + chr(255)
  assert gob_uvarint_encode(2147483648) == chr(252) + chr(128) + chr(0) + chr(0) + chr(0)
  assert gob_uvarint_encode(2147483649) == chr(252) + chr(128) + chr(0) + chr(0) + chr(1)
  assert gob_uvarint_encode(4294967294) == chr(252) + chr(255) + chr(255) + chr(255) + chr(254)
  assert gob_uvarint_encode(4294967295) == chr(252) + chr(255) + chr(255) + chr(255) + chr(255)

if __name__ == "__main__":
  #parse_svnlog_out_test2()
  #test_load_config()
  test_gob()
  pass
