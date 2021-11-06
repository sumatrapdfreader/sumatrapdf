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
	ensureSpacesAndS3Creds()
	fileSize := fileSizeMust(fpath)
	sha1, err := fileSha1Hex(fpath)
	must(err)
	dstFileName := sha1[:6] + "-" + urlify(filepath.Base(fpath))
	remotePath := path.Join(filesRemoteDir, dstFileName)
	sizeStr := formatSize(fileSize)
	logf(ctx(), "uploading '%s' of size %s as '%s'\n", fpath, sizeStr, remotePath)

	timeStart := time.Now()

	s3Client := newMinioS3Client()
	spacesClient := newMinioSpacesClient()

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
	wg.Add(2)
	go func() {
		upload(s3Client)
		wg.Done()
	}()

	go func() {
		upload(spacesClient)
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
	ensureSpacesAndS3Creds()
	//minioFilesList(newMinioSpacesClient())
	minioFilesList(newMinioS3Client())
}

func deleteFilesOneOff() {
	doDelete := false
	prefix := "vack/"

	//mc := newMinioSpacesClient()
	mc := newMinioS3Client()
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
