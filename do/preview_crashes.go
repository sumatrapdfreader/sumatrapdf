package main

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"path"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/kjk/u"
	"github.com/minio/minio-go/v6"
)

// we don't want want to show crsahes for outdated builds
// so this is usually set to the latest pre-release build
// https://www.sumatrapdfreader.org/prerelease.html
const lowestCrashingBuildToShow = 12064

const crashesPrefix = "updatecheck/uploadedfiles/sumatrapdf-crashes/"

type crashVersion struct {
	Main         string
	Build        int
	IsPreRelease bool
	Is64bit      bool
}

type crashLine struct {
	Text string
	URL  string
}

type crashInfo struct {
	N                int
	Day              string // yy-mm-dd format
	Version          string
	Ver              *crashVersion
	GitSha1          string
	CrashFile        string
	OS               string
	CrashLines       []string
	CrashLinesLinked []crashLine
	crashLinesAll    string
	ExceptionInfo    []string
	exceptionInfoAll string
	path             string
}

func symbolicateCrashLine(s string, gitSha1 string) crashLine {
	parts := strings.SplitN(s, " ", 4)
	if len(parts) < 2 {
		return crashLine{
			Text: s,
		}
	}
	parts = parts[2:]
	text := strings.Join(parts, " ")
	uri := ""
	if len(parts) == 2 {
		s = parts[1]
		// D:\a\sumatrapdf\sumatrapdf\src\EbookController.cpp+329
		idx := strings.LastIndex(s, `\sumatrapdf\`)
		if idx > 0 {
			s = s[idx+len(`\sumatrapdf\`):]
			// src\EbookController.cpp+329
			parts = strings.Split(s, "+")
			// https://github.com/sumatrapdfreader/sumatrapdf/blob/67e5328b235fa3d5c23622bc8f05f43865fa03f8/.gitattributes#L2
			line := ""
			filePath := parts[0]
			if len(parts) == 2 {
				line = parts[1]
			}
			filePath = strings.Replace(filePath, "\\", "/", -1)
			uri = "https://github.com/sumatrapdfreader/sumatrapdf/blob/" + gitSha1 + "/" + filePath + "#L" + line
		}
	}
	res := crashLine{
		Text: text,
		URL:  uri,
	}
	return res
}

func symbolicateCrashInfoLines(ci *crashInfo) {
	for _, s := range ci.CrashLines {
		cl := symbolicateCrashLine(s, ci.GitSha1)
		ci.CrashLinesLinked = append(ci.CrashLinesLinked, cl)
	}
}

/*
given:
Ver: 3.2.11495 pre-release 64-bit
produces:
	main: "3.2"
	build: 11495
	isPreRelease: true
	is64bit: tru
*/
func parseCrashVersion(line string) *crashVersion {
	s := strings.TrimPrefix(line, "Ver: ")
	s = strings.TrimSpace(s)
	parts := strings.Split(s, " ")
	res := &crashVersion{}
	v := parts[0] // 3.2.11495
	for _, s = range parts[1:] {
		if s == "pre-release" {
			res.IsPreRelease = true
			continue
		}
		if s == "64-bit" {
			res.Is64bit = true
		}
	}
	// v is like: "3.2.11495 (dbg)"
	parts = strings.Split(v, ".")
	switch len(parts) {
	case 3:
		build, err := strconv.Atoi(parts[2])
		must(err)
		res.Build = build
	case 2:
		res.Main = parts[0] + "." + parts[1]
	default:
		// shouldn't happen but sadly there are badly generated crash reports
		res.Main = v
	}
	return res
}

func isEmptyLine(s string) bool {
	return len(strings.TrimSpace((s))) == 0
}

func removeEmptyLines(a []string) []string {
	var res []string
	for _, s := range a {
		s = strings.TrimSpace(s)
		if len(s) > 0 {
			res = append(res, s)
		}
	}
	return res
}

func parseCrash(d []byte) *crashInfo {
	d = u.NormalizeNewlines(d)
	s := string(d)
	lines := strings.Split(s, "\n")
	res := &crashInfo{}
	var tmpLines []string
	inExceptionInfo := false
	inCrashLines := false
	for _, l := range lines {
		if inExceptionInfo {
			if isEmptyLine(s) || len(tmpLines) > 5 {
				res.ExceptionInfo = removeEmptyLines(tmpLines)
				tmpLines = nil
				inExceptionInfo = false
				continue
			}
			tmpLines = append(tmpLines, l)
			continue
		}
		if inCrashLines {
			if isEmptyLine(s) || len(tmpLines) > 6 {
				res.CrashLines = removeEmptyLines(tmpLines)
				tmpLines = nil
				inCrashLines = false
				continue
			}
			tmpLines = append(tmpLines, l)
			continue
		}
		if strings.HasPrefix(l, "Crash file:") {
			res.CrashFile = l
			continue
		}
		if strings.HasPrefix(l, "OS:") {
			res.OS = l
			continue
		}
		if strings.HasPrefix(l, "Git:") {
			parts := strings.Split(l, " ")
			res.GitSha1 = parts[1]
			continue
		}
		if strings.HasPrefix(l, "Exception:") {
			inExceptionInfo = true
			tmpLines = []string{l}
			continue
		}
		if strings.HasPrefix(l, "Crashed thread:") {
			inCrashLines = true
			tmpLines = nil
			continue
		}
		if strings.HasPrefix(l, "Ver:") {
			res.Version = l
			res.Ver = parseCrashVersion(l)
		}
	}
	res.crashLinesAll = strings.Join(res.CrashLines, "\n")
	res.exceptionInfoAll = strings.Join(res.ExceptionInfo, "\n")
	symbolicateCrashInfoLines(res)
	return res
}

var crashesDirCached = ""

func crashesDataDir() string {
	if crashesDirCached != "" {
		return crashesDirCached
	}
	dir := u.UserHomeDirMust()
	dir = filepath.Join(dir, "data", "sumatra-crashes")
	u.CreateDirMust((dir))
	crashesDirCached = dir
	return dir
}

// filePath is:
// C:\Users\kjk\data\sumatra-crashes\2020\01\28\3f91b0910000006.txt
// return "2020-01-28"
func dayFromPath(path string) string {
	// normalize to use / as dir separator
	path = filepath.ToSlash(path)
	// dir should be: C:/Users/kjk/data/sumatra-crashes/2020/01/28
	parts := strings.Split(path, "/")
	start := len(parts) - 4
	parts = parts[start : start+3]
	panicIf(len(parts) != 3)
	return strings.Join(parts, "-")
}

func parseCrashFile(path string) *crashInfo {
	d := u.ReadFileMust(path)
	ci := parseCrash(d)
	ci.Day = dayFromPath(path)
	ci.path = path
	return ci
}

func isCreateThumbnailCrash(ci *crashInfo) bool {
	s := ci.crashLinesAll
	if strings.Contains(s, "!CreateThumbnailForFile+0x1ff") {
		return true
	}
	if strings.Contains(s, "CreateThumbnailForFile+0x175") {
		return true
	}
	return false
}

func shouldShowCrash(ci *crashInfo) bool {
	build := ci.Ver.Build
	// filter out outdated builds
	if build > 0 && build < lowestCrashingBuildToShow {
		return false
	}
	if isCreateThumbnailCrash(ci) {
		return false
	}
	return true
}

var (
	nTotalCrashes    = 0
	nNotShownCrashes = 0
)

func loadCrashes() []*crashInfo {
	dataDir := crashesDataDir()
	nTotalCrashes = 0
	logf("loadCrashes: data dir: '%s'", dataDir)
	timeStart := time.Now()
	defer logf("  finsished in %s, crashes: %d\n", time.Since(timeStart), nTotalCrashes)
	var res []*crashInfo
	fn := func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}
		nTotalCrashes++
		ci := parseCrashFile(path)
		if ci == nil || ci.Ver == nil {
			logf("Failed to parse crash file '%s'\n", path)
			return nil
		}
		res = append(res, ci)
		return nil
	}
	filepath.Walk(dataDir, fn)
	return res
}

func showCrashesToTerminal() {
	nNotShownCrashes = 0
	dataDir := crashesDataDir()
	logf("testParseCrashes: data dir: '%s'\n", dataDir)
	fn := func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}
		nTotalCrashes++
		ci := parseCrashFile(path)
		if ci == nil || ci.Ver == nil {
			logf("Failed to parse crash file '%s'\n", path)
			return nil
		}
		if !shouldShowCrash(ci) {
			nNotShownCrashes++
			return nil
		}
		logf("%s\n", path)
		if len(ci.CrashFile) != 0 {
			logf("%s\n", ci.CrashFile)
		}
		logf("ver: %s\n", ci.Version)
		for _, s := range ci.CrashLines {
			logf("%s\n", s)
		}
		logf("\n")
		return nil
	}
	filepath.Walk(dataDir, fn)
	logf("Total crashes: %d, shown: %d\n", nTotalCrashes, nTotalCrashes-nNotShownCrashes)
}

func listRemoteFiles(c *u.MinioClient, prefix string) ([]*minio.ObjectInfo, error) {
	var res []*minio.ObjectInfo
	client, err := c.GetClient()
	if err != nil {
		return nil, err
	}
	doneCh := make(chan struct{})
	defer close(doneCh)

	files := client.ListObjects(c.Bucket, prefix, true, doneCh)
	for oi := range files {
		oic := oi
		res = append(res, &oic)
	}
	return res, nil
}

func crashPathFromKey(key string) string {
	dataDir := crashesDataDir()
	name := strings.TrimPrefix(key, crashesPrefix)
	path := filepath.Join(dataDir, name)
	return path
}

func downloadCrashes() {
	timeStart := time.Now()
	defer func() {
		logf("downloadCrashes took %s\n", time.Since(timeStart))
	}()
	mc := newMinioClient()
	if false {
		c, err := mc.GetClient()
		must(err)
		c.TraceOn(os.Stdout)
	}

	// this fails with digital ocean because in ListObjectsV2 they seemingly don't return
	// continuation token
	//remoteFiles, err := mc.ListRemoteFiles(crashesPrefix)
	//must(err)

	remoteFiles, err := listRemoteFiles(mc, crashesPrefix)
	must(err)

	nRemoteFiles := len(remoteFiles)
	fmt.Printf("nRemoteFiles: %d\n", nRemoteFiles)
	nDownloaded := 0
	for _, rf := range remoteFiles {
		must(rf.Err)
		path := crashPathFromKey(rf.Key)
		if u.FileExists(path) {
			continue
		}
		nDownloaded++
		u.CreateDirForFileMust(path)
		err = mc.DownloadFileAtomically(path, rf.Key)
		panicIf(err != nil, "mc.DownloadFileAtomc.DownloadFileAtomically('%s', '%s') failed with '%s'", path, rf.Key, err)
		logf("Downloaded '%s' => '%s'\n", rf.Key, path)
	}
	logf("%d total crashes, downloaded %d\n", nRemoteFiles, nDownloaded)
}

const nDaysToKeep = 14

func deleteCrashRemoteAndLocal(mc *u.MinioClient, rf *minio.ObjectInfo) {
	must(rf.Err)
	var err error
	if true {
		err = mc.Delete(rf.Key)
		must(err)
	}
	logf("Deleted '%s'\n", rf.Key)
	path := crashPathFromKey(rf.Key)
	if os.Remove(path) == nil {
		logf("Deleted '%s'\n", path)
	}
}

func deleteWithPrefix(prefix string) {
	timeStart := time.Now()
	defer func() {
		logf("deleteWithPrefix('%s') took %s\n", prefix, time.Since(timeStart))
	}()
	mc := newMinioClient()
	remoteFiles, err := listRemoteFiles(mc, prefix)
	must(err)
	for _, rf := range remoteFiles {
		deleteCrashRemoteAndLocal(mc, rf)
	}
}

func deleteOldCrashes() {
	timeStart := time.Now()
	defer func() {
		logf("deleteOldCrashes took %s\n", time.Since(timeStart))
	}()
	mc := newMinioClient()
	remoteFiles, err := listRemoteFiles(mc, crashesPrefix)
	must(err)
	days := map[string]bool{}
	for _, rf := range remoteFiles {
		must(rf.Err)
		day := strings.TrimPrefix(rf.Key, crashesPrefix)
		// now day is YYYY/MM/DD/rest
		day = path.Dir(day)
		panicIf(len(strings.Split(day, "/")) != 3)
		days[day] = true
	}
	var sortedDays []string
	for day := range days {
		sortedDays = append(sortedDays, day)
	}
	sort.Strings(sortedDays)
	n := len(sortedDays)
	nToDelete := n - nDaysToKeep
	if nToDelete < 1 {
		logf("nothing to delete, %d days of crashes\n", n)
		return
	}
	for i := 0; i < nToDelete; i++ {
		day := sortedDays[i]
		prefix := crashesPrefix + day + "/"
		deleteWithPrefix(prefix)
	}
}

func previewCrashes() {
	panicIf(!hasSpacesCreds())
	dataDir := crashesDataDir()
	logf("previewCrashes: data dir: '%s'\n", dataDir)
	deleteOldCrashes()

	downloadCrashes()
	logf("dataDir: %s\n", dataDir)
	//showCrashesToTerminal()
	showCrashesWeb()
}

var crashesCached []*crashInfo

func getCrashesCached() []*crashInfo {
	if crashesCached == nil {
		crashesCached = loadCrashes()
		for i := 0; i < len(crashesCached); i++ {
			crashesCached[i].N = i
		}
	}
	return crashesCached
}

func handleIndex(w http.ResponseWriter, r *http.Request) {
	uri := r.URL.String()
	if uri == "/" {
		crashes := loadCrashes()
		d := map[string]interface{}{
			"Crashes": crashes,
		}
		serveHTMLTemplate(w, r, 200, "index.tmpl.html", d)
		return
	}
	servePlainText(w, r, http.StatusNotFound, "'%s' not found", uri)
}

func handleIndex2(w http.ResponseWriter, r *http.Request) {
	uri := r.URL.String()
	path := strings.TrimPrefix(uri, "/")
	if uri == "/" {
		path = "index.html"
	}
	serveRelativeFile(w, r, path)
}

func handleCrash(w http.ResponseWriter, r *http.Request) {
	uri := r.URL.String()
	crashNoStr := strings.TrimPrefix(uri, "/crash/")
	crashNo, err := strconv.Atoi(crashNoStr)
	must(err)
	crashes := getCrashesCached()
	crash := crashes[crashNo]
	crashBody := u.ReadFileMust(crash.path)
	d := map[string]interface{}{
		"CrashLinesLinked": crash.CrashLinesLinked,
		"CrashBody":        string(crashBody),
	}
	serveHTMLTemplate(w, r, 200, "crash.tmpl.html", d)
}

func handleAPIGetCrashes(w http.ResponseWriter, r *http.Request) {
	crashes := getCrashesCached()
	d := map[string]interface{}{
		"Crashes": crashes,
	}
	serveJSON(w, r, d)
}

func handle404(w http.ResponseWriter, r *http.Request) {
	//w.Header().Set("Content-Type", htmlMimeType)
	w.WriteHeader(http.StatusNotFound)
	msg := "Not found"
	_, _ = w.Write([]byte(msg))
}

// https://blog.gopheracademy.com/advent-2016/exposing-go-on-the-internet/
func makeHTTPServer() *http.Server {
	mux := &http.ServeMux{}
	mux.HandleFunc("/", handleIndex2)
	mux.HandleFunc("/favicon.ico", handle404)
	mux.HandleFunc("/crash/", handleCrash)
	mux.HandleFunc("/api/crashes", handleAPIGetCrashes)

	var handler http.Handler = mux

	srv := &http.Server{
		ReadTimeout:  120 * time.Second,
		WriteTimeout: 120 * time.Second,
		IdleTimeout:  120 * time.Second, // introduced in Go 1.8
		Handler:      handler,
	}
	return srv
}

const (
	httpPort    = ":8945"
	flgHTTPAddr = "127.0.0.1" + httpPort
)

func showCrashesWeb() {
	getCrashesCached()

	httpSrv := makeHTTPServer()
	httpSrv.Addr = flgHTTPAddr

	chServerClosed := make(chan bool, 1)
	go func() {
		err := httpSrv.ListenAndServe()
		// mute error caused by Shutdown()
		if err == http.ErrServerClosed {
			err = nil
		}
		must(err)
		chServerClosed <- true
	}()
	_ = u.OpenBrowser("http://" + flgHTTPAddr)

	time.Sleep(time.Second * 2)

	c := make(chan os.Signal, 2)
	signal.Notify(c, os.Interrupt /* SIGINT */, syscall.SIGTERM)
	<-c

	ctx := context.Background()
	if httpSrv != nil {
		// Shutdown() needs a non-nil context
		_ = httpSrv.Shutdown(ctx)
		select {
		case <-chServerClosed:
			// do nothing
		case <-time.After(time.Second * 5):
			// timeout
		}
	}
}
