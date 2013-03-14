import types
from util import gob_varint_encode, gob_uvarint_encode

def is_valid_signed(bits, val):
    if type(val) not in (types.IntType, types.LongType): return False
    e = bits - 1
    min_val = -(2**e)
    if val < min_val: return False
    max_val = (2**e)-1
    if val > max_val: return False
    return True

def is_valid_unsigned(bits, val):
    if type(val) not in (types.IntType, types.LongType): return False
    if val < 0: return False
    if val > 2**bits: return False
    return True

def is_valid_string(val):
    if val == None: return True
    return type(val) in (types.StringType, types.UnicodeType)

class Type(object):
    def __init__(self, def_val):
        self.c_type_override = None
        self.val = def_val
        assert self.is_valid_val(def_val), "%s is not a valid value of %s" % (str(val), str(self))

    def c_type(self):
        if self.c_type_override != None:
            return self.c_type_override
        return c_type_class

    def is_struct(self):
        return isinstance(self, Struct)

class Bool(Type):
    c_type_class = "bool"
    type_enum = "TYPE_BOOL"

    def __init__(self, def_val):
        super(Bool, self).__init__(def_val)

    def is_valid_val(self, val):
        return val in (True, False)

class U16(Type):
    c_type_class = "uint16_t"
    type_enum = "TYPE_U16"

    def is_valid_val(self, val):
        return is_valid_unsigned(16, val)

class I32(Type):
    c_type_class = "int32_t"
    type_enum = "TYPE_I32"

    def is_valid_val(self, val):
        return is_valid_signed(32, val)

class U32(Type):
    c_type_class = "uint32_t"
    type_enum = "TYPE_U32"

    def is_valid_val(self, val):
        return is_valid_unsigned(32, val)

class U64(Type):
    c_type_class = "uint64_t"
    type_enum = "TYPE_U64"

    def is_valid_val(self, val):
        return is_valid_unsigned(64, val)

# behaves like uint32_t, using unique name to signal intent
class Color(U32):
    pass

class String(Type):
    c_type_class = "const char *"
    type_enum = "TYPE_STR"

    def is_valid_val(self, val):
        return is_valid_string(val)

class WString(Type):
    c_type_class = "const WCHAR *"
    type_enum = "TYPE_WSTR"

    def is_valid_val(self, val):
        return is_valid_string(val)

class Float(Type):
    c_type_class = "float"
    type_enum = "TYPE_FLOAT"

    def is_valid_val(self, val):
        return type(val) in (types.IntType, types.LongType, types.FloatType)

# When generating C struct definitions we need the structs
# in the right order (if Bar refers to Foo, Foo must be defined first).
# This is a list of all structs in the right order
g_all_structs = []

def serialize_string(val):
    # empty strings are encoded as 0 (0 length)
    # non-empty strings are encoded as uvariant(len(s)+1)
    # (+1 for terminating 0), followed by string data (including terminating 0)
    if val == None:
        data = gob_uvarint_encode(0)
    else:
        data = gob_uvarint_encode(len(val)+1)
        data = data + val + chr(0)
    return data

class Field(object):
    def __init__(self, name, typ_val):
        self.name = name
        self.typ = typ_val
        if typ_val.is_struct():
            # TODO: support NULL values for the struct, represented by using
            # class for typ_val
            self.val = typ_val
        else:
            self.val = typ_val.val
        self._serialized = None

    def is_struct(self):
        return self.typ.is_struct()

    def is_signed(self):
        return type(self.typ) == I32

    def is_unsigned(self):
        return type(self.typ) in (Bool, U16, U32, U64, Color)

    def is_string(self):
        return type(self.typ) in (String, WString)

    def is_float(self):
        return type(self.typ) == Float

    def _serialize(self):
        if self.is_signed():
            return gob_varint_encode(long(self.val))
        if self.is_unsigned():
            return gob_uvarint_encode(long(self.val))
        if self.is_string():
            return serialize_string(self.val)
        # floats are serialied as strings
        if self.is_float():
            return serialize_string(str(self.val))
        if self.is_struct():
            off = self.val.offset
            assert type(off) in (types.IntType, types.LongType)
            return gob_uvarint_encode(off)
        assert False, "don't know how to serialize %s" % str(self.typ)

    def serialized(self):
        if self._serialized == None:
            self._serialized = self._serialize()
        return self._serialized

# struct is just a base class
# subclasses should have class instance fields which is a list of tuples:
# defining name and type of the struct members:
# fields = [ ("boolField", Bool(True), ("u32Field", U32(32))]
#
# TODO: implement struct inheritance i.e. a subclass should inherit all
# fields from its parent
class Struct(Type):
    c_type_class = ""
    type_enum = "TYPE_STRUCT_PTR"
    fields = []

    def __init__(self):
        global g_all_structs

        self.values = []
        # fields must be a class variable in Struct's subclass
        for field in self.fields:
            (name, typ_val) = field

            assert isinstance(name, type(""))
            assert isinstance(typ_val, Type)
            self.values.append(Field(name, typ_val))

        cls = self.__class__
        if cls not in g_all_structs:
            g_all_structs.append(cls)
        self.c_type_override = "%s *" % cls.__name__

        self.offset = None

    def is_valid_val(self, val):
        return issubclass(val, Struct)

# TODO: need to supprt more than one C struct field from a single
# python Type
class Array(Type):
    c_type_class = "int32_t"
    type_enum = "TYPE_ARRAY"

    def __init__(self, typ):
        assert isinstance(typ, Type)
        assert issubclass(typ, Var)
        self.typ = typ

def serialize(typ, val):
    assert isinstance(typ, Type)
    if isinstance(typ, Bool):
        # TODO: actually serialize the values
        return ""
    assert False, "Unkown typ: %s" % str(typ)

def GetAllStructs(): return g_all_structs

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
