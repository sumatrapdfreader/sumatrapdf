package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/u"
)

func readFile(path string) string {
	// it's ok if file doesn't ext
	d, _ := ioutil.ReadFile(path)
	return string(d)
}

func uploadStringsToServer(strs string, secret string) {
	fmt.Printf("Uploading strings to the server...\n")
	uri := fmt.Sprintf("%s/uploadstrings", translationServer)

	data := url.Values{}
	data.Set("strings", strs)
	data.Set("app", "SumatraPDF")
	data.Set("secret", secret)
	dataStr := data.Encode()
	r := strings.NewReader(dataStr)
	req, err := http.NewRequest(http.MethodPost, uri, r)
	must(err)
	req.Header.Add("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Add("Accept", "text/plain")
	req.Header.Add("Content-Length", strconv.Itoa(len(dataStr)))
	rsp, err := http.DefaultClient.Do(req)
	must(err)
	defer rsp.Body.Close()
	u.PanicIf(rsp.StatusCode != http.StatusOK)
	d, err := ioutil.ReadAll(rsp.Body)
	must(err)
	fmt.Printf("Response:\n%s\n", string(d))
	fmt.Printf("Upload finished\n")
}

func getTransSecret() string {
	v := os.Getenv("TRANS_UPLOAD_SECRET")
	panicIf(v == "", "must set TRANS_UPLOAD_SECRET env variable")
	return v
}

func uploadStringsIfChanged() {
	path := filepath.Join("strings", "last_uploaded.txt")
	// needs to have upload secret to protect apptranslator.org server from abuse
	// TODO: we used to have a check if svn is up-to-date
	// should we restore it for git?
	a1 := extractStringsFromCFiles()
	a := extractJustStrings(a1)
	sort.Strings(a)
	s := "AppTranslator strings\n" + strings.Join(a, "\n")
	if s == readFile(path) {
		logf("Skipping upload because strings haven't changed since last upload\n")
		return
	}
	uploadsecret := getTransSecret()
	uploadStringsToServer(s, uploadsecret)
	u.WriteFileMust(path, []byte(s))
	logf("Don't forget to checkin strings/last_uploaded.txt\n")
}

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
	secret := getTransSecret()
	uri := apptranslatoServer + "/api/dltransfor?app=SumatraPDF&secret=" + secret
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
