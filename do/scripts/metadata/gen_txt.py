import sys, os, codecs, random, metadata
from metadata import Field

sys.path.append("scripts")  # assumes is being run as ./scrpts/metadata/gen_txt.py
import util

"""
TODO:
  - test perf on a large document i.e. ~ 1 MB. Randomly generate a big
    file and see how fast is SerializeTxtParser.cpp on that file
  - add a way to pass Allocator to Serialize/Deserialize

Todo maybe: arrays of basic types
"""

# kind of ugly that those are globals

# character we use for escaping strings
g_escape_char = "$"

# if true, will add whitespace at the end of the string, just for testing
g_add_whitespace = False

# if True, adds per-object reflection info. Not really working
g_with_reflection = False

def to_win_newlines(s):
    #s = s.replace("\r\n", "\n")
    #s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))

def write_to_file_utf8_bom(file_path, s):
    with codecs.open(file_path, "w", "utf-8-sig") as fo:
        fo.write(to_win_newlines(s))

# fooBar => foo_bar
def name2name(s):
    if s is None:
        return None
    res = ""
    n_upper = 0
    for c in s:
        if c.isupper():
            if n_upper == 0:
                res += "_"
            n_upper += 1
            res += c.lower()
        else:
            if n_upper > 1:
                res += "_"
            res += c
            n_upper = 0
    return res

def prefix_str(indent): return "  " * indent

def field_val_as_str(field):
    assert isinstance(field, Field)
    if field.is_bool():
        if field.val:
            return "true"
        else:
            return "false"
    if field.is_color():
        if field.val > 0xffffff:
            return "#%08x" % field.val
        else:
            return "#%06x" % field.val
    if field.is_signed() or field.is_unsigned() or field.is_float():
        return str(field.val)
    if field.is_string():
        return field.val
    assert False, "don't know how to serialize %s" % str(field.typ)

def _field_def_val_for_FieldMetada(field):
    if field.is_struct() or field.is_array():
        return "&g%sMetadata" % field.typ.name()
    if field.is_bool():
        return ["0", "1"][field.val]
    if field.is_color():
        if field.val > 0xffffff:
            return "0x%08x" % field.val
        else:
            return "0x%06x" % field.val
    # Note: doesn't handle default values outside of uintptr_t range,
    # which is 32bits on 32-bit arch
    if field.is_signed():
        assert metadata.is_valid_signed(32, field.val)
        return str(field.val)
    if field.is_unsigned():
        assert metadata.is_valid_unsigned(32, field.val)
        return str(field.val)
    # TODO: too lazy to do proper utf8 conversion and escaping, so
    # just returning nullptr. We use non-null only for testing
    if field.is_string():
        return "nullptr"
    if field.is_float():
        return '"%s"' % str(field.val)
    assert False, "don't know how to serialize %s" % str(field.typ)

def field_def_val_for_FieldMetada(field):
    s = _field_def_val_for_FieldMetada(field)
    s = "(uintptr_t)" + s
    return s

def escape_char(c):
    if c == g_escape_char:
        return c + c
    if c in "[]":
        return g_escape_char + c
    if c == '\r':
        return g_escape_char + 'r'
    if c == '\n':
        return g_escape_char + 'n'
    return c

def escape_str(s):
    if 0 == g_escape_char: return s
    res = ""
    for c in s:
        res += escape_char(c)
    return res

def ser_field(field, lines, indent):
    val_str = field_val_as_str(field)
    # omit serializing empty strings
    if val_str is None: return
    val_str = escape_str(val_str)
    var_name = name2name(field.name)
    prefix = prefix_str(indent)
    lines += ["%s%s: %s" % (prefix, var_name, val_str)]

def ser_array(field, lines, indent):
    assert field.is_array()

    n = len(field.val.values)
    if 0 == n: return

    prefix = prefix_str(indent)
    var_name = name2name(field.name)
    lines += ["%s%s [" % (prefix, var_name)]
    prefix += "  "
    for val in field.val.values:
        #print(str(val))
        #print(val.as_str())
        lines += [prefix + "["]
        ser_struct(val, None, lines, indent + 1)
        lines += [prefix + "]"]
    prefix = prefix[:-2]
    lines += ["%s]" % prefix]

def ser_struct_compact(struct, name, lines, indent):
    assert struct.is_struct()
    vals = []
    for field in struct.values:
        val_str = escape_str(field_val_as_str(field))
        assert " " not in val_str
        vals += [val_str]
    prefix = prefix_str(indent)
    lines += [prefix + name2name(name) + ": " + " ".join(vals)]

def ser_struct(struct, name, lines, indent):
    assert struct.is_struct()
    prefix = prefix_str(indent)

    #print("name: %s, indent: %d, prefix: '%s'" % (name, indent, prefix))

    if name is not None:
        lines += ["%s%s [" % (prefix, name2name(name))]

    for field in struct.values:
        if field.is_no_store():
            continue

        if field.is_array():
            ser_array(field, lines, indent + 1)
            continue

        if field.is_compact():
            ser_struct_compact(field.val, field.name, lines, indent + 1)
            continue

        if field.is_struct():
            if field.val.offset == 0:
                if False:  # Note: this omits empty values
                    lines += ["%s%s: " % (prefix, name2name(field.name))]
                continue
            ser_struct(field.val, field.name, lines, indent + 1)
            continue

        ser_field(field, lines, indent + 1)

    if name is not None:
        lines += ["%s]" % prefix]

def gen_struct_def(stru_cls):
    name = stru_cls.__name__
    lines = ["struct %s {" % name]
    rows = [[field.c_type(), field.name] for field in stru_cls.fields]
    if g_with_reflection:
        rows = [["const StructMetadata *", "def"]] + rows
    lines += ["    %s  %s;" % (e1, e2) for (e1, e2) in util.fmt_rows(rows, [util.FMT_RIGHT])]
    lines += ["};\n"]
    return "\n".join(lines)

