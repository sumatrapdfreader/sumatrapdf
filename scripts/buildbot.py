"""
Builds Sumatra with /analyze flag and upload results to s3 for easy analysis.
"""
import os, os.path, shutil, sys, time, re

from util import log, run_cmd_throw, test_for_flag, s3UploadFilePublic
from util import s3UploadDataPublic, ensure_s3_doesnt_exist, ensure_path_exists
from util import parse_svninfo_out, s3List, s3Delete

# TODO: do this in a loop, wake up every 1hr, update svn to see if there were changes
# and if there were, do the build
# TODO: more build types (regular 32bit release and debug, 64bit?)
def main():
	(out, err) = run_cmd_throw("svn", "info")
	ver = str(parse_svninfo_out(out))
	# TODO: check if results of this already exist on s3, abort if they do
	print("Building revision %s" % ver)

	config = "CFG=rel"
	obj_dir = "obj-rel"
	shutil.rmtree(obj_dir, ignore_errors=True)
	extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
	platform = "PLATFORM=X86"

	# disable ucrt because vs2012 doesn't support it and I give it priority
	# over 2010 (if both installed) hoping it has better /analyze
	(out, err) = run_cmd_throw("nmake", "-f", "makefile.msvc", "WITH_SUM_ANALYZE=yes", "WITH_UCRT=no", config, extcflags, platform, "all_sumatrapdf")
	print(out)
	print(err)
	open("analyze-%s-out.txt" % ver, "w").write(out)
	open("analyze-%s-err.txt" % ver, "w").write(err)
	# TODO:
	# - upload the result as html to s3 kjkpub bucket /sumatrapdf/buildres/${ver}/data
	# - in the html extract the errors, remove dups, show them at top and link to
	#   source code in svn

if __name__ == "__main__":
	main()
