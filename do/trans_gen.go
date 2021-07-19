package main

import (
	"fmt"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/kjk/u"
)

// number of missing translations for a language to be considered
// incomplete (will be excluded from Translations_txt.cpp)
const incompleteMissingThreshold = 100

// Lang describes a single language
type Lang struct {
	desc          []string
	code          string // "af"
	name          string // "Afrikaans"
	msLangID      string
	isRtl         bool
	translations  []string // TODO: maybe remove
	cEscapedLines []string
	seq           string
}

// NewLang creates a new language
func NewLang(desc []string) *Lang {
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

func getLangObjects(langDefs [][]string) []*Lang {
	var res []*Lang
	for _, desc := range langDefs {
		res = append(res, NewLang(desc))
	}
	return res
}

func getTransForLang(stringsDict map[string][]*Translation, keys []string, langArg string) []string {
	if langArg == "en" {
		return keys
	}
	var trans []string
	var untrans []string
	for _, k := range keys {
		var found []string
		for _, trans := range stringsDict[k] {
			if trans.Lang == langArg {
				found = append(found, trans.Translation)
			}
		}
		if len(found) > 0 {
			panicIf(len(found) != 1)
			// don't include a translation, if it's the same as the default
			if found[0] == k {
				found[0] = ""
			}
			trans = append(trans, found[0])
		} else {
			trans = append(trans, "")
			untrans = append(untrans, k)
		}
	}

	if len(untrans) > int(incompleteMissingThreshold) {
		return nil
	}
	return trans
}

var gIncompleteLangs []*Lang

func removeLang(langs []*Lang, lang *Lang) []*Lang {
	for idx, el := range langs {
		if el == lang {
			return append(langs[:idx], langs[idx+1:]...)
		}
	}
	panic("didn't find lang in langs")
}

func buildTransForLangs(langs []*Lang, stringsDict map[string][]*Translation, keys []string) []*Lang {
	gIncompleteLangs = nil
	for _, lang := range langs {
		lang.translations = getTransForLang(stringsDict, keys, lang.code)
		if len(lang.translations) == 0 {
			gIncompleteLangs = append(gIncompleteLangs, lang)
		}
	}
	logf("gIncompleteLangs: %d\n", len(gIncompleteLangs))
	panicIf(len(gIncompleteLangs) > 20) // should be ~10
	for _, il := range gIncompleteLangs {
		nBefore := len(langs)
		langs = removeLang(langs, il)
		panicIf(len(langs) != nBefore-1)
	}
	return langs
}

const compactCTmpl = `/*
 DO NOT EDIT MANUALLY !!!
 Generated with .\doit.bat -trans-regen
*/

#include "utils/BaseUtil.h"

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

func fileNameFromDirName(dirName string) string {
	// strip "src/"" from dir_name
	s := strings.TrimPrefix(dirName, "src")
	if len(s) > 0 {
		s = s[1:]
	}
	if s == "" {
		return "Trans_sumatra_txt.cpp"
	}
	return fmt.Sprintf("Trans_%s_txt.cpp", s)
}

func buildTranslations(langs []*Lang) {
	for _, lang := range langs[1:] {
		var cEscaped []string
		seq := ""
		for _, t := range lang.translations {
			s := fmt.Sprintf("  %s", cEscapeForCompact(t))
			cEscaped = append(cEscaped, s)
			seq += t
			seq += `\0`
		}
		lang.cEscapedLines = cEscaped
		lang.seq = seq
	}
}

func printIncompleteLangs(dirName string) {
	var a []string
	for _, lang := range gIncompleteLangs {
		a = append(a, lang.code)
	}
	langs := strings.Join(a, ", ")
	count := fmt.Sprintf("%d out of %d", len(gIncompleteLangs), len(gLangs))
	logf("\nIncomplete langs in %s: %s %s", fileNameFromDirName(dirName), count, langs)
}

func genCCodeForDir(stringsDict map[string][]*Translation, keys []string, dirName string) {
	logf("genCCodeForDir: '%s', %d strings, len(stringsDict): %d\n", dirName, len(keys), len(stringsDict))

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
	langs := getLangObjects(gLangs)
	panicIf(langs[0].code != "en")

	langs = buildTransForLangs(langs, stringsDict, keys)
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
	buildTranslations(langs)

	langsCount := len(langs)
	translationsCount := len(keys)

	v2 := struct {
		LangsCount        int
		TranslationsCount int
		Langcodes         string
		Langnames         string
		Langids           string
		Islangrtl         string
	}{
		LangsCount:        langsCount,
		TranslationsCount: translationsCount,
		Langcodes:         langcodes,
		Langnames:         langnames,
		Langids:           langids,
		Islangrtl:         islangrtl,
	}
	path := filepath.Join(dirName, fileNameFromDirName(dirName))
	fileContent := evalTmpl(compactCTmpl, v2)
	logf("fileContent: path: %s, file size: %d\n", path, len(fileContent))
	u.WriteFileMust(path, []byte(fileContent))
	printIncompleteLangs(dirName)
	// print_stats(langs)
}

func genCCode(stringsDict map[string][]*Translation, strings2 []*stringWithPath) {
	for _, dir := range dirsToProcess {
		dirToCheck := filepath.Base(dir)
		var keys []string
		for _, el := range strings2 {
			if el.Dir == dirToCheck {
				s := el.Text
				if _, ok := stringsDict[s]; ok {
					keys = append(keys, s)
				}
			}
		}
		keys = uniquifyStrings(keys)
		sort.Slice(keys, func(i, j int) bool {
			a := strings.Replace(keys[i], `\t`, "\t", -1)
			b := strings.Replace(keys[j], `\t`, "\t", -1)
			return a < b
		})
		genCCodeForDir(stringsDict, keys, dir)
	}
}
