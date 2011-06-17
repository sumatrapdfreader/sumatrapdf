"""
Generates a list of all exports from libmupdf.dll from the function lists
contained in the mupdf/*/*.h headers.
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

; MuXPS exports

%(muxps_exports)s

; jpeg exports

	jpeg_resync_to_restart
	jpeg_finish_decompress
	jpeg_read_scanlines
	jpeg_start_decompress
	jpeg_read_header
	jpeg_CreateDecompress
	jpeg_destroy_decompress
	jpeg_std_error

; zlib exports

	gzerror
	gzprintf
	gzopen
	gzwopen
	gzseek
	gztell
	gzread
	gzclose
	inflateInit2_
	inflate
	inflateEnd
	compress
	compressBound
	crc32
"""

def main():
	fitz_exports = generateExports("fitz/fitz.h", ["fz_accelerate_arch", "fz_paint_affine_color"])
	mupdf_exports = generateExports("pdf/mupdf.h")
	muxps_exports = generateExports("xps/muxps.h", ["xps_parse_solid_color_brush", "xps_debug_path"])
	
	list = LIBMUPDF_DEF % locals()
	open("libmupdf.def", "wb").write(list.replace("\n", "\r\n"))

if __name__ == "__main__":
	if os.path.exists("generate-libmupdf.def.py"):
		os.chdir("..")
	verify_started_in_right_directory()
	
	os.chdir("mupdf")
	main()
