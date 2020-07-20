package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/kjk/u"
)

const translationServer = "http://www.apptranslator.org"

func translationsPath() string {
	return filepath.Join("strings", "translations.txt")
}

func translationsSha1HexMust(d []byte) string {
	lines := toTrimmedLines(d)
	sha1 := lines[1]
	fatalIf(len(sha1) != 40, "lastTranslationsSha1HexMust: '%s' doesn't look like sha1", sha1)
	return sha1
}

func lastTranslationsSha1HexMust() string {
	d, err := ioutil.ReadFile(translationsPath())
	panicIfErr(err)
	return translationsSha1HexMust(d)
}

func saveTranslationsMust(d []byte) {
	err := ioutil.WriteFile(translationsPath(), d, 0644)
	panicIfErr(err)
}

func verifyTranslationsMust() {
	if flgSkipTranslationVerify {
		return
	}
	sha1 := lastTranslationsSha1HexMust()
	url := fmt.Sprintf("%s/dltrans?app=SumatraPDF&sha1=%s", translationServer, sha1)
	d := httpDlMust(url)
	lines := toTrimmedLines(d)
	fatalIf(lines[1] != "No change", "translations changed, run ./doit.bat -trans-dl\n")
}

func validSha1(s string) bool {
	return len(s) == 40
}

func lastDownloadFilePath() string {
	return filepath.Join("strings", "translations.txt")
}

func dummySha1() string {
	s := ""
	for i := 0; i < 10; i++ {
		s += "0000"
	}
	return s
}

func lastDownloadHash() string {
	path := lastDownloadFilePath()
	if !u.FileExists(path) {
		return dummySha1()
	}
	d := u.ReadFileMust(path)
	lines := toTrimmedLines(d)
	sha1 := lines[1]
	u.PanicIf(!validSha1(sha1), "'%s' is not a valid sha1", sha1)
	return sha1
}

func saveLastDownload(d []byte) {
	path := lastDownloadFilePath()
	u.WriteFileMust(path, d)
}

func downloadTranslations() []byte {
	logf("Downloading translations from the server...\n")

	app := "SumatraPDF"
	sha1 := lastDownloadHash()
	// when testing locally
	// SERVER = "172.21.12.12"  // mac book
	// SERVER = "10.37.129.2"    // mac pro
	// PORT = 5000
	uri := fmt.Sprintf("http://www.apptranslator.org/dltrans?app=%s&sha1=%s", app, sha1)
	d := httpDlMust(uri)
	return d
}

// Translation describes a single translated text
type Translation struct {
	Text        string
	Lang        string
	Translation string
}

func trimEmptyLinesFromEnd(a []string) []string {
	for len(a) > 0 {
		lastIdx := len(a) - 1
		s := strings.TrimSpace(a[lastIdx])
		if len(s) > 0 {
			return a
		}
		a = a[:lastIdx]
	}
	return a
}

func parseTranslations(s string) map[string][]*Translation {
	res := map[string][]*Translation{}
	lines := strings.Split(s, "\n")[2:]
	// strip empty lines from the end
	lines = trimEmptyLinesFromEnd(lines)
	currStr := ""
	var currTranslations []*Translation
	for _, l := range lines {
		if len(l) == 0 {
			continue
		}
		// TODO: looks like apptranslator doesn't deal well with strings that
		// have newlines in them. Newline at the end ends up as an empty line
		// apptranslator should escape newlines and tabs etc. but for now
		// skip those lines as harmless
		if l[0] == ':' {
			if currStr != "" {
				panicIf(len(currTranslations) == 0)
				res[currStr] = currTranslations
			}
			currStr = l[1:]
			currTranslations = nil
		} else {
			parts := strings.SplitN(l, ":", 2)
			panicIf(len(parts) != 2, "Invalid line: '%s'", l)
			lang, trans := parts[0], parts[1]
			tr := &Translation{
				Text:        currStr,
				Lang:        lang,
				Translation: trans,
			}
			currTranslations = append(currTranslations, tr)
		}
	}

	if currStr != "" {
		panicIf(len(currTranslations) == 0)
		res[currStr] = currTranslations
	}

	return res
}

func shouldTranslate(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".cpp"
}

var (
	dirsToProcess = []string{"src"}
)

func getFilesToProcess() []string {
	var res []string
	for _, dir := range dirsToProcess {
		files, err := ioutil.ReadDir(dir)
		must(err)
		for _, f := range files {
			path := filepath.Join(dir, f.Name())
			if shouldTranslate(path) {
				res = append(res, path)
			}
		}
	}
	return res
}

