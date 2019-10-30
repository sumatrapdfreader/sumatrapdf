#!/usr/bin/env python

# Pre-generates the expected output subset files (via fonttools) for
# specified subset test suite(s).

import io
import os
import sys

from subprocess import check_call
from subset_test_suite import SubsetTestSuite


def usage():
	print "Usage: generate-expected-outputs.py <test suite file> ..."


def generate_expected_output(input_file, unicodes, profile_flags, output_path):
	args = ["fonttools", "subset", input_file]
	args.extend(profile_flags)
	args.extend(["--notdef-outline",
		     "--name-IDs=*",
		     "--name-languages=*",
		     "--name-legacy",
		     "--drop-tables+=DSIG,GPOS,GSUB,GDEF",
		     "--unicodes=%s" % unicodes,
		     "--output-file=%s" % output_path])
	check_call(args)


args = sys.argv[1:]
if not args:
	usage()

for path in args:
	with io.open(path, mode="r", encoding="utf-8") as f:
		test_suite = SubsetTestSuite(path, f.read())
		output_directory = test_suite.get_output_directory()

		print "Generating output files for %s" % output_directory
		for test in test_suite.tests():
			unicodes = test.unicodes()
			font_name = test.get_font_name()
			print "Creating subset %s/%s" % (output_directory, font_name)
			generate_expected_output(test.font_path, unicodes, test.get_profile_flags(),
						 os.path.join(output_directory,
							      font_name))
