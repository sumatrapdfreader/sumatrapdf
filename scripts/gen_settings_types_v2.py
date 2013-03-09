# Represents a variable: its type and default value
class Var(object):
    def __init__(self, name, c_type, c_size, pack_format, def_val = None):
        self.name = name
        self.def_val = def_val

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
        c_size = 8
        pack_format = "<Ixxxx" # 4 bytes offset + 4 padding bytes
        super(StructPtr, self).__init__(name, c_type, c_size, pack_format, def_val)
        self.struct_def = struct_def

# When generating C struct definitions we need the structs
# in the right order (if Bar refers to Foo, Foo must be defined first).
# This is a list of all structs in the right order
g_all_structs = []

# defines a struct i.e. all its variables
class StructDef(object):
    def __init__(self, name, parent, vars):
        global g_all_structs
        g_all_structs.append(self)

        self.name = name
        self.vars = vars
        if parent != None:
            self.vars = parent.vars + vars

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

def GetAllStructs(): return g_all_structs

def DefineStruct(name, parent, vars):
    return StructDef(name, parent, vars)

# defines a member variable of the struct: its type (Var), value and offset
# a value can be:
#  - a simple value, where the type is a subclass of Var other than StructPtr
#    and the value is is a value of the corresponding type
#  - a struct value, where the type is StructPtr and the value is a list of Val
#    object, one for each var of the corresponding StructDef
class Val(object):
    def __init__(self, typ, val):
        assert isinstance(typ, Var)
        self.typ = typ
        self.val = val

        # calculated
        self.offset = None   # offset of value within serialized data of top-level struct

        self.is_struct = False

class StructVal(Val):
    def __init__(self, typ, val):
        assert isinstance(typ, StructPtr)
        assert val is None or len(val) >= 0 # TODO: hacky way to check that val is a list. check the type instead
        #assert isinstance(val, types.List) ???
        self.typ = typ
        self.val = val

        self.struct_def = typ.struct_def
        self.offset = None
        self.is_struct = True

# primitive value is uses default value of types
def MakeValFromPrimitiveVar(var):
    assert isinstance(var, Var) and not isinstance(var, StructPtr)
    return Val(var, var.def_val)

# primitive value is a copy
def MakeValFromPrimitiveVal(val):
    assert isinstance(val, Val) and not isinstance(val, StructVal)
    return Val(val.typ, val.val)

def MakeVal(src):
    if isinstance(src, StructDef):
        return MakeValFromStructDef(src)
    if isinstance(src, StructPtr):
        return MakeValFromStructPtr(src)
    if isinstance(src, Var):
        return MakeValFromPrimitiveVar(src)
    if isinstance(src, StructVal):
        assert False, "MakeVal doesn't handle StructVal yet"
    if isinstance(src, Val):
        return MakeValFromPrimitiveVal(src)
    print(src)
    assert False, "src not supported by MakeVal()"

# for struct ptr, the type is struct_ptr and the value is a list of
# values
def MakeValFromStructPtr(struct_ptr):
    assert isinstance(struct_ptr, StructPtr)
    if struct_ptr.def_val == None:
        val = None
        return StructVal(struct_ptr, val)
    else:
        def_val = struct_ptr.def_val
        assert isinstance(def_val, StructVal)
        val = [MakeVal(v) for v in def_val.val]
        return StructVal(struct_ptr, val)

def MakeValFromStructDef(struct_def):
    assert isinstance(struct_def, StructDef)
    struct_ptr = StructPtr("dummyStructPtr", struct_def, None)
    return MakeValFromStructPtr(struct_ptr)

def MakeStruct(struct_def):
    val = [MakeVal(v) for v in struct_def.vars]
    struct_ptr = StructPtr("dummyStructPtr", struct_def, None)
    return StructVal(struct_ptr, val)

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
    assert isinstance(struct_def, StructDef)
    name = struct_def.name
    c_size = struct_def.c_size
    vars = struct_def.vars
    lines = ["struct %s {" % name]
    fmt = "    %%-%ds %%s;" % struct_def.max_type_len
    for var in struct_def.vars:
        lines += [fmt % (var.c_type, var.name)]
    lines += ["};\n"]

    lines += ["STATIC_ASSERT(sizeof(%(name)s)==%(c_size)d, %(name)s_is_%(c_size)d_bytes);\n" % locals()]

    fmt = "STATIC_ASSERT(offsetof(%(name)s, %(field_name)s) == %(offset)d, %(field_name)s_is_%(offset)d_bytes_in_%(name)s);"
    for var in vars:
        field_name = var.name
        offset = var.offset
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
    assert isinstance(struct_def, StructDef)
    name = struct_def.name
    lines = []
    lines += ["StructPointerInfo g%(name)sPointers[] = {" % locals()]
    for var in struct_def.get_struct_vars():
        offset = var.offset
        name = var.struct_def.name
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
    vars = struct_def.get_struct_vars()
    pointer_infos_count = len(vars)
    pointer_infos = "NULL"
    lines = []
    if len(vars) > 0:
        pointer_infos = "&g%sPointers[0]" % name
        lines += gen_struct_pointer_infos_one(struct_def)
    lines += ["StructDef g%(name)sStructDef = { %(size)d, %(pointer_infos_count)d, %(pointer_infos)s };\n" % locals()]
    return "\n".join(lines)

def gen_structs_metadata():
    return "\n".join([gen_struct_metadata(stru) for stru in GetAllStructs()])
