#!/usr/bin/env python

import io
import os

# A single test in a subset test suite. Identifies a font
# a subsetting profile, and a subset to be cut.
class Test:
	def __init__(self, font_path, profile_path, subset):
		self.font_path = font_path
		self.profile_path = profile_path
		self.subset = subset

	def unicodes(self):
		return ",".join("%X" % ord(c) for (i, c) in enumerate(self.subset))

	def get_profile_flags(self):
		with io.open(self.profile_path, mode="r", encoding="utf-8") as f:
		    return f.read().splitlines();

	def get_font_name(self):
		font_base_name = os.path.basename(self.font_path)
		font_base_name_parts = os.path.splitext(font_base_name)
		profile_name = os.path.splitext(os.path.basename(self.profile_path))[0]

		return "%s.%s.%s%s" % (font_base_name_parts[0],
				       profile_name,
				       self.unicodes(),
				       font_base_name_parts[1])

# A group of tests to perform on the subsetter. Each test
# Identifies a font a subsetting profile, and a subset to be cut.
class SubsetTestSuite:

	def __init__(self, test_path, definition):
		self.test_path = test_path
		self.fonts = set()
		self.profiles = set()
		self.subsets = set()
		self._parse(definition)

	def get_output_directory(self):
		test_name = os.path.splitext(os.path.basename(self.test_path))[0]
		data_dir = os.path.join(os.path.dirname(self.test_path), "..")

		output_dir = os.path.normpath(os.path.join(data_dir, "expected", test_name))
		if not os.path.exists(output_dir):
			os.mkdir(output_dir)
		if not os.path.isdir(output_dir):
			raise Error("%s is not a directory." % output_dir)

		return output_dir

	def tests(self):
		for font in self.fonts:
			font = os.path.join(self._base_path(), "fonts", font)
			for profile in self.profiles:
				profile = os.path.join(self._base_path(), "profiles", profile)
				for subset in self.subsets:
					yield Test(font, profile, subset)

	def _base_path(self):
		return os.path.dirname(os.path.dirname(self.test_path))

	def _parse(self, definition):
		destinations = {
				"FONTS:": self.fonts,
				"PROFILES:": self.profiles,
				"SUBSETS:": self.subsets
		}

		current_destination = None
		for line in definition.splitlines():
			line = line.strip()

			if line.startswith("#"):
				continue

			if not line:
				continue

			if line in destinations:
				current_destination = destinations[line]
			elif current_destination is not None:
				current_destination.add(line)
			else:
				raise Exception("Failed to parse test suite file.")
