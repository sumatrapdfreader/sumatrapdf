import types

# Represents a variable: its type and default value
class Var(object):
    def __init__(self, name, c_type, typ_enum, def_val = None):
        self.name = name
        self.def_val = def_val

        self.c_type = c_type
        self.typ_enum = typ_enum

        self.struct_def = None
        self.offset = None        # offset of variable within the struct
        self.is_signed = False    # when an integer, is it signed or unsigned

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
        self.is_signed = True

class U32(Var):
    def __init__(self, name, def_val = 0):
        super(U32, self).__init__(name, "uint32_t", "TYPE_U32", def_val)

class U64(Var):
    def __init__(self, name, def_val = 0):
        super(U64, self).__init__(name, "uint64_t", "TYPE_U64", def_val)

class Color(Var):
    def __init__(self, name, def_val = 0):
        super(Color, self).__init__(name, "uint32_t", "TYPE_U32", def_val)

class String(Var):
    def __init__(self, name, def_val = ""):
        # we don't support None values for strings, we use ""
        # for empty strings
        if def_val == None: def_val = ""
        super(String, self).__init__(name, "const char *", "TYPE_STR", def_val)

class WString(Var):
    def __init__(self, name, def_val = ""):
        # we don't support None values for strings, we use ""
        # for empty strings
        if def_val == None: def_val = ""
        super(WString, self).__init__(name, "const WCHAR *", "TYPE_WSTR", def_val)

class Float(Var):
    def __init__(self, name, def_val = 0):
        def_val = str(def_val)
        super(Float, self).__init__(name, "float", "TYPE_FLOAT", def_val)

class Array(Var):
    def __init__(self, name, typ, def_val = []):
        # def_val is an array of values that are subclasses of Var
        # all values have to be of the same type
        assert isinstance(typ, type(types.ClassType))
        assert issubclass(typ, Var)
        assert is_valid_array(typ, def_val)
        super(Array, self).__init__(name, "int32_t", "TYPE_ARRAY", def_val)

# return True if all elements in the array are of the same typ
def is_valid_array(typ, arr):
    for el in arr:
        if not isinstance(el, typ):
            return False
    return True

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

# Structs for which we have already generated definition for .h file
g_structs_h_def_generated = []

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
        self.offset = None     # offset of value within serialized data of top-level struct
        self.serialized = None # this value serialized as bytes
        self.is_struct = False

class StructVal(Val):
    def __init__(self, typ, val):
        assert isinstance(typ, StructPtr)
        assert val is None or len(val) >= 0 # TODO: hacky way to check that val is a list. check the type instead
        #assert isinstance(val, types.List) ???
        self.typ = typ
        self.val = val

        self.struct_def = typ.struct_def

        # calculated
        self.offset = None     # offset of value within serialized data of top-level struct
        self.serialized = None # this value serialized as bytes
        self.is_struct = True

        self.referenced = None

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
        return src
    if isinstance(src, Val):
        return MakeValFromPrimitiveVal(src)
    print(src)
    assert False, "src not supported by MakeVal()"

# for struct ptr, the type is struct_ptr and the value is a list of
# values
def MakeValFromStructPtr(struct_ptr):
    assert isinstance(struct_ptr, StructPtr)
    if struct_ptr.def_val == None:
        return StructVal(struct_ptr, None)
    else:
        # TODO: this is suspect, should probably make a deep copy
        # of def_val and store that as a val
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
    global g_structs_h_def_generated

    assert isinstance(struct_def, StructDef)
    if struct_def in g_structs_h_def_generated:
        return []
    assert struct_def in GetAllStructs()
    name = struct_def.name
    lines = ["struct %s {" % name]
    fmt = "    %%-%ds %%s;" % struct_def.max_type_len
    for var in struct_def.vars:
        lines += [fmt % (var.c_type, var.name)]
    lines += ["};\n"]
    return "\n".join(lines)

prototypes_tmpl = """#define %(name)sVersion "%(version_str)s"

%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut);
uint8_t *Serialize%(name)s(%(name)s *, int *dataLenOut);
void Free%(name)s(%(name)s *);
"""
def gen_struct_defs(vals, version_str):
    top_level = vals[-1]
    assert isinstance(top_level, StructVal)
    name = top_level.struct_def.name
    struct_defs = [val.struct_def for val in vals]
    lines = [gen_h_struct_def(stru) for stru in struct_defs]
    lines += [prototypes_tmpl % locals()]
    return "\n".join(lines)

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
            lines += ["    { %(typ_enum)s, %(offset)s, &g%(field_type)sMetadata }," % locals()]
        else:
            lines += ["    { %(typ_enum)s, %(offset)s, NULL }," % locals()]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata(vals):
    lines = []
    for val in vals:
        struct_def = val.struct_def
        struct_name = struct_def.name
        nFields = len(struct_def.vars)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields(struct_def)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

top_level_funcs_tmpl = """
%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen, bool *usedDefaultOut)
{
    void *res = NULL;
    res = Deserialize(data, dataLen, %(name)sVersion, &g%(name)sMetadata);
    if (res) {
        *usedDefaultOut = false;
        return (%(name)s*)res;
    }
    res = Deserialize(&g%(name)sDefault[0], sizeof(g%(name)sDefault), %(name)sVersion, &g%(name)sMetadata);
    CrashAlwaysIf(!res);
    *usedDefaultOut = true;
    return (%(name)s*)res;
}

uint8_t *Serialize%(name)s(%(name)s *val, int *dataLenOut)
{
    return Serialize((const uint8_t*)val, %(name)sVersion, &g%(name)sMetadata, dataLenOut);
}

void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val, &g%(name)sMetadata);
}

"""

def gen_top_level_funcs(vals):
    top_level = vals[-1]
    assert isinstance(top_level, StructVal)
    name = top_level.struct_def.name
    return top_level_funcs_tmpl % locals()
