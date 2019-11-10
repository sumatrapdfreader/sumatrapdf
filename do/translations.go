package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"
	"strings"

	"github.com/kjk/u"
)

var (
	SERVER = "www.apptranslator.org"
	PORT   = 80

	// when testing locally
	// SERVER = "172.21.12.12"  // mac book
	// SERVER = "10.37.129.2"    // mac pro
	// PORT = 5000
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
	uri := fmt.Sprintf("http://www.apptranslator.org/dltrans?app=%s&sha1=%s", app, sha1)
	d := httpDlMust(uri)
	return d
}

func generate_code(s string) {
	fmt.Print("generate_code\n")
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
