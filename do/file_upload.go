package main

import (
	path2 "path"
	"path/filepath"
	"sync"
	"time"
)

const filesRemoteDir = "sumatraTestFiles/"

func fileUpload(path string) {
	ensureSpacesAndS3Creds()
	fileSize := fileSizeMust(path)
	sha1, err := sha1HexOfFile(path)
	must(err)
	dstFileName := sha1[:6] + "-" + urlify(filepath.Base(path))
	remotePath := path2.Join(filesRemoteDir, dstFileName)
	sizeStr := formatSize(fileSize)
	logf(ctx(), "uploading '%s' of size %s as '%s'\n", path, sizeStr, remotePath)

	timeStart := time.Now()

	s3Client := newMinioS3Client()
	spacesClient := newMinioSpacesClient()

	upload := func(mc *MinioClient) {
		uri := minioURLForPath(mc, remotePath)
		if minioExists(mc, remotePath) {
			logf(ctx(), "Skipping upload, '%s' already exists\n", uri)
		} else {
			err := minioUploadFilePublic(mc, remotePath, path)
			must(err)
			logf(ctx(), "Uploaded '%s' in %s\n", uri, time.Since(timeStart))
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

func minioFilesList(mc *MinioClient) {
	uri := minioURLForPath(mc, "")
	logf(ctx(), "filesList in '%s'\n", uri)

	files := minioListObjects(mc, "")
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
	uri := minioURLForPath(mc, "")
	logf(ctx(), "deleteFiles in '%s'\n", uri)
	files := minioListObjects(mc, prefix)
	for f := range files {
		if doDelete {
			err := minioRemove(mc, f.Key)
			must(err)
			uri := minioURLForPath(mc, f.Key)
			logf(ctx(), "Deleted %s\n", uri)
		} else {
			sizeStr := formatSize(f.Size)
			logf(ctx(), "%s : %s\n", f.Key, sizeStr)
		}
	}
}
