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
        return self.c_type_class

    def get_typ_enum(self):
        return self.type_enum

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

    def c_type(self):
        return self.typ.c_type()

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

    def get_max_field_type_len(self):
        max_len = 0
        for v in self.values:
            if len(v.c_type()) > max_len:
                max_len = len(v.c_type())
        return max_len + 3

    def is_valid_val(self, val):
        return issubclass(val, Struct)

    def name(self):
        return self.__class__.__name__

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

class PaddingSettings(Struct):
    fields =[
        ("top", U16(2)),
        ("bottom", U16(2)),
        ("left", U16(4)),
        ("right", U16(4)),
        ("spaceX", U16(4)),
        ("spaceY", U16(4))
    ]

class ForwardSearch(Struct):
    fields = [
        ("highlightOffset", I32(0)),
        ("highlightWidth", I32(15)),
        ("highlightPermanent", I32(0)),
        ("highlightColor", Color(0x6581FF)),
        ("enableTeXEnhancements", Bool(False)),
]

class RectInt(Struct):
    fields = [
        ("x", I32(0)),
        ("y", I32(0)),
        ("dx", I32(0)),
        ("dy", I32(0)),
    ]

class BasicSettings(Struct):
    fields = [
        ("globalPrefsOnly", Bool(False)),
        ("currLanguage", String(None)), # auto-detect
        ("toolbarVisible", Bool(True)),
        ("pdfAssociateDontAsk", Bool(False)),
        ("pdfAssociateDoIt", Bool(False)),
        ("checkForUpdates", Bool(True)),
        ("rememberMRUFiles", Bool(True)),
        # TODO: useSystemColorScheme obsolete by textColor/pageColor ?
        ("useSystemColorScheme", Bool(False)),
        ("inverseSearchCmdLine", String(None)),
        ("versionToSkip", String(None)),
        ("lastUpdateTime", String(None)),
        ("defaultDisplayMode", U16(0)),  # DM_AUTOMATIC
        # -1 == Fit Page
        ("defaultZoom", Float(-1)),
        ("windowState", I32(1)), # WIN_STATE_NORMAL
        ("windowPos", RectInt()),
        ("tocVisible", Bool(True)),
        ("favVisible", Bool(False)),
        ("sidebarDx", I32(0)),
        ("tocDy", I32(0)),
        ("showStartPage", Bool(True)),
        ("openCountWeek", I32(0)),
        ("lastPrefUpdate", U64(0)),
    ]

class AdvancedSettings(Struct):
    fields = [
        ("traditionalEbookUI", Bool(True)),
        ("escToExit", Bool(False)),
        # TODO: different for different document types? For example, ebook
        # really needs one just for itself
        ("textColor", Color(0x0)),      # black
        ("pageColor", Color(0xffffff)), # white
        ("mainWindowBackground", Color(0xFFF200)),
        ("pagePadding", PaddingSettings()),
        ("forwardSearch", ForwardSearch()),

        # TODO: just for testing
        ("s", String("Hello")),
        ("defaultZoom", Float(-1)),
        ("ws", WString("A wide string")),
    ]

# TODO: merge basic/advanced into one?
class Settings(Struct):
    fields = [
        ("basic", BasicSettings()),
        ("advanced", AdvancedSettings()),
        # TODO: just for testing
        #Array("intArray", I32, [I32("", 1), I32("", 3)]),
    ]

settings = Settings()
version = "2.3"
