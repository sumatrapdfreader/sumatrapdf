
class Struct(object):
	def __init__(self, name, fields):
		self.name = name
		self.fields = fields

def is_struct_type(typ): return type(typ) == type(Struct(None, None))

class TypeDef(object):
	def __init__(self, c_name, c_size, pack_format=""):
		self.c_name = c_name
		self.c_size = c_size
		self.pack_format = pack_format

# TODO: add pack_format
g_types_map = {
	"bool"   : TypeDef("bool", 4),
	"u16"    : TypeDef("uint16_t", 2),
	"i32"    : TypeDef("int32_t", 4),
	"u32"    : TypeDef("uint32_t", 4),
	"color"  : TypeDef("uint32_t", 4),
}

def verify_type(typ):
	assert is_struct_type(typ) or typ in g_types_map, "Unknown type %s" % str(typ)

def get_typ_c_name(typ):
	verify_type(typ)
	if typ in g_types_map:
		return g_types_map[typ].c_name
	if is_struct_type(typ): return "Ptr<%s>" % typ.name

def get_typ_c_size(typ):
	verify_type(typ)
	if typ in g_types_map:
		return g_types_map[typ].c_size
	if is_struct_type(typ): return 8

def get_pack_format(typ):
	verify_type(typ)
	if typ in g_types_map:
		return g_types_map[typ].pack_format
	if is_struct_type(typ): return "" # TODO: pack format for pointers

class Field(object):
	def __init__(self, name, typ, def_val):
		verify_type(typ)
		self.name = name
		self.typ = typ
		self.def_val = def_val

	def is_struct(self):   return is_struct_type(self.typ)
	def c_type(self):      return get_typ_c_name(self.typ)
	def c_size(self):      return get_typ_c_size(self.typ)
	def pack_format(self): return get_pack_format(self.typ)
