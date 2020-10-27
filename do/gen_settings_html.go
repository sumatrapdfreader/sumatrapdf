package main

import (
	"fmt"
	"html"
	"path/filepath"
	"sort"
	"strings"

	"github.com/kjk/u"
)

const tmplHTML = `<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Customizing SumatraPDF %VER%</title>
<style type="text/css">
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

<div class="desc">

<h2>Customizing SumatraPDF %VER%</h2>

<p>You can change the look and behavior of
<a href="http://www.sumatrapdfreader.org/">SumatraPDF</a>
by editing the file <code>SumatraPDF-settings.txt</code>. The file is stored in
<code>%LOCALAPPDATA%\SumatraPDF</code> directory for the installed version or in the
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

<pre class="txt">
%INSIDE%
</pre>

<div class="desc">
<h3 id="color">Syntax for color values</h3>

<p>
The syntax for colors is: <code>#rrggbb</code>.</p>
<p>The components are hex values (ranging from 00 to FF) and stand for:
<ul>
  <li><code>rr</code> : red component</li>
  <li><code>gg</code> : green component</li>
  <li><code>bb</code> : blue component</li>
</ul>
For example #ff0000 means red color. You can use <a href="https://galactic.ink/sphere/">Sphere</a> to pick a color.
</p>
</div>

</body>
</html>
`

const tmplLangsHTML = `<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Languages supported by SumatraPDF %VER%</title>
<style type="text/css">
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

<h2>Languages supported by SumatraPDF %VER%</h2>

<p>Languages supported by SumatraPDF. You can use ISO code as a value
of <code>UiLanguage</code> setting in <a href="settings%VER%.html">settings file</a>.
</p>

<p>Note: not all languages are fully translated. Help us <a href="http://www.apptranslator.org/app/SumatraPDF">translate SumatraPDF</a>.</p>

<table>
<tr><th>Language name</th><th>ISO code</th></tr>
%INSIDE%
</table>

</body>
</html>
`

var indentStr = "    "

func extractURL(s string) []string {
	if !strings.HasSuffix(s, ")") {
		return []string{s}
	}
	wordEnd := strings.Index(s, "]")
	panicIf(wordEnd == -1)
	word := s[:wordEnd]
	panicIf(s[wordEnd+1] != '(')
	url := s[wordEnd+2 : len(s)-1]
	return []string{word, url}
}

func cgiEscape(s string) string {
	return html.EscapeString(s)
}

func genComment(comment string, fieldID string, start string, first bool) string {
	lineLen := 100
	s := "\n"
	if first {
		s = ""
	}
	s = s + start + fmt.Sprintf(`<span class="cm" id="%s">`, fieldID)
	left := lineLen - len(start)
	// [foo](bar.html) is turned into <a href="bar.html">foo</a>
	hrefText := ""
	comment = cgiEscape(comment)
	words := strings.Split(comment, " ")
	for _, word := range words {
		if word[0] == '[' {
			wordURL := extractURL(word[1:])
			if len(wordURL) == 2 {
				s += fmt.Sprintf(`<a href="%s">%s</a>`, wordURL[1], wordURL[0])
				continue
			}
			hrefText = wordURL[0]
			continue
		} else if hrefText != "" {
			wordURL := extractURL(word)
			hrefText = hrefText + " " + wordURL[0]
			if len(wordURL) == 2 {
				s += fmt.Sprintf(`<a href="%s">%s</a> `, wordURL[1], hrefText)
				hrefText = ""
			}
			continue
		}
		if left < len(word) {
			s = rstrip(s) + "\n" + start
			left = lineLen - len(start)
		}
		word += " "
		left -= len(word)
		if word == "color " {
			word = `<a href="#color">color</a> `
		} else if word == "colors " {
			word = `<a href="#color">colors</a> `
		}
		s += word
	}
	s = rstrip(s)
	s += `</span>`
	return s
}

func lstrip(s string) string {
	return strings.TrimLeft(s, " \n\r\t")
}

func rstrip(s string) string {
	return strings.TrimRight(s, " \n\r\t")
}

