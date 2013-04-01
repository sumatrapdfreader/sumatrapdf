#!/usr/bin/env python
import sys
sys.path.append("scripts") # assumes is being run as ./tools/sertxt_test/gen_settings_txt.py

import os, util, codecs, random, gen_settings_types
from gen_settings_types import Field

"""
TODO:
  - add a way to pass Allocator to Serialize/Deserialize

Todo maybe: arrays of basic types
"""

g_script_dir = os.path.realpath(os.path.dirname(__file__))

# kind of ugly that those are globals

# character we use for escaping strings
g_escape_char = "$"

# if true, will add whitespace at the end of the string, just for testing
g_add_whitespace = False

def settings_src_dir():
    return util.verify_path_exists(g_script_dir)

def to_win_newlines(s):
    #s = s.replace("\r\n", "\n")
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

def escape_char(c):
    if c == g_escape_char:
        return c + c
    if c in "[]":
        return g_escape_char + c
    if c == '\r':
        return g_escape_char + 'r'
    if c == '\n':
        return g_escape_char + 'n'
    return c

def escape_str(s):
    if 0 == g_escape_char: return s
    res = ""
    for c in s:
        res += escape_char(c)
    return res

def ser_field(field, lines, indent):
    val_str = field_val_as_str(field)
    # omit serializing empty strings
    if val_str == None: return
    val_str = escape_str(val_str)
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

def ser_struct_compact(struct, name, lines, indent):
    assert struct.is_struct()
    vals = []
    for field in struct.values:
        val_str = escape_str(field_val_as_str(field))
        assert " " not in val_str
        vals += [val_str]
    prefix = prefix_str(indent)
    lines += [prefix + name2name(name) + ": " + " ".join(vals)]

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

        if field.is_compact():
            ser_struct_compact(field.val, field.name,  lines, indent + 1)
            continue

        if field.is_struct():
            if field.val.offset == 0:
                if False: # Note: this omits empty values
                    lines += ["%s%s: " % (prefix, name2name(field.name))]
                continue
            ser_struct(field.val, field.name, lines, indent + 1)
            continue

        ser_field(field, lines, indent + 1)

    if name != None:
        lines += ["%s]" % prefix]

def gen_struct_def(stru_cls):
    name = stru_cls.__name__
    lines = ["struct %s {" % name]
    rows = [[field.c_type(), field.name] for field in stru_cls.fields]
    rows = [["const StructMetadata *", "def"]] + rows
    lines += ["    %s  %s;" % (e1, e2) for (e1, e2) in util.fmt_rows(rows, [util.FMT_RIGHT])]
    lines += ["};\n"]
    return "\n".join(lines)

def gen_struct_defs(structs):
    return "\n".join([gen_struct_def(stru) for stru in structs])

prototypes_tmpl = """%(name)s *Deserialize%(name)s(const char *data, size_t dataLen);
%(name)s *Deserialize%(name)sWithDefault(const char *data, size_t dataLen, const char *defaultData, size_t defaultDataLen);
uint8_t *Serialize%(name)s(%(name)s *, size_t *dataLenOut);
void Free%(name)s(%(name)s *);
"""

def gen_prototypes(stru_cls):
    name = stru_cls.__name__
    return prototypes_tmpl % locals()

h_txt_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#ifndef %(file_name)s_h
#define %(file_name)s_h

namespace sertxt {

%(struct_defs)s
%(prototypes)s
} // namespace sertxt
#endif
"""

cpp_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#include "BaseUtil.h"
#include "SerializeTxt.h"
#include "%(file_name)s.h"

namespace sertxt {

#define of offsetof
%(structs_metadata)s
#undef of
%(top_level_funcs)s

} // namespace sertxt
"""

"""
const FieldMetadata g${name}FieldMetadata[] = {
    { $name_off, $offset, $type, &g${name}StructMetadata },
};
"""
def gen_struct_fields_txt(stru_cls):
    struct_name = stru_cls.__name__
    field_names = stru_cls.field_names
    lines = ["const FieldMetadata g%sFieldMetadata[] = {" % struct_name]
    rows = []
    for field in stru_cls.fields:
        assert isinstance(field, Field)
        typ_enum = field.get_typ_enum()
        name_off = field_names.get_offset(name2name(field.name))
        offset = "of(%s, %s)" % (struct_name, field.name)
        val = "NULL"
        if field.is_struct() or field.is_array():
            val = "&g%sMetadata" % field.typ.name()
        col = [str(name_off) + ",", offset + ",", typ_enum + ",", val]
        rows.append(col)
    rows = util.fmt_rows(rows, [util.FMT_LEFT, util.FMT_RIGHT, util.FMT_RIGHT, util.FMT_RIGHT])
    lines += ["    { %s %s %s %s }," % (e1, e2, e3, e4) for (e1, e2, e3, e4) in rows]
    lines += ["};\n"]
    return lines

