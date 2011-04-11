"""
Builds a (pre)release build of SumatraPDF, including the installer,
and optionally uploads it to s3.
"""

import os
import os.path
import shutil
import sys
import time

from util import log, run_cmd_throw, test_for_flag, s3UploadFilePublic, s3UploadDataPublic, ensure_s3_doesnt_exist, ensure_path_exists, zip_file, extract_sumatra_version, verify_started_in_right_directory, build_installer_native, parse_svninfo_out

args = sys.argv
upload               = test_for_flag(args, "-upload")
upload_tmp           = test_for_flag(args, "-uploadtmp")
testing              = test_for_flag(args, "-test") or test_for_flag(args, "-testing")
build_test_installer = test_for_flag(args, "-test-installer") or test_for_flag(args, "-testinst") or test_for_flag(args, "-testinstaller")
build_prerelease     = test_for_flag(args, "-prerelease")

def usage():
  print("build-release.py [-upload][-uploadtmp][-test][-test-installer][-prerelease]")
  sys.exit(1)

# Terms:
#  static build  - SumatraPDF.exe single executable with mupdf code staticall
#                  linked in
#  library build - SumatraPDF.exe executable that uses libmupdf.dll

# Building release version:
#   * extract version from Version.h
#   * build with nmake, sending version as argument
#   * compress with mpress
#   * build an installer
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/rel/SumatraPDF-<ver>.exe
#          uncompressed portable executable, for archival
#       sumatrapdf/rel/SumatraPDF-<ver>.zip
#          mpress-compressed portable executable inside zip
#       sumatrapdf/rel/SumatraPDF-<ver>.pdb.zip
#          pdb symbols for libmupdf.dll, and Sumatra's static and library builds
#       sumatrapdf/rel/SumatraPDF-<ver>-install.exe
#          installer for library build
#
#   * file sumatrapdf/sumpdf-latest.txt must be manually updated

# Building pre-release version:
#   * get svn version
#   * build with nmake, sending svn version as argument
#   * compress with mpress
#   * build an installer
#   * upload to s3 kjkpub bucket. Uploaded files:
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.exe
#          mpress compressed portable executable
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>.pdb.zip
#          pdb symbols for libmupdf.dll and Sumatra's static and library builds
#       sumatrapdf/prerel/SumatraPDF-prerelease-<svnrev>-install.exe
#          installer for library build
#       sumatrapdf/sumatralatest.js
#       sumatrapdf/sumpdf-prerelease-latest.txt

def copy_from_obj_rel(name_in_obj_rel, dst_path):
  src_path = os.path.join("obj-rel", name_in_obj_rel)
  shutil.copy(src_path, dst_path)

