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
        self.set_val(def_val)

    def set_val(self, val):
        assert self.is_valid_val(val), "%s is not a valid value of %s" % (str(self.val), str(self))
        self.val = val

    def c_type(self):
        if self.c_type_override != None:
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
        self.c_type_override = "ListNode<%s> *" % typ.__name__
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
            assert typ_val.is_struct()
            for field in typ_val.fields:
                assert not field.is_struct()

        if typ_val.is_struct():
            # TODO: support NULL values for the struct, represented by using
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

class PaddingSettings(Struct):
    fields =[
        Field("top", U16(2)),
        Field("bottom", U16(2)),
        Field("left", U16(4)),
        Field("right", U16(4)),
        Field("spaceX", U16(4)),
        Field("spaceY", U16(4))
    ]

class ForwardSearch(Struct):
    fields = [
        Field("highlightOffset", I32(0)),
        Field("highlightWidth", I32(15)),
        Field("highlightPermanent", I32(0)),
        Field("highlightColor", Color(0x6581FF)),
        Field("enableTexEnhancements", Bool(False)),
]

class RectInt(Struct):
    fields = [
        Field("x", I32(0)),
        Field("y", I32(0)),
        Field("dx", I32(0)),
        Field("dy", I32(0)),
    ]

class Fav(Struct):
    fields = [
        Field("name", String(None)),
        Field("pageNo", I32(0)),
        Field("pageLabel", String(None)),
        Field("menuId", I32(0), NoStore),
    ]

class BasicSettings(Struct):
    fields = [
        Field("globalPrefsOnly", Bool(False)),
        Field("currLanguage", String(None)), # auto-detect
        Field("toolbarVisible", Bool(True)),
        Field("pdfAssociateDontAsk", Bool(False)),
        Field("pdfAssociateDoIt", Bool(False)),
        Field("checkForUpdates", Bool(True)),
        Field("rememberMruFiles", Bool(True)),
        # TODO: useSystemColorScheme obsolete by textColor/pageColor ?
        Field("useSystemColorScheme", Bool(False)),
        Field("inverseSearchCmdLine", WString(None)),
        Field("versionToSkip", String(None)),
        Field("lastUpdateTime", String(None)),
        Field("defaultDisplayMode", U16(0)),  # DM_AUTOMATIC
        # -1 == Fit Page
        Field("defaultZoom", Float(-1)),
        Field("windowState", I32(1)), # WIN_STATE_NORMAL
        Field("windowPos", RectInt(), Compact),
        Field("tocVisible", Bool(True)),
        Field("favVisible", Bool(False)),
        Field("sidebarDx", I32(0)),
        Field("tocDy", I32(0)),
        Field("showStartPage", Bool(True)),
        Field("openCountWeek", I32(0)),
        Field("lastPrefUpdate", U64(0)),
    ]

class AdvancedSettings(Struct):
    fields = [
        Field("traditionalEbookUI", Bool(True)),
        Field("escToExit", Bool(False)),
        # TODO: different for different document types? For example, ebook
        # really needs one just for itself
        Field("textColor", Color(0x0)),      # black
        Field("pageColor", Color(0xffffff)), # white
        Field("mainWindowBackground", Color(0xFFF200)),
        Field("pagePadding", PaddingSettings()),
        Field("forwardSearch", ForwardSearch()),
    ]

fav1 = Fav("my first fav", 22,  "my label for first fav")
fav2 = Fav("my second fav", 13, "my label for second fav")

class AppState(Struct):
    fields = [
        Field("favorites", Array(Fav, [fav1, fav2]))
    ]

g_escape_test_str =  "[lo\r $fo\to\\ l\na]]"
# TODO: merge basic/advanced into one?
class Settings(Struct):
    fields = [
        Field("basic", BasicSettings()),
        Field("advanced", AdvancedSettings()),
        Field("appState", AppState()),
        Field("str_escape_test", String(g_escape_test_str)),
        Field("wstr_1", WString(u"wide string Πραγματικό &Μέγεθος\tCtrl+1")),
    ]

class Simple(Struct):
    fields = [
        Field("bTrue", Bool(True)),
        Field("bFalse", Bool(False)),
        Field("u16_1", U16(1)),
        Field("i32_1", I32(-12)),
        Field("u32_1", U32(89)),
        Field("u64_1", U64(123)),
        Field("col_1", Color(0xacff00ed)),
        Field("float_1", Float(3.12348)),
        Field("str_1", String("lola")),
        Field("str_escape", String(g_escape_test_str)),
        Field("wstr_1", WString(u"wide string Πραγματικό &Μέγεθος\nCtrl+1")),
    ]

class ForDefaultTesting(Struct):
    fields = [
        Field("bFalse", Bool(False)),
    ]

