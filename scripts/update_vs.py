#!/usr/bin/env python

import os, codecs

"""
This script keeps Visual Studio project files up-to-date with the source files.
"""

pj = os.path.join

def top_dir():
    d = os.path.join(os.path.dirname(__file__), "..")
    return os.path.realpath(d)

def ext(path):
    return os.path.splitext(path)[1].lower()

def is_clcompile_file(path):
    return ext(path) in [".cpp", ".c"]

def is_clinclude_file(path):
    return ext(path) in [".h"]

def is_c_src_file(path):
    return is_clcompile_file(path) or is_clinclude_file(path)

def is_none_file(path):
    return ext(path) in [".txt", ".ini", ".asm", ".msvc", ".bmp", ".cur", ".ico"]

def is_manifest_file(path):
    return ext(path) in [".manifest"]

def is_resource_file(path):
    return ext(path) in [".rc"]

def is_docs_file(path):
    return ext(path) in [".txt", ".ini"]

# returns full paths of files in a given directory, potentially recursively,
# potentially filtering file names by filter_func (which takes file path as
# an argument)
def list_files_g(d, filter_func=None, recur=False):
    to_visit = [d]
    while len(to_visit) > 0:
        d = to_visit.pop(0)
        for f in os.listdir(d):
            path = os.path.join(d, f)
            isdir = os.path.isdir(path)
            if isdir:
                if recur:
                    to_visit.append(path)
            else:
                if filter_func != None:
                    if filter_func(path):
                        yield path
                else:
                    yield path

# generator => array
def list_files(d, filter_func=None, recur=False):
    return [path for path in list_files_g(d, filter_func, recur)]

# TODO: this doesn't handle if the case:
# path         = "foo/bar/x.c"
# relative_dir = "foo/bar/me"
# should return:
#   "../x.c"
# instead returns:
#   "x.c"
def path_relative_to(path, relative_dir):
    common = os.path.commonprefix([path, relative_dir])
    path = path[len(common)+1:]
    relative_dir = relative_dir[len(common)+1:]
    return path

# models concept of a filter in .vcxproj.filters file:
#    <Filter Include="jpeg">
#      <UniqueIdentifier>{066558ef-98f1-4e63-b5d4-b5aaebd0d65e}</UniqueIdentifier>
#    </Filter>
class Filter(object):
    def __init__(self, name, guid, files):
        self.name = name
        self.guid = guid
        self.files = files

def list_top_dir_files(d, filter_func, recur=False):
    return list_files(pj(top_dir(), d), filter_func, recur)

