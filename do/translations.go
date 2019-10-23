package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"

	"github.com/kjk/u"
)

func translationsPath() string {
	return pj("strings", "translations.txt")
}

func translationsSha1HexMust(d []byte) string {
	lines := toTrimmedLines(d)
	sha1 := lines[1]
	fatalIf(len(sha1) != 40, "lastTranslationsSha1HexMust: '%s' doesn't look like sha1", sha1)
	return sha1
}

func lastTranslationsSha1HexMust() string {
	d, err := ioutil.ReadFile(translationsPath())
	fatalIfErr(err)
	return translationsSha1HexMust(d)
}

func saveTranslationsMust(d []byte) {
	err := ioutil.WriteFile(translationsPath(), d, 0644)
	fatalIfErr(err)
}

func verifyTranslationsMust() {
	sha1 := lastTranslationsSha1HexMust()
	url := fmt.Sprintf("http://www.apptranslator.org/dltrans?app=SumatraPDF&sha1=%s", sha1)
	d := httpDlMust(url)
	lines := toTrimmedLines(d)
	fatalIf(lines[1] != "No change", "translations changed, run python scripts/trans_download.py\n")
}

func validSha1(s string) bool {
	return len(s) == 40
}

func lastDownloadFilePath() string {
	return filepath.Join("strings", "translations.txt")
}

func lastDownloadHash() string {
	path := lastDownloadFilePath()
	if !u.FileExists(path) {
		// return dummy sha1
		s := ""
		for i := 0; i < 40; i++ {
			s += "0"
		}
		return s
	}
	d := u.ReadFileMust(path)
	lines := toTrimmedLines(d)
	sha1 := lines[1]
	u.PanicIf(!validSha1(sha1), "'%s' is not a valid sha1", sha1)
	return sha1
}

func saveLastDownload(s []byte) {
	path := lastDownloadFilePath()
	u.WriteFileMust(path, s)
}

func downloadTranslations() []byte {
	logf("Downloading translations from the server...\n")
	uri := "http://www.apptranslator.org/dltrans?app=%s&sha1=%s"
	d := httpDlMust(uri)
	return d

}

/*
# Returns 'strings' dict that maps an original, untranslated string to
# an array of translation, where each translation is a tuple
# (language, text translated into this language)
def parseTranslations(s):
    lines = [l for l in s.split("\n")[2:]]
    # strip empty lines from the end
    if len(lines[-1]) == 0:
        lines = lines[:-1]
    strings = {}
    curr_str = None
    curr_translations = None
    for l in lines:
        #print("'%s'" % l)
        #TODO: looks like apptranslator doesn't deal well with strings that
        # have newlines in them. Newline at the end ends up as an empty line
        # apptranslator should escape newlines and tabs etc. but for now
        # skip those lines as harmless
        if len(l) == 0:
            continue
        if l[0] == ':':
            if curr_str != None:
                assert curr_translations != None
                strings[curr_str] = curr_translations
            curr_str = l[1:]
            curr_translations = []
        else:
            (lang, trans) = l.split(":", 1)
            curr_translations.append([lang, trans])
    if curr_str != None:
        assert curr_translations != None
        strings[curr_str] = curr_translations
    return strings
*/

/*
def get_lang_list(strings_dict):
    langs = []
    for translations in strings_dict.values():
        for t in translations:
            lang = t[0]
            if lang not in langs:
                langs.append(lang)
    return langs
*/

/*
def get_missing_for_language(strings, strings_dict, lang):
    untranslated = []
    for s in strings:
        if not s in strings_dict:
            untranslated.append(s)
            continue
        translations = strings_dict[s]
        found = filter(lambda tr: tr[0] == lang, translations)
        if not found and s not in untranslated:
            untranslated.append(s)
    return untranslated
*/

/*
def langs_sort_func(x, y):
    return cmp(len(y[1]), len(x[1])) or cmp(x[0], y[0])
*/

/*
def dump_missing_per_language(strings, strings_dict, dump_strings=False):
    untranslated_dict = {}
    for lang in get_lang_list(strings_dict):
        untranslated_dict[lang] = get_missing_for_language(
            strings, strings_dict, lang)
    items = untranslated_dict.items()
    items.sort(langs_sort_func)

    print("\nMissing translations:")
    strs = []
    for (lang, untranslated) in items:
        if len(untranslated) > 0:
            strs.append("%5s: %3d" % (lang, len(untranslated)))
    per_line = 5
    while len(strs) > 0:
        line_strs = strs[:per_line]
        strs = strs[per_line:]
        print("  ".join(line_strs))
    return untranslated_dict
*/

/*
def get_untranslated_as_list(untranslated_dict):
    return util.uniquify(sum(untranslated_dict.values(), []))
*/

/*
# Generate the various Translations_txt.cpp files based on translations
# in s that we downloaded from the server


def generate_code(s):
    strings_dict = parseTranslations(s)
    strings = extract_strings_from_c_files(True)
    strings_list = [tmp[0] for tmp in strings]
    for s in strings_dict.keys():
        if s not in strings_list:
            del strings_dict[s]
    untranslated_dict = dump_missing_per_language(strings_list, strings_dict)
    untranslated = get_untranslated_as_list(untranslated_dict)
    for s in untranslated:
        if s not in strings_dict:
            strings_dict[s] = []
	gen_c_code(strings_dict, strings)
*/

/*
def downloadAndUpdateTranslationsIfChanged():
    try:
        s = downloadTranslations()
    except:
        # might fail due to intermitten network problems, ignore that
        print("skipping because downloadTranslations() failed")
        return
    lines = s.split("\n")
    if len(lines) < 2:
        print("Bad response, less than 2 lines: '%s'" % s)
        return False
    if lines[0] != "AppTranslator: SumatraPDF":
        print("Bad response, invalid first line: '%s'" % lines[0])
        print(s)
        return False
    sha1 = lines[1]
    if sha1.startswith("No change"):
        print("skipping because translations haven't changed")
        return False
    if not validSha1(sha1):
        print("Bad reponse, invalid sha1 on second line: '%s'", sha1)
        return False
    print("Translation data size: %d" % len(s))
    # print(s)
    generate_code(s)
    saveLastDownload(s)
    return True
*/

/*
def regenerateLangs():
    s = open(lastDownloadFilePath(), "rb").read()
    generate_code(s)
    sys.exit(1)
*/
