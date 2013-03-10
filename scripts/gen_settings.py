#!/usr/bin/env python

import os, types, struct
import util, gen_settings_2_3
from gen_settings_types import gen_struct_defs, gen_structs_metadata, gen_top_level_funcs, StructVal, Val
from util import varint

"""
Script that generates C code for compact storage of settings.

In C settings are represented as a top-level struct (which can then refer to
other structs). 

A serialized form is:
  u32 magic id
  u32 version
  u32 offfset of top-level struct
  ... serialized values follow, with integers serialized as varint

A version supports up to 4 components, each component in range <0,255>.
For example version "2.1.3" is encoded as: u32 version = (2 << 24) | (1 << 16) | (3 << 8) | 0.
This way versions can be compared with ">" in C code with the right semantics
(e.g. 2.2.1 > 2.1).

We support generating multiple top-level structs (which can be used for multiple
different purposes).

In order to support forward compatibility, we can make the following struct
changes in version n + 1 (compared to same struct in version n):
 - add new fields at the end
 - we can change the name of existing fields (they are not serialized)
 - technically changing a type to a compatible one (e.g. u16 => u32)
   is possible, but strongly discouraged
What we cannot do:
 - remove fields
 - change the type to an incompatible one

By convention, settings for each version of Sumatra are in gen_settings_$ver.py
file. For example, settings for version 2.3 are in gen_settings_2_3.py.
That way we can easily inherit settings for version N from settings for version N-1.

TODO:
 - write Deserialize()
 - write Serialize()
 - support arrays i.e. a count + type of values + pointer to values
 - maybe: for additional safety, add a number of fields in a given struct
 - maybe: add a signature (magic value) at the beginning of each struct to
   detect corruption of the values (crash if that happens)
 - maybe: when encoding values, also encode the type, so that decoding is
   possible even if we don't have struct metadata to guide the process
   type could be encoded in 2 low bits of integer value, values shifted
   int type would have tag 0, string would have tag 1 and we would have 2
   tags for future expansion
 - maybe: add a compare function. That way we could optimize saving
   (only save if the values have changed). It's really simple:
   call Serialize() on both and memcmp() on result
"""

g_magic_id = 0x53756d53  # 'SumS' as 'Sumatra Settings'

def script_dir(): return os.path.dirname(__file__)
def src_dir():
    p = os.path.join(script_dir(), "..", "src")
    return util.verify_path_exists(p)

def to_win_newlines(s):
    s = s.replace("\r\n", "\n")
    s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))

def read_file(file_path): return file(file_path, "r").read()

h_tmpl   = read_file(os.path.join(script_dir(), "gen_settings_tmpl.h"))
cpp_tmpl = read_file(os.path.join(script_dir(), "gen_settings_tmpl.cpp"))

def ver_from_string(ver_str):
    parts = ver_str.split(".")
    assert len(parts) <= 4
    parts = [int(p) for p in parts]
    for n in parts:
        assert n > 0 and n < 255
    n = 4 - len(parts)
    if n > 0:
        parts = parts + [0]*n
    return parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]

# val is a top-level StructVal with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list, in the reverse order of
# tree traversal
def flatten_values_tree(val):
    assert isinstance(val, StructVal)
    vals = []
    left = [val]
    while len(left) > 0:
        val = left.pop(0)
        assert isinstance(val, StructVal)
        vals += [val]
        for field in val.val:
            if field.is_struct:
                if field.val != None:
                    assert isinstance(field, StructVal)
                    left += [field]
    vals.reverse()
    return vals

def is_num(val):
    tp = type(val)
    return tp == types.IntType or tp == types.LongType or tp == types.BooleanType

def is_str(val): return type(val) == types.StringType

def serialize_val(val):
    if is_num(val):
        val = long(val)
        return varint(val)
    if is_str(val):
        data = varint(len(val))
        return data + val
    assert False, "%s is of unkown type" % val

