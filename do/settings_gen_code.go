package main

import (
	"fmt"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
)

// generates C code from struct definitions

// Type represents a type definition
type Type struct {
	Name  string
	Ctype string
}

var (
	// Bool defines a primitive bool type
	Bool = &Type{"Bool", "bool"}
	// Color defines a primitive color type
	Color = &Type{"Color", "char*"}
	// Float defines a float type
	Float = &Type{"Float", "float"}
	// Int defines an int
	Int = &Type{"Int", "int"}
	// String defines a utf8 string
	String = &Type{"String", "char*"}
	// Comment defines a comment
	Comment = &Type{"Comment", ""}
)

// Field defines a field in a struct
type Field struct {
	Name    string
	Type    *Type
	Default interface{}
	Comment string
	// internal settings are not serialized, only valid during program runtime
	Internal   bool
	CName      string
	Expert     bool // expert prefs are not exposed by the UI
	DocComment string
	Version    string // version in which this setting was introduced
	PreRelease bool   // prefs which aren't written out in release builds

	StructName string // only valid for some types
}

func (f *Field) setExpert() *Field {
	f.Expert = true
	return f
}

func (f *Field) setInternal() *Field {
	f.Internal = true
	return f
}

func (f *Field) setPreRelease() *Field {
	f.PreRelease = true
	if f.Type.Name == "Struct" {
		f.Type.Name = "Prerelease"
	}
	return f
}

func (f *Field) setVersion(v string) *Field {
	f.Version = v
	return f
}

func (f *Field) setDoc(s string) *Field {
	f.DocComment = s
	return f
}

func (f *Field) isComment() bool {
	return f.Type.Name == "Comment"
}

func (f *Field) cdefault(built map[string]int) string {
	if f.Type == Bool {
		// "true" or "false", happens to be the same in C++ as in Go
		return fmt.Sprintf("%v", f.Default)
	}
	if f.Type == Color {
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	if f.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`(intptr_t)"%v"`, f.Default)
	}
	if f.Type == Int {
		return fmt.Sprintf("%d", f.Default)
	}
	if f.Type == String {
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	typeName := f.Type.Name
	switch typeName {
	case "Struct", "Array", "Compact", "Prerelease":
		idStr := ""
		id := built[f.StructName]
		if id > 0 {
			idStr = fmt.Sprintf("_%d_", id)
		}
		return fmt.Sprintf("(intptr_t)&g%s%sInfo", f.StructName, idStr)
	}
	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	if typeName == "StringArray" {
		if f.Default == nil {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Default)
	}
	if typeName == "Comment" {
		if f.Comment == "" {
			return "0"
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, f.Comment)
	}
	logf(ctx(), "Unkonwn type name: '%s'\n", typeName)
	panicIf(true)
	return ""
}

func (f *Field) initDefault() string {
	commentChar := ""
	if f.Type == Bool {
		// "true" or "false", happens to be the same in C++ as in Go
		return fmt.Sprintf("%s = %v", f.Name, f.Default)
	}
	if f.Type == Color {
		col := f.Default.(string)
		return fmt.Sprintf("%s = %s", f.Name, col)
	}
	if f.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`%s = %v`, f.Name, f.Default)
	}
	if f.Type == Int {
		return fmt.Sprintf("%s = %d", f.Name, f.Default)
	}
	if f.Type == String {
		if f.Default != nil {
			return fmt.Sprintf(`%s = %s`, f.Name, f.Default)
		}
		return fmt.Sprintf(`%s %s =`, commentChar, f.Name)
	}
	typeName := f.Type.Name
	if typeName == "Compact" {
		fields := f.Default.([]*Field)
		var vals []string
		for _, field := range fields {
			v := field.initDefault()
			parts := strings.SplitN(v, " = ", 2)
			vals = append(vals, parts[1])
		}
		v := strings.Join(vals, " ")
		return fmt.Sprintf("%s = %s", f.Name, v)
	}
	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if f.Default != nil {
			return fmt.Sprintf("%s = %v", f.Name, f.Default)
		}
		return fmt.Sprintf("%s %s =", commentChar, f.Name)
	}
	if typeName == "StringArray" {
		if f.Default != nil {
			return fmt.Sprintf("%s = %v", f.Name, f.Default)
		}
		return fmt.Sprintf("%s %s =", commentChar, f.Name)
	}
	panicIf(true)
	return ""
}

