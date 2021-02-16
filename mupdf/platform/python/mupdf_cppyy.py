'''
MuPDF Python bindings using cppyy: https://cppyy.readthedocs.io

Cppyy generates bindings at runtime, so we don't need to build a .so like SWIG.

However we still need the mupdf.so (MuPDF C API) and mupdfcpp.so (MuPDF C++
API) libraries to be present and accessible via LD_LIBRARY_PATH.

Usage:
    import mupdf_cppyy
    mupdf = mupdf_cppyy.cppyy.gbl.mupdf

    document = mupdf.Document(...)

Requirements:
    Install cppyy; for example:
        python -m pip install cppyy
'''

import ctypes
import os
import re
import sys

import cppyy

mupdf_dir = os.path.abspath( f'{__file__}/../../..')
cppyy.add_include_path( f'{mupdf_dir}/include')
cppyy.add_include_path( f'{mupdf_dir}/platform/c++/include')
cppyy.include('mupdf/fitz/version.h')
cppyy.load_library('mupdf.so')
cppyy.load_library('mupdfcpp.so')
cppyy.include('mupdf/classes.h')
cppyy.include('mupdf/functions.h')
cppyy.include('mupdf/fitz.h')

#
# Would be convenient to do:
#
#   from cppyy.gbl.mupdf import *
#
# - but unfortunately this is not possible, e.g. see:
#
#   https://cppyy.readthedocs.io/en/latest/misc.html#reduced-typing
#
# So instead it is suggested that users of this api do:
#
#   import mupdf
#   mupdf = mupdf.cppyy.gbl.mupdf
#
# If access to mupdf.cppyy.gbl is required (e.g. to see globals that are not in
# the C++ mupdf namespace), caller can additionally do:
#
#   import cppyy
#   cppyy.gbl.std...
#

# We make various modifications of cppyy.gbl.mupdf to simplify usage.
#

# Set cppyy.gbl.mupdf.FZ_VERSION. Cppyy seems to ignore macros so we do this by
# manually parsing fitz/version.h.
#
path = f'{mupdf_dir}/include/mupdf/fitz/version.h'
with open(path) as f:
    for line in f:
        m = re.match('^#define FZ_VERSION "(.*)"$', line)
        if m:
            cppyy.gbl.mupdf.FZ_VERSION = m.group(1)
            break
    else:
        raise Exception(f'Unable to find FZ_VERSION in {path}')


# Override various functions so that, for example, functions with
# out-parameters instead return tuples.
#

# Override cppyy.gbl.mupdf.Document.lookup_metadata() so it returns the string
# value directly.
#
_Document_lookup_metadata_0 = cppyy.gbl.mupdf.Document.lookup_metadata
def _Document_lookup_metadata(self, key):
    """
    Returns string or None if not found.
    """
    e = ctypes.c_int(0)
    ret = cppyy.gbl.mupdf.lookup_metadata(self.m_internal, key, e)
    e = e.value
    if e < 0:
        return None
    return ret
cppyy.gbl.mupdf.Document.lookup_metadata = _Document_lookup_metadata

# Override cppyy.gbl.mupdf.Bitmap.bitmap_details() to return out-params in a
# tuple.
#
_Bitmap_bitmap_details_0 = cppyy.gbl.mupdf.Bitmap.bitmap_details
def _Bitmap_bitmap_details(self):
    '''
    Returns (w, h, n, stride).
    '''
    w = ctypes.c_int(0)
    h = ctypes.c_int(0)
    n = ctypes.c_int(0)
    stride = ctypes.c_int(0)
    _Bitmap_bitmap_details_0(self, w, h, n, stride)
    return w.value, h.value, n.value, stride.value
cppyy.gbl.mupdf.Bitmap.bitmap_details = _Bitmap_bitmap_details

