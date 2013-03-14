import types

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
        # fields must be a class variable in Struct's subclass
        for field in self.fields:
            (name, typ) = field
            assert isinstance(name, type(""))
            assert isinstance(typ, Type)
        class_name = self.__class__.__name__
        self.c_type_override = "%s *" % class_name

    def is_valid_type(self, val):
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
