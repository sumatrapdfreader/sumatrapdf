# Represents a variable: its type and default value
class Var(object):
    def __init__(self, name, c_type, typ_enum, def_val = None):
        self.name = name
        self.def_val = def_val

        self.c_type = c_type
        self.typ_enum = typ_enum

        self.struct_def = None
        self.offset = None        # offset of variable within the struct

    def is_struct(self):   return self.struct_def != None

class Bool(Var):
    def __init__(self, name, def_val = False):
        super(Bool, self).__init__(name, "bool", "TYPE_BOOL", def_val)

class U16(Var):
    def __init__(self, name, def_val = 0):
        super(U16, self).__init__(name, "uint16_t", "TYPE_U16", def_val)

class I32(Var):
    def __init__(self, name, def_val = 0):
        super(I32, self).__init__(name, "int32_t", "TYPE_I32", def_val)

class U32(Var):
    def __init__(self, name, def_val = 0):
        super(U32, self).__init__(name, "uint32_t", "TYPE_U32", def_val)

class Color(Var):
    def __init__(self, name, def_val = 0):
        super(Color, self).__init__(name, "uint32_t", "TYPE_U32", def_val)

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

def Version(ver_str):
    ver = ver_from_string(ver_str)
    if ver_str == "2.3": assert ver == 0x02030000
    return U32("version", ver)

class StructPtr(Var):
    def __init__(self, name, struct_def, def_val=None):
        assert isinstance(struct_def, StructDef)
        if def_val != None:
            assert isinstance(def_val, StructVal)
            assert isinstance(def_val.struct_def, StructDef)
        c_type = "%s *" % struct_def.name
        typ_enum = "TYPE_STRUCT_PTR"
        super(StructPtr, self).__init__(name, c_type, typ_enum, def_val)
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
            # special case: if this and parent both have version at the beginning
            # replace version of the parent with version of this
            if parent.vars[0].name == "version":
                assert vars[0].name == "version"
                self.vars = [vars[0]] + parent.vars[1:] + vars[1:]
            else:
                assert vars[0].name != "version"
                self.vars = parent.vars + vars

        self.build_def()

    # calculate:
    #   self.c_size (StructPtr needs to know that)
    #   self.max_type_len (for better formatting of generated C code)
    def build_def(self):
        self.max_type_len = 0
        for var in self.vars:
            self.max_type_len = max(self.max_type_len, len(var.c_type))

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
...
"""
def gen_h_struct_def(struct_def):
    assert isinstance(struct_def, StructDef)
    name = struct_def.name
    vars = struct_def.vars
    lines = ["struct %s {" % name]
    fmt = "    %%-%ds %%s;" % struct_def.max_type_len
    for var in struct_def.vars:
        lines += [fmt % (var.c_type, var.name)]
    lines += ["};\n"]
    return "\n".join(lines)

def gen_struct_defs():
    return "\n".join([gen_h_struct_def(stru) for stru in GetAllStructs()])

"""
FieldMetadata g${name}FieldMetadata[] = {
    { $type, $offset, &g${name}StructMetadata },
};
"""
def gen_struct_fields(struct_def):
    assert isinstance(struct_def, StructDef)
    struct_name = struct_def.name
    lines = ["FieldMetadata g%(struct_name)sFieldMetadata[] = {" % locals()]
    for field in struct_def.vars:
        assert isinstance(field, Var)
        typ_enum = field.typ_enum
        field_name = field.name
        offset = "offsetof(%(struct_name)s, %(field_name)s)" % locals()
        if field.is_struct():
            field_type = field.struct_def.name
            lines += ["    { %(typ_enum)s, %(offset)s, &g%(field_type)sStructMetadata }," % locals()]
        else:
            lines += ["    { %(typ_enum)s, %(offset)s, NULL }," % locals()]            
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $nFields, $fields };
"""
def gen_struct_metadata(struct_def):
    struct_name = struct_def.name
    nFields = len(struct_def.vars)
    fields = "&g%sFieldMetadata[0]" % struct_name
    lines = gen_struct_fields(struct_def)
    lines += ["StructMetadata g%(struct_name)sStructMetadata = { %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

def gen_structs_metadata():
    return "\n".join([gen_struct_metadata(stru) for stru in GetAllStructs()])
