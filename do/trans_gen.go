package main

import (
	"fmt"
	"sort"
	"strings"
)

type Lang struct {
	desc                      []string
	code                      string // "af"
	name                      string // "Afrikaans"
	ms_lang_id                string
	isRtl                     bool
	code_safe                 string
	c_translations_array_name string
	translations              []string
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
		return x[0] < y[0]
	})
	langs := get_lang_objects(g_langs)
	panicIf("en" != langs[0].code)

	/*
		langs = build_trans_for_langs(langs, strings_dict, keys)

		langcodes = " \\\n".join(["  %s" % c_escape_for_compact(lang.code)
														for lang in langs])

		langnames = " \\\n".join(["  %s" % c_escape_for_compact(lang.name)
														for lang in langs])
		langids = ",\n".join(["  %s" % lang.ms_lang_id for lang in langs])

		rtl_info = ["(%d == idx)" % langs.index(lang)
								for lang in langs if lang.isRtl]
		islangrtl = "return %s;" % (" || ".join(rtl_info) or "false")

		build_translations(langs)

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
