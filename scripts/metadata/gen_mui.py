#!/usr/bin/env python
import os
from metadata import Struct, Field, String, WString
from gen_txt import gen_for_top_level_val, set_whitespace

import sys
sys.path.append("scripts") # assumes is being run as ./scrpts/metadata/gen_mui.py
import util

g_script_dir = os.path.realpath(os.path.dirname(__file__))
def mui_src_dir():
    d = os.path.join(g_script_dir, "..", "..", "src", "mui")
    return util.verify_path_exists(os.path.realpath(d))

def src_dir():
    d = os.path.join(g_script_dir, "..", "..", "src")
    return util.verify_path_exists(os.path.realpath(d))

class ButtonVectorDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("path", String(None)),
        Field("styleDefault", String(None)),
        Field("styleMouseOver", String(None)),
    ]

class ButtonDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("text", WString(None)),
        Field("style", String(None)),
    ]

class ScrollBarDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("style", String(None)),
        Field("cursor", String(None)),
    ]

class EbookPageDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("style", String(None)),
    ]

def gen_mui():
    dst_dir = mui_src_dir()
    file_path_base = os.path.join(dst_dir, "MuiButtonVectorDef")
    gen_for_top_level_val(ButtonVectorDef(), file_path_base)
    file_path_base = os.path.join(dst_dir, "MuiButtonDef")
    gen_for_top_level_val(ButtonDef(), file_path_base)
    file_path_base = os.path.join(dst_dir, "MuiScrollBarDef")
    gen_for_top_level_val(ScrollBarDef(), file_path_base)

    file_path_base = os.path.join(src_dir(), "MuiEbookPageDef")
    gen_for_top_level_val(EbookPageDef(), file_path_base)

def main():
    gen_mui()

if __name__ == "__main__":
    main()
