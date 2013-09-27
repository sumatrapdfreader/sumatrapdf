#!/usr/bin/env python

import os, util2, gen_settingsstructs, trans_langs

"""
TODO:
 * for gen_langs_html, show languages that don't have enough translations
   in a separate table
"""

g_version = util2.get_sumatrapdf_version()

html_tmpl = """\
<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Customizing SumatraPDF</title>
<style type=text/css>
body {
    font-size: 90%;
    background-color: #f5f5f5;
}

.desc {
    padding: 0px 10px 0px 10px;
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
    border: 1px solid #fff;
    border-radius: 10px;
    -webkit-border-radius: 10px;
    box-shadow: rgba(0, 0, 0, .15) 3px 3px 4px;
    -webkit-box-shadow: rgba(0, 0, 0, .15) 3px 3px 4px;
    padding: 10px 10px 10px 20px;
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

<div class=desc>

<h2>Customizing SumatraPDF</h2>

<p>You can change the look and behavior of
<a href="http://blog.kowalczyk.info/software/sumatrapdf/">SumatraPDF</a>
by editing the file <code>SumatraPDF-settings.txt</code>. The file is stored in
<code>%APPDATA%\SumatraPDF</code> directory for the installed version or in the
same directory as <code>SumatraPDF.exe</code> executable for the portable version.</p>

<p>Use the menu item <code>Settings -> Advanced Settings...</code> to open the settings file
with your default text editor.</p>

<p>The file is in a simple text format. Below is an explanation of
what the different settings mean and what their default values are.</p>

<p>Highlighted settings can't be changed from the UI. Modifying other settings
directly in this file is not recommended.</p>

<p>If you add or remove lines with square brackets, <b>make sure to always add/remove
square brackets in pairs</b>! Else you risk losing all the data following them.</p>

</div>

<pre class=txt>
%INSIDE%
</pre>

<div class=desc>
<h3 id="color">Syntax for color values</h3>

<p>
The syntax for colors is: <code>#rrggbb</code>.</p>
<p>The components are hex values (ranging from 00 to FF) and stand for:
<ul>
  <li><code>rr</code> : red component</li>
  <li><code>gg</code> : green component</li>
  <li><code>bb</code> : blue component</li>
</ul>
For example #ff0000 means red color. You can use <a href="http://www.colorpicker.com/">
Color Picker</a> or <a href="http://mudcu.be/sphere/">Sphere</a> or
<a href="http://colorschemedesigner.com/">ColorScheme Designer</a> to pick a color.
</p>
</div>

</body>
</html>
"""

langs_html_tmpl = """\
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

<h2>Languages supported by SumatraPDF</h2>

<p>Languages supported by SumatraPDF. You can use ISO code as a value
of <code>UiLanguage</code> setting in <a href="settings.html">settings file</a>.
</p>

<p>Note: not all languages are fully translated. Help us <a href="http://www.apptranslator.org/app/SumatraPDF">translate SumatraPDF</a>.</p>

<table>
<tr><th>Language name</th><th>ISO code</th></tr>
%INSIDE%
</table>

</body>
</html>
"""

#indent_str = "&nbsp;&nbsp;"
indent_str = "    "

# if s in the form: "foo](bar.html)", returns ["foo", "bar.html"].
# otherwise returns ["foo"]
def extract_url(s):
    if not s.endswith(")"):
        return [s]
    word_end = s.find("]")
    assert word_end != -1
    word = s[:word_end]
    assert s[word_end+1] == "("
    url = s[word_end+2:-1]
    return [word, url]