// TODO: maybe don't change C name so that we can search the code
// using the same name across C and Go
func toCName(name string) string {
	if name == "URL" {
		return "url"
	}
	return strings.ToLower(name[0:1]) + name[1:]
}

func mkField(name string, typ *Type, def interface{}, comment string) *Field {
	res := &Field{
		Name:       name,
		Type:       typ,
		Default:    def,
		Comment:    comment,
		DocComment: comment,
		Version:    "2.3",
	}
	if name != "" {
		res.CName = toCName(name)
	}
	return res
}

func (f *Field) setStructName(structName string) *Field {
	f.StructName = structName
	if f.Type.Name == "Array" {
		ctype := fmt.Sprintf("Vec<%s*>*", structName)
		f.Type.Ctype = ctype
	}
	if f.Type.Name == "Struct" {
		f.Type.Ctype = structName
	}
	if f.Type.Name == "Compact" {
		f.Type.Ctype = structName
	}
	return f
}

func mkStruct(name string, fields []*Field, comment string) *Field {
	structName := name
	typ := &Type{"Struct", structName}
	res := mkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func mkCompactStruct(name string, fields []*Field, comment string) *Field {
	res := mkStruct(name, fields, comment)
	res.Type.Name = "Compact"
	return res
}

func mkArray(name string, fields []*Field, comment string) *Field {
	structName := name
	structName = strings.TrimSuffix(structName, "s")
	ctype := fmt.Sprintf("Vec<%s*>*", structName)
	typ := &Type{"Array", ctype}
	res := mkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func mkCompactArray(name string, typ *Type, def interface{}, comment string) *Field {
	typ2Name := fmt.Sprintf("%sArray", typ.Name)
	typ2CType := fmt.Sprintf("Vec<%s>*", typ.Ctype)
	typ2 := &Type{typ2Name, typ2CType}
	res := mkField(name, typ2, def, comment)
	return res
}

func mkComment(comment string) *Field {
	return mkField("", Comment, nil, comment)
}

func mkEmptyLine() *Field {
	return mkComment("")
}

func mkRGBA(r uint32, g uint32, b uint32, a uint32) string {
	return fmt.Sprintf("#%02x%02x%02x%02x", a, r, g, b)
}

func mkRGB(r uint32, g uint32, b uint32) string {
	return fmt.Sprintf("#%02x%02x%02x", r, g, b)
}

// limit comment lines to 72 chars
func formatComment(comment string, start string) []string {
	var lines []string
	parts := strings.Split(comment, " ")
	line := start
	for _, part := range parts {
		if len(line)+len(part) > 71 {
			lines = append(lines, line)
			line = start
		}
		line += " " + part
	}
	if line != start {
		lines = append(lines, line)
	}
	return lines
}

func formatArrayLines(data [][]string) []string {
	var lines []string
	for _, ld := range data {
		s := fmt.Sprintf("\t{ %s, %s, %s },", ld[0], ld[1], ld[2])
		lines = append(lines, s)
	}
	return lines
}

func buildStruct(struc *Field, built map[string]int) string {
	lines := []string{}
	required := []string{}
	var s string
	if struc.Comment != "" {
		comments := formatComment(struc.Comment, "//")
		lines = append(lines, comments...)
	}
	s = fmt.Sprintf("struct %s {", struc.StructName)
	lines = append(lines, s)
	fields := struc.Default.([]*Field)
	for _, field := range fields {
		if field.isComment() {
			continue
		}
		comments := formatComment(field.Comment, "\t//")
		lines = append(lines, comments...)
		s = fmt.Sprintf("\t%s %s;", field.Type.Ctype, field.CName)
		lines = append(lines, s)
		switch field.Type.Name {
		case "Color":
			// for color, we need to add field that represents
			// parsed color
			s = fmt.Sprintf("\tParsedColor %s;", field.CName+"Parsed")
			lines = append(lines, s)
		case "Struct", "Compact", "Array", "Prerelease":
			name := field.Name
			if name == field.StructName || name == field.StructName+"s" {
				if _, ok := built[name]; !ok {
					s = buildStruct(field, built)
					required = append(required, s)
					required = append(required, "")
					built[name]++
				}
			}
		}
	}
	lines = append(lines, "};")
	lines = append(lines, "")
	s1 := strings.Join(required, "\n")
	s2 := strings.Join(lines, "\n")
	return s1 + s2
}

func buildMetaData(struc *Field, built map[string]int) string {
	var lines, names []string
	var data [][]string
	suffix := ""
	n := built[struc.StructName]
	if n > 0 {
		suffix = fmt.Sprintf("_%d_", n)
	}
	fullName := struc.StructName + suffix
	fields := struc.Default.([]*Field)
	var s string
	for _, field := range fields {
		if field.Internal {
			continue
		}
		dataLine := []string{}
		s = fmt.Sprintf("offsetof(%s, %s)", struc.StructName, field.CName)
		dataLine = append(dataLine, s)

		tpName := field.Type.Name
		s = fmt.Sprintf("SettingType::%s", tpName)
		dataLine = append(dataLine, s)
		s = field.cdefault(built)
		dataLine = append(dataLine, s)

		names = append(names, field.Name)
		switch field.Type.Name {
		case "Struct", "Prerelease", "Compact", "Array":
			sublines := buildMetaData(field, built) // TODO: pass built?
			lines = append(lines, sublines)
			lines = append(lines, "")
			built[field.StructName]++
		case "Comment":
			// replace "offsetof(%s, %s)" with "(size_t)-1"
			dataLine[0] = "(size_t)-1"
		}
		data = append(data, dataLine)
	}
	s = fmt.Sprintf("static const FieldInfo g%sFields[] = {", fullName)
	lines = append(lines, s)
	dataLines := formatArrayLines(data)
	lines = append(lines, dataLines...)
	lines = append(lines, "};")
	// gFileStateInfo isn't const so that the number of fields can be changed at runtime (cf. UseDefaultState)
	constStr := ""
	if fullName != "FileState" {
		constStr = "const "
	}
	namesStr := strings.Join(names, "\\0")
	s = fmt.Sprintf("static %sStructInfo g%sInfo = { sizeof(%s), %d, g%sFields, \"%s\" };", constStr, fullName, struc.StructName, len(names), fullName, namesStr)
	lines = append(lines, s)
	return strings.Join(lines, "\n")
}

const settingsStructsHeader = `// !!!!! This file is auto-generated by do/settings_gen_code.go

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplifed BSD (see COPYING) */

struct RenderedBitmap;

enum class DisplayMode {
	// automatic means: the continuous form of single page, facing or
	// book view - depending on the document's desired PageLayout
	Automatic = 0,
	SinglePage,
	Facing,
	BookView,
	Continuous,
	ContinuousFacing,
	ContinuousBookView,
};

constexpr float kZoomFitPage = -1.f;
constexpr float kZoomFitWidth = -2.f;
constexpr float kZoomFitContent = -3.f;
constexpr float kZoomActualSize = 100.0f;
constexpr float kZoomMax = 6400.f; /* max zoom in % */
constexpr float kZoomMin = 8.33f;  /* min zoom in % */
constexpr float kInvalidZoom = -99.0f;

{{structDef}}

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

{{structMetadata}}

#endif
`

func genSettingsStruct() string {
	built := map[string]int{}
	structDef := buildStruct(globalPrefsStruct, built)
	structMetaData := buildMetaData(globalPrefsStruct, map[string]int{})

	content := settingsStructsHeader
	content = strings.Replace(content, "{{structDef}}", structDef, -1)
	content = strings.Replace(content, "{{structMetadata}}", structMetaData, -1)
	return content
}

func updateSumatraWebsite() string {
	dir := filepath.Join("..", "sumatra-website")
	dir, err := filepath.Abs(dir)
	logf(ctx(), "sumatra website dir: '%s'\n", dir)
	must(err)
	panicIf(!dirExists(dir), "directory for sumatra website '%s' doesn't exist", dir)
	panicIf(!isGitClean(dir), "github repository '%s' must be clean", dir)
	{
		cmd := exec.Command("git", "pull")
		cmd.Dir = dir
		runCmdLoggedMust(cmd)
	}
	dir = filepath.Join(dir, "server", "www")
	panicIf(!dirExists(dir), "directory for sumatra website '%s' doesn't exist", dir)
	return dir
}

func genAndSaveSettingsStructs() {
	websiteDir := updateSumatraWebsite()
	websiteSettingsDir := filepath.Join(websiteDir, "settings")
	ver := extractSumatraVersionMust()
	// this we do to work-around a bug in Cloudflare Pages that doesn't support '.' in file name
	verUrlized := strings.Replace(ver, ".", "-", -1)

	settingsFileName := fmt.Sprintf("settings%s.html", verUrlized)
	langsFileName := fmt.Sprintf("langs%s.html", verUrlized)

	helpURI := fmt.Sprintf("For documentation, see https://www.sumatrapdfreader.org/settings/settings%s.html", verUrlized)

	globalPrefs[0].Comment = helpURI

	s := genSettingsStruct()

	// TODO: port this
	/*
		beforeUseDefaultState = true
		for field in FileSettings:
			if field.name == "UseDefaultState":
				beforeUseDefaultState = false
			elif beforeUseDefaultState:
				assert field.name not in rememberedDisplayState, "%s shouldn't be serialized when UseDefaultState is true" % field.name
			else:
				assert field.name in rememberedDisplayState or field.internal, "%s won't be serialized when UseDefaultState is true" % field.name
	*/

	//fmt.Printf("%s\n", s)
	s = strings.Replace(s, "\n", "\r\n", -1)
	s = strings.Replace(s, "\t", "    ", -1)
	path := filepath.Join("src", "Settings.h")
	writeFileMust(path, []byte(s))
	detectClangFormat()
	clangFormatFile(path)
	fmt.Printf("Wrote '%s'\n", path)

	genLangsHTML := func() {
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
		ver := extractSumatraVersionMust()
		verUrlized := strings.Replace(ver, ".", "-", -1)
		s = strings.Replace(s, "%VER%", ver, -1)
		s = strings.Replace(s, "%VER_URL%", verUrlized, -1)
		s = strings.Replace(s, "settings.html", settingsFileName, -1)
		s = strings.Replace(s, "\n", "\r\n", -1)
		// undo html escaping that differs from Python
		// TODO: possibly remove
		//s = strings.Replace(s, "&#39;", "'", -1)

		path := filepath.Join(websiteSettingsDir, langsFileName)
		writeFileMust(path, []byte(s))
	}

	genSettingsHTML := func() {
		prefs := globalPrefsStruct
		inside := genStruct(prefs, "", false)
		s := strings.Replace(tmplHTML, "%INSIDE%", inside, -1)
		s = strings.Replace(s, "%VER%", extractSumatraVersionMust(), -1)
		s = strings.Replace(s, "langs.html", langsFileName, -1)
		s = strings.Replace(s, "\n", "\r\n", -1)
		// undo html escaping that differs from Python
		// TODO: possibly remove
		//s = strings.Replace(s, "&#39;", "'", -1)

		path := filepath.Join(websiteSettingsDir, settingsFileName)
		writeFileMust(path, []byte(s))
		fmt.Printf("Wrote '%s'\n", path)
	}

	genSettingsHTML()
	genLangsHTML()
	logf(ctx(), "!!!!!! checkin sumatra website repo!!!!\n")
}