def main():
  if len(args) != 1:
    usage()
  verify_started_in_right_directory()

  if build_test_installer:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=dbg")
    build_installer_native("obj-dbg", None)
    sys.exit(0)

  if build_prerelease:
    run_cmd_throw("svn", "update")
    (out, err) = run_cmd_throw("svn", "info")
    ver = str(parse_svninfo_out(out))
  else:
    ver = extract_sumatra_version(os.path.join("src", "Version.h"))
  log("Version: '%s'" % ver)

  filename_base = "SumatraPDF-%s" % ver
  if build_prerelease:
    filename_base = "SumatraPDF-prerelease-%s" % ver

  s3_dir = "sumatrapdf/rel"
  if build_prerelease:
    s3_dir = "sumatrapdf/prerel"
  if upload_tmp:
    s3_dir += "tmp"

  if upload or upload_tmp:
    log("Will upload to s3 at %s" % s3_dir)

  s3_prefix = "%s/%s" % (s3_dir, filename_base)
  s3_exe           = s3_prefix + ".exe"
  s3_installer     = s3_prefix + "-install.exe"
  s3_pdb_zip       = s3_prefix + ".pdb.zip"
  s3_exe_zip       = s3_prefix + ".zip"

  s3_files = [s3_exe, s3_installer, s3_pdb_zip]
  if not build_prerelease:
    s3_files.append(s3_exe_zip)

  if upload:
    map(ensure_s3_doesnt_exist, s3_files)

  if not testing:
    shutil.rmtree("obj-rel", ignore_errors=True)

  builds_dir = os.path.join("builds", ver)
  if os.path.exists(builds_dir):
    shutil.rmtree(builds_dir)
  if not os.path.exists(builds_dir):
    os.makedirs(builds_dir)

  if build_prerelease:
    extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel", extcflags)
  else:
    run_cmd_throw("nmake", "-f", "makefile.msvc", "CFG=rel")

  exe_uncompressed = os.path.join(builds_dir, "%s-uncompr.exe" % filename_base)
  copy_from_obj_rel("SumatraPDF.exe", exe_uncompressed)

  exe_compressed = os.path.join(builds_dir, "%s.exe" % filename_base)
  copy_from_obj_rel("SumatraPDF.exe", exe_compressed)

  exe_no_mupdf = os.path.join(builds_dir, "SumatraPDF-no-MuPDF.exe")
  copy_from_obj_rel("SumatraPDF-no-MuPDF.exe", exe_no_mupdf)

  libmupdf = os.path.join(builds_dir, "libmupdf.dll")
  copy_from_obj_rel("libmupdf.dll", libmupdf)

  plugin = os.path.join(builds_dir, "npPdfViewer.dll")
  copy_from_obj_rel("npPdfViewer.dll", plugin)

  pdffilter = os.path.join(builds_dir, "PdfFilter.dll")
  copy_from_obj_rel("PdfFilter.dll", pdffilter)

  pdb_zip = os.path.join(builds_dir, "%s.pdb.zip" % filename_base)
  zip_file(pdb_zip, os.path.join("obj-rel", "libmupdf.pdb"))
  zip_file(pdb_zip, os.path.join("obj-rel", "SumatraPDF-no-MuPDF.pdb"), append=True)
  zip_file(pdb_zip, os.path.join("obj-rel", "SumatraPDF.pdb"), "%s.pdb" % filename_base, append=True)

  installer_stub = os.path.join(builds_dir, "Installer.exe")
  copy_from_obj_rel("Installer.exe", installer_stub)

  # run mpress and StripReloc from inside builds_dir for better
  # compat across python version
  prevdir = os.getcwd(); os.chdir(builds_dir)
  run_cmd_throw("StripReloc", "Installer.exe")
  run_cmd_throw("mpress", "-s", "-r", os.path.basename(exe_compressed))
  os.chdir(prevdir)

  installer = build_installer_native(builds_dir, filename_base)

  if not build_prerelease:
    exe_zip = os.path.join(builds_dir, "%s.zip" % filename_base)
    zip_file(exe_zip, exe_compressed, "SumatraPDF.exe", compress=False)
    ensure_path_exists(exe_zip)

  if upload or upload_tmp:
    if build_prerelease:
      jstxt  = 'var sumLatestVer = %s;\n' % ver
      jstxt  = 'var sumBuiltOn = "%s";\n' % time.strftime("%Y-%m-%d")
      jstxt += 'var sumLatestName = "%s";\n' % s3_exe.split("/")[-1]
      jstxt += 'var sumLatestExe = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_exe
      jstxt += 'var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_pdb_zip
      jstxt += 'var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/%s";\n' % s3_installer

    s3UploadFilePublic(installer, s3_installer)
    s3UploadFilePublic(pdb_zip, s3_pdb_zip)

    if build_prerelease:
      s3UploadFilePublic(exe_compressed, s3_exe)
      s3UploadDataPublic(jstxt, "sumatrapdf/sumatralatest.js")
      txt = "%s\n" % ver
      s3UploadDataPublic(txt, "sumatrapdf/sumpdf-prerelease-latest.txt")
    else:
      s3UploadFilePublic(exe_uncompressed, s3_exe)
      s3UploadFilePublic(exe_zip, s3_exe_zip)

    # Note: for release builds, must update sumatrapdf/sumpdf-latest.txt in s3
    # manually to: "%s\n" % ver

  # temporary files that were in builds_dir to make creating other files possible
  temp = [installer_stub, installer_stub + ".bak", exe_no_mupdf, libmupdf, plugin, pdffilter]
  map(os.remove, temp)
  if not build_prerelease:
    os.remove(exe_compressed) # is in exe_zip


if __name__ == "__main__":
  main()