def gen_comment(comment, start, first=False):
    line_len = 100
    s = start + '<span class=cm>'
    if not first:
        s = "\n" + s
    left = line_len - len(start)
    # [foo](bar.html) is turned into <a href="bar.html">foo</a>
    href_text = None
    for word in comment.split():
        if word[0] == "[":
            word_url = extract_url(word[1:])
            if len(word_url) == 2:
                s += '<a href="%s">%s</a>' % (word_url[1], word_url[0])
                continue
            href_text = word_url[0]
            continue
        elif href_text != None:
            word_url = extract_url(word)
            href_text = href_text + " " + word_url[0]
            if len(word_url) == 2:
                s += '<a href="%s">%s</a> ' % (word_url[1], href_text)
                href_text = None
            continue

        if left < len(word):
            s += "\n" + start
            left = line_len - len(start)
        word += " "
        left -= len(word)
        if word == "color ":
            word = '<a href="#color">color</a> '
        elif word == "colors ":
            word = '<a href="#color">colors</a> '
        s += word
    s = s.rstrip()
    s += '</span>'
    return [s]

def gen_struct(struct, indent="", prerelease=False):
    lines = []
    first = True
    inside_expert = False
    for field in struct.default:
        if field.internal or type(field) is gen_settingsstructs.Comment or not prerelease and field.prerelease:
            continue
        start_idx = len(lines)
        if type(field) is gen_settingsstructs.Array and not field.type.name == "Compact":
            lines += gen_comment(field.docComment, indent, first)
            indent2 = indent + indent_str[:len(indent_str)/2]
            start = "%s%s [\n%s[" % (indent, field.name, indent2)
            end = "%s]\n%s]" % (indent2, indent)
            inside = gen_struct(field, indent + indent_str, prerelease)
            lines += [start, inside, end]
        elif type(field) is gen_settingsstructs.Struct and not field.type.name == "Compact":
            lines += gen_comment(field.docComment, indent, first)
            start = "%s%s [" % (indent, field.name)
            end = "%s]" % indent
            inside = gen_struct(field, indent + indent_str, prerelease)
            lines += [start, inside, end]
        else:
            s = field.inidefault(commentChar="").lstrip()
            lines += gen_comment(field.docComment, indent, first) + [indent + s]
        first = False
        if field.expert and not inside_expert:
            lines[start_idx] = '<div>' + lines[start_idx]
        elif not field.expert and inside_expert:
            lines[start_idx] = '</div>' + lines[start_idx]
        inside_expert = field.expert
    return "\n".join(lines)

class Lang(object):
    def __init__(self, name, code):
        self.name = name
        self.code = code

def blog_dir():
    script_dir = os.path.realpath(os.path.dirname(__file__))
    blog_dir = os.path.realpath(os.path.join(script_dir, "..", "..", "web", "blog", "www", "software", "sumatrapdf"))
    if os.path.exists(blog_dir): return blog_dir
    return None

def gen_langs_html():
    langs = trans_langs.g_langs
    langs = [Lang(el[1], el[0]) for el in langs]
    lines = []
    langs = sorted(langs, key=lambda lang: lang.name)
    for l in langs:
        s = '<tr><td>%s</td><td>%s</td></tr>' % (l.name, l.code)
        lines += [s]
    inside = "\n".join(lines)
    s = langs_html_tmpl.replace("%INSIDE%", inside)
    file_name = "langs.html"
    p = os.path.join("scripts", file_name)
    open(p, "w").write(s)
    if blog_dir():
        p = os.path.join(blog_dir(), file_name)
        open(p, "w").write(s)

def gen_html():
    prefs = gen_settingsstructs.GlobalPrefs
    inside = gen_struct(prefs)
    s = html_tmpl.replace("%INSIDE%", inside)
    file_name = "settings" + g_version + ".html"
    p = os.path.join("scripts", file_name)
    open(p, "w").write(s)
    if blog_dir():
        p = os.path.join(blog_dir(), file_name)
        open(p, "w").write(s)
        # also save the latest version as settings.html so that there is a
        # permament version we can link from from docs that is independent of
        # program version number
        p = os.path.join(blog_dir(), "settings.html")
        open(p, "w").write(s)

if __name__ == "__main__":
    util2.chdir_top()
    gen_langs_html()
    gen_html()
    gen_settingsstructs.main()