# serialize values in vals and calculate offset of each
# val in encoded data.
# values are serialized in reverse order because 
# it would be very complicated to serialize forward
# offsets in variable-length encoding
def serialize_top_level_struct(top_level_struct):
    vals = flatten_values_tree(top_level_struct)
    # the first 12 bytes are:
    #  - 4 bytes magic constant (for robustness)
    #  - 4 bytes for version
    #  - 4 bytes offset pointing to a top-level structure
    #      within the data
    offset = 12
    for val in vals:
        val.offset = offset
        for field in val.val:
            val = field.val
            if field.is_struct:
                assert isinstance(field, StructVal)
                val = 0
                if field.val != None:
                    # must have been serialized
                    assert field.offset not in (None, 0)
                    assert offset > field.offset
                    val = field.offset
            data = serialize_val(val)
            field.serialized = data
            offset += len(data)
    return vals

def data_to_hex(data):
    els = ["0x%02x" % ord(c) for c in data]
    return ", ".join(els)

def dump_val(val):
    print("%s name: %s val: %s offset: %s\n" % (str(val), "", str(val.val), str(val.offset)))

def data_with_comment_as_c(data, comment):
    data_hex = data_to_hex(data)
    return "    %(data_hex)s, // %(comment)s" % locals()

# change:
#   <gen_settings_types.StructVal object at 0x7fddfc4c>
# =>
#   StructVal@0x7fddfc4c
def short_object_id(obj):
    if isinstance(obj, StructVal):
        s = str(obj)[1:-1]
        s = s.replace("gen_settings_types.", "")
        return s.replace(" object at ", "@")
    if isinstance(obj, Val):
        assert is_str(obj.val)
        return '"' + obj.val + '"'
    assert False, "%s is object of unkown type" % obj

"""
  // $StructName
  0x00, 0x01, // $type $name = $val
  ...
"""
def get_cpp_data_for_struct_val(struct_val):
    assert isinstance(struct_val, StructVal)
    name = struct_val.struct_def.name
    offset = struct_val.offset
    lines = ["", "    // offset: %s %s %s" % (hex(offset), short_object_id(struct_val), name)]
    assert struct_val.val != None
    if struct_val.val == None:
        assert False, "it happened"
        return lines
    for field in struct_val.val:
        data = field.serialized
        data_hex = data_to_hex(data)
        var_type = field.typ.c_type
        var_name = field.typ.name
        val = field.val
        if is_num(val):
            val_str = hex(val)
        else:
            if field.val == None:
                val_str = "NULL"
            else:
                val_str = short_object_id(field)
        s = "    %(data_hex)s, // %(var_type)s %(var_name)s = %(val_str)s" % locals()
        lines += [s]
    return lines

"""
static uint8_t g$(StructName)Default[] = {
   ... data    
};
"""
def gen_cpp_data_for_struct_values(vals, version_str):
    top_level = vals[-1]
    assert isinstance(top_level, StructVal)
    name = top_level.struct_def.name
    lines = ["static const uint8_t g%sDefault[] = {" % name]
    # magic id
    data = struct.pack("<I", g_magic_id)
    comment = "magic id 'SumS'"
    lines += [data_with_comment_as_c(data, comment)]
    # version
    data = struct.pack("<I", ver_from_string(version_str))
    comment = "version %s" % version_str
    lines += [data_with_comment_as_c(data, comment)]
    # offset of top-level struct
    data = struct.pack("<I", top_level.offset)
    comment = "top-level struct offset %s" % hex(top_level.offset)
    lines += [data_with_comment_as_c(data, comment)]

    for val in vals:
        struct_lines = get_cpp_data_for_struct_val(val)
        lines += struct_lines
    lines += ["};"]
    return "\n".join(lines)

def main():
    dst_dir = src_dir()

    val = gen_settings_2_3.advancedSettings
    version_str = gen_settings_2_3.version
    vals = serialize_top_level_struct(val)

    h_struct_defs = gen_struct_defs(vals, version_str)
    write_to_file(os.path.join(dst_dir, "Settings.h"),  h_tmpl % locals())

    structs_metadata = gen_structs_metadata(vals)

    values_global_data = gen_cpp_data_for_struct_values(vals, version_str)
    top_level_funcs = gen_top_level_funcs(vals)
    write_to_file(os.path.join(dst_dir, "Settings.cpp"), cpp_tmpl % locals())

if __name__ == "__main__":
    main()
