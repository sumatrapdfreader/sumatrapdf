import types

def is_valid_signed(bits, val):
    if type(val) not in (types.IntType, types.LongType): return False
    e = bits - 1
    min_val = -(2 ** e)
    if val < min_val: return False
    max_val = (2 ** e) - 1
    if val > max_val: return False
    return True

def is_valid_unsigned(bits, val):
    if type(val) not in (types.IntType, types.LongType): return False
    if val < 0: return False
    if val > 2 ** bits: return False
    return True

def is_valid_string(val):
    if val is None: return True
    return type(val) in (types.StringType, types.UnicodeType)

class Type(object):
    def __init__(self, def_val):
        self.c_type_override = None
        self.set_val(def_val)

    def set_val(self, val):
        assert self.is_valid_val(val), "%s is not a valid value of %s" % (str(self.val), str(self))
        self.val = val

    def c_type(self):
        if self.c_type_override is not None:
            return self.c_type_override
        return self.c_type_class

    def get_type_typ_enum(self):
        return self.type_enum

    def is_struct(self):
        return isinstance(self, Struct)

    def is_array(self):
        return isinstance(self, Array)

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

    def __init__(self, def_val=0):
        super(I32, self).__init__(def_val)

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
    type_enum = "TYPE_COLOR"

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

    def __init__(self, *vals):
        # fields must be a class variable in Struct's subclass
        self.values = [Field(f.name, f.typ, f.flags) for f in self.fields]
        self.c_type_override = "%s *" % self.name()
        self.offset = None
        for i in range(len(vals)):
            self.values[i].set_val(vals[i])

    def is_valid_val(self, val):
        return issubclass(val, Struct)

    def name(self):
        return self.__class__.__name__

    def as_str(self):
        s = str(self) + "\n"
        for v in self.values:
            if isinstance(v, Field):
                s += "%s: %s\n" % (v.name, str(v.val))
        return s

    def __setattr__(self, name, value):
        # special-case self.values, which we refer to
        if name == "values":
            object.__setattr__(self, name, value)
            return

        for field in self.values:
            if field.name == name:
                field.set_val(value)
                return
        object.__setattr__(self, name, value)

class Array(Type):
    c_type_class = ""
    type_enum = "TYPE_ARRAY"

    def __init__(self, typ, values):
        # TODO: we don't support arrays of primitve values, just structs
        assert issubclass(typ, Struct)
        self.typ = typ
        self.values = values
        for v in values:
            assert self.is_valid_val(v)
        self.c_type_override = "Vec<%s*> *" % typ.__name__
        self.offset = None

    def is_valid_val(self, val):
        return isinstance(val, self.typ)

    def name(self):
        try:
            return self.typ.__name__
        except:
            print(self.typ)
            raise

# those are bit flags
NoStore = 1
Compact = 2

class Field(object):
    def __init__(self, name, typ_val, flags=0):
        self.name = name
        self.typ = typ_val
        self.flags = flags

        if self.is_no_store(): assert not self.is_compact()
        if self.is_compact():
            to_test = typ_val
            if typ_val.is_array():
                to_test = typ_val.typ
            else:
                assert to_test.is_struct()
            for field in to_test.fields:
                assert not field.is_struct()

        if typ_val.is_struct():
            # TODO: support nullptr values for the struct, represented by using
            # class for typ_val
            self.val = typ_val
        elif typ_val.is_array():
            self.val = typ_val
        else:
            self.val = typ_val.val

    def c_type(self):
        return self.typ.c_type()

    def is_struct(self):
        return self.typ.is_struct()

    def is_signed(self):
        return type(self.typ) == I32

    def is_unsigned(self):
        return type(self.typ) in (Bool, U16, U32, U64, Color)

    def is_bool(self):
        return type(self.typ) == Bool

    def is_color(self):
        return type(self.typ) == Color

    def is_string(self):
        return type(self.typ) in (String, WString)

    def is_float(self):
        return type(self.typ) == Float

    def is_no_store(self):
        return self.flags & NoStore == NoStore

    def is_compact(self):
        return self.flags & Compact == Compact

    def is_array(self):
        return type(self.typ) == Array

    def set_val(self, val):
        # Note: we don't support this for struct or arrays
        assert not (self.is_struct() or self.is_array())
        assert self.typ.is_valid_val(val)
        self.val = val

    def get_typ_enum(self, for_bin=False):
        type_enum = self.typ.get_type_typ_enum()
        # binary doesn't have a notion of compact storage
        is_compact = self.is_compact() and not for_bin
        if self.is_no_store() or is_compact:
            s = "(Type)(" + type_enum
            if self.is_no_store():
                s = s + " | TYPE_NO_STORE_MASK"
            if self.is_compact():
                s = s + " | TYPE_STORE_COMPACT_MASK"
            return s + ")"
        return type_enum
