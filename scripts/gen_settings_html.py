#!/usr/bin/env python

import os
import gen_settingsstructs
from gen_settingsstructs import Struct, Array

"""
TODO:
 * ability to link to a more comprehensive documentation e.g. for color formats,
   languages
 * for gen_langs_html, show languages that don't have enough translations
   in a separate table
"""

g_version = "2.3"

html_tmpl = """
<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Description of SumatraPDF settings</title>
<style type=text/css>
body {
	font-size: 90%;
	background-color: #f5f5f5;
}

.txt1 {
	/* bold doesn't look good in the fonts above */
	font-family: Monaco, 'DejaVu Sans Mono', 'Bitstream Vera Sans Mono', 'Lucida Console', monospace;
	font-size: 88%;
	color: #800; /* this is brown */
}

.txt2 {
	font-family: Verdana, Arial, sans-serif;
	font-family: serif;
	font-size: 90%;
	font-weight: bold;
	color: #800; /* this is brown */
}

.txt {
	font-family: serif;
	font-size: 95%;
	font-weight: bold;
	color: #800; /* this is brown */
	color: #000;
	background-color: #ececec;
}

.cm {
	color: #800;   /* this is brown, a bit aggressive */
	color: #8c8c8c; /* this is gray */
	color: #555; /* this is darker gray */
	font-weight: normal;
}
</style>
</head>

<body>

<p>You can change the look and behavior of
<a href="http://blog.kowalczyk.info/software/sumatrapdf/">SumatraPDF</a>
by editing <code>SumatraPDF-settings.txt</code> file.</p>

<p>Use menu <code>Settings/Advanced Settings...</code> to open settings file
with your default text editor.</p>

<p>The file is stored in <code>%APPDATA%\SumatraPDF</code> directory for the
installed version or in the same directory as <code>SumataPDF.exe</code>
executable for the portable version.</p>

<p>The file is in a simple text format. Below is an explanation of
what different settings mean and what is their default value:</p>

<p>Do not modify settings marked as internal.</p>

<pre class=txt>
%INSIDE%
</pre>

<p id="#colors">
The syntax for colors is: <code>#aarrggbb</code> or <code>#rrggbb</code>.
The components are hex values (ranging from 00 to FF) and stand for:
<ul>
  <li><code>aa</code> : alpha (transparency). 0 means fully transparent (invisible)
      color. FF (255) means fully opaque color</li>
  <li><code>rr</code> : red component</li>
  <li><code>gg</code> : green component</li>
  <li><code>bb</code> : blue component</li>
</ul>
For example #ff0000 means red color. You can use <a href="http://colorschemedesigner.com/">
ColorScheme Designer</a> to pick a color.
</p>

</body>
</html>
"""

langs_html_tmpl = """
<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Languages supported by SumatraPDF</title>
<style type=text/css>
body {
	font-size: 90%;
	background-color: #f5f5f5;
}

.txt1 {
	/* bold doesn't look good in the fonts above */
	font-family: Monaco, 'DejaVu Sans Mono', 'Bitstream Vera Sans Mono', 'Lucida Console', monospace;
	font-size: 88%;
	color: #800; /* this is brown */
}

.txt2 {
	font-family: Verdana, Arial, sans-serif;
	font-family: serif;
	font-size: 90%;
	font-weight: bold;
	color: #800; /* this is brown */
}

.txt {
	font-family: serif;
	font-size: 95%;
	font-weight: bold;
	color: #800; /* this is brown */
	color: #000;
	background-color: #ececec;
}

.cm {
	color: #800;   /* this is brown, a bit aggressive */
	color: #8c8c8c; /* this is gray */
	color: #555; /* this is darker gray */
	font-weight: normal;
}
</style>
</head>

<body>

<p>Languages supported by SumatraPDF. You can use ISO code as a value
of <code>UILanguage</code> in <a href="settings.html">settings file</a>.
</p>

<table>
<tr><th>Language name</th><th>ISO code</th></tr>
%INSIDE%
</table>

</body>
</html>
"""

#indent_str = "&nbsp;&nbsp;"
indent_str = "  "

def gen_comment(comment, start, first = False):
	line_len = 80
	s = start + '<span class=cm>'
	if not first:
		s = "\n" + s
	left = line_len - len(start)
	for word in comment.split():
		if left < len(word):
			s += "\n" + start
			left = line_len - len(start)
		word += " "
		left -= len(word)
		s += word
	s = s.rstrip()
	s += '</span>'
	return [s]

def gen_struct(struct, comment=None, indent=""):
	lines = []
	if comment:
		lines += gen_comment(comment, "") + [""]
	for field in struct.default:
		if field.internal:
			continue
		first = (field == struct.default[0])
		if type(field) in [Struct, Array] and not field.type.name == "Compact":
			lines += gen_comment(field.comment, indent, first)
			#lines += ["%s%s [" % (indent, field.name), gen_struct(field, None, indent + "  "), "%s]" % indent, ""]
			lines += ["%s%s [" % (indent, field.name), gen_struct(field, None, indent + indent_str), "%s]" % indent]
		else:
			s = field.inidefault(commentChar="").lstrip()
			lines += gen_comment(field.comment, indent, first) + [indent + s]
	return "\n".join(lines)

class Lang(object):
	def __init__(self, name, code):
		self.name = name
		self.code = code

def gen_langs_html():
	import trans_langs
	langs = trans_langs.g_langs
	langs = [Lang(el[1], el[0]) for el in langs]
	lines = []
	langs = sorted(langs, key=lambda lang: lang.name)
	for l in langs:
		s = '<tr><td>%s</td><td>%s</td></tr>' % (l.name, l.code)
		lines += [s]
	inside = "\n".join(lines)
	s = langs_html_tmpl.replace("%INSIDE%", inside)
	p = os.path.join("scripts", "langs.html")
	open(p, "w").write(s)

def gen_html():
	prefs = gen_settingsstructs.GlobalPrefs
	inside = gen_struct(prefs)
	s = html_tmpl.replace("%INSIDE%", inside)
	p = os.path.join("scripts", "settings" + g_version + ".html")
	open(p, "w").write(s)

if __name__ == "__main__":
	gen_langs_html()
	gen_html()
