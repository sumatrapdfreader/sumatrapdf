
# maps struct name to StructDef object
g_structs_map = {}

# When generating C struct definitions we need the structs
# in the right order (if Bar refers to Foo, Foo must be defined first).
# This is a list of all structs in the right order
g_all_structs = []

def GetAllStructs(): return g_all_structs

class StructDef(object):
    def __init__(self, name, fields):
        global g_structs_map, g_all_structs
        assert name not in g_structs_map
        g_structs_map[name] = self
        g_all_structs.append(self)

        self.name = name
        self.fields = fields

        self.build_def()

    # calculate:
    #   self.size
    #   self.max_type_len (for better formatting of generated C code)
    def build_def(self):
        field_off = 0
        self.max_type_len = 0
        for field in self.fields:
            field.offset = field_off
            field_off += field.c_size()
            typ = field.c_type()
            if len(typ)  > self.max_type_len:
                self.max_type_len = len(typ)
        self.size = field_off

    def get_struct_fields(self):
        return [f for f in self.fields if f.is_struct()]

def DefineStruct(name, fields):
    return StructDef(name, fields)

# for struct types, we have a separate class for values
# of the struct
class StructValue(object):
    def __init__(self, name):
        assert name in g_structs_map
        self.struct = g_structs_map[name]
        self.val = None

        # forward some values to corresponding StructDef
        self.fields = self.struct.fields
        self.size = self.struct.size
        self.name = self.struct.name
        self.max_type_len = self.struct.max_type_len

        # calculated
        self.base_offset = None  # offset from the beginning of top-level struct

    def c_size(self):
        return get_typ_c_size(self.struct)

    def is_ref(self):
        return True

def MakeStruct(structDef):
    assert isinstance(structDef, StructDef)
    return StructValue(structDef.name)

def is_struct_type(typ): return isinstance(typ, StructDef)

# TODO: also define which python type is allowed for default values
# (to be used in Field.__init__)
class TypeDef(object):
    def __init__(self, c_name, c_size, pack_format):
        self.c_name = c_name
        self.c_size = c_size
        self.pack_format = pack_format

    def is_ref(self):
        return False

g_types_map = {
    "bool"   : TypeDef("int32_t",  4, "<i"),
    "u16"    : TypeDef("uint16_t", 2, "<H"),
    "i32"    : TypeDef("int32_t",  4, "<i"),
    "u32"    : TypeDef("uint32_t", 4, "<I"),
    "color"  : TypeDef("uint32_t", 4, "<I"),
}

def verify_type(typ, def_val):
    if is_struct_type(typ):
        assert def_val == None, "struct type must have default value be None"
    else:
        assert typ in g_types_map, "Unknown type %s" % str(typ)
        assert def_val != None, "primitive type must have default value"

def get_typ_c_name(typ):
    if typ in g_types_map:
        return g_types_map[typ].c_name
    if is_struct_type(typ): return "Ptr<%s>" % typ.name

def get_typ_c_size(typ):
    if typ in g_types_map:
        return g_types_map[typ].c_size
    if is_struct_type(typ): return 8

def get_pack_format(typ):
    if typ in g_types_map:
        return g_types_map[typ].pack_format
    if is_struct_type(typ): return "<Ixxxx" # 4 bytes offset + 4 padding bytes

# for primitive types a single class represents both a definition
# of the field as well as the value
class Field(object):
    def __init__(self, name, typ, def_val = None):
        verify_type(typ, def_val)
        self.name = name
        self.typ = typ

        self.val = def_val

        # calculated
        self.offset = None        # offset of field within the struct
        self.base_offset = None   # offset of value within 

    def is_struct(self):   return is_struct_type(self.typ)
    def c_type(self):      return get_typ_c_name(self.typ)
    def c_size(self):      return get_typ_c_size(self.typ)
    def pack_format(self): return get_pack_format(self.typ)
    def get_struct_def(self):
        assert self.is_struct()
        return self.typ

"""
struct $name {
   $type $field_name;
   ... 
};
STATIC_ASSERT(sizeof($name)==$size, $name_is_$size_bytes);

STATIC_ASSERT(offsetof($field_name), $name) == $offset, $field_name_is_$offset_bytes_in_$name);
...
"""
def gen_h_struct_def(stru):
    stru_name = stru.name
    size = stru.size
    lines = ["struct %s {" % stru.name]
    fmt = "    %%-%ds %%s;" % stru.max_type_len
    for field in stru.fields:
        lines += [fmt % (field.c_type(), field.name)]
    lines += ["};\n"]

    lines += ["STATIC_ASSERT(sizeof(%(stru_name)s)==%(size)d, %(stru_name)s_is_%(size)d_bytes);\n" % locals()]

    fmt = "STATIC_ASSERT(offsetof(%(stru_name)s, %(field_name)s) == %(offset)d, %(field_name)s_is_%(offset)d_bytes_in_%(stru_name)s);"
    for field in stru.fields:
        field_name = field.name
        offset = field.offset
        lines += [fmt % locals()]
    lines += [""]
    return "\n".join(lines)


def gen_struct_defs():
    return "\n".join([gen_h_struct_def(stru) for stru in GetAllStructs()])

"""
StructPointerInfo gFooPointers[] = {
    { $offset, &gFooStructDef },
};
"""
def gen_struct_pointer_infos_one(stru):
    name = stru.name
    lines = []
    lines += ["StructPointerInfo g%(name)sPointers[] = {" % locals()]
    for field in stru.get_struct_fields():
        offset = field.offset
        name = field.typ.name
        lines += ["    { %(offset)d, &g%(name)sStructDef }," % locals()]
    lines += ["};\n"]
    return lines

def gen_struct_pointer_infos(fields):
    lines = []
    for field in  fields:
        lines += gen_struct_pointer_infos_one(field.typ)
    return lines

"""
StructDef gFooStructDef = { $size, $pointersCount, &g${Foo}Pointers[0] };"""
def gen_struct_metadata(stru):
    name = stru.name
    size = stru.size
    fields = stru.get_struct_fields()
    pointer_infos_count = len(fields)
    pointer_infos = "NULL"
    lines = []
    if len(fields) > 0:
        pointer_infos = "&g%sPointers[0]" % name
        lines += gen_struct_pointer_infos_one(stru)
    lines += ["StructDef g%(name)sStructDef = { %(size)d, %(pointer_infos_count)d, %(pointer_infos)s };\n" % locals()]
    return "\n".join(lines)

def gen_structs_metadata():
    return "\n".join([gen_struct_metadata(stru) for stru in GetAllStructs()])
