
class Struct(object):
	def __init__(self, name, fields):
		self.name = name
		self.fields = fields

g_struct_type = type(Struct(None, None))

def is_struct_type(typ): return type(typ) == g_struct_type

def typ_to_c_type(typ):
	types_map = {
		"bool"  : "bool",
		"u16"   : "uint16_t",
		"i32"    : "int32_t",
		"u32"    : "uint32_t",
		"color"  : "uint32_t"
	}
	if typ in types_map:
		return types_map[typ]
	if is_struct_type(typ): return "Ptr<%s>" % typ.name
	assert False, "Unknown typ" + str(typ)

class Field(object):
	types = ["bool", "color", "u16", "i32", "u32"]

	def __init__(self, name, typ, def_val):
		assert typ in Field.types or is_struct_type(typ)
		self.name = name
		self.typ = typ
		self.def_val = def_val

	def is_struct(self): return is_struct_type(self.typ)

	def c_type(self): return typ_to_c_type(self.typ)
