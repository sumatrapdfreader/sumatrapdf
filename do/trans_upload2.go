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
	apptranslatoServer = "https://apptranslator-ihc3k.ondigitalocean.app"
)

func downloadTranslations2() {
	timeStart := time.Now()
	defer func() {
		fmt.Printf("downloadTranslations2() finished in %s\n", time.Since(timeStart))
	}()
	strs := extractStringsFromCFilesNoPaths()
	sort.Strings(strs)
	for _, s := range strs {
		fmt.Printf("%s\n", s)
	}
	uri := apptranslatoServer + "/dltransfor?app=SumatraPDF"
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
	path := filepath.Join("strings", "translations2.txt")
	u.WriteFileMust(path, d)
	fmt.Printf("Wrote response of size %d to %s\n", len(d), path)
}
