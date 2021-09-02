package main

import (
	path2 "path"
	"path/filepath"
	"sync"
	"time"

	"github.com/minio/minio-go/v7"
)

const filesRemoteDir = "sumatraTestFiles/"

func fileUpload(path string) {
	ensureCanUpload()
	fileSize := fileSizeMust(path)
	sha1, err := sha1HexOfFile(path)
	must(err)
	dstFileName := sha1[:6] + "-" + urlify(filepath.Base(path))
	remotePath := path2.Join(filesRemoteDir, dstFileName)
	sizeStr := humanizeSize(fileSize)
	logf("uploading '%s' of size %s as '%s'\n", path, sizeStr, remotePath)

	var wg sync.WaitGroup
	wg.Add(2)

	timeStart := time.Now()

	go func() {
		c := newS3Client()
		if c.Exists(remotePath) {
			logf("Skipping upload to s3    because '%s' already exists\n", remotePath)
		} else {
			err := s3UploadFilePublic(c, remotePath, path)
			must(err)
			uri := "https://kjkpub.s3.amazonaws.com/" + remotePath
			logf("Uploaded to s3 in %s\n%s\n", time.Since(timeStart), uri)
		}
		wg.Done()
	}()
	go func() {
		c := newMinioSpacesClient()
		if minioExists(c, remotePath) {
			logf("Skipping upload to minio because '%s' already exists\n", remotePath)
		} else {
			uri := minioURLForPath(c, remotePath)
			logf("Uploaded to spaces in %s\n%s\n", time.Since(timeStart), uri)
			err := minioUploadFilePublic(c, remotePath, path)
			must(err)
		}
		wg.Done()
	}()
	wg.Wait()
}

func filesListSpaces() {
	c := newMinioSpacesClient()
	opts := minio.ListObjectsOptions{
		Prefix:    "",
		Recursive: true,
	}
	files := c.c.ListObjects(ctx(), c.bucket, opts)
	for f := range files {
		sizeStr := humanizeSize(f.Size)
		logf("%s : %s\n", f.Key, sizeStr)
	}
}

func filesList() {
	ensureCanUpload()
	filesListSpaces()
}