# Override cppyy.gbl.mupdf.parse_page_range() to return out-params in a tuple.
#
_parse_page_range_0 = cppyy.gbl.mupdf.parse_page_range
def _parse_page_range(s, n):
    '''
    Returns s, a, b.
    '''
    # It seems that cppyy converts any returned NULL to an empty string rather
    # than None, which means we can't distinguish between the last range (which
    # returns '') and beyond the last range (which returns NULL).
    #
    # Luckily fz_parse_page_range() leaves the out-params unchanged when it
    # returns NULL, so we can detect whether NULL was returned by initialsing
    # with special values that would never be ordinarily be returned.
    #
    a = ctypes.c_int(-1)
    b = ctypes.c_int(-1)
    s = _parse_page_range_0(s, a, b, n)
    if a.value == -1 and b.value == -1:
        s = None
    return s, a.value, b.value
cppyy.gbl.mupdf.parse_page_range = _parse_page_range

# Override cppyy.gbl.mupdf.new_test_device() to return out-params in a tuple.
#
_new_test_device_0 = cppyy.gbl.mupdf.new_test_device
def _new_test_device(threshold, options, passthrough):
    '''
    Returns ret, is_color.value.
    '''
    is_color = ctypes.c_int(0)
    sys.stdout.flush()
    if passthrough is None:
        passthrough = 0
    ret = _new_test_device_0(is_color, threshold, options, passthrough)
    return ret, is_color.value
cppyy.gbl.mupdf.new_test_device = _new_test_device

# Provide native python implementation of cppyy.gbl.mupdf.format_output_path()
# (-> fz_format_output_path). (The underlying C/C++ functions take a fixed-size
# buffer for the output string so isn't useful for Python code.)
#
def _format_output_path(format, page):
    m = re.search( '(%[0-9]*d)', format)
    if m:
        ret = format[ :m.start(1)] + str(page) + format[ m.end(1):]
    else:
        dot = format.rfind( '.')
        if dot < 0:
            dot = len( format)
        ret = format[:dot] + str(page) + format[dot:]
    return ret
cppyy.gbl.mupdf.format_output_path = _format_output_path

# Override cppyy.gbl.mupdf.Pixmap.n and cppyy.gbl.mupdf.Pixmap.alpha so
# that they return int. (The underlying C++ functions return unsigned char
# so cppyy's default bindings end up returning a python string which isn't
# useful.)
#
_Pixmap_n0 = cppyy.gbl.mupdf.Pixmap.n
def _Pixmap_n(self):
    return ord(_Pixmap_n0(self))
cppyy.gbl.mupdf.Pixmap.n = _Pixmap_n

_Pixmap_alpha0 = cppyy.gbl.mupdf.Pixmap.alpha
def _Pixmap_alpha(self):
    return ord(_Pixmap_alpha0(self))
cppyy.gbl.mupdf.Pixmap.alpha = _Pixmap_alpha

# Override cppyy.gbl.mupdf.ppdf_clean_file() so that it takes a Python
# container instead of (argc, argv).
#
_ppdf_clean_file0 = cppyy.gbl.mupdf.ppdf_clean_file
def _ppdf_clean_file(infile, outfile, password, opts, argv):
    a = 0
    if argv:
        a = (ctypes.c_char_p * len(argv))(*argv)
        a = ctypes.pointer(a)
    _ppdf_clean_file0(infile, outfile, password, opts, len(argv), a)
cppyy.gbl.mupdf.ppdf_clean_file = _ppdf_clean_file


# Import selected globals and macros into the cppyy.gbl.mupdf namespace.
#
cppyy.gbl.mupdf.fz_identity = cppyy.gbl.fz_identity
cppyy.gbl.mupdf.fz_empty_rect = cppyy.gbl.fz_empty_rect
cppyy.gbl.mupdf.fz_empty_irect = cppyy.gbl.fz_empty_irect
cppyy.gbl.mupdf.fz_infinite_rect = cppyy.gbl.fz_infinite_rect
cppyy.gbl.mupdf.fz_infinite_irect = cppyy.gbl.fz_infinite_irect

cppyy.gbl.mupdf.FZ_LAYOUT_KINDLE_W = cppyy.gbl.FZ_LAYOUT_KINDLE_W
cppyy.gbl.mupdf.FZ_LAYOUT_KINDLE_H = cppyy.gbl.FZ_LAYOUT_KINDLE_H
cppyy.gbl.mupdf.FZ_LAYOUT_KINDLE_EM = cppyy.gbl.FZ_LAYOUT_KINDLE_EM

