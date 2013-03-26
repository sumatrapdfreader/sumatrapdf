#!/usr/bin/env python
import sys
sys.path.append("scripts") # assumes is being run as ./tools/sertxt_test/gen_settings_txt.py

import os, util, codecs
from gen_settings_types import Struct, settings, Field, field_from_def

"""
TODO:
 - default values
 - escape values with '\n' in them
 - test that we're robust against whitespace after values when parsing
   key/value

Maybe: support arrays of basic types

Maybe: compact serialization of some structs e.g.
  window_pos [
    x: 0
    y: 0
    dx: 0
    dy: 0
  ]
=>
  window_pos: 0 0 0 0

via a Compact flag (like NoStore flag)
"""

g_script_dir = os.path.realpath(os.path.dirname(__file__))

def settings_src_dir():
    return util.verify_path_exists(g_script_dir)

def to_win_newlines(s):
    s = s.replace("\r\n", "\n")
    s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))
def write_to_file_utf8_bom(file_path, s):
    with codecs.open(file_path, "w", "utf-8-sig") as fo:
        fo.write(to_win_newlines(s))

# fooBar => foo_bar
def name2name(s):
    if s == None:
        return None
    res = ""
    n_upper = 0
    for c in s:
        if c.isupper():
            if n_upper == 0:
                res += "_"
            n_upper += 1
            res += c.lower()
        else:
            if n_upper > 1:
                res += "_"
            res += c
            n_upper = 0
    return res

# kind of ugly
g_escape_char = chr(0)

def prefix_str(indent): return "  " * indent

def field_val_as_str(field):
    assert isinstance(field, Field)
    if field.is_bool():
        if field.val:
            return "true"
        else:
            return "false"
    if field.is_color():
        if field.val > 0xffffff:
            return "#%08x" % field.val
        else:
            return "#%06x" % field.val
    if field.is_signed() or field.is_unsigned() or field.is_float():
        return str(field.val)
    if field.is_string():
        return field.val
    assert False, "don't know how to serialize %s" % str(field.typ)

def ser_field(field, lines, indent):
    val_str = field_val_as_str(field)
    # omit serializing empty strings
    if val_str == None: return
    var_name = name2name(field.name)
    prefix = prefix_str(indent)
    lines += ["%s%s: %s" % (prefix, var_name, val_str)]

def ser_array(field, lines, indent):
    assert field.is_array()

    n = len(field.val.values)
    if 0 == n: return

    prefix = prefix_str(indent)
    var_name = name2name(field.name)
    lines += ["%s%s [" % (prefix, var_name)]
    prefix += "  "
    for val in field.val.values:
        #print(str(val))
        #print(val.as_str())
        lines += [prefix + "["]
        ser_struct(val, None, lines, indent+1)
        lines += [prefix + "]"]
    prefix = prefix[:-2]
    lines += ["%s]" % prefix]

def ser_struct(struct, name, lines, indent):
    assert struct.is_struct()
    prefix = prefix_str(indent)

    #print("name: %s, indent: %d, prefix: '%s'" % (name, indent, prefix))

    if name != None:
        lines += ["%s%s [" % (prefix, name2name(name))]

    for field in struct.values:
        if field.is_no_store():
            continue

        if field.is_array():
            ser_array(field, lines, indent+1)
            continue

        if field.is_struct():
            if field.val.offset == 0:
                if False: # Note: this omits empty values
                    lines += ["%s%s: " % (prefix, name2name(field.name))]
            else:
                ser_struct(field.val, field.name, lines, indent + 1)
            continue

        ser_field(field, lines, indent + 1)

    if name != None:
        lines += ["%s]" % prefix]

(FMT_NONE, FMT_LEFT, FMT_RIGHT) = (0, 1, 2)

def get_col_fmt(col_fmt, col):
    if col >= len(col_fmt):
        return FMT_NONE
    return col_fmt[col]

def fmt_str(s, max, fmt):
    add = max - len(s)
    if fmt == FMT_LEFT:
        return " " * add + s
    elif fmt == FMT_RIGHT:
        return s + " " * add
    return s

"""
[
  ["a",  "bc",   "def"],
  ["ab", "fabo", "d"]
]
=>
[
  ["a ", "bc  ", "def"],
  ["ab", "fabo", "d  "]
]
"""
def fmt_rows(rows, col_fmt = []):
    col_max_len = {}
    for row in rows:
        for col in range(len(row)):
            el_len = len(row[col])
            curr_max = col_max_len.get(col, 0)
            if el_len > curr_max:
                col_max_len[col] = el_len
    res = []
    for row in rows:
        res_row = []
        for col in range(len(row)):
            s = fmt_str(row[col], col_max_len[col], get_col_fmt(col_fmt, col))
            res_row.append(s)
        res.append(res_row)
    return res

def gen_struct_def(stru_cls):
    #assert isinstance(stru, Struct)
    name = stru_cls.__name__
    lines = ["struct %s {" % name]
    rows = [[tup[1].c_type(), tup[0]] for tup in stru_cls.fields]
    lines += ["    %s  %s;" % (e1, e2) for (e1, e2) in fmt_rows(rows, [FMT_RIGHT])]
    lines += ["};\n"]
    return "\n".join(lines)