"""
    <Filter Include="jbig2dec">
      <UniqueIdentifier>{215eb158-b04c-4cd9-8fb8-7a3a6c770934}</UniqueIdentifier>
    </Filter>
    <Filter Include="mupdf">
      <UniqueIdentifier>{078d86a8-74f1-49fa-af7f-8d12c180a485}</UniqueIdentifier>
    </Filter>
    <Filter Include="mupdf\fitz">
      <UniqueIdentifier>{8a33d4a4-1f54-4fe9-98c5-a0dfd57a601e}</UniqueIdentifier>
    </Filter>
    <Filter Include="Resource Files">
      <UniqueIdentifier>{67DA6AB6-F800-4c08-8B7A-83BB121AAD01}</UniqueIdentifier>
      <Extensions>rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx;tiff;tif;png;wav</Extensions>
    </Filter>
    <Filter Include="jpeg">
      <UniqueIdentifier>{066558ef-98f1-4e63-b5d4-b5aaebd0d65e}</UniqueIdentifier>
    </Filter>
    <Filter Include="mupdf\pdf">
      <UniqueIdentifier>{a35cfdd9-b833-4410-afd7-a4771ef8887b}</UniqueIdentifier>
    </Filter>
    <Filter Include="mupdf\\xps">
      <UniqueIdentifier>{29dc662e-3fee-4c3b-8186-205cf82221c0}</UniqueIdentifier>
    </Filter>
    <Filter Include="zlib">
      <UniqueIdentifier>{310fe4d2-d0c3-429d-a84b-43e01d893c84}</UniqueIdentifier>
    </Filter>
    <Filter Include="bzip2">
      <UniqueIdentifier>{3af8a3ab-791c-4c29-b28f-43adfe940d8c}</UniqueIdentifier>
    </Filter>
    <Filter Include="zlib\minizip">
      <UniqueIdentifier>{6cc62844-e2e6-47cd-bdbf-4724dc28da51}</UniqueIdentifier>
    </Filter>
    <Filter Include="mupdf\draw">
      <UniqueIdentifier>{bdd43343-e63b-467d-b5e5-ba10a0e94337}</UniqueIdentifier>
    </Filter>
    <Filter Include="sumatra">
      <UniqueIdentifier>{4FC737F1-C7A5-4376-A066-2A32D752A2FF}</UniqueIdentifier>
      <Extensions>cpp;c;cc;cxx;def;odl;idl;hpj;bat;asm;asmx</Extensions>
    </Filter>
    <Filter Include="memtrace">
      <UniqueIdentifier>{fd8ec5df-2865-47af-8513-c20645a2e26c}</UniqueIdentifier>
    </Filter>
    <Filter Include="installer">
      <UniqueIdentifier>{b0cee761-6a1e-4847-955e-f0eb80c32cd6}</UniqueIdentifier>
    </Filter>
    <Filter Include="dlls">
      <UniqueIdentifier>{8d1ef194-ad72-4aeb-93e7-628a89158c73}</UniqueIdentifier>
    </Filter>
    <Filter Include="dlls\ifilter">
      <UniqueIdentifier>{f3b78d8d-cb6d-4728-9f92-10059ca368a7}</UniqueIdentifier>
    </Filter>
    <Filter Include="dlls\browser">
      <UniqueIdentifier>{b04f4d08-164c-4b30-ba0c-26ec812e2c88}</UniqueIdentifier>
    </Filter>
    <Filter Include="dlls\preview">
      <UniqueIdentifier>{b4798144-bcb4-46dd-b39d-a5e4bbdb93ae}</UniqueIdentifier>
    </Filter>
    <Filter Include="sumatra\documents">
      <UniqueIdentifier>{2fe13b22-1504-45f2-95a0-8e2d5978dd9f}</UniqueIdentifier>
    </Filter>
    <Filter Include="sumatra\ebook">
      <UniqueIdentifier>{232267dd-50d1-4b01-83e1-89ab2c6dde73}</UniqueIdentifier>
    </Filter>
    <Filter Include="sumatra\regress">
      <UniqueIdentifier>{c705e6c9-81a5-41c1-9b39-902909c11ec9}</UniqueIdentifier>
    </Filter>
    <Filter Include="tester">
      <UniqueIdentifier>{55c58e95-d6b6-47b8-8433-4010c683a5f3}</UniqueIdentifier>
    </Filter>
    <Filter Include="browserplugin">
      <UniqueIdentifier>{b1b93410-5990-49dc-bc3f-d40cfea39fd6}</UniqueIdentifier>
    </Filter>
"""

g_sumatra_files = list_top_dir_files("src", is_c_src_file)
g_mui_files = list_top_dir_files(pj("src", "mui"), is_c_src_file)
g_utils_files = list_top_dir_files(pj("src", "utils"), is_c_src_file)
g_docs_files = list_top_dir_files("docs", is_docs_file)
g_chm_files = list_top_dir_files(pj("ext", "CHMLib", "src"), is_c_src_file)

g_filters = [
    Filter("sumatra", "{4FC737F1-C7A5-4376-A066-2A32D752A2FF}", g_sumatra_files),
    Filter("mui", "{0cbc9131-0370-46a1-a361-d8c441d9f9c5}", g_mui_files),
    Filter("utils", "{88c95b88-1f3f-4262-835b-91b32349401b}", g_utils_files),
    Filter("docs", "{367a3cbe-0f88-4739-8890-c93e3b756c3f}", g_docs_files),
    Filter("chm", "{87c09434-b151-4582-b0b3-eab39e5a51ef}", g_chm_files),
]

class FileType(object):
    def __init__(self, name, filter_func):
        self.name = name # "ClCompile" etc.
        self.filter_func = filter_func
    def file_matches(self, path):
        return self.filter_func(path)

g_file_types = [
    FileType("ClCompile", is_clcompile_file),
    FileType("ClInclude", is_clinclude_file),
    FileType("None", is_none_file),
    FileType("Manifest", is_manifest_file),
    FileType("ResourceCompile", is_resource_file),
]

