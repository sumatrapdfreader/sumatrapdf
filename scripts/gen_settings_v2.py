#!/usr/bin/env python

import os, struct, types
import util, gen_settings_v2_2_3
from gen_settings_types_v2 import gen_struct_defs, gen_structs_metadata, Var, StructPtr

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

We rely on an exact layout of data in the struct, so:
 - we don't use C types whose size is not fixed (e.g. int)
 - we use a fixed struct packing (that's TODO)
 - our pointers are always 8 bytes, to support both 64-bit and 32-bit archs
   with the same definition

TODO:
 - add a notion of Struct inheritance to make it easy to support forward/backward
   compatibility
 - write const char *serialize_struct(char *data, StructDef *def);
 - support strings
 - solve compiler compatibility issues by changing serialization scheme
 - introduce a concept of array i.e. a count + type of values + pointer to 
   values (requires adding a notion of value first)
 - add size as the first field of each struct, for forward-compatibilty
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

# val is a top-level struct value with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list and calculate base_offset
# on each value
def flatten_values_tree(val):
    assert isinstance(val, StructVal)
    
    # first field of the top-level struct must be a version
    assert "version" == val.struct_def.fields[0].name
    vals = []
    left = [val]
    base_offset = 0
    while len(left) > 0:
        struct_val = left.pop(0)
        struct_val.base_offset = base_offset + struct_val.struct_def.c_size
        for field in struct_val.struct_def.fields:
            if field.is_struct() and field.val != None:
                left += [field.val]
        base_offset += struct_val.struct_def.c_size
        vals += [struct_val]
    for val in vals:
        dump_field(val)

    return vals

def data_to_hex(data):
    els = ["0x%02x" % ord(c) for c in data]
    return ", ".join(els)

def is_of_num_type(val):
    tp = type(val)
    return tp == types.IntType or tp == types.LongType

def dump_field(field):
    print("%s name: %s val: %s base_offset: %s" % (str(field), field.name, str(field.val), str(field.base_offset)))

"""
  // $StructName
  0x00, 0x01, // $type $name = $val
  ...
"""
def get_cpp_data_for_struct_val(struct_val):
    assert isinstance(struct_val, Struct)
    name = struct_val.name
    lines = ["", "  // %s" % name]
    for field in struct_val.fields:
        val = field.val
        if field.is_struct():
            val = 0
            if field.val != None:
                val = field.base_offset
        try:
            assert val != None
        except:
            print("")
            dump_field(struct_val)
            dump_field(field)
            raise

        fmt = field.pack_format
        try:
            data = struct.pack(fmt, val)
        except:
            print(struct_val)
            print(val)
            raise
        data_hex = data_to_hex(data)
        var_type = field.c_type
        var_name = field.name
        if is_of_num_type(val):
            val_str = hex(val)
        else:
            val_str = str(val)
        s = "  %(data_hex)s, // %(var_type)s %(var_name)s = %(val_str)s" % locals()
        lines += [s]
    return lines

"""
static uint8_t g$(StructName)Default[] = {
   ... data    
};
"""
def gen_cpp_data_for_struct_values(vals):
    name = vals[0].name
    lines = ["static uint8_t g%sDefault[] = {" % name]
    for val in vals:
        lines += get_cpp_data_for_struct_val(val)
    lines += ["};"]
    return "\n".join(lines)

def main():
    dst_dir = src_dir()

    h_struct_defs = gen_struct_defs()
    write_to_file(os.path.join(dst_dir, "Settings.h"),  h_tmpl % locals())

    vals = flatten_values_tree(gen_settings_2_3.advancedSettingsStruct)

    structs_metadata = gen_structs_metadata()
    values_global_data = gen_cpp_data_for_struct_values(vals)
    write_to_file(os.path.join(dst_dir, "Settings.cpp"), cpp_tmpl % locals())

if __name__ == "__main__":
    main()