def gen_struct_defs(structs):
    return "\n".join([gen_struct_def(stru) for stru in structs])

prototypes_tmpl = """%(name)s *Deserialize%(name)s(const char *data, size_t dataLen);
uint8_t *Serialize%(name)s(%(name)s *, size_t *dataLenOut);
void Free%(name)s(%(name)s *);
"""

def gen_prototypes(stru_cls):
    name = stru_cls.__name__
    return prototypes_tmpl % locals()

h_txt_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#ifndef SettingsSumatra_h
#define SettingsSumatra_h

%(struct_defs)s
%(prototypes)s
#endif
"""

cpp_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#include "BaseUtil.h"
#include "SerializeTxt.h"
#include "SettingsSumatra.h"

using namespace sertxt;

%(filed_names_seq_strings)s
#define of offsetof
%(structs_metadata)s
#undef of
%(top_level_funcs)s
"""

"""
FieldMetadata g${name}FieldMetadata[] = {
    { $name_off, $offset, $type, &g${name}StructMetadata },
};
"""
def gen_struct_fields_txt(stru_cls, field_names):
    #assert isinstance(stru, Struct)
    struct_name = stru_cls.__name__
    lines = ["FieldMetadata g%sFieldMetadata[] = {" % struct_name]
    rows = []
    for field_def in stru_cls.fields:
        #assert isinstance(field, Field)
        field_name = field_def[0]
        field = field_from_def(field_def)
        typ_enum = field.get_typ_enum()
        name_off = field_names.get_offset(name2name(field_name))
        offset = "of(%s, %s)" % (struct_name, field_name)
        val = "NULL"
        if field.is_struct() or field.is_array():
            val = "&g%sMetadata" % field.typ.name()
        col = [str(name_off) + ",", offset + ",", typ_enum + ",", val]
        rows.append(col)
    rows = fmt_rows(rows, [FMT_LEFT, FMT_RIGHT, FMT_RIGHT, FMT_RIGHT])
    lines += ["    { %s %s %s %s }," % (e1, e2, e3, e4) for (e1, e2, e3, e4) in rows]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata_txt(structs, field_names):
    lines = []
    for stru_cls in structs:
        struct_name = stru_cls.__name__
        nFields = len(stru_cls.fields)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_txt(stru_cls, field_names)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

top_level_funcs_txt_tmpl = """
%(name)s *Deserialize%(name)s(const char *data, size_t dataLen)
{
    char *dataCopy = str::DupN(data, dataLen);
    void *res = Deserialize(dataCopy, dataLen, &g%(name)sMetadata, FIELD_NAMES_SEQ);
    free(dataCopy);
    return (%(name)s*)res;
}

uint8_t *Serialize%(name)s(%(name)s *val, size_t *dataLenOut)
{
    return Serialize((const uint8_t*)val, &g%(name)sMetadata, FIELD_NAMES_SEQ, dataLenOut);
}

void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val, &g%(name)sMetadata);
}"""

def add_cls(cls, structs):
    if cls not in structs:
        structs.append(cls)

# return a list of Struct subclasses that are needed to define val
def structs_from_top_level_value_rec(struct, structs):
    assert isinstance(struct, Struct)
    for field in struct.values:
        if field.is_array():
            add_cls(field.val.typ, structs)
        elif field.is_struct():
            structs_from_top_level_value_rec(field.val, structs)
    add_cls(struct.__class__, structs)

def structs_from_top_level_value(val):
    structs = []
    structs_from_top_level_value_rec(val, structs)
    return structs

def gen_top_level_funcs_txt(top_level):
    assert isinstance(top_level, Struct)
    name = top_level.name()
    return top_level_funcs_txt_tmpl % locals()

def gen_for_top_level_val(top_level_val, file_path):
    structs = structs_from_top_level_value(top_level_val)
    prototypes = gen_prototypes(top_level_val.__class__)
    struct_defs = gen_struct_defs(structs)
    field_names = util.SeqStrings()
    structs_metadata = gen_structs_metadata_txt(structs, field_names)
    top_level_funcs = gen_top_level_funcs_txt(top_level_val)
    filed_names_seq_strings = "#define FIELD_NAMES_SEQ %s\n" % field_names.get_all_c_escaped()
    write_to_file(file_path + ".h",  h_txt_tmpl % locals())
    write_to_file(file_path + ".cpp", cpp_txt_tmpl % locals())

def gen_txt_for_top_level_val(top_level_val, file_path):
    lines = ["; see http://blog.kowalczyk.info/software/sumatrapdf/settings.html for documentation"]
    # -1 is a bit hackish, because we elide the name of the top-level struct
    # to make it more readable
    ser_struct(top_level_val, None, lines, -1)
    s = "\n".join(lines)
    write_to_file_utf8_bom(file_path, s)

def gen_sumatra_settings():
    dst_dir = settings_src_dir()
    top_level_val = settings
    file_path = os.path.join(dst_dir, "SettingsSumatra")
    gen_for_top_level_val(top_level_val, file_path)
    gen_txt_for_top_level_val(top_level_val, os.path.join(dst_dir, "data.txt"))

def main():
    gen_sumatra_settings()

if __name__ == "__main__":
    main()
