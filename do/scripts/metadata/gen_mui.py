#!/usr/bin/env python
import os
from metadata import Struct, Field, String, WString, I32, Array, Compact
from gen_txt import gen_for_top_level_vals

import sys
sys.path.append("scripts")  # assumes is being run as ./scrpts/metadata/gen_mui.py
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
        Field("clicked", String(None)),
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

class DirectionalLayoutDataDef(Struct):
    fields = [
        Field("controlName", String(None)),
        Field("sla", String(None)),  # this is really a float
        Field("snla", String(None)),  # this is really a float
        Field("align", String(None)),
    ]

class HorizontalLayoutDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("children", Array(DirectionalLayoutDataDef, []), Compact),
    ]

class VerticalLayoutDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("children", Array(DirectionalLayoutDataDef, []), Compact),
    ]

class PagesLayoutDef(Struct):
    fields = [
        Field("name", String(None)),
        Field("page1", String(None)),
        Field("page2", String(None)),
        Field("spaceDx", I32(4))
    ]

def gen_mui():
    dst_dir = mui_src_dir()
    structs = [
        ButtonVectorDef(), ButtonDef(), ScrollBarDef(),
        DirectionalLayoutDataDef(), HorizontalLayoutDef(),
        VerticalLayoutDef()
    ]

    file_path_base = os.path.join(dst_dir, "MuiDefs")
    gen_for_top_level_vals(structs, file_path_base)

    file_path_base = os.path.join(src_dir(), "MuiEbookPageDef")
    gen_for_top_level_vals([EbookPageDef()], file_path_base)

    file_path_base = os.path.join(src_dir(), "PagesLayoutDef")
    gen_for_top_level_vals([PagesLayoutDef()], file_path_base)

def main():
    gen_mui()

if __name__ == "__main__":
    main()
