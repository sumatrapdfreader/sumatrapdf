package main

import (
	"bytes"
	"html/template"
	"io/fs"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/dustin/go-humanize"
	"github.com/kjk/u"
)

const crashesPrefix = "updatecheck/uploadedfiles/sumatrapdf-crashes/"
const fasterSubset = false
const filterNoSymbols = true
const filterOlderVersions = true

// we don't want want to show crsahes for outdated builds
// so this is usually set to the latest pre-release build
// https://www.sumatrapdfreader.org/prerelease.html
const lowestCrashingBuildToShow = 13112
const nDaysToKeep = 14

var (
	// maps day key as YYYY-MM-DD to a list of crashInfo
	crashesPerDay   = map[string][]*crashInfo{}
	nRemoteFiles    = 0
	nWritten        = 0
	tmplCrashParsed *template.Template
	tmplDayParsed   *template.Template
)

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
	VersionDisp      string
	Ver              *crashVersion
	GitSha1          string
	CrashFile        string
	OS               string
	CrashLines       []string
	CrashLinesLinked []crashLine
	crashLinesAll    string
	ExceptionInfo    []string
	exceptionInfoAll string
	body             []byte

	// soft-delete
	isDeleted bool

	// path of a file on disk, if exists
	path string

	// unique name of the crash
	FileName string
	// full key in remote store
	storeKey string
}

func (ci *crashInfo) ShortCrashLine() string {
	if len(ci.CrashLines) == 0 {
		return "(none)"
	}
	s := ci.CrashLines[0]
	// "000000007791A365 01:0000000000029365 ntdll.dll!RtlFreeHeap+0x1a5" => "dll!RtlFreeHeap+0x1a5"
	parts := strings.Split(s, " ")
	if len(parts) <= 2 {
		return s
	}
	return strings.Join(parts[2:], " ")
}

func (ci *crashInfo) CrashLine() string {
	if len(ci.CrashLines) == 0 {
		return "(none)"
	}
	return ci.CrashLines[0]
}

func (ci *crashInfo) URL() string {
	return "/" + ci.FileName
}

