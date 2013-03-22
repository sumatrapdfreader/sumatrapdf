"""
Updates the vc2008 project, trying to preserve its internal structure.
"""

import os, re, util2
pjoin = os.path.join

VC2008_PROJ = pjoin("vs", "sumatrapdf-vc2008.vcproj")
DIR_STRUCT = [
	("src", ("Source Files",)),
	(pjoin("src", "utils"), ("baseutils",)),
	(pjoin("src", "installer"), ("Installer",)),
	(pjoin("src", "ifilter"), ("Integration DLLs", "IFilter")),
	(pjoin("src", "browserplugin"), ("Integration DLLs", "Browser Plugin")),
	(pjoin("src", "previewer"), ("Integration DLLs", "Previewer")),
	(pjoin("src", "mui"), ("baseutils", "mui")),
	(pjoin("mupdf", "draw"), ("mupdf", "draw")),
	(pjoin("mupdf", "fitz"), ("mupdf", "fitz")),
	(pjoin("mupdf", "pdf"), ("mupdf", "pdf")),
	(pjoin("mupdf", "xps"), ("mupdf", "xps")),
]
SOURCE_EXTS = [".cpp", ".c", ".h", ".rc"]
EXCLUDE = [
	pjoin("mupdf", "draw", "draw_simple_scale.c"),
	pjoin("mupdf", "pdf", "pdf_js.c"),
	pjoin("mupdf", "pdf", "pdf_jsimp_cpp.c"),
	pjoin("mupdf", "pdf", "pdf_jsimp_cpp.h"),
	pjoin("mupdf", "pdf", "pdf_jsimp_v8.cpp"),
]

class XmlNode:
	def __init__(self, data):
		self.xml, self.indent, self.name, self.attrs, self.content = data

	def getAttr(self, name):
		return (re.findall(name + r'="([^"]+)"', self.attrs) + [None])[0]

def getSiblingNodes(xml):
	return [XmlNode(data) for data in re.findall(r"(?m)(^(\s*)<(\S+)([^>]*)>\n((?:.*\n)*?)\2</\3>\n)", xml)]

def getFilesNode(xml):
	root = getSiblingNodes(xml)
	assert len(root) == 1 and root[0].name == "VisualStudioProject"
	nodes = getSiblingNodes(root[0].content)
	filesNodes = [node for node in nodes if node.name == "Files"]
	assert len(filesNodes) == 1
	return filesNodes[0]

def parseNodes(root):
	nodes = [root]
	for node in getSiblingNodes(root.content):
		assert node.name == "File" and node.getAttr("RelativePath") or node.name == "Filter" and node.getAttr("Name")
		assert not node.indent.replace("\t", "")
		nodes += parseNodes(node)
	return nodes

def extractPath(node):
	return node.getAttr("RelativePath")[3:].replace("\\", os.path.sep)

def extractPaths(nodes):
	return [extractPath(node) for node in nodes if node.name == "File"]

def isRelevant(path):
	return any(path.endswith(ext) for ext in SOURCE_EXTS) and path not in EXCLUDE

def findAddedFiles(knownPaths):
	added = []
	for dir in DIR_STRUCT:
		all_files = [pjoin(dir[0], fname) for fname in os.listdir(dir[0])]
		added += [(path, dir[1]) for path in all_files if not path in knownPaths and isRelevant(path)]
	return added

def updateProj(vcproj, nodes, addedPaths):
	filter = []
	for node in nodes:
		if node.name != "File":
			if node.name == "Filter":
				filter = filter[:len(node.indent) - 2] + [node.getAttr("Name")]
			addXml = ""
			for path in [path[0] for path in addedPaths if path[1] == tuple(filter)]:
				addXml += (node.indent + "\t").join(["", "<File\n", '\tRelativePath="..\\%s"\n' % path.replace(os.path.sep, "\\"), "\t>\n", "</File>\n"])
			if addXml:
				vcproj = vcproj.replace(node.content, node.content + addXml)
		elif not os.path.exists(extractPath(node)):
			vcproj = vcproj.replace(node.xml, "")
	return vcproj

def main():
	util2.chdir_top()
	
	vcproj = open(VC2008_PROJ, "rb").read().replace("\r\n", "\n")
	fileNodes = parseNodes(getFilesNode(vcproj))
	addedPaths = findAddedFiles(extractPaths(fileNodes))
	vcprojNew = updateProj(vcproj, fileNodes, addedPaths)
	
	if vcprojNew != vcproj:
		open(VC2008_PROJ, "wb").write(vcprojNew.replace("\n", "\r\n"))

if __name__ == "__main__":
	main()
