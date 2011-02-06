# What: build a pre-release version of sumatra and
#       upload it to s3
# How:
#   * get svn version
#   * build with nmake, sending svn version as argument
#   * compress with mpress
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.exe
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.pdb.zip
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-dbg.exe
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-dbg.pdb.zip
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-install.exe
#       sumatrapdf/sumatralatest.js
#       sumatrapdf/sumpdf-prerelease-latest.txt

import os
import os.path
import shutil
import sys
import time

from util import log, run_cmd_throw, s3UploadFilePublic, s3UploadDataPublic, parse_svninfo_out, zip_file, verify_started_in_right_directory, ensure_s3_doesnt_exist, build_installer_native

def usage():
  print("sumatra-build-pre-release.py [sumatra-source-dir]")
  sys.exit(1)

def main():
  if len(sys.argv) > 2:
    usage()
  verify_started_in_right_directory()

  s3dir = "sumatrapdf/prerel"

  run_cmd_throw("svn", "update")
  (out,err) = run_cmd_throw("svn", "info")
  ver = str(parse_svninfo_out(out))
  log("Version: '%s'" % ver)

  tmp = "%s/SumatraPDF-prerelease-%s" % (s3dir, ver)
  remote_exe           = "%s.exe" % tmp
  remote_pdb_zip       = "%s.pdb.zip" % tmp
  remote_exe_dbg       = "%s-dbg.exe" % tmp
  remote_pdb_dbg_zip   = "%s-dbg.pdb.zip" % tmp
  remote_installer_exe = "%s-install.exe" % tmp

  map(ensure_s3_doesnt_exist, [remote_exe, remote_pdb_zip, remote_exe_dbg, remote_pdb_dbg_zip, remote_installer_exe])

  builds_dir = os.path.join("builds", ver)
  shutil.rmtree(builds_dir, ignore_errors=True)
  os.makedirs(builds_dir)

  shutil.rmtree("obj-rel", ignore_errors=True)
  shutil.rmtree("obj-dbg", ignore_errors=True)
  extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", extcflags)
  run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg", extcflags)

  files = ["SumatraPDF.exe", "SumatraPDF.pdb", "npPdfViewer.dll", "Installer.exe"]
  [tmp_exe, tmp_pdb, tmp_plugin, tmp_installer] = [os.path.join("obj-rel", t) for t in files]
  [tmp_exe_dbg, tmp_pdb_dbg] = [os.path.join("obj-dbg", t) for t in files[0:2]]

  local_exe = os.path.join(builds_dir, "SumatraPDF-prerelease-%s.exe" % ver)
  local_exe_uncompr = os.path.join(builds_dir, "SumatraPDF-uncompr.exe")
  local_exe_dbg = os.path.join(builds_dir, "SumatraPDF-prerelease-%s-dbg.exe" % ver)
  local_pdb = os.path.join(builds_dir, "SumatraPDF-prerelease-%s.pdb" % ver)
  local_pdb_zip = os.path.join(builds_dir, "SumatraPDF-prerelease-%s.pdb.zip" % ver)
  local_pdb_dbg = os.path.join(builds_dir, "SumatraPDF-prerelease-%s-dbg.pdb" % ver)
  local_pdb_dbg_zip = os.path.join(builds_dir, "SumatraPDF-prerelease-%s-dbg.pdb.zip" % ver)
  local_plugin = os.path.join(builds_dir, "npPdfViewer.dll")
  local_installer = os.path.join(builds_dir, "Installer.exe")

  shutil.copy(tmp_exe, local_exe)
  shutil.copy(tmp_exe, local_exe_uncompr)
  shutil.copy(tmp_exe_dbg, local_exe_dbg)
  shutil.copy(tmp_plugin, local_plugin)
  shutil.copy(tmp_pdb, local_pdb)
  shutil.copy(tmp_pdb_dbg, local_pdb_dbg)
  shutil.copy(tmp_installer, local_installer)

  # run mpress and StripReloc from inside builds_dir for better
  # compat across python version
  prevdir = os.getcwd(); os.chdir(builds_dir)
  run_cmd_throw("StripReloc", "Installer.exe")
  run_cmd_throw("mpress", "-s", "-r", "SumatraPDF-prerelease-%s.exe" % ver)
  run_cmd_throw("mpress", "-s", "-r", "SumatraPDF-prerelease-%s-dbg.exe" % ver)
  os.chdir(prevdir)

  nameprefix = "SumatraPDF-prerelease-%s" % ver
  local_installer_native_exe = build_installer_native(builds_dir, nameprefix)

  zip_file(local_pdb_zip, local_pdb, "SumatraPDF-prerelease-%s.pdb" % ver)
  zip_file(local_pdb_dbg_zip, local_pdb_dbg, "SumatraPDF-prerelease-%s.pdb" % ver)

  s3UploadFilePublic(local_exe, remote_exe)
  s3UploadFilePublic(local_exe_dbg, remote_exe_dbg)
  s3UploadFilePublic(local_pdb_zip, remote_pdb_zip)
  s3UploadFilePublic(local_pdb_dbg_zip, remote_pdb_dbg_zip)
  s3UploadFilePublic(local_installer_native_exe, remote_installer_exe)
  map(os.remove, [local_installer, local_installer + ".bak"])

  jstxt  = 'var sumLatestVer = %s;\n' % ver
  jstxt  = 'var sumBuiltOn = "%s";\n' % time.strftime("%Y-%m-%d")
  jstxt += 'var sumLatestName = "%s";\n' % remote_exe.split("/")[-1]
  jstxt += 'var sumLatestExe = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_exe
  jstxt += 'var sumLatestExeDbg = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_exe_dbg
  jstxt += 'var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_pdb_zip
  jstxt += 'var sumLatestPdbDbg = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_pdb_dbg_zip
  jstxt += 'var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/%s";\n' % remote_installer_exe
  s3UploadDataPublic(jstxt, "sumatrapdf/sumatralatest.js")
  txt = "%s\n" % ver
  s3UploadDataPublic(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")

if __name__ == "__main__":
  main()
