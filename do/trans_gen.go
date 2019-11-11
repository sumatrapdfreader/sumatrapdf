package main

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
)

// number of missing translations for a language to be considered
// incomplete (will be excluded from Translations_txt.cpp) as a
// percentage of total string count of that specific file
const INCOMPLETE_MISSING_THRESHOLD = 0.2

type Lang struct {
	desc                      []string
	code                      string // "af"
	name                      string // "Afrikaans"
	ms_lang_id                string
	isRtl                     bool
	code_safe                 string
	c_translations_array_name string
	translations              []string
	c_escaped_lines           []string
	seq                       string
}

func NewLang(desc []string) *Lang {
	panicIf(len(desc) > 4)
	res := &Lang{
		desc:       desc,
		code:       desc[0],
		name:       desc[1],
		ms_lang_id: desc[2],
	}
	if len(desc) > 3 {
		panicIf(desc[3] != "RTL")
		res.isRtl = true
	}
	// code that can be used as part of C identifier i.e.:
	// "ca-xv" => "ca_xv"
	res.code_safe = strings.Replace(res.code, "-", "_", -1)
	res.c_translations_array_name = "gTranslations_" + res.code_safe
	return res
}

func get_lang_objects(langs_defs [][]string) []*Lang {
	var res []*Lang
	for _, desc := range langs_defs {
		res = append(res, NewLang(desc))
	}
	return res
}