func genStruct(struc *Field, indent string, isPreRelease bool) string {
	var lines []string
	first := true
	insideExpert := false

	fields := struc.Default.([]*Field)
	for _, field := range fields {
		if field.Internal || field.isComment() || (!isPreRelease && field.PreRelease) {
			continue
		}
		startIdx := len(lines)
		comment := field.DocComment
		if field.Version != "2.3" {
			comment += fmt.Sprintf(" (introduced in version %s)", field.Version)
		}

		fieldID := field.Name
		if indent != "" {
			fieldID = struc.Name + "_" + field.Name
		}
		s := genComment(comment, fieldID, indent, first)
		lines = append(lines, s)

		if field.Type.Name == "Array" {
			indent2 := indent + indentStr[:len(indentStr)/2]
			start := fmt.Sprintf("%s%s [\n%s[", indent, field.Name, indent2)
			end := fmt.Sprintf("%s]\n%s]", indent2, indent)
			inside := genStruct(field, indent+indentStr, isPreRelease)
			lines = append(lines, start, inside, end)
		} else if field.Type.Name == "Struct" {
			start := fmt.Sprintf("%s%s [", indent, field.Name)
			end := fmt.Sprintf("%s]", indent)
			inside := genStruct(field, indent+indentStr, isPreRelease)
			lines = append(lines, start, inside, end)
		} else {
			s = field.initDefault()
			s = lstrip(s)
			lines = append(lines, indent+s)
		}
		first = false
		if field.Expert && !insideExpert {
			lines[startIdx] = `<div>` + lines[startIdx]
		} else if !field.Expert && insideExpert {
			lines[startIdx] = `</div>` + lines[startIdx]
		}
		insideExpert = field.Expert
	}
	return strings.Join(lines, "\n")
}

func mkLang(name string, code string) *Lang {
	return &Lang{
		name: name,
		code: code,
	}
}

func websiteSettingsDir() string {
	return filepath.Join("website", "settings")
}

func langsFileName() string {
	ver := extractSumatraVersionMust()
	return fmt.Sprintf("langs%s.html", ver)
}

func settingsFileName() string {
	ver := extractSumatraVersionMust()
	return fmt.Sprintf("settings%s.html", ver)
}

func genLangsHTML() {
	var langs []*Lang
	for _, el := range gLangs {
		langs = append(langs, mkLang(el[1], el[0]))
	}
	sort.Slice(langs, func(i, j int) bool {
		return langs[i].name < langs[j].name
	})
	var lines []string
	for _, l := range langs {
		s := fmt.Sprintf(`<tr><td>%s</td><td>%s</td></tr>`, l.name, l.code)
		lines = append(lines, s)
	}
	inside := strings.Join(lines, "\n")
	s := strings.Replace(tmplLangsHTML, "%INSIDE%", inside, -1)
	s = strings.Replace(s, "%VER%", extractSumatraVersionMust(), -1)
	s = strings.Replace(s, "settings.html", settingsFileName(), -1)
	s = strings.Replace(s, "\n", "\r\n", -1)
	// undo html escaping that differs from Python
	// TODO: possibly remove
	//s = strings.Replace(s, "&#39;", "'", -1)

	path := filepath.Join(websiteSettingsDir(), langsFileName())
	u.WriteFileMust(path, []byte(s))
}

func genSettingsHTML() {
	prefs := globalPrefsStruct
	inside := genStruct(prefs, "", false)
	s := strings.Replace(tmplHTML, "%INSIDE%", inside, -1)
	s = strings.Replace(s, "%VER%", extractSumatraVersionMust(), -1)
	s = strings.Replace(s, "langs.html", langsFileName(), -1)
	s = strings.Replace(s, "\n", "\r\n", -1)
	// undo html escaping that differs from Python
	// TODO: possibly remove
	//s = strings.Replace(s, "&#39;", "'", -1)

	path := filepath.Join(websiteSettingsDir(), settingsFileName())
	u.WriteFileMust(path, []byte(s))
	fmt.Printf("Wrote '%s'\n", path)
}
