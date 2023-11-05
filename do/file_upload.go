package main

import (
	"path"
	"path/filepath"
	"sync"
	"time"

	"github.com/kjk/minioutil"
)

const filesRemoteDir = "sumatraTestFiles/"

func fileUpload(fpath string) {
	ensureAllUploadCreds()
	fileSize := fileSizeMust(fpath)
	sha1, err := fileSha1Hex(fpath)
	must(err)
	dstFileName := sha1[:6] + "-" + urlify(filepath.Base(fpath))
	remotePath := path.Join(filesRemoteDir, dstFileName)
	sizeStr := formatSize(fileSize)
	logf("uploading '%s' of size %s as '%s'\n", fpath, sizeStr, remotePath)

	timeStart := time.Now()

	upload := func(mc *minioutil.Client) {
		uri := mc.URLForPath(remotePath)
		if mc.Exists(remotePath) {
			logf("Skipping upload, '%s' already exists\n", uri)
		} else {
			_, err := mc.UploadFile(remotePath, fpath, true)
			must(err)
			logf("Uploaded in %s\n%s\n", time.Since(timeStart), uri)
		}
	}

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		upload(newMinioR2Client())
		wg.Done()
	}()

	wg.Add(1)
	go func() {
		upload(newMinioBackblazeClient())
		wg.Done()
	}()

	wg.Wait()
}

func minioFilesList(mc *minioutil.Client) {
	uri := mc.URLForPath("")
	logf("filesList in '%s'\n", uri)

	files := mc.ListObjects("")
	for f := range files {
		sizeStr := formatSize(f.Size)
		logf("%s : %s\n", f.Key, sizeStr)
	}
}

func filesList() {
	ensureAllUploadCreds()
	minioFilesList(newMinioBackblazeClient())
}

func deleteFilesOneOff() {
	doDelete := false
	prefix := "vack/"

	var mc *minioutil.Client
	//mc = newMinioR2Client()
	//mc = newMinioBackblazeClient()
	uri := mc.URLForPath("")
	logf("deleteFiles in '%s'\n", uri)
	files := mc.ListObjects(prefix)
	for f := range files {
		if doDelete {
			err := mc.Remove(f.Key)
			must(err)
			uri := mc.URLForPath(f.Key)
			logf("Deleted %s\n", uri)
		} else {
			sizeStr := formatSize(f.Size)
			logf("%s : %s\n", f.Key, sizeStr)
		}
	}
}
