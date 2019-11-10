package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"
	"strings"

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
	sha1 = dummySha1()
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
			panicIf(len(parts) != 2)
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

func generate_code(s string) {
	fmt.Print("generate_code\n")
	strings_dict := parseTranslations(s)
	fmt.Printf("%d strings\n", len(strings_dict))
}

func downloadAndUpdateTranslationsIfChanged() bool {
	d := downloadTranslations()
	s := string(d)
	//fmt.Printf("Downloaded translations:\n%s\n", s)
	lines := strings.Split(s, "\n")
	panicIf(len(lines) < 2, "Bad response, less than 2 lines: '%s'", s)
	panicIf(lines[0] != "AppTranslator: SumatraPDF", "Bad response, invalid first line: '%s'", lines[0])
	sha1 := lines[1]
	if strings.HasPrefix(sha1, "No change") {
		fmt.Printf("skipping because translations haven't changed\n")
		return false
	}
	panicIf(!validSha1(sha1), "Bad reponse, invalid sha1 on second line: '%s'", sha1)
	fmt.Printf("Translation data size: %d\n", len(s))
	generate_code(s)
	saveLastDownload(d)
	return true
}

func downloadTranslationsMain() {
	changed := downloadAndUpdateTranslationsIfChanged()
	if changed {
		fmt.Printf("\nNew translations downloaded from the server! Check them in!")
	}
}
