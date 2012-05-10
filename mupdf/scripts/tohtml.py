import sys, os, re

HEADER="""<head>
<style>
body { background-color:#fffff0; color:black; margin:16pt; }
a { text-decoration:none; color:darkblue; }
a.line { position:relative; padding-top:300px; }
.comment { color:green; font-style:italic; }
.comment a { color:darkgreen; }
</style>
</head>
<body><pre><pre>"""

FOOTER="""</pre></body>"""

prefixes = [ 'fz_', 'pdf_', 'xps_', 'cbz_', 'pdfapp_' ]

def is_public(s):
	for prefix in prefixes:
		if s.startswith(prefix):
			return True
	return False

def load_tags():
	tags = {}
	for line in open("tags-xref").readlines():
		ident, type, line, file, text = line.split(None, 4)
		if not is_public(ident):
			continue
		if type == 'function':
			tags[ident] = '<a class="function" href="%s#%s">%s</a>' % (os.path.basename(file), line, ident)
		if type == 'typedef' or type == 'struct':
			tags[ident] = '<a class="typedef" href="%s#%s">%s</a>' % (os.path.basename(file), line, ident)
	return tags

tags = load_tags()

def quote(s):
	return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')

print HEADER

N = 1
for line in sys.stdin.readlines():
	# expand tabs, html-quote special characters and colorize comments
	line = line.replace('\t', '    ').rstrip()
	line = quote(line)
	line = line.replace("/*", '<span class="comment">/*')
	line = line.replace("*/", '*/</span>')

	line = re.sub('^#include "([a-z-]*\.h)"', '#include "<a href="\\1">\\1</a>"', line)

	# find identifiers and hyperlink to their definitions
	words = re.split("(\W+)", line)
	line = ""
	for word in words:
		if word in tags:
			word = tags[word]
		line += word

	#print('<a class="line" name="%d">%4d</a> %s' % (N, N, line))
	print('<a class="line" name="%d"></a>%s' % (N, line))

	N = N + 1

print FOOTER
