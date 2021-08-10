package main

import (
	"fmt"
	"io"
	"net/http"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/kjk/u"
)

var (
	apptranslatoServer = "https://www.apptranslator.org"
)

func printSusTranslations(d []byte) {
	a := strings.Split(string(d), "\n")
	currString := ""
	isSus := func(s string) bool {
		/*
			if strings.Contains(s, `\n\n`) {
				return true
			}
		*/
		if strings.HasPrefix(s, `\n`) {
			return true
		}
		if strings.HasSuffix(s, `\n`) {
			return true
		}
		if strings.HasPrefix(s, `\r`) {
			return true
		}
		if strings.HasSuffix(s, `\r`) {
			return true
		}
		return false
	}

	for _, s := range a {
		if strings.HasPrefix(s, ":") {
			currString = s[1:]
			continue
		}
		if isSus(s) {
			fmt.Printf("Suspicious translation:\n%s\n%s\n\n", currString, s)
		}
	}
}

func downloadTranslations2() {
	timeStart := time.Now()
	defer func() {
		fmt.Printf("downloadTranslations2() finished in %s\n", time.Since(timeStart))
	}()
	strs := extractStringsFromCFilesNoPaths()
	sort.Strings(strs)
	fmt.Printf("uploading %d strings for translation\n", len(strs))
	// TODO: add secret
	uri := apptranslatoServer + "/api/dltransfor?app=SumatraPDF"
	s := strings.Join(strs, "\n")
	body := strings.NewReader(s)
	req, err := http.NewRequest(http.MethodPost, uri, body)
	must(err)
	client := http.DefaultClient
	rsp, err := client.Do(req)
	must(err)
	panicIf(rsp.StatusCode != http.StatusOK)
	d, err := io.ReadAll(rsp.Body)
	must(err)
	path := filepath.Join("strings", "translations.txt")
	u.WriteFileMust(path, d)
	fmt.Printf("Wrote response of size %d to %s\n", len(d), path)
	printSusTranslations(d)
}
