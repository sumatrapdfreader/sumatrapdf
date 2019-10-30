#!/usr/bin/env python

# Runs a subsetting test suite. Compares the results of subsetting via harfbuz
# to subsetting via fonttools.

from __future__ import print_function, division, absolute_import

import io
from difflib import unified_diff
import os
import re
import subprocess
import sys
import tempfile

from subset_test_suite import SubsetTestSuite

# https://stackoverflow.com/a/377028
def which(program):
	def is_exe(fpath):
		return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

	fpath, _ = os.path.split(program)
	if fpath:
		if is_exe(program):
			return program
	else:
		for path in os.environ["PATH"].split(os.pathsep):
			exe_file = os.path.join(path, program)
			if is_exe(exe_file):
				return exe_file

	return None

ttx = which ("ttx")
ots_sanitize = which ("ots-sanitize")

if not ttx:
	print("TTX is not present, skipping test.")
	sys.exit (77)

def cmd(command):
	p = subprocess.Popen (
		command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	(stdoutdata, stderrdata) = p.communicate ()
	print (stderrdata, end="") # file=sys.stderr
	return stdoutdata, p.returncode

def read_binary (file_path):
	with open (file_path, 'rb') as f:
		return f.read ()

def fail_test(test, cli_args, message):
	print ('ERROR: %s' % message)
	print ('Test State:')
	print ('  test.font_path    %s' % os.path.abspath (test.font_path))
	print ('  test.profile_path %s' % os.path.abspath (test.profile_path))
	print ('  test.unicodes	    %s' % test.unicodes ())
	expected_file = os.path.join(test_suite.get_output_directory (),
				     test.get_font_name ())
	print ('  expected_file	    %s' % os.path.abspath (expected_file))
	return 1

def run_test(test, should_check_ots):
	out_file = os.path.join(tempfile.mkdtemp (), test.get_font_name () + '-subset.ttf')
	cli_args = [hb_subset,
		    "--font-file=" + test.font_path,
		    "--output-file=" + out_file,
		    "--unicodes=%s" % test.unicodes ()]
	cli_args.extend (test.get_profile_flags ())
	print (' '.join (cli_args))
	_, return_code = cmd (cli_args)

	if return_code:
		return fail_test (test, cli_args, "%s returned %d" % (' '.join (cli_args), return_code))

	expected_ttx, return_code = run_ttx (os.path.join (test_suite.get_output_directory (),
					    test.get_font_name ()))
	if return_code:
		return fail_test (test, cli_args, "ttx (expected) returned %d" % (return_code))

	actual_ttx, return_code = run_ttx(out_file)
	if return_code:
		return fail_test (test, cli_args, "ttx (actual) returned %d" % (return_code))

	print ("stripping checksums.")
	expected_ttx = strip_check_sum (expected_ttx)
	actual_ttx = strip_check_sum (actual_ttx)

	if not actual_ttx == expected_ttx:
		for line in unified_diff (expected_ttx.splitlines (1), actual_ttx.splitlines (1)):
			sys.stdout.write (line)
		sys.stdout.flush ()
		return fail_test(test, cli_args, 'ttx for expected and actual does not match.')

	if should_check_ots:
		print ("Checking output with ots-sanitize.")
		if not check_ots (out_file):
			return fail_test (test, cli_args, 'ots for subsetted file fails.')

	return 0

def run_ttx (file):
	print ("ttx %s" % file)
	return cmd([ttx, "-q", "-o-", file])

def strip_check_sum (ttx_string):
	return re.sub ('checkSumAdjustment value=["]0x([0-9a-fA-F])+["]',
		       'checkSumAdjustment value="0x00000000"',
		       ttx_string.decode (), count=1)

def has_ots ():
	if not ots_sanitize:
		print("OTS is not present, skipping all ots checks.")
		return False
	return True

def check_ots (path):
	ots_report, returncode = cmd ([ots_sanitize, path])
	if returncode:
		print("OTS Failure: %s" % ots_report);
		return False
	return True

args = sys.argv[1:]
if not args or sys.argv[1].find('hb-subset') == -1 or not os.path.exists (sys.argv[1]):
	print ("First argument does not seem to point to usable hb-subset.")
	sys.exit (1)
hb_subset, args = args[0], args[1:]

if not len (args):
	print ("No tests supplied.")
	sys.exit (1)

has_ots = has_ots()

fails = 0
for path in args:
	with io.open (path, mode="r", encoding="utf-8") as f:
		print ("Running tests in " + path)
		test_suite = SubsetTestSuite (path, f.read())
		for test in test_suite.tests ():
			fails += run_test (test, has_ots)

if fails != 0:
	print (str (fails) + " test(s) failed.")
	sys.exit(1)
else:
	print ("All tests passed.")
