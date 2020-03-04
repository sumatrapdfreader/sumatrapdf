package main

import (
	"fmt"
	"strings"
)

type Type struct {
	Name  string
	Ctype string
}

var (
	Bool       = &Type{"Bool", "bool"}
	Color      = &Type{"Color", "COLORREF"}
	Float      = &Type{"Float", "float"}
	Int        = &Type{"Int", "int"}
	String     = &Type{"String", "WCHAR*"}
	Utf8String = &Type{"Utf8String", "char*"}
	Comment    = &Type{"Comment", ""}
)

type Field struct {
	Name       string
	Type       *Type
	Default    interface{}
	Comment    string
	Internal   string
	CName      string
	Expert     bool // expert prefs are not exposed by the UI
	DocComment string
	Version    string // version in which this setting was introduced
	PreRelease bool   // prefs which aren't written out in release builds

	StructName string // only valid form some types
}

func (self *Field) cdefault(built map[string]int) string {
	if self.Type == Bool {
		if self.Default == nil {
			return "false"
		}
		return "true"
	}

	if self.Type == Color {
		return fmt.Sprintf("0x%06x", self.Default)
	}

	if self.Type == Float {
		// converting float to intptr_t rounds the value
		return fmt.Sprintf(`(intptr_t)"%g"`, self.Default)
	}

	if self.Type == Int {
		return fmt.Sprintf("%d", self.Default)
	}

	if self.Type == String {
		if self.Default == nil {
			return `(intptr_t)L"0"` // TODO: is this correct?
		}
		return fmt.Sprintf(`(intptr_t)L"%s"`, self.Default)
	}

	if self.Type == Utf8String {
		if self.Default == nil {
			return `(intptr_t)"0"` // TODO: is this correct?
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	typeName := self.Type.Name
	switch typeName {
	case "Struct", "Array", "Compact", "Prerelease":
		id := built[self.StructName]
		if id == 0 {
			return fmt.Sprintf("(intptr_t)&g%sInfo", self.StructName)
		}
		return fmt.Sprintf("(intptr_t)&g%sInfo_%d_", self.StructName, id)
	}

	switch typeName {
	case "ColorArray", "FloatArray", "IntArray":
		if self.Default == nil {
			return `(intptr_t)"0"`
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	if typeName == "StringArray" {
		if self.Default == nil {
			return `(intptr_t)"0"`
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Default)
	}

	if typeName == "Comment" {
		if self.Comment == "" {
			return `(intptr_t)"0"`
		}
		return fmt.Sprintf(`(intptr_t)"%s"`, self.Comment)
	}
	return ""
}

func toCName(s string) string {
	return s
}

func MkField(name string, typ *Type, def interface{}, comment string) *Field {
	res := &Field{
		Name:       name,
		Type:       typ,
		Default:    def,
		Comment:    comment,
		DocComment: comment,
		Version:    "2.3",
	}
	if name != "" {
		res.CName = strings.ToLower(name[0:1]) + name[1:]
	}
	return res
}

func RGB(r uint32, g uint32, b uint32) uint32 {
	var a uint32 = 0
	return r | (g << 8) | (b << 16) | (a << 24)
}

func get_sumatrapdf_version() string {
	// TODO: implement me
	return "3.2"
}

func MkStruct(name string, fields []*Field, comment string) *Field {
	structName := name
	typ := &Type{"Struct", structName}
	res := MkField(name, typ, fields, comment)
	res.StructName = structName
	return res
}

func (f *Field) SetPreRelease() *Field {
	f.PreRelease = true
	f.Type.Name = "Prerelease"
	return f
}

func MkComment(comment string) *Field {
	return MkField("", Comment, nil, comment)
}

func (f *Field) SetExpert() *Field {
	f.Expert = true
	return f
}

func (f *Field) SetVersion(v string) *Field {
	f.Version = v
	return f
}

// ##### setting definitions for SumatraPDF #####

var (
	WindowPos = []*Field{
		MkField("X", Int, 0, "y coordinate"),
		MkField("Y", Int, 0, "y coordinate"),
		MkField("Dx", Int, 0, "width"),
		MkField("Dy", Int, 0, "height"),
	}

	GlobalPrefs = []*Field{
		MkComment(fmt.Sprintf("For documentation, see https://www.sumatrapdfreader.org/settings%s.html", get_sumatrapdf_version())),
	}

	GlobalPrefsStruct = MkStruct("GlobalPrefs", GlobalPrefs,
		"Most values on this structure can be updated through the UI and are persisted "+
			"in SumatraPDF-settings.txt (previously in sumatrapdfprefs.dat)")
)

func BuildStruct(struc *Field, built map[string]int) string {
	// lines := []string{
	// 	fmt.Sprintf("struct %s {", struc.StructName),
	// }
	// required := []string{}
	// if struc.Comment != "" {
	// }
	return ""
}

func genSettingsStruct() string {
	built := map[string]int{}
	structDef := BuildStruct(GlobalPrefsStruct, built)
	return structDef
}
