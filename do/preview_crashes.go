package main

import (
	"path"
	"path/filepath"
	"time"

	"github.com/kjk/u"
)

// name looks like "3dc72bd6f000006.txt"
func crashFilePath(dataDir string, name string) string {
	// file system don't deal with lots of files in the same directory
	// so we split them into 2-level directory structure
	return filepath.Join(dataDir, name[0:2], name[2:4], name)
}

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
		name := path.Base(rf.Key)
		path := crashFilePath(dataDir, name)
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
