#!/usr/bin/env python

import os, types
import util, gen_settings_2_3
from gen_settings_types import gen_struct_defs, gen_structs_metadata, StructVal
from util import varint

"""
Script that generates C code for compact storage of settings.

In C settings are represented as a top-level struct (which can then refer to
other structs). The first field of top-level struct must be a u32 version
field. A version supports up to 4 components, each component in range <0,255>.
For example version "2.1.3" is encoded as: u32 version = (2 << 24) | (1 << 16) | (3 << 8) | 0.
This way versions can be compared with ">" in C code with the right semantics
(e.g. 2.2.1 > 2.1).

We support generating multiple top-level structs (which can be used for multiple
different purposes).

In order to support forward and backward compatibility, struct for a given
version must be a strict superset of struct for a previous version. We can
only add, we can't remove fields or change their types (we can change the names
of fields).

By convention, settings for each version of Sumatra are in gen_settings_$ver.py
file. For example, settings for version 2.3 are in gen_settings_2_3.py.

That way we can inherit settings for version N from settings for version N-1.

TODO:
 - support strings
 - solve compiler compatibility issues by changing serialization scheme. The serialized
   format for each struct would be:
       varint number_of_vals
       for val in vals:
          if val is string:
            varint len(string)
            char[len(string)] string data
          if val is StructVal:
            varint offset to struct data
          if val is a numeric:
            varing val
 - write const char *serialize_struct(char *data, StructDef *def);
 - introduce a concept of array i.e. a count + type of values + pointer to 
   values
 - maybe: add a signature at the beginning of each struct to detect
   corruption of the values (crash if that happens)
 - maybe: add a compare function. That way we could optimize saving
   (only save if the values have changed). It's really simple:
   call serialize_struct() on both and memcmp() on result
"""

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

# val is a top-level StructVal with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list and calculate base_offset
# on each value
def flatten_values_tree(val):
    assert isinstance(val, StructVal)
    
    # first field of the top-level struct must be a version
    assert "version" == val.struct_def.vars[0].name
    vals = []
    left = [val]
    offset = 0
    while len(left) > 0:
        val = left.pop(0)
        val.offset = offset
        vals += [val]
        for field in val.val:
            if field.is_struct and field.val != None:
                assert isinstance(field, StructVal)
                left += [field]
        offset += 4
    #for val in vals: dump_val(val)
    return vals

def data_to_hex(data):
    els = ["0x%02x" % ord(c) for c in data]
    return ", ".join(els)

def is_of_num_type(val):
    tp = type(val)
    return tp == types.IntType or tp == types.LongType

def dump_val(val):
    print("%s name: %s val: %s offset: %s\n" % (str(val), "", str(val.val), str(val.offset)))

"""
  // $StructName
  0x00, 0x01, // $type $name = $val
  ...
"""
def get_cpp_data_for_struct_val(struct_val, offset):
    assert isinstance(struct_val, StructVal)
    name = struct_val.struct_def.name
    lines = ["", "  // %s offset: %s" % (name, hex(offset))]
    if struct_val.val == None:
        return (lines, 0)
    size = 0
    for field in struct_val.val:
        if field.is_struct:
            val = field.offset
            if None == val:
                val = 0
        else:
            val = field.val
        # TODO: if val is string, encode len as varint with string bytes following
        data = varint(val)

        size += len(data)
        data_hex = data_to_hex(data)
        var_type = field.typ.c_type
        var_name = field.typ.name
        if is_of_num_type(val):
            val_str = hex(val)
        else:
            val_str = str(val)
        s = "  %(data_hex)s, // %(var_type)s %(var_name)s = %(val_str)s" % locals()
        lines += [s]
    return (lines, size)

"""
static uint8_t g$(StructName)Default[] = {
   ... data    
};
"""
def gen_cpp_data_for_struct_values(vals):
    val = vals[0]
    assert isinstance(val, StructVal)
    name = val.struct_def.name
    lines = ["static uint8_t g%sDefault[] = {" % name]
    offset = 0
    for val in vals:
        (struct_lines, size) = get_cpp_data_for_struct_val(val, offset)
        lines += struct_lines
        offset += size
    lines += ["};"]
    return "\n".join(lines)

def main():
    dst_dir = src_dir()

    val = gen_settings_2_3.advancedSettings;

    h_struct_defs = gen_struct_defs()
    write_to_file(os.path.join(dst_dir, "Settings.h"),  h_tmpl % locals())

    vals = flatten_values_tree(val)

    structs_metadata = gen_structs_metadata()

    values_global_data = gen_cpp_data_for_struct_values(vals)
    write_to_file(os.path.join(dst_dir, "Settings.cpp"), cpp_tmpl % locals())

if __name__ == "__main__":
    main()