var (
	translationPattern = regexp.MustCompile(`\b_TRN?\("(.*?)"\)`)
)

func extractTranslations(s string) []string {
	var res []string
	a := translationPattern.FindAllStringSubmatch(s, -1)
	for _, el := range a {
		res = append(res, el[1])
	}
	return res
}

func extractStringsFromCFile(path string) []string {
	d := u.ReadFileMust(path)
	return extractTranslations(string(d))
}

func uniquifyStrings(a []string) []string {
	m := map[string]bool{}
	for _, s := range a {
		m[s] = true
	}
	var res []string
	for k := range m {
		res = append(res, k)
	}
	return res
}

func extractStringsFromCFilesNoPaths() []string {
	filesToProcess := getFilesToProcess()
	logf("Files to process: %d\n", len(filesToProcess))
	var res []string
	for _, path := range filesToProcess {
		a := extractStringsFromCFile(path)
		res = append(res, a...)
	}
	res = uniquifyStrings(res)
	logf("%d strings to translate\n", len(res))
	return res
}

type stringWithPath struct {
	Text string
	Path string
	Dir  string
}

func extractStringsFromCFiles() []*stringWithPath {
	filesToProcess := getFilesToProcess()
	logf("Files to process: %d\n", len(filesToProcess))
	var res []*stringWithPath
	for _, path := range filesToProcess {
		a := extractStringsFromCFile(path)
		for _, s := range a {
			swp := &stringWithPath{
				Text: s,
				Path: path,
				Dir:  filepath.Base(filepath.Dir(path)),
			}
			res = append(res, swp)
		}
	}
	logf("%d strings to translate\n", len(res))
	return res
}

func extractJustStrings(a []*stringWithPath) []string {
	var res []string
	for _, el := range a {
		res = append(res, el.Text)
	}
	res = uniquifyStrings(res)
	return res
}

func dumpMissingPerLanguage(strings []string, stringsDict map[string][]*Translation, dumpStrings bool) map[string]bool {
	/*
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
	return nil
}

func getUntranslatedAsList(untranslatedDict map[string]bool) []string {
	var a []string
	for s := range untranslatedDict {
		a = append(a, s)
	}
	return uniquifyStrings(a)
}

func generateCode(s string) {
	fmt.Print("generate_code\n")
	stringsDict := parseTranslations(s)
	logf("%d strings\n", len(stringsDict))

	strings := extractStringsFromCFiles()
	stringsList := extractJustStrings(strings)

	// remove obsolete strings from the server
	var obsolete []string
	for s := range stringsDict {
		if !u.StringInSlice(stringsList, s) {
			obsolete = append(obsolete, s)
			delete(stringsDict, s)
		}
	}
	if len(obsolete) > 0 {
		logf("Removed %d obsolete strings\n", len(obsolete))
	}

	untranslatedDict := dumpMissingPerLanguage(stringsList, stringsDict, false)
	untranslated := getUntranslatedAsList(untranslatedDict)
	if len(untranslated) > 0 {
		logf("%d untranslated\n", len(untranslated))
		// add untranslated
		for _, s := range untranslated {
			if _, ok := stringsDict[s]; !ok {
				stringsDict[s] = []*Translation{}
			}
		}
	}
	genCCode(stringsDict, strings)
}

func downloadAndUpdateTranslationsIfChanged() bool {
	d := downloadTranslations()
	s := string(d)
	//logf("Downloaded translations:\n%s\n", s)
	lines := strings.Split(s, "\n")
	panicIf(len(lines) < 2, "Bad response, less than 2 lines: '%s'", s)
	panicIf(lines[0] != "AppTranslator: SumatraPDF", "Bad response, invalid first line: '%s'", lines[0])
	sha1 := lines[1]
	if strings.HasPrefix(sha1, "No change") {
		logf("skipping because translations haven't changed\n")
		return false
	}
	panicIf(!validSha1(sha1), "Bad reponse, invalid sha1 on second line: '%s'", sha1)
	logf("Translation data size: %d\n", len(s))
	generateCode(s)
	saveLastDownload(d)
	return true
}

func downloadTranslationsMain() {
	changed := downloadAndUpdateTranslationsIfChanged()
	if changed {
		logf("\nNew translations downloaded from the server! Check them in!\n")
	}
}

func regenerateLangs() {
	d := u.ReadFileMust(lastDownloadFilePath())
	generateCode(string(d))
}
