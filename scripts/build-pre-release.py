# What: build a pre-release version of sumatra and
#       upload it to s3
# How:
#   * get svn version
#   * build with nmake, sending svn version as argument
#   * compress with mpress
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/SumatraPDF-prerelase-<svnrev>.exe
#       sumatrapdf/SumatraPDF-prerelease-<svnrev>.pdb.zip
#       sumatrapdf/sumatralatest.js
#       sumatrapdf/sumpdf-prerelease-latest.txt

import sys, os, os.path, subprocess, time, shutil, zipfile, zlib
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

S3_BUCKET = "kjkpub"
g_s3conn = None

def s3connection():
  global g_s3conn
  if g_s3conn is None:
    g_s3conn = boto.s3.connection.S3Connection(awscreds.access, awscreds.secret, True)
  return g_s3conn

def s3PubBucket(): return s3connection().get_bucket(S3_BUCKET)

def ul_cb(sofar, total):
  print("So far: %d, total: %d" % (sofar , total))

def s3UploadFilePublic(local_file_name, remote_file_name):
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_filename(local_file_name, cb=ul_cb)
  k.make_public()

def s3UploadDataPublic(data, remote_file_name):
  bucket = s3PubBucket()
  k = Key(bucket)
  k.key = remote_file_name
  k.set_contents_from_string(data)
  k.make_public()

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

def usage():
  print("sumatra-build-pre-release.py [sumatra-source-dir]")
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

def get_src_dir():
  srcdir = os.path.realpath(".")
  if not os.path.exists(os.path.join(srcdir, "src")):
    print("%s is not a source dir" % srcdir)
    sys.exit(1)
  return srcdir

def get_src_dir2():
  srcdir = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))
  if not direxists(srcdir):
    print("%s dir doesn't exists" % srcdir)
    sys.exit(1)
  print("srcdir: %s" % srcdir)
  return srcdir

def test_upload():
  srcdir = get_src_dir()
  exe = os.path.join(srcdir, "obj-rel", "SumatraPDF.exe")
  remote = "sumatrapdf2/SumatraPDF-prerelease-333.txt"
  print("Uploading '%s' as '%s" % (exe, remote))
  s3UploadFilePublic(exe, remote)

def zip_file(dst_zip_file, src, src_name=None):
    zf = zipfile.ZipFile(dst_zip_file, "w", zipfile.ZIP_DEFLATED)
    if not src_name:
        src_name = os.path.basename(src)
    zf.write(src, src_name)
    zf.close()

def main():
  if len(sys.argv) > 2:
    usage()
  srcdir = get_src_dir()
  os.chdir(srcdir)
  objdir = "obj-rel"
  if os.path.exists(objdir): shutil.rmtree(objdir, ignore_errors=True)
  run_cmd_throw("svn", "update")
  (out,err) = run_cmd_throw("svn", "info")
  rev = parse_svninfo_out(out)
  #run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "cleanall")
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "NASM=bin\\nasm.exe", "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%d" % rev)

  mpress = os.path.join(srcdir, "bin", "mpress")
  exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  run_cmd_throw(mpress, "-s", "-r", exe)
  remote = "sumatrapdf/SumatraPDF-prerelease-%d.exe" % rev

  pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  pdb_zip = pdb + ".zip"
  zip_file(pdb_zip, pdb, "SumatraPDF-prerelease-%d.pdb" % rev)
  remote_pdb = "sumatrapdf/SumatraPDF-prerelease-%d.pdb.zip" % rev

  print("Uploading '%s' as '%s" % (exe, remote))
  s3UploadFilePublic(exe, remote)

  print("Uploading '%s' as '%s" % (pdb_zip, remote_pdb))
  s3UploadFilePublic(pdb_zip, remote_pdb)

  objdir = "obj-dbg"
  if os.path.exists(objdir): shutil.rmtree(objdir, ignore_errors=True)
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg", "NASM=bin\\nasm.exe", "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%d" % rev)

  exe = os.path.join(srcdir, objdir, "SumatraPDF.exe")
  run_cmd_throw(mpress, "-s", "-r", exe)
  remote_dbg = "sumatrapdf/SumatraPDF-prerelease-%d-dbg.exe" % rev

  pdb = os.path.join(srcdir, objdir, "SumatraPDF.pdb")
  pdb_zip = pdb + ".zip"
  zip_file(pdb_zip, pdb, "SumatraPDF-prerelease-%d-dbg.pdb" % rev)
  remote_dbg_pdb = "sumatrapdf/SumatraPDF-prerelease-%d-dbg.pdb.zip" % rev

  print("Uploading '%s' as '%s" % (exe, remote_dbg))
  s3UploadFilePublic(exe, remote_dbg)

  print("Uploading '%s' as '%s" % (pdb_zip, remote_dbg_pdb))
  s3UploadFilePublic(pdb_zip, remote_dbg_pdb)

  jstxt  = 'var sumLatestRev = %d;\n' % rev
  jstxt  = 'var sumBuiltOn = "%s";\n' % time.strftime("%Y-%m-%d")
  jstxt += 'var sumLatestName = "%s";\n' % remote.split("/")[1]
  jstxt += 'var sumLatestUrl = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote
  jstxt += 'var sumLatestUrlPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_pdb
  jstxt += 'var sumLatestDbgUrl = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_dbg
  jstxt += 'var sumLatestDbgUrlPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_dbg_pdb
  s3UploadDataPublic(jstxt, "sumatrapdf/sumatralatest.js")
  txt = "%d\n" % rev
  s3UploadDataPublic(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")

if __name__ == "__main__":
  main()
  #test_upload()
