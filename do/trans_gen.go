package main

import (
	"fmt"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// Lang describes a single language
type Lang struct {
	desc     []string
	code     string // "af"
	name     string // "Afrikaans"
	msLangID string
	isRtl    bool
}

const compactCTmpl = `/*
 DO NOT EDIT MANUALLY !!!
 Generated with .\doit.bat -trans-regen
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace trans {

constexpr int kLangsCount = {{.LangsCount}};

const char *gLangCodes = {{.Langcodes}} "\0";

const char *gLangNames = {{.Langnames}} "\0";

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
const LANGID gLangIds[kLangsCount] = {
{{.Langids}}
};
#undef _LANGID

bool IsLangRtl(int idx)
{
  {{.Islangrtl}}
}

int gLangsCount = kLangsCount;

const LANGID *GetLangIds() { return &gLangIds[0]; }

} // namespace trans
`

// escape as octal number for C, as \nnn
func cOct(c byte) string {
	panicIf(c < 0x80)
	s := strconv.FormatInt(int64(c), 8) // base 8 for octal
	for len(s) < 3 {
		s = "0" + s
	}
	return `\` + s
}

func cEscapeForCompact(txt string) string {
	if len(txt) == 0 {
		return `"\0"`
	}
	// escape all quotes
	txt = strings.Replace(txt, `"`, `\"`, -1)
	// and all non-7-bit characters of the UTF-8 encoded string
	var res string
	n := len(txt)
	for i := 0; i < n; i++ {
		c := txt[i]
		if c < 0x80 {
			res += string(c)
			continue
		}
		res += cOct(c)
	}
	return fmt.Sprintf(`"%s\0"`, res)
}

// generate TranslationLangs.cpp
func genTranslationInfoCpp() {

	sort.Slice(gLangs, func(i, j int) bool {
		x := gLangs[i]
		y := gLangs[j]
		if x[0] == "en" {
			return true
		}
		if y[0] == "en" {
			return false
		}
		return x[1] < y[1]
	})

	getLangObjects := func(langDefs [][]string) []*Lang {
		newLang := func(desc []string) *Lang {
			panicIf(len(desc) > 4)
			res := &Lang{
				desc:     desc,
				code:     desc[0],
				name:     desc[1],
				msLangID: desc[2],
			}
			if len(desc) > 3 {
				panicIf(desc[3] != "RTL")
				res.isRtl = true
			}
			return res
		}

		var res []*Lang
		for _, desc := range langDefs {
			res = append(res, newLang(desc))
		}
		return res
	}

	langs := getLangObjects(gLangs)
	panicIf(langs[0].code != "en")

	logf("langs: %d, gLangs: %d\n", len(langs), len(gLangs))

	var a []string
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", cEscapeForCompact((lang.code)))
		a = append(a, s)
	}
	langcodes := strings.Join(a, " \\\n")
	logf("langcodes: %d bytes\n", len(langcodes))

	a = nil
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", cEscapeForCompact(lang.name))
		a = append(a, s)
	}
	langnames := strings.Join(a, " \\\n")
	logf("langnames: %d bytes\n", len(langnames))

	a = nil
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", lang.msLangID)
		a = append(a, s)
	}
	langids := strings.Join(a, ",\n")
	logf("langids: %d bytes\n", len(langids))

	var rtlInfo []string
	for idx, lang := range langs {
		if !lang.isRtl {
			continue
		}
		logf("lang rtl: %s %s\n", lang.code, lang.name)
		s := fmt.Sprintf("(%d == idx)", idx)
		rtlInfo = append(rtlInfo, s)
	}

	// there are 4 rtl langs but `langs` has incomplete langs removed,
	// so this check is not true
	if false && len(rtlInfo) != 4 {
		logf("len(rtlInfo) = %d (expected 4)\n", len(rtlInfo))
		panicIf(len(rtlInfo) != 4)
	}

	islangrtl := strings.Join(rtlInfo, " || ")
	if len(rtlInfo) == 0 {
		islangrtl = "false"
	}
	islangrtl = "return " + islangrtl + ";"
	//logf("islangrtl:\n%s\n", islangrtl)

	langsCount := len(langs)

	v2 := struct {
		LangsCount int
		Langcodes  string
		Langnames  string
		Langids    string
		Islangrtl  string
	}{
		LangsCount: langsCount,
		Langcodes:  langcodes,
		Langnames:  langnames,
		Langids:    langids,
		Islangrtl:  islangrtl,
	}
	path := filepath.Join("src", "TranslationLangs.cpp")
	fileContent := evalTmpl(compactCTmpl, v2)
	logf("fileContent: path: %s, file size: %d\n", path, len(fileContent))
	writeFileMust(path, []byte(fileContent))
	// print_stats(langs)
}
