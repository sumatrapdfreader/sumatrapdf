package main

import (
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/kjk/u"
)

func readFile(path string) string {
	// it's ok if file doesn't ext
	d, _ := ioutil.ReadFile(path)
	return string(d)
}

func uploadStringsToServer(strs string, secret string) {
	fmt.Printf("Uploading strings to the server...\n")
	uri := fmt.Sprintf("%s/uploadstrings", TRANSLATION_SERVER)

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
	a1 := extract_strings_from_c_files()
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
	logf("Don't forget to checkin strings/last_uploaded.txt")
}
