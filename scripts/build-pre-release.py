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

import os
import os.path
import shutil
import sys
import time

from util import run_cmd_throw, s3UploadFilePublic, s3UploadDataPublic, parse_svninfo_out, zip_file

SCRIPT_DIR = os.path.dirname(__file__)
if SCRIPT_DIR:
  SCRIPT_DIR = os.path.split(SCRIPT_DIR)[0]
else:
  SCRIPT_DIR = os.getcwd()

def usage():
  print("sumatra-build-pre-release.py [sumatra-source-dir]")
  sys.exit(1)

def get_src_dir():
  srcdir = os.path.realpath(".")
  if not os.path.exists(os.path.join(srcdirb, "src")):
    print("%s is not a source dir" % srcdir)
    sys.exit(1)
  return srcdir

def test_upload():
  srcdir = get_src_dir()
  exe = os.path.join(srcdir, "obj-rel", "SumatraPDF.exe")
  remote = "sumatrapdf2/SumatraPDF-prerelease-333.txt"
  print("Uploading '%s' as '%s" % (exe, remote))
  s3UploadFilePublic(exe, remote)

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
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%d" % rev)

  mpress = os.path.join(SCRIPT_DIR, "bin", "mpress")
  exe = os.path.join(objdir, "SumatraPDF.exe")
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
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg", "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%d" % rev)

  exe = os.path.join(objdir, "SumatraPDF.exe")
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
