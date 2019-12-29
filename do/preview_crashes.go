package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/u"
	"github.com/minio/minio-go/v6"
)

// we don't want want to show crsahes for outdated builds
// so this is usually set to the latest pre-release build
// https://www.sumatrapdfreader.org/prerelease.html
const lowestCrashingBuildToShow = 11807

type CrashVersion struct {
	main         string
	build        int
	isPreRelease bool
	is64bit      bool
}

type CrashInfo struct {
	version          string
	ver              *CrashVersion
	crashFile        string
	os               string
	crashLines       []string
	crashLinesAll    string
	exceptionInfo    []string
	exceptionInfoAll string
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
func parseCrashVersion(s string) *CrashVersion {
	s = strings.TrimPrefix(s, "Ver: ")
	s = strings.TrimSpace(s)
	parts := strings.Split(s, " ")
	res := &CrashVersion{}
	v := parts[0] // 3.2.11495
	for _, s = range parts[1:] {
		if s == "pre-release" {
			res.isPreRelease = true
			continue
		}
		if s == "64-bit" {
			res.is64bit = true
		}
	}
	// 3.2.11495
	parts = strings.Split(v, ".")
	u.PanicIf(len(parts) < 3, "has %d parts in '%s'", len(parts), v)
	if len(parts) == 3 {
		build, err := strconv.Atoi(parts[2])
		must(err)
		res.build = build
	}
	if len(parts) == 2 {
		res.main = parts[0] + "." + parts[1]
	} else {
		res.main = parts[0]
	}
	return res
}

func isEmptyLine(s string) bool {
	return len(strings.TrimSpace((s))) == 0
}

func parseCrash(d []byte) *CrashInfo {
	d = u.NormalizeNewlines(d)
	s := string(d)
	lines := strings.Split(s, "\n")
	res := &CrashInfo{}
	var tmpLines []string
	inExceptionInfo := false
	inCrashLines := false
	for _, l := range lines {
		if inExceptionInfo {
			if isEmptyLine(s) || len(tmpLines) > 5 {
				res.exceptionInfo = tmpLines
				tmpLines = nil
				inExceptionInfo = false
				continue
			}
			tmpLines = append(tmpLines, l)
			continue
		}
		if inCrashLines {
			if isEmptyLine(s) || len(tmpLines) > 6 {
				res.crashLines = tmpLines
				tmpLines = nil
				inCrashLines = false
				continue
			}
			tmpLines = append(tmpLines, l)
			continue
		}
		if strings.HasPrefix(l, "Crash file:") {
			res.crashFile = l
			continue
		}
		if strings.HasPrefix(l, "OS:") {
			res.os = l
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
			res.version = l
			res.ver = parseCrashVersion(l)
		}
	}
	res.crashLinesAll = strings.Join(res.crashLines, "\n")
	res.exceptionInfoAll = strings.Join(res.exceptionInfo, "\n")
	return res
}

func crashesDataDir() string {
	dir := u.UserHomeDirMust()
	dir = filepath.Join(dir, "data", "sumatra-crashes")
	u.CreateDirMust((dir))
	return dir
}

func parseCrashFile(path string) *CrashInfo {
	d := u.ReadFileMust(path)
	return parseCrash(d)
}

func isCreateThumbnailCrash(ci *CrashInfo) bool {
	s := ci.crashLinesAll
	if strings.Contains(s, "!CreateThumbnailForFile+0x1ff") {
		return true
	}
	if strings.Contains(s, "CreateThumbnailForFile+0x175") {
		return true
	}
	return false
}

func shouldShowCrash(ci *CrashInfo) bool {
	build := ci.ver.build
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
		if !shouldShowCrash(ci) {
			nNotShownCrashes++
			return nil
		}
		logf("%s\n", path)
		if len(ci.crashFile) != 0 {
			logf("%s\n", ci.crashFile)
		}
		logf("ver: %s\n", ci.version)
		for _, s := range ci.crashLines {
			logf("%s\n", s)
		}
		logf("\n")
		return nil
	}
	filepath.Walk(dataDir, fn)
	logf("Total crashes: %d, not shown: %d\n", nTotalCrashes, nNotShownCrashes)
}

// ListRemoveFiles returns a list of files under a given prefix
func ListRemoteFiles2(c *u.MinioClient, prefix string) ([]*minio.ObjectInfo, error) {
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

func downloadCrashes(dataDir string) {
	timeStart := time.Now()
	defer func() {
		logf("downloadCrashes took %s\n", time.Since(timeStart))
	}()
	mc := newMinioClient()
	c, err := mc.GetClient()
	must(err)
	c.TraceOn(os.Stdout)

	//remoteFiles, err := mc.ListRemoteFiles("updatecheck/uploadedfiles/sumatrapdf-crashes/")
	//must(err)

	remoteFiles, err := ListRemoteFiles2(mc, "updatecheck/uploadedfiles/sumatrapdf-crashes/")
	must(err)

	nRemoteFiles := len(remoteFiles)
	fmt.Printf("nRemoteFiles: %d\n", nRemoteFiles)
	nDownloaded := 0
	for _, rf := range remoteFiles {
		must(rf.Err)
		name := strings.TrimPrefix(rf.Key, "updatecheck/uploadedfiles/sumatrapdf-crashes/")
		path := filepath.Join(dataDir, name)
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
	logf("dataDir: %s\n", dataDir)
}

func previewCrashes() {
	panicIf(!hasSpacesCreds())
	dataDir := crashesDataDir()
	logf("previewCrashes: data dir: '%s'\n", dataDir)
	downloadCrashes(dataDir)
	showCrashesToTerminal()
}
