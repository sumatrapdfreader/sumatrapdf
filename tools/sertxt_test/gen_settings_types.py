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

def field_from_def(field_def):
    flags = 0
    assert len(field_def) >= 2 and len(field_def) <= 3
    if len(field_def) > 2:
        (name, typ_val, flags) = field_def
    else:
        (name, typ_val) = field_def

    assert isinstance(name, type(""))
    assert isinstance(typ_val, Type)
    return Field(name, typ_val, flags)

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
        self.values = [field_from_def(fd) for fd in self.fields]

        cls = self.__class__
        self.c_type_override = "%s *" % cls.__name__

        self.offset = None

    @classmethod
    def get_max_field_type_len(cls):
        max_len = 0
        for field_def in cls.fields:
            def_val = field_def[1]
            c_type = def_val.c_type()
            if len(c_type) > max_len:
                max_len = len(c_type)
        return max_len + 3

    """
    @classmethod
    def get_max_field_enum_len(cls):
        max_len = 0
        for field_def in cls.fields:
            def_val = field_def[1]
            max_len = max(max_len, len(def_val.get_typ_enum()))
        return max_len + 1
"""

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
                obj = str(self)
                obj = obj.replace("<gen_settings_types.", "")
                obj = obj.replace("object at" , "@")
                #print("on %s setting '%s' to '%s'" % (obj, name, value))
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
            try:
                assert self.is_valid_val(v)
            except:
                print(v)
                print(self.typ)
                raise

        self.c_type_override = "sertxt::ListNode<%s> *" % typ.__name__

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

class Field(object):
    def __init__(self, name, typ_val, flags):
        self.name = name
        self.typ = typ_val
        self.flags = flags

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

    def is_array(self):
        return type(self.typ) == Array

    def set_val(self, val):
        # Note: we don't support this for struct or arrays
        assert not (self.is_struct() or self.is_array())
        assert self.typ.is_valid_val(val)
        self.val = val

    def get_typ_enum(self):
        type_enum = self.typ.get_type_typ_enum()
        if self.is_no_store():
            return "(Type)(%s | TYPE_NO_STORE_MASK)" % type_enum
        return type_enum

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
        ("enableTexEnhancements", Bool(False)),
]

class RectInt(Struct):
    fields = [
        ("x", I32(0)),
        ("y", I32(0)),
        ("dx", I32(0)),
        ("dy", I32(0)),
    ]

class Fav(Struct):
    fields = [
        ("name", String(None)),
        ("pageNo", I32(0)),
        ("pageLabel", String(None)),
        ("menuId", I32(0), NoStore),
    ]

class BasicSettings(Struct):
    fields = [
        ("globalPrefsOnly", Bool(False)),
        ("currLanguage", String(None)), # auto-detect
        ("toolbarVisible", Bool(True)),
        ("pdfAssociateDontAsk", Bool(False)),
        ("pdfAssociateDoIt", Bool(False)),
        ("checkForUpdates", Bool(True)),
        ("rememberMruFiles", Bool(True)),
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
        ("ws", WString("A wide string")),
    ]

fav1 = Fav()
fav1.name = "my first fav"
fav1.pageNo = 22
fav1.pageLabel = "my label for first fav"

fav2 = Fav()
fav2.name = "my second fav"
fav2.pageNo = 13
fav2.pageLabel = "my label for second fav"

fav3 = Fav()
fav3.name = "third fav"
fav3.pageNo = 3
fav3.pageLabel = "my label for third fav"

class AppState(Struct):
    fields = [
        ("favorites", Array(Fav, [fav1, fav2, fav3]))
    ]

# TODO: merge basic/advanced into one?
class Settings(Struct):
    fields = [
        ("basic", BasicSettings()),
        ("advanced", AdvancedSettings()),
        ("appState", AppState()),
    ]

settings = Settings()
