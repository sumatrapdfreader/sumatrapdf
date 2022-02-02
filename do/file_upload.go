package main

import (
	"path"
	"path/filepath"
	"sync"
	"time"

	"github.com/kjk/minio"
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
	logf(ctx(), "uploading '%s' of size %s as '%s'\n", fpath, sizeStr, remotePath)

	timeStart := time.Now()

	upload := func(mc *minio.Client) {
		uri := mc.URLForPath(remotePath)
		if mc.Exists(remotePath) {
			logf(ctx(), "Skipping upload, '%s' already exists\n", uri)
		} else {
			_, err := mc.UploadFile(remotePath, fpath, true)
			must(err)
			logf(ctx(), "Uploaded in %s\n%s\n", time.Since(timeStart), uri)
		}
	}

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		upload(newMinioS3Client())
		wg.Done()
	}()

	wg.Add(1)
	go func() {
		upload(newMinioBackblazeClient())
		wg.Done()
	}()

	wg.Add(1)
	go func() {
		upload(newMinioSpacesClient())
		wg.Done()
	}()

	wg.Wait()
}

func minioFilesList(mc *minio.Client) {
	uri := mc.URLForPath("")
	logf(ctx(), "filesList in '%s'\n", uri)

	files := mc.ListObjects("")
	for f := range files {
		sizeStr := formatSize(f.Size)
		logf(ctx(), "%s : %s\n", f.Key, sizeStr)
	}
}

func filesList() {
	ensureAllUploadCreds()
	minioFilesList(newMinioBackblazeClient())
}

func deleteFilesOneOff() {
	doDelete := false
	prefix := "vack/"

	var mc *minio.Client
	//mc = newMinioSpacesClient()
	//mc = newMinioS3Client()
	//mc = newMinioBackblazeClient()
	uri := mc.URLForPath("")
	logf(ctx(), "deleteFiles in '%s'\n", uri)
	files := mc.ListObjects(prefix)
	for f := range files {
		if doDelete {
			err := mc.Remove(f.Key)
			must(err)
			uri := mc.URLForPath(f.Key)
			logf(ctx(), "Deleted %s\n", uri)
		} else {
			sizeStr := formatSize(f.Size)
			logf(ctx(), "%s : %s\n", f.Key, sizeStr)
		}
	}
}