func get_trans_for_lang(strings_dict map[string][]*Translation, keys []string, lang_arg string) []string {
	if lang_arg == "en" {
		return keys
	}
	var trans []string
	var untrans []string
	for _, k := range keys {
		var found []string
		for _, trans := range strings_dict[k] {
			if trans.Lang == lang_arg {
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

	if len(untrans) > int(INCOMPLETE_MISSING_THRESHOLD*float64(len(keys))) {
		return nil
	}
	return trans
}

var g_incomplete_langs []*Lang

func removeLang(langs []*Lang, lang *Lang) []*Lang {
	for idx, el := range langs {
		if el == lang {
			return append(langs[:idx], langs[idx+1:]...)
		}
	}
	panic("didn't find lang in langs")
}

func build_trans_for_langs(langs []*Lang, strings_dict map[string][]*Translation, keys []string) []*Lang {
	g_incomplete_langs = nil
	for _, lang := range langs {
		lang.translations = get_trans_for_lang(strings_dict, keys, lang.code)
		if len(lang.translations) == 0 {
			g_incomplete_langs = append(g_incomplete_langs, lang)
		}
	}
	fmt.Printf("g_incomplete_langs: %d\n", len(g_incomplete_langs))
	for _, il := range g_incomplete_langs {
		nBefore := len(langs)
		langs = removeLang(langs, il)
		panicIf(len(langs) != nBefore-1)
	}
	return langs
}

// escape as octal number for C, as \nnn
func c_oct(c byte) string {
	panicIf(c < 0x80)
	s := strconv.FormatInt(int64(c), 8) // base 8 for octal
	for len(s) < 3 {
		s = "0" + s
	}
	return `\` + s
}

func c_escape(txt string) string {
	if len(txt) == 0 {
		return `"NULL"`
	}
	// escape all quotes
	txt = strings.Replace(txt, `"`, `\"`, -1)
	// and all non-7-bit characters of the UTF-8 encoded string
	res := ""
	for i := range txt {
		c := txt[i]
		if c < 0x80 {
			res += string(c)
			continue
		}
		res += c_oct(c)
	}
	return fmt.Sprintf(`"%s"`, res)
}

func c_escape_for_compact(txt string) string {
	if len(txt) == 0 {
		return `"\0"`
	}
	// escape all quotes
	txt = strings.Replace(txt, `"`, `\"`, -1)
	// and all non-7-bit characters of the UTF-8 encoded string
	var res string
	for i := range txt {
		c := txt[i]
		if c < 0x80 {
			res += string(c)
			continue
		}
		res += c_oct(c)
	}
	return fmt.Sprintf(`"%s\0"`, res)
}

func build_translations(langs []*Lang) {
	for _, lang := range langs[1:] {
		var c_escaped []string
		seq := ""
		for _, t := range lang.translations {
			s := fmt.Sprintf("  %s", c_escape_for_compact(t))
			c_escaped = append(c_escaped, s)
			seq += t
			seq += `\0`
		}
		lang.c_escaped_lines = c_escaped
		lang.seq = seq
	}
}

func gen_c_code_for_dir(strings_dict map[string][]*Translation, keys []string, dir_name string) {
	fmt.Printf("gen_c_code_for_dir: '%s', %d strings, len(strings_dict): %d\n", dir_name, len(keys), len(strings_dict))

	sort.Slice(g_langs, func(i, j int) bool {
		x := g_langs[i]
		y := g_langs[j]
		if x[0] == "en" {
			return true
		}
		if y[0] == "en" {
			return false
		}
		return x[1] < y[1]
	})
	langs := get_lang_objects(g_langs)
	panicIf("en" != langs[0].code)

	langs = build_trans_for_langs(langs, strings_dict, keys)
	fmt.Printf("langs: %d, g_langs: %d\n", len(langs), len(g_langs))

	var a []string
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", c_escape_for_compact((lang.code)))
		a = append(a, s)
	}
	langcodes := strings.Join(a, " \\\n")
	fmt.Printf("langcodes:\n%s\n", langcodes)

	a = nil
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", c_escape_for_compact(lang.name))
		a = append(a, s)
	}
	langnames := strings.Join(a, " \\\n")
	fmt.Printf("langnames:\n%s\n", langnames)

	a = nil
	for _, lang := range langs {
		s := fmt.Sprintf("  %s", lang.ms_lang_id)
		a = append(a, s)
	}
	langids := strings.Join(a, ",\n")
	fmt.Printf("langids:\n%s\n", langids)

	var rtl_info []string
	n := 0
	for idx, lang := range langs {
		if !lang.isRtl {
			continue
		}
		s := fmt.Sprintf("(%d == idx)", idx)
		rtl_info = append(rtl_info, s)
		n++
	}
	// TODO: only 1, should be 4
	fmt.Printf("n: %d, rtl_info: %#v\n", n, rtl_info)
	panicIf(len(rtl_info) != 4)

	islangrtl := strings.Join(rtl_info, " || ")
	if len(rtl_info) == 0 {
		islangrtl = "false"
	}
	islangrtl = "return " + islangrtl + ";"
	fmt.Printf("islangrtl:\n%s\n", islangrtl)
	build_translations(langs)

	/*

		translations_refs = "  NULL,\n" + \
				", \n".join(["  %s" %
										lang.c_translations_array_name for lang in langs[1:]])
		translations = gen_translations(langs)
		translations = uncompressed_tmpl % locals()

		lines = ["  %s" % c_escape(t) for t in langs[0].translations]
		orignal_strings = ",\n".join(lines)

		langs_count = len(langs)
		translations_count = len(keys)
		file_content = compact_c_tmpl % locals()
		file_path = os.path.join(
				SRC_DIR, dir_name, file_name_from_dir_name(dir_name))
		file(file_path, "wb").write(file_content)

		print_incomplete_langs(dir_name)
		# print_stats(langs)
	*/
}

func gen_c_code(strings_dict map[string][]*Translation, strings2 []*StringWithPath) {
	for _, dir := range dirsToProcess {
		var keys []string
		for _, el := range strings2 {
			if strings.HasPrefix(el.Path, dir) {
				s := el.Text
				if _, ok := strings_dict[s]; ok {
					keys = append(keys, s)
				}
			}
			keys = uniquifyStrings(keys)
			sort.Slice(keys, func(i, j int) bool {
				a := strings.Replace(keys[i], `\t`, "\t", -1)
				b := strings.Replace(keys[j], `\t`, "\t", -1)
				return a < b
			})
		}
		gen_c_code_for_dir(strings_dict, keys, dir)
	}
}
