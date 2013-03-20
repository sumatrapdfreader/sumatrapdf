#!/usr/bin/env python

"""
Parses the output of sizer.exe.
Shows a summary or a comparison.
"""

g_file_name = "sum_sizer.txt"

class ParseState(object):
	def __init__(self):
		self.types = []
		self.symbols = []

def parse_start(fo, state):
	l = fo.readline().strip()
	assert l == "Format: 1", "unexpected line: %s" % l
	return parse_next_section

def parse_next_section(fo, state):
	return None

def parse_file_object(fo):
	curr = parse_start
	state = ParseState()
	while curr:
		curr = curr(fo, state)
	print("Finished parsing")

def parse_file(file_name):
	with open(file_name, "r") as fo:
		return parse_file_object(fo)

def main():
	parse_file(g_file_name)

if __name__ == "__main__":
	main()
