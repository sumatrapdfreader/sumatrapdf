package main

import (
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/u"
)

func downloadCrashes(dataDir string) {
	timeStart := time.Now()
	defer func() {
		logf("downloadCrashes took %s\n", time.Since(timeStart))
	}()
	mc := newMinioClient()
	remoteFiles, err := mc.ListRemoteFiles("updatecheck/uploadedfiles/sumatrapdf-crashes")
	must(err)

	nRemoteFiles := len(remoteFiles)
	nDownloaded := 0
	for _, rf := range remoteFiles {
		name := strings.TrimPrefix(rf.Key, "updatecheck/uploadedfiles/sumatrapdf-crashes/")
		path := filepath.Join(dataDir, name)
		if u.FileExists(path) {
			continue
		}
		nDownloaded++
		u.CreateDirForFileMust(path)
		err = mc.DownloadFileAtomically(path, rf.Key)
		must(err)
		logf("Downloaded '%s' => '%s'\n", rf.Key, path)
	}
	logf("%d total crashes, downloaded %d\n", nRemoteFiles, nDownloaded)
	logf("dataDir: %s\n", dataDir)
}

func previewCrashes() {
	panicIf(!hasSpacesCreds())
	dataDir := u.UserHomeDirMust()
	dataDir = filepath.Join(dataDir, "data", "sumatra-crashes")
	u.CreateDirMust((dataDir))
	logf("data dir: '%s'\n", dataDir)
	downloadCrashes(dataDir)
}
