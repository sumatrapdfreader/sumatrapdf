"""
This is a helper library for parsing SquareTree files into a Python tree
for data extraction and for reserializing such data trees. See
../src/utils/SquareTreeParser.cpp for the full specification of the format.

Usage:
	import SquareTree
	root = SquareTree.Parse(data)
	print root.GetValue("key") # returns a string
	print root.GetChild("node") # returns a Node
	# returns the value of key of the second node named "node"
	print root.GetChild("node", 1).GetValue("key")
	print SquareTree.Serialize(root)
"""

import os, re, util

class Node(object):
	def __init__(self):
		self.data = []

	def __repr__(self):
		return repr(self.data)

	def GetChild(self, name, idx=0):
		children = [item for item in self.data if type(item[1]) is Node and item[0].lower() == name.lower()]
		return (children + [(name, None)] * (idx + 1))[idx][1]

	def GetValue(self, name, idx=0):
		values = [item for item in self.data if type(item[1]) is not Node and item[0].lower() == name.lower()]
		return (values + [(name, None)] * (idx + 1))[idx][1]

def Parse(data, level=0):
	if level == 0:
		# decode the file from UTF-8, UTF-16 or ANSI
		if data.startswith("\xef\xbb\xbf"):
			data = data.decode("utf-8-sig")
		elif data.startswith("\xff\xfe"):
			data = data[2:].decode("utf-16")
		else:
			import locale
			data = data.decode(locale.getpreferredencoding())
		data += "\n"

	node = Node()
	while data:
		# skip blank lines, comments and whitespace at the beginning of a line
		skip = re.match(r"(?:\s+|[#;].*)+", data)
		if skip:
			data = data[len(skip.group(0)):]
			if not data:
				break
		# parse a single line into key, separator and value
		line = re.match(r"([^=:\[\]\n]*?)[^\S\n]*([=:\[\]])[^\S\n]*([^\n]*?)[^\S\n]*\n", data)
		if not line:
			line = re.match(r"([^=:\[\]\n]*?)((?=\s))[^\S\n]*([^\n]*?)[^\S\n]*\n", data)
		assert line and len(line.groups()) == 3
		key, sep, value = line.groups()
		assert sep in ["=", ":", "[", "]", ""]
		# if a line contains just a key and the next non-empty line contains an opening bracket,
		# the key is the name of that subnode (instead of having a key with an empty value and a
		# subnode with an empty key)
		nodeKey = re.match(r"([^=:\[\]\n]*?)(?:\s*(?:[#;].*\n)?)+\[", data) if not sep and not value else None
		if sep == "[" and (not value or value[0] in ["#", ";"]) or nodeKey:
			# parse the subnode
			data = data[len(nodeKey.group(0) if nodeKey else line.group(0)):]
			subnode, data = Parse(data, level + 1)
			node.data.append((key, subnode))
			# if a subnode is directly followed by another unnamed subnode, reuse the same key
			# (instead of giving it an empty key or ignoring the line)
			while True:
				next = re.match(r"(?:\s*(?:[#;].*\n)?)*\[(?=\s*?[#;\n])", data)
				if not next:
					break
				data = data[len(next.group(0)):]
				subnode, data = Parse(data, level + 1)
				node.data.append((key, subnode))
		elif sep == "]" and not key:
			# close the subnode (or ignore it if we're already at the top level)
			if level > 0:
				return node, data[1:]
			data = data[1:]
		elif not key and sep == "[" and value.endswith("]"):
			# interpret INI style section headers as the names of top-level nodes
			if level > 0:
				return node, data
			data = data[len(line.group(0)):]
			key = line.group(0).strip()[1:-1].strip()
			subnode, data = Parse(data, level + 1)
			node.data.append((key, subnode))
		elif sep in ["=", ":", ""]:
			# this is a plain key-value pair
			data = data[len(line.group(0)):]
			node.data.append((key, value))
		else:
			assert False, "invalid data line: %s" % line.group(0)

	if level > 0:
		return node, data
	return node

def Serialize(root, level=0):
	result = []
	for node in (root.data if type(root) is Node else root):
		if type(node[1]) in [Node, list]:
			result += ["\t" * level + (node[0] + " [" if node[0] else "[")]
			result += Serialize(node[1], level + 1)
			result += ["\t" * level + "]"]
		elif type(node[1]) in [str, unicode]:
			result += ["\t" * level + node[0] + " = " + node[1]]
		else:
			assert False, "value must be Node/list or string"

	if level > 0:
		return result
	# encode the result as UTF-8
	return ("\n".join(result) + "\n").encode("utf-8-sig")

if __name__ == "__main__":
	util.chdir_top()

	data = " Key : Value \nNode\n[\n[ Node ]\nKey=Value2".encode("utf-8-sig")
	root = Parse(data)
	assert root.GetValue("key") == "Value"
	assert root.GetChild("node")
	assert root.GetChild("node", 1).GetValue("key") == "Value2"
	data = Serialize(root)
	assert Serialize(Parse(data)) == data

	path = os.path.join("obj-dbg", "SumatraPDF-settings.txt")
	if os.path.exists(path):
		data = open(path).read()
		root = Parse(data)
		# modification example: filter out all states for missing files
		root.GetChild("FileStates").data = [
			state
			for state in root.GetChild("FileStates").data
			if state[1].GetValue("IsMissing") != u"true"
		]
		data = Serialize(root)
		assert Serialize(Parse(data)) == data
