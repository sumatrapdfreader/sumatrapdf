#!/usr/bin/env python
"""
Gyp generates solution where files are needlesly nested.
I don't know how to tell gyp to generate better files. This tool
does post-processing on .vcxproj.filters files and improves them.

Run as: gyp-vsproj-fix foo.vcxproj.filters
"""

import sys
from xml.dom.minidom import parseString

def get_include_attr(node):
    if not node.hasAttributes(): return None
    return node.attributes["Include"].value

def set_include_attr(node, attr):
    node.attributes["Include"].value = attr

# Nodes like:
# <Filter Include="..\..\tools\sizer">
def get_filter_nodes(node):
    nodes = node.getElementsByTagName('Filter')
    # could also check if parent is ItemGroup
    return [n for n in  nodes if get_include_attr(n) != None]

# Nodes like:
#    <ClCompile Include="..\..\tools\sizer\PdbFile.cpp">
#      <Filter>..\..\tools\sizer</Filter>
#   </ClCompile>
def get_file_nodes(node):
    n1 = node.getElementsByTagName('ClInclude')
    n2 = node.getElementsByTagName('ClCompile')
    n3 = node.getElementsByTagName('None')
    return n1 + n2 + n3

def get_include_filter(node):
    #print(str(node))
    include = get_include_attr(node)
    filter = node.getElementsByTagName('Filter')[0].firstChild.nodeValue
    #print(str(filter))
    return (include, filter)

# given:
#    <ClInclude Include="..\..\tools\sizer\PdbFile.h">
#      <Filter>..\..\tools\sizer</Filter>
#    </ClInclude>
# sets the value of Filter node to val
def set_filter(node, val):
    node.getElementsByTagName('Filter')[0].firstChild.nodeValue = val

def get_filter(node):
    #print(str(node))
    v = node.getElementsByTagName('Filter')[0].firstChild.nodeValue
    #print(str(v))
    return v

def remove_node(node):
    #print("removing %s" % str(node))
    parent = node.parentNode
    parent.removeChild(node)
    #node.unlink()

def fix_include_filters(filter_nodes, path):
    for n in filter_nodes:
        include_path = get_include_attr(n)
        if include_path == None:
            print(str(n))
            print(n.toxml())
        if path.startswith(include_path):
            remove_node(n)
        elif include_path.startswith(path):
            include_path = include_path[len(path):]
            set_include_attr(n, include_path)

def fix_file_nodes(file_nodes, path):
    for n in file_nodes:
        include_path = get_filter(n)
        if include_path.startswith(path):
            include_path = include_path[len(path):]
            set_filter(n, include_path)

def fix_file(path):
    data = open(path).read()
    dom = parseString(data)
    filter_nodes = get_filter_nodes(dom)
    file_nodes = get_file_nodes(dom)
    #for n in filter_nodes: print(get_include_attr(n))
    filter_to_files = {}
    for n in filter_nodes:
        filter_to_files[get_include_attr(n)] = []
    for n in file_nodes:
        (include, filter) = get_include_filter(n)
        #print("%s => %s" % (filter, include))
        filter_to_files[filter].append(include)
    for (k, v) in filter_to_files.items():
        if len(v) == 0:
            print("filter %s has no files" % k)

    # TODO: should figure out what to delete by analyzing
    # include paths (and remove those that add nothing but nesting)
    path_to_remove = "..\\..\\"
    fix_include_filters(filter_nodes, path_to_remove)
    fix_file_nodes(file_nodes, path_to_remove)

    path_to_remove = "tools\\"
    fix_include_filters(filter_nodes, path_to_remove)
    fix_file_nodes(file_nodes, path_to_remove)

    #s = dom.toprettyxml("  ", "\n")
    s = dom.toxml()
    #dst = path + ".copy.xml"
    dst = path
    open(dst, "w").write(s)

def main():
    files = sys.argv[1:]
    for f in files: fix_file(f)

if __name__ == "__main__":
    main()