func symbolicateCrashLine(s string, gitSha1 string) crashLine {
	if strings.HasPrefix(s, "GetStackFrameInfo()") {
		return crashLine{
			Text: s,
		}
	}
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

func removeEmptyCrashLines(a []string) []string {
	var res []string
	for _, s := range a {
		s = strings.TrimSpace(s)
		if strings.Contains(s, " ") || strings.Contains(s, ".") {
			res = append(res, s)
			continue
		}
		if len(s) == len("0000000000000001") || len(s) == len("0000003F") {
			continue
		}
		res = append(res, s)
	}
	return res
}

func parseCrash(res *crashInfo) {
	d := res.body
	d = u.NormalizeNewlines(d)
	s := string(d)
	lines := strings.Split(s, "\n")
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
			if isEmptyLine(s) || strings.HasPrefix(s, "Thread:") || len(tmpLines) > 32 {
				res.CrashLines = removeEmptyCrashLines(tmpLines)
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

var crashesHTMLDirCached = ""

func crashesHTMLDataDir() string {
	if crashesHTMLDirCached != "" {
		return crashesHTMLDirCached
	}
	dir := u.UserHomeDirMust()
	dir = filepath.Join(dir, "data", "sumatra-crashes-html")
	// we want to empty this dir every time
	must(os.RemoveAll(dir))
	u.CreateDirMust((dir))
	crashesHTMLDirCached = dir
	return dir
}

// convert YYYY/MM/DD => YYYY-MM-DD
func storeDayToDirDay(s string) string {
	parts := strings.Split(s, "/")
	panicIf(len(parts) != 3)
	return strings.Join(parts, "-")
}

func getCrashInfoForDayName(day, name string) *crashInfo {
	panicIf(len(day) != len("yyyy-dd-mm"))
	crashes := crashesPerDay[day]
	// foo.txt => foo.html
	fileName := strings.Replace(name, ".txt", ".html", -1)
	for _, c := range crashes {
		if c.FileName == fileName {
			return c
		}
	}
	crash := &crashInfo{
		Day:      day,
		FileName: fileName,
	}
	crashes = append(crashes, crash)
	crashesPerDay[day] = crashes
	return crash
}

// return true if crash in that day is outdated
func isOutdated(day string) bool {
	d1, err := time.Parse("2006-01-02", day)
	panicIfErr(err)
	diff := time.Since(d1)
	return diff > time.Hour*24*nDaysToKeep
}

// decide if should delete crash based on the content
// this is to filter out crashes we don't care about
// like those from other apps
func shouldDeleteParsedCrash(ci *crashInfo) bool {
	if bytes.Contains(ci.body, []byte("einkreader.exe")) {
		return true
	}
	return bytes.Contains(ci.body, []byte("jikepdf.exe"))
}

const nDownloaders = 64

var (
	nInvalid    = 0
	nReadFiles  = 0
	nDownloaded = 0
	nDeleted    = 0
)

func downloadOrReadOrDelete(mc *u.MinioClient, crash *crashInfo) {
	dataDir := crashesDataDir()
	crash.path = filepath.Join(dataDir, crash.Day, crash.FileName)

	if isOutdated(crash.Day) {
		crash.isDeleted = true
		mc.Delete(crash.storeKey)
		os.Remove(crash.path)
		nDeleted++
		if nDeleted < 32 || nDeleted%100 == 0 {
			logf("deleted outdated %s %d\n", crash.storeKey, nRemoteFiles)
		}
		return
	}

	var err error
	crash.body, err = ioutil.ReadFile(crash.path)
	if err == nil {
		nReadFiles++
		if nReadFiles < 32 || nReadFiles%200 == 0 {
			logf("read %s for %s %d\n", crash.path, crash.storeKey, nRemoteFiles)
		}
	} else {
		err = mc.DownloadFileAtomically(crash.path, crash.storeKey)
		panicIf(err != nil, "mc.DownloadFileAtomc.DownloadFileAtomically('%s', '%s') failed with '%s'", crash.path, crash.storeKey, err)
		crash.body, err = ioutil.ReadFile(crash.path)
		panicIfErr(err)
		nDownloaded++
		if nDownloaded < 50 || nDownloaded%200 == 0 {
			logf("downloaded '%s' => '%s' %d\n", crash.storeKey, crash.path, nRemoteFiles)
		}
	}

	parseCrash(crash)
	if shouldDeleteParsedCrash(crash) {
		crash.isDeleted = true
		mc.Delete(crash.storeKey)
		os.Remove(crash.path)
		nInvalid++
		if nInvalid < 32 || nInvalid%100 == 0 {
			logf("deleted invalid %s %d\n", crash.path, nRemoteFiles)
		}
		return
	}
	if filterNoSymbols {
		// those are not hard deleted but we don't want to show them
		// TODO: maybe delete them as well?
		if hasNoSymbols(crash) {
			crash.isDeleted = true
		}
	}
	if filterOlderVersions && crash.Ver != nil {
		build := crash.Ver.Build
		// filter out outdated builds
		if build > 0 && build < lowestCrashingBuildToShow {
			crash.isDeleted = true
		}

	}
}

// if crash line ends with ".exe", we assume it's because it doesn't
// have symbols
func hasNoSymbols(ci *crashInfo) bool {
	s := ci.CrashLine()
	return strings.HasSuffix(s, ".exe")
}

func previewCrashes() {
	panicIf(!hasSpacesCreds())
	if true {
		panicIf(os.Getenv("NETLIFY_AUTH_TOKEN") == "", "missing NETLIFY_AUTH_TOKEN env variable")
		panicIf(os.Getenv("NETLIFY_SITE_ID") == "", "missing NETLIFY_SITE_ID env variable")
	}
	dataDir := crashesDataDir()
	logf("previewCrashes: data dir: '%s'\n", dataDir)

	var wg sync.WaitGroup
	wg.Add(1)
	// semaphore for limiting concurrent downloaders
	sem := make(chan bool, nDownloaders)
	go func() {
		mc := newMinioClient()
		client, err := mc.GetClient()
		panicIfErr(err)

		doneCh := make(chan struct{})
		defer close(doneCh)

		remoteFiles := client.ListObjects(mc.Bucket, crashesPrefix, true, doneCh)

		must(err)
		finished := false
		for rf := range remoteFiles {
			if finished {
				continue
			}
			nRemoteFiles++
			must(rf.Err)
			//logf("key: %s\n", rf.Key)
			s := strings.TrimPrefix(rf.Key, crashesPrefix)
			name := path.Base(s)
			// now day is YYYY/MM/DD/rest
			day := path.Dir(s)
			day = storeDayToDirDay(day)
			crash := getCrashInfoForDayName(day, name)
			crash.storeKey = rf.Key

			wg.Add(1)
			sem <- true
			go func() {
				downloadOrReadOrDelete(mc, crash)
				<-sem
				wg.Done()
			}()
			//
			if fasterSubset && len(crashesPerDay) > 2 {
				doneCh <- struct{}{}
				finished = true
			}
		}
		wg.Done()
	}()
	wg.Wait()

	logf("%d remote files, %d invalid\n", nRemoteFiles, nInvalid)
	filterDeletedCrashes()
	filterBigCrashes()

	days := getDaysSorted()
	for idx, day := range days {
		a := crashesPerDay[day]
		logf("%s: %d\n", day, len(a))
		sort.Slice(a, func(i, j int) bool {
			v1 := a[i].Version
			v2 := a[j].Version
			if v1 == v2 {
				c1 := getFirstString(a[i].CrashLines)
				c2 := getFirstString(a[j].CrashLines)
				if len(c1) == len(c2) {
					return c1 < c2
				}
				return len(c1) > len(c2)
			}
			return v1 > v2
		})
		crashesPerDay[day] = a

		if false && idx == 0 {
			for _, ci := range a {
				logf("%s %s\n", ci.Version, getFirstString(ci.CrashLines))
			}
		}
	}
	genCrashesHTML()
	if false {
		// using https://github.com/netlify/cli
		cmd := exec.Command("netlify", "dev", "-p", "8765", "--dir", ".")
		cmd.Dir = crashesHTMLDataDir()
		u.RunCmdLoggedMust(cmd)
	}
	if true {
		cmd := exec.Command("netlify", "deploy", "--prod", "--dir", ".")
		if strings.Contains(runtime.GOOS, "windows") {
			// if on windows assume running locally so open browser
			// automatically after deploying
			cmd = exec.Command("netlify", "deploy", "--prod", "--open", "--dir", ".")
		}
		cmd.Dir = crashesHTMLDataDir()
		u.RunCmdLoggedMust(cmd)
	}
}

func dirSize(dir string) {
	totalSize := int64(0)
	fun := func(path string, info fs.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		totalSize += info.Size()
		return nil
	}
	filepath.Walk(dir, fun)
	logf("Size of dir %s: %s\n", dir, humanize.Bytes(uint64(totalSize)))
}

const tmplDay = `
<!doctype html>
<html>
<head>
<style>
		html, body {
				font-family: monospace;
				font-size: 10pt;
				margin: 1em;
				padding: 0;
		}
		.td1 {
			white-space: nowrap;
			padding-right: 1em;
		}
		.tdh {
			text-align: center;
		}
</style>
</head>
<body>
	<p>
		{{range .Days}}<a href="/{{.}}.html" target="_blank">{{.}}</a>&nbsp;&nbsp;{{end}}
	</p>
	<p>
		<table>
			<thead>
				<tr>
					<td class="tdh">ver</td>
					<td class="tdh">crash</td>
				</tr>
			</thead>
			<tbody>
				{{range .CrashSummaries}}
				<tr>
					<td class="td1">{{.VersionDisp}}</td>
					<td><a href="{{.URL}}" target="_blank">{{.ShortCrashLine}}</a></td>
				</tr>
				{{end}}
			</tbody>
		</table>
	</p>
</body>
</html>
`

const tmplCrash = `
<!doctype html>
<httml>
    <head>
        <style>
            html, body {
                font-family: monospace;
                font-size: 10pt;
								margin: 1em;
								padding: 0;
								}
        </style>
    </head>
    <body>
        <div style="padding-bottom: 1em;"><a href="/">All crashes</a></div>
            {{range .CrashLinesLinked}}
                {{if .URL}}
                    <div><a href="{{.URL}}" target="_blank">{{.Text}}</a></div>
                {{else}}
                    <div>{{.Text}}</div>
                {{end}}
            {{end}}
        <pre>{{.CrashBody}}</pre>
    </body>
</html>
`

func getTmplCrash() *template.Template {
	if tmplCrashParsed == nil {
		tmplCrashParsed = template.Must(template.New("t2").Parse(tmplCrash))
	}
	return tmplCrashParsed
}

func getTmplDay() *template.Template {
	if tmplDayParsed == nil {
		tmplDayParsed = template.Must(template.New("t1").Parse(tmplDay))
	}
	return tmplDayParsed
}

func genCrashHTML(dir string, ci *crashInfo) {
	name := ci.FileName
	path := filepath.Join(dir, name)
	var buf bytes.Buffer
	v := struct {
		CrashBody        string
		CrashLinesLinked []crashLine
	}{
		CrashLinesLinked: ci.CrashLinesLinked,
		CrashBody:        string(ci.body),
	}
	err := getTmplCrash().Execute(&buf, v)
	must(err)
	d := buf.Bytes()

	u.WriteFileMust(path, d)
	nWritten++
	if nWritten < 8 || nWritten%100 == 0 {
		logf("wrote %s %d\n", path, nWritten)
	}
}

func genCrashesHTMLForDay(dir string, day string, isIndex bool) {
	days := getDaysSorted()
	path := filepath.Join(dir, day+".html")

	a := crashesPerDay[day]
	prevVer := ""
	for _, ci := range a {
		ver := ci.Version
		if ver != prevVer {
			ver = strings.TrimPrefix(ver, "Ver: ")
			ver = strings.TrimSpace(ver)
			ci.VersionDisp = ver
			prevVer = ci.Version
		}
	}

	v := struct {
		Days           []string
		CrashSummaries []*crashInfo
	}{
		Days:           days,
		CrashSummaries: a,
	}

	var buf bytes.Buffer
	err := getTmplDay().Execute(&buf, v)
	must(err)
	d := buf.Bytes()

	u.WriteFileMust(path, d)
	if isIndex {
		path = filepath.Join(dir, "index.html")
		u.WriteFileMust(path, d)
	}

	for _, ci := range a {
		genCrashHTML(dir, ci)
	}
}

func genCrashesHTML() {
	dir := crashesHTMLDataDir()
	logf("genCrashesHTML in %s\n", dir)
	days := getDaysSorted()
	for idx, day := range days {
		genCrashesHTMLForDay(dir, day, idx == 0)
	}
	dirSize(dir)
}

func getFirstString(a []string) string {
	if len(a) == 0 {
		return "(none)"
	}
	return a[0]
}

func filterDeletedCrashes() {
	nFiltered := 0
	for day, a := range crashesPerDay {
		var filtered []*crashInfo
		for _, ci := range a {
			if ci.isDeleted {
				nFiltered++
				continue
			}
			filtered = append(filtered, ci)
		}
		crashesPerDay[day] = filtered
	}
	logf("filterDeletedCrashes: filtered %d\n", nFiltered)
}

// to avoid uploading too much to netlify, we filter
// crashes from 3.2 etc. that are 256kb or more in size
func filterBigCrashes() {
	nFiltered := 0
	for day, a := range crashesPerDay {
		var filtered []*crashInfo
		for _, ci := range a {
			if ci.Ver == nil {
				nFiltered++
				continue
			}
			if len(ci.body) > 256*1024 && ci.Ver.Main == "3.2" {
				nFiltered++
				continue
			}
			filtered = append(filtered, ci)
		}
		crashesPerDay[day] = filtered
	}
	logf("filterBigCrashes: filtered %d\n", nFiltered)
}

func getDaysSorted() []string {
	var days []string
	for day := range crashesPerDay {
		days = append(days, day)
	}
	sort.Sort(sort.Reverse(sort.StringSlice(days)))
	return days
}