"""
const StructMetadata g${name}StructMetadata = { $size, $nFields, $fields, $fieldNames };
"""
def gen_structs_metadata_txt(structs):
    lines = []
    for stru_cls in structs:
        stru_cls.field_names = util.SeqStrings()
        struct_name = stru_cls.__name__
        nFields = len(stru_cls.fields)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_txt(stru_cls)
        field_names = stru_cls.field_names.get_all_c_escaped()
        lines += ["""const StructMetadata g%(struct_name)sMetadata = {
    sizeof(%(struct_name)s),
    %(nFields)d,
    %(field_names)s,
    %(fields)s
};\n""" % locals()]
    return "\n".join(lines)

top_level_funcs_txt_tmpl = """
%(name)s *Deserialize%(name)s(const char *data, size_t dataLen)
{
    return Deserialize%(name)sWithDefault(data, dataLen, NULL, 0);
}

%(name)s *Deserialize%(name)sWithDefault(const char *data, size_t dataLen, const char *defaultData, size_t defaultDataLen)
{
    char *dataCopy = str::DupN(data, dataLen);
    char *defaultDataCopy = str::DupN(defaultData, defaultDataLen);
    void *res = DeserializeWithDefault(dataCopy, dataLen, defaultDataCopy, defaultDataLen, &g%(name)sMetadata);
    free(dataCopy);
    free(defaultDataCopy);
    return (%(name)s*)res;
}

uint8_t *Serialize%(name)s(%(name)s *val, size_t *dataLenOut)
{
    return Serialize((const uint8_t*)val, dataLenOut);
}

void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val);
}"""

def add_cls(cls, structs):
    if cls not in structs:
        structs.append(cls)

# return a list of Struct subclasses that are needed to define val
def structs_from_top_level_value_rec(struct, structs):
    assert struct.is_struct()
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
    assert top_level.is_struct()
    name = top_level.name()
    return top_level_funcs_txt_tmpl % locals()

def gen_for_top_level_val(top_level_val, file_path):
    structs = structs_from_top_level_value(top_level_val)
    prototypes = gen_prototypes(top_level_val.__class__)
    struct_defs = gen_struct_defs(structs)
    structs_metadata = gen_structs_metadata_txt(structs)
    top_level_funcs = gen_top_level_funcs_txt(top_level_val)
    file_name = os.path.basename(file_path)
    write_to_file(file_path + ".h",  h_txt_tmpl % locals())
    write_to_file(file_path + ".cpp", cpp_txt_tmpl % locals())

# we add whitespace to all lines that don't have "str" in them
# which is how we prevent adding whitespace to string fields,
# where whitespace at end is significant. For that to work all
# string field names must have "str" in them. This is only for testing
def add_random_ws(s):
    if "str" in s: return s
    return s + " " * random.randint(1, 4)

def gen_txt_for_top_level_val(top_level_val, file_path):
    lines = ["; see http://blog.kowalczyk.info/software/sumatrapdf/settings.html for documentation"]
    # -1 is a bit hackish, because we elide the name of the top-level struct
    # to make it more readable
    ser_struct(top_level_val, None, lines, -1)
    if g_add_whitespace:
        lines = [add_random_ws(s) for s in lines]
    s = "\n".join(lines) + "\n" # for consistency with how C code does it
    write_to_file_utf8_bom(file_path, s)

def gen_sumatra_settings():
    global g_add_whitespace
    g_add_whitespace = False
    dst_dir = settings_src_dir()
    top_level_val = gen_settings_types.Settings()
    file_path = os.path.join(dst_dir, "SettingsTxtSumatra")
    gen_for_top_level_val(top_level_val, file_path)
    gen_txt_for_top_level_val(top_level_val, os.path.join(dst_dir, "data.txt"))

def gen_simple():
    global g_add_whitespace
    g_add_whitespace = True
    dst_dir = settings_src_dir()
    top_level_val = gen_settings_types.Simple()
    file_path = os.path.join(dst_dir, "SettingsTxtSimple")
    gen_for_top_level_val(top_level_val, file_path)
    gen_txt_for_top_level_val(top_level_val, os.path.join(dst_dir, "data_simple_with_ws.txt"))
    g_add_whitespace = False
    gen_txt_for_top_level_val(top_level_val, os.path.join(dst_dir, "data_simple_no_ws.txt"))

    top_level_val = gen_settings_types.ForDefaultTesting()
    gen_txt_for_top_level_val(top_level_val, os.path.join(dst_dir, "data_for_default.txt"))

def main():
    gen_sumatra_settings()
    gen_simple()

if __name__ == "__main__":
    main()
