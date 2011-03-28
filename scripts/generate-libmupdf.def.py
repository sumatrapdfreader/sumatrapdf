"""
Generates a list of all exports from libmupdf.dll from the function lists
contained in the mupdf/fitz/fitz.h and mupdf/mupdf/mupdf.h headers.
"""

import os, re
from util import verify_started_in_right_directory

def generateExports(header, exclude=[]):
	data = open(header, "r").read()
	functions = re.findall(r"^\w+ (?:\w+ )?\*?(\w+)\(.*?\);", data, re.MULTILINE | re.DOTALL)
	exports = "\n".join(["\t" + name for name in functions if name not in exclude])
	return exports

LIBMUPDF_DEF = """\
LIBRARY libmupdf
EXPORTS

; Fitz exports

%(fitz_exports)s

; MuPDF exports

%(mupdf_exports)s

; zlib exports

	gzerror
	gzprintf
	gzopen
	gzseek
	gztell
	gzread
	gzclose
	inflateInit2_
	inflate
	inflateEnd
	crc32
"""

def main():
	fitz_exports = generateExports("fitz/fitz.h", ["fz_acceleratearch", "fz_paintaffinecolor"])
	mupdf_exports = generateExports("mupdf/mupdf.h")
	
	list = LIBMUPDF_DEF % locals()
	open("libmupdf.def", "wb").write(list.replace("\n", "\r\n"))

if __name__ == "__main__":
	if os.path.exists("generate-libmupdf.def.py"):
		os.chdir("..")
	verify_started_in_right_directory()
	
	os.chdir("mupdf")
	main()
