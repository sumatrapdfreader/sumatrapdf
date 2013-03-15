#!/usr/bin/env python

import os

"""
This script keeps Visual Studio project files up-to-date with the source
files.
"""

pj = os.path.join

# models concept of a filter in .vcxproj.filters file:
#    <Filter Include="jpeg">
#      <UniqueIdentifier>{066558ef-98f1-4e63-b5d4-b5aaebd0d65e}</UniqueIdentifier>
#    </Filter>
class Filter(object):
    def __init__(self, name, d, filter_func, guid):
        self.name = name
        self.dir = d
        self.filter_func = filter_func
        self.guid = guid
        self.files = []

def ext(path):
    return os.path.splitext(path)[1].lower()

def is_clcompile_file(path):
    return ext(path) in [".cpp", ".c"]

def is_clinclude_file(path):
    return ext(path) in [".h"]

def is_src_file(path):
    return is_clcompile_file(path) or is_clinclude_file(path)

def is_none_file(path):
    return ext(path) in [".txt", ".ini", ".asm", ".msvc", ".bmp", ".cur", ".ico"]

def is_manifest_file(path):
    return ext(path) in [".manifest"]

def is_resource_file(path):
    return ext(path) in [".rc"]

def src_non_recur(path, isdir):
    if isdir: return False
    return is_src_file(path)

def docs_non_recur(path, isdir):
    if isdir: return False
    return ext(path) in [".txt", ".ini"]

def get_files_matching_filter(d, filter_func):
    to_visit = [d]
    res = []
    while len(to_visit) > 0:
        d = to_visit.pop(0)
        files = os.listdir(d)
        for f in files:
            path = os.path.join(d, f)
            isdir = os.path.isdir(path)
            if filter_func(path, isdir):
                res.append(path)
                if isdir:
                    to_visit.append(path)
    return res

def top_dir():
    d = os.path.join(os.path.dirname(__file__), "..")
    return os.path.realpath(d)

g_filters = [
    Filter("sumatra", "src", src_non_recur, "{4FC737F1-C7A5-4376-A066-2A32D752A2FF}"),
    Filter("mui", pj("src", "mui"), src_non_recur, "{0cbc9131-0370-46a1-a361-d8c441d9f9c5}"),
    Filter("utils", pj("src", "utils"), src_non_recur, "{88c95b88-1f3f-4262-835b-91b32349401b}"),
    Filter("docs", "docs", docs_non_recur, "{367a3cbe-0f88-4739-8890-c93e3b756c3f}"),
]

def is_valid_item_group_filter_name(name):
    for f in g_filters:
        if f.name == name: return True
    return False

def all_files(filters):
    for f in filters:
        for path in f.files:
            yield path

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

def get_files(filters, filter_func):
    return [path for path in all_files(filters) if filter_func(path)]

g_vcxproj_itemgroups = [
    ["ClCompile", is_clcompile_file],
    ["ClInclude", is_clinclude_file],
    ["None", is_none_file],
    ["Manifest", is_manifest_file],
    ["ResourceCompile", is_resource_file],
]

def itemgroup_name_from_file(path):
    name = None
    # check all item groups to make sure 2 don't claim to own the same file
    for ig in g_vcxproj_itemgroups:
        filter_func= ig[1]
        if filter_func(path):
            assert name == None
            name = ig[0]
    assert name != None
    return name

def gen_item_group(name, files, relative_dir):
    lines = []
    for path in files:
        path.replace("/", "\\") # potential unix => win path separator
        s = """    <%s Include="%s" />""" % (name, path)
        lines.append(s)
    if len(lines) == 0:
        return ""
    return ["  <ItemGroup>"] + lines + ["  </ItemGroup>"]

def gen_vcxproj_part(item_groups, relative_dir):
    lines = []
    for ig in item_groups:
        lines += gen_item_group(ig.name, files, relative_dir)
    return "\n".join(lines)

class ItemGroupFilter(object):
    def __init__(self, name, guid):
        self.name = name
        self.guid = guid
        assert is_valid_item_group_filter_name(name)
        self.files = []

class ItemGroup(object):
    def __init__(self, name):
        assert is_valid_item_group_name(name)
        self.name = name
        self.filters = {}

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

def gen_vcxproj_filters_2(filters):
    return []

def gen_vcxproj_filters_part(item_groups):
    lines = []
    filters = []
    for ig in item_groups:
        filters += ig.filters.values()
    lines += gen_vcxproj_filters_1(filters)
    lines += gen_vcxproj_filters_2(item_groups)
    return "\n".join(lines)

def is_valid_item_group_name(name):
    for item_group_def in g_vcxproj_itemgroups:
        if name == item_group_def[0]: return True
    return False

# group files into ItemGroup objects, returns ItemGroup objects
def group_files(filters):
    item_group_by_name = {}
    for f in filters:
        for path in f.files:
            name = itemgroup_name_from_file(path)
            if name not in item_group_by_name:
                item_group_by_name[name] = ItemGroup(name)
            ig = item_group_by_name[name]
            assert ig != None
            if f.name not in ig.filters:
                ig.filters[f.name] = ItemGroupFilter(f.name, f.guid)
            igf = ig.filters[f.name]
            igf.files.append(path)
    keys = sorted(item_group_by_name.keys())
    return [item_group_by_name[key] for key in keys]

def calc_relative_paths(item_groups, relative_dir):
    for ig in item_groups:
        for igf in ig.filters.values():
            files = []
            for path in igf.files:
                path = path_relative_to(path, relative_dir)
                files.append(path)
            igf.relative_files = files

def calc_files(filters, top_dir):
    for f in filters:
        d = f.dir
        f.files = get_files_matching_filter(pj(top_dir, d), f.filter_func)

def main():
    calc_files(g_filters, top_dir())
    item_groups = group_files(g_filters)
    calc_relative_paths(item_groups, top_dir())
    #vcxproj_part = gen_vcxproj_part(item_groups, top_dir())
    #print(vcxproj_part)

    filters_part = gen_vcxproj_filters_part(item_groups)
    print(filters_part)

if __name__ == "__main__":
    main()
