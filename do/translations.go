package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"

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

func lastDownloadHash() string {
	path := lastDownloadFilePath()
	if !u.FileExists(path) {
		// return dummy sha1
		s := ""
		for i := 0; i < 10; i++ {
			s += "00000"
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

	app := "SumatraPDF"
	sha1 := lastDownloadHash()
	uri := fmt.Sprintf("http://www.apptranslator.org/dltrans?app=%s&sha1=%s", app, sha1)
	d := httpDlMust(uri)
	return d
}

func downloadAndUpdateTranslationsIfChanged() bool {
	d := downloadTranslations()
	s := string(d)
	fmt.Printf("Downloaded translations:\n%s\n", s)
	return false
	/*
	   try:
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
}

func downloadTranslationsMain() {
	changed := downloadAndUpdateTranslationsIfChanged()
	if changed {
		fmt.Printf("\nNew translations downloaded from the server! Check them in!")
	}
}
