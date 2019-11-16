package main

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/kjk/u"
)

func readFile(path string) string {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(d)
}

func uploadStringsToServer(strings string, secret string) {
	print("Uploading strings to the server...")
	panic("NYI")
	/*
		params = urllib.urlencode(
				{'strings': strings, 'app': 'SumatraPDF', 'secret': secret})
		headers = {"Content-type": "application/x-www-form-urlencoded",
								"Accept": "text/plain"}
		conn = httplib.HTTPConnection(SERVER, PORT)
		conn.request("POST", "/uploadstrings", params, headers)
		resp = conn.getresponse()
		print resp.status
		print resp.reason
		print resp.read()
		conn.close()
		print("Upload done")
	*/
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