cppyy.gbl.mupdf.FZ_LAYOUT_US_POCKET_W = cppyy.gbl.FZ_LAYOUT_US_POCKET_W
cppyy.gbl.mupdf.FZ_LAYOUT_US_POCKET_H = cppyy.gbl.FZ_LAYOUT_US_POCKET_H
cppyy.gbl.mupdf.FZ_LAYOUT_US_POCKET_EM = cppyy.gbl.FZ_LAYOUT_US_POCKET_EM

cppyy.gbl.mupdf.FZ_LAYOUT_US_TRADE_W = cppyy.gbl.FZ_LAYOUT_US_TRADE_W
cppyy.gbl.mupdf.FZ_LAYOUT_US_TRADE_H = cppyy.gbl.FZ_LAYOUT_US_TRADE_H
cppyy.gbl.mupdf.FZ_LAYOUT_US_TRADE_EM = cppyy.gbl.FZ_LAYOUT_US_TRADE_EM

cppyy.gbl.mupdf.FZ_LAYOUT_UK_A_FORMAT_W = cppyy.gbl.FZ_LAYOUT_UK_A_FORMAT_W
cppyy.gbl.mupdf.FZ_LAYOUT_UK_A_FORMAT_H = cppyy.gbl.FZ_LAYOUT_UK_A_FORMAT_H
cppyy.gbl.mupdf.FZ_LAYOUT_UK_A_FORMAT_EM = cppyy.gbl.FZ_LAYOUT_UK_A_FORMAT_EM

cppyy.gbl.mupdf.FZ_LAYOUT_UK_B_FORMAT_W = cppyy.gbl.FZ_LAYOUT_UK_B_FORMAT_W
cppyy.gbl.mupdf.FZ_LAYOUT_UK_B_FORMAT_H = cppyy.gbl.FZ_LAYOUT_UK_B_FORMAT_H
cppyy.gbl.mupdf.FZ_LAYOUT_UK_B_FORMAT_EM = cppyy.gbl.FZ_LAYOUT_UK_B_FORMAT_EM

cppyy.gbl.mupdf.FZ_LAYOUT_UK_C_FORMAT_W = cppyy.gbl.FZ_LAYOUT_UK_C_FORMAT_W
cppyy.gbl.mupdf.FZ_LAYOUT_UK_C_FORMAT_H = cppyy.gbl.FZ_LAYOUT_UK_C_FORMAT_H
cppyy.gbl.mupdf.FZ_LAYOUT_UK_C_FORMAT_EM = cppyy.gbl.FZ_LAYOUT_UK_C_FORMAT_EM

cppyy.gbl.mupdf.FZ_LAYOUT_A5_W = cppyy.gbl.FZ_LAYOUT_A5_W
cppyy.gbl.mupdf.FZ_LAYOUT_A5_H = cppyy.gbl.FZ_LAYOUT_A5_H
cppyy.gbl.mupdf.FZ_LAYOUT_A5_EM = cppyy.gbl.FZ_LAYOUT_A5_EM

cppyy.gbl.mupdf.FZ_DEFAULT_LAYOUT_W = cppyy.gbl.FZ_DEFAULT_LAYOUT_W
cppyy.gbl.mupdf.FZ_DEFAULT_LAYOUT_H = cppyy.gbl.FZ_DEFAULT_LAYOUT_H
cppyy.gbl.mupdf.FZ_DEFAULT_LAYOUT_EM = cppyy.gbl.FZ_DEFAULT_LAYOUT_EM

cppyy.gbl.mupdf.FZ_PERMISSION_PRINT = cppyy.gbl.FZ_PERMISSION_PRINT
cppyy.gbl.mupdf.FZ_PERMISSION_COPY = cppyy.gbl.FZ_PERMISSION_COPY
cppyy.gbl.mupdf.FZ_PERMISSION_EDIT = cppyy.gbl.FZ_PERMISSION_EDIT
cppyy.gbl.mupdf.FZ_PERMISSION_ANNOTATE = cppyy.gbl.FZ_PERMISSION_ANNOTATE

cppyy.gbl.mupdf.pdf_write_options = cppyy.gbl.pdf_write_options