def gen_struct_defs(structs):
    return "\n".join([gen_struct_def(stru) for stru in structs])

prototypes_tmpl = """extern const StructMetadata g%(name)sMetadata;

inline %(name)s *Deserialize%(name)s(char *data, size_t dataLen)
{
    auto s = std::string_view(data, dataLen);
    return (%(name)s*)Deserialize(s, &g%(name)sMetadata);
}

inline %(name)s *Deserialize%(name)s(TxtNode* root)
{
    return (%(name)s*)Deserialize(root, &g%(name)sMetadata);
}

inline std::string_view Serialize%(name)s(%(name)s *val)
{
    return Serialize((const uint8_t*)val, &g%(name)sMetadata);
}

inline void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val, &g%(name)sMetadata);
}
"""

top_level_funcs_txt_tmpl = """
"""

h_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_txt.py !!!!

using namespace sertxt;

%(struct_defs)s
%(prototypes)s
"""

cpp_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_txt.py !!!!

#include "utils/BaseUtil.h"
#include "utils/SerializeTxt.h"
#include "%(file_name)s.h"

using namespace sertxt;

#define of offsetof
%(structs_metadata)s
#undef of
%(top_level_funcs)s
"""

def gen_prototypes(stru_cls):
    name = stru_cls.__name__
    return prototypes_tmpl % locals()


"""
const FieldMetadata g${name}FieldMetadata[] = {
    { $offset, $type, &g${name}StructMetadata },
};
"""
def gen_struct_fields_txt(stru_cls):
    struct_name = stru_cls.__name__
    lines = ["const FieldMetadata g%sFieldMetadata[] = {" % struct_name]
    rows = []
    for field in stru_cls.fields:
        assert isinstance(field, Field)
        typ_enum = field.get_typ_enum()
        offset = "of(%s, %s)" % (struct_name, field.name)
        stru_cls.field_names.add(name2name(field.name))
        val = field_def_val_for_FieldMetada(field)
        col = [offset + ",", typ_enum + ",", val]
        rows.append(col)
    rows = util.fmt_rows(rows, [util.FMT_RIGHT, util.FMT_RIGHT, util.FMT_RIGHT])
    lines += ["    { %s %s %s }," % (e1, e2, e3) for (e1, e2, e3) in rows]
    lines += ["};\n"]
    return lines

"""
const StructMetadata g${name}StructMetadata = {
    $size,
    $nFields,
    $fieldNames,
    $fields
};
"""
def gen_structs_metadata_txt(structs):
    lines = []
    for stru_cls in structs:
        stru_cls.field_names = util.SeqStrings()
        struct_name = stru_cls.__name__
        nFields = len(stru_cls.fields)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_txt(stru_cls)
        field_names = stru_cls.field_names.get_all_c_escaped()
        lines += ["""const StructMetadata g%(struct_name)sMetadata = {
    sizeof(%(struct_name)s),
    %(nFields)d,
    %(field_names)s,
    %(fields)s
};\n""" % locals()]
    return "\n".join(lines)

def add_cls(cls, structs):
    if cls not in structs:
        structs.append(cls)

# return a list of Struct subclasses that are needed to define val
def structs_from_top_level_value_rec(struct, structs):
    assert struct.is_struct()
    for field in struct.values:
        if field.is_array():
            try:
                add_cls(field.val.typ, structs)
            except:
                print(field)
                print(field.name)
                raise
        elif field.is_struct():
            structs_from_top_level_value_rec(field.val, structs)
    add_cls(struct.__class__, structs)

def gen_top_level_funcs_txt(top_level):
    assert top_level.is_struct()
    name = top_level.name()
    return top_level_funcs_txt_tmpl % locals()

def _gen_for_top_level_vals(top_level_vals, file_path):
    structs = []
    for v in top_level_vals:
        structs_from_top_level_value_rec(v, structs)

    prototypes = ""
    for v in top_level_vals:
        prototypes += gen_prototypes(v.__class__)

    struct_defs = gen_struct_defs(structs)
    structs_metadata = gen_structs_metadata_txt(structs)
    top_level_funcs = ""
    for v in top_level_vals:
        top_level_funcs += gen_top_level_funcs_txt(v)

    file_name = os.path.basename(file_path)
    write_to_file(file_path + ".h", h_txt_tmpl % locals())
    write_to_file(file_path + ".cpp", cpp_txt_tmpl % locals())

def gen_for_top_level_vals(top_level_vals, file_path):
    # TODO: if element, wrap in a list
    _gen_for_top_level_vals(top_level_vals, file_path)

# we add whitespace to all lines that don't have "str" in them
# which is how we prevent adding whitespace to string fields,
# where whitespace at end is significant. For that to work all
# string field names must have "str" in them. This is only for testing
def add_random_ws(s):
    if "str" in s: return s
    return s + " " * random.randint(1, 4)

def gen_txt_for_top_level_val(top_level_val, file_path):
    lines = []
    # -1 is a bit hackish, because we elide the name of the top-level struct
    # to make it more readable
    ser_struct(top_level_val, None, lines, -1)
    if g_add_whitespace:
        new_lines = []
        for l in lines:
            # add empty lines to test resilience of the parser
            if 1 == random.randint(1, 3):
                new_lines.append(add_random_ws(" "))
            new_lines.append(add_random_ws(l))
        lines = new_lines
    s = "\n".join(lines) + "\n"  # for consistency with how C code does it
    write_to_file_utf8_bom(file_path, s)

def set_whitespace(add_whitespace):
    global g_add_whitespace
    g_add_whitespace = add_whitespace