def file_type_from_file(path):
    matching = [file_type for file_type in g_file_types if file_type.file_matches(path)]
    assert len(matching) == 1 # should match one and only one FileType
    return matching[0]

class File(object):
    def __init__(self, path, file_type, filter):
        self.path = path
        self.file_type = file_type
        self.filter = filter

def build_files(filters, relative_dir):
    res = []
    for filter in filters:
        for path in filter.files:
            file_type = file_type_from_file(path)
            path = path_relative_to(path, relative_dir)
            f = File(path, file_type, filter)
            res.append(f)
    return res

# returns dict FileType => [File]
def group_by_file_type(files):
    file_type_to_files = {}
    for f in files:
        ft = f.file_type.name
        if ft not in file_type_to_files:
            file_type_to_files[ft] = [f]
        else:
            file_type_to_files[ft].append(f)
    return file_type_to_files

def gen_vcxproj_part_item_group(name, files):
    if len(files) == 0: return ""
    lines = []
    for f in files:
        path = f.path
        path.replace("/", "\\") # potential unix => win path separator
        s = """    <%s Include="%s" />""" % (name, path)
        lines.append(s)
    return ["  <ItemGroup>"] + lines + ["  </ItemGroup>"]

def gen_vcxproj_part(files):
    file_type_to_files = group_by_file_type(files)
    lines = []
    for file_type_name in sorted(file_type_to_files):
        ft_files = file_type_to_files[file_type_name]
        lines += gen_vcxproj_part_item_group(file_type_name, ft_files)
    return "\n".join(lines)

# generate:
#  <ItemGroup>
#    <Filter Include="jbig2dec">
#      <UniqueIdentifier>{215eb158-b04c-4cd9-8fb8-7a3a6c770934}</UniqueIdentifier>
#    </Filter>
def gen_vcxproj_filters_1(filters):
    lines = ["  <ItemGroup>"]
    for f in filters:
        s = """    <Filter Include="%s">""" % f.name
        lines.append(s)
        s = """      <UniqueIdentifier>%s</UniqueIdentifier>""" % f.guid
        lines.append(s)
        s = """    </Filter>"""
        lines.append(s)
    lines += ["  <ItemGroup>"]
    return lines

#  <ItemGroup>
#    <ClCompile Include="ext\jbig2dec\jbig2.c">
#      <Filter>jbig2dec</Filter>
#    </ClCompile>
def gen_vcxproj_filters_2(files):
    file_type_to_files = group_by_file_type(files)
    lines = []
    for file_type_name in sorted(file_type_to_files):
        lines += ["  <ItemGroup>"]
        ft_files = file_type_to_files[file_type_name]
        for f in ft_files:
            lines += ["""    <%s Include="%s">""" % (f.file_type.name, f.path)]
            lines += ["""      <Filter>%s</Filter>""" % f.filter.name]
            lines += ["""    </%s>""" % f.file_type.name]
        lines += ["  </ItemGroup>"]
    return lines

def gen_vcxproj_filters_part(filters, files):
    lines = []
    lines += gen_vcxproj_filters_1(filters)
    lines += gen_vcxproj_filters_2(files)
    return "\n".join(lines)

def read_file_utf8(path):
    with codecs.open(path, "r", "utf-8") as fo:
        d = fo.read()
    return d

def write_file_utf8(path, s):
    with codecs.open(path, "w", "utf-8") as fo:
        fo.write(s)

def replace_item_group_in_string(s, replacement):
    start = s.find("  <ItemGroup>")
    assert -1 != start
    end = s.rfind("  </ItemGroup>")
    assert -1 != end
    end = end + len("  </ItemGroup>")
    return s[:start] + replacement + s[end:]

def replace_item_group(src_file, dst_file, s):
    d = read_file_utf8(src_file)
    d = replace_item_group_in_string(d, s)
    write_file_utf8(dst_file, d)

def main():
    files = build_files(g_filters, top_dir())

    vcxproj_part = gen_vcxproj_part(files)
    replace_item_group("sumatrapdf-vc2010.vcxproj", "sumatrapdf-vc2010-2.vcxproj", vcxproj_part)

    filters_part = gen_vcxproj_filters_part(g_filters, files)
    replace_item_group("sumatrapdf-vc2010.vcxproj.filters", "sumatrapdf-vc2010-2.vcxproj.filters", filters_part)

if __name__ == "__main__":
    main()
