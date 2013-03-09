# When generating C struct definitions we need the structs
# in the right order (if Bar refers to Foo, Foo must be defined first).
# This is a list of all structs in the right order
g_all_structs = []

def GetAllStructs(): return g_all_structs

# Represents a variable: its type and default value
class Var(object):
    def __init__(self, name, c_type, c_size, pack_format, def_val = None):
        self.name = name
        self.val = def_val

        self.c_type = c_type
        self.c_size = c_size
        self.pack_format = pack_format

        self.struct_def = None
        self.offset = None        # offset of variable within the struct

    def is_struct(self):   return self.struct_def != None

class Bool(Var):
    def __init__(self, name, def_val = False):
        super(Bool, self).__init__(name, "int32_t", 4, "<i", def_val)

class U16(Var):
    def __init__(self, name, def_val = 0):
        super(U16, self).__init__(name, "uint16_t", 2, "<H", def_val)

class I32(Var):
    def __init__(self, name, def_val = 0):
        super(I32, self).__init__(name, "int32_t", 4, "<i", def_val)

class U32(Var):
    def __init__(self, name, def_val = 0):
        super(U32, self).__init__(name, "uint32_t", 4, "<I", def_val)

class Color(Var):
    def __init__(self, name, def_val = 0):
        super(Color, self).__init__(name, "uint32_t", 4, "<I", def_val)

class StructPtr(Var):
    def __init__(self, name, struct_def, def_val=None):
        assert isinstance(struct_def, StructDef)
        if def_val != None:
            assert isinstance(def_val, StructVal)
            assert isinstance(def_val.struct_def, StructDef)
        c_type = "Ptr<%s>" % struct_def.name
        c_size = struct_def.c_size
        pack_format = "<Ixxxx" # 4 bytes offset + 4 padding bytes
        super(StructPtr, self).__init__(name, c_type, c_size, pack_format, def_val)
        self.struct_def = struct_def

# defines a struct i.e. all its variables
class StructDef(object):
    def __init__(self, name, vars):
        global g_all_structs
        g_all_structs.append(self)

        self.name = name
        self.vars = vars

        self.build_def()

    # calculate:
    #   self.c_size (StructPtr needs to know that)
    #   self.max_type_len (for better formatting of generated C code)
    def build_def(self):
        var_off = 0
        self.max_type_len = 0
        for var in self.vars:
            var.offset = var_off
            var_off += var.c_size
            self.max_type_len = max(self.max_type_len, len(var.c_type))
        self.c_size = var_off

    def get_struct_vars(self):
        return [var for var in self.vars if var.is_struct()]

def DefineStruct(name, vars):
    return StructDef(name, vars)

# defines a member variable of the struct: its type and its value
class Val(object):
    def __init__(self, typ, val):
        assert isinstance(typ, Var)
        self.typ = typ
        self.val = val

        # calculated
        self.base_offset = None   # offset of value within serialized data of top-level struct

        self.is_struct = False
        if isinstance(typ, StructDef):
            self.is_struct = True
            self.val = [Val(typ) for typ in typ.vars]
        elif isinstance(typ, StructPtr):
            self.is_struct = True
            self.val = [Val(typ) for typ in typ]
        else:
            assert isinstance(typ, Field) and not isinstance(typ, StructPtr)
            self.val = 

        self.val = typ.def_val

        self.struct_def = None
        self.vals = None

def MakeVal(val):
    if isinstance(val, Var):
        if isinstance(val, StructPtr):
            assert False, "what to do?"
        else:
            return Val(val.typ)

def MakeStruct(struct_def):

    return MakeVal(struct_def)


"""
struct $name {
   $type $field_name;
   ... 
};
STATIC_ASSERT(sizeof($name)==$size, $name_is_$size_bytes);

STATIC_ASSERT(offsetof($field_name), $name) == $offset, $field_name_is_$offset_bytes_in_$name);
...
"""
def gen_h_struct_def(struct_def):
    name = struct_def.name
    c_size = struct_def.c_size
    fields = struct_def.fields
    lines = ["struct %s {" % name]
    fmt = "    %%-%ds %%s;" % struct_def.max_type_len
    for field in struct_def.fields:
        lines += [fmt % (field.c_type, field.name)]
    lines += ["};\n"]

    lines += ["STATIC_ASSERT(sizeof(%(name)s)==%(c_size)d, %(name)s_is_%(c_size)d_bytes);\n" % locals()]

    fmt = "STATIC_ASSERT(offsetof(%(name)s, %(field_name)s) == %(offset)d, %(field_name)s_is_%(offset)d_bytes_in_%(name)s);"
    for field in fields:
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
def gen_struct_pointer_infos_one(struct_def):
    name = struct_def.name
    lines = []
    lines += ["StructPointerInfo g%(name)sPointers[] = {" % locals()]
    for field in struct_def.get_struct_fields():
        offset = field.offset
        name = field.name
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
def gen_struct_metadata(struct_def):
    name = struct_def.name
    size = struct_def.c_size
    fields = struct_def.get_struct_fields()
    pointer_infos_count = len(fields)
    pointer_infos = "NULL"
    lines = []
    if len(fields) > 0:
        pointer_infos = "&g%sPointers[0]" % name
        lines += gen_struct_pointer_infos_one(struct_def)
    lines += ["StructDef g%(name)sStructDef = { %(size)d, %(pointer_infos_count)d, %(pointer_infos)s };\n" % locals()]
    return "\n".join(lines)

def gen_structs_metadata():
    return "\n".join([gen_struct_metadata(stru) for stru in GetAllStructs()])
