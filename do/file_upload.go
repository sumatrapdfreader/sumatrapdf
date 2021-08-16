package main

import (
	path2 "path"
	"path/filepath"
	"sync"
	"time"

	"github.com/kjk/u"
)

const filesRemoteDir = "sumatraTestFiles/"

func fileUpload(path string) {
	ensureCanUpload()
	fileSize, err := u.GetFileSize(path)
	must(err)
	sha1, err := u.Sha1HexOfFile(path)
	must(err)
	dstFileName := sha1[:6] + "-" + urlify(filepath.Base(path))
	remotePath := path2.Join(filesRemoteDir, dstFileName)
	sizeStr := u.FmtSizeHuman(fileSize)
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
		c := newMinioClient()
		if minioExists(c, remotePath) {
			logf("Skipping upload to minio because '%s' already exists\n", remotePath)
		} else {
			uri := minioURLForPath(c, remotePath)
			logf("Uploaded to spaces in %s\n%s\n", time.Since(timeStart), uri)
			err := c.UploadFilePublic(remotePath, path)
			must(err)
		}
		wg.Done()
	}()
	wg.Wait()
}

func minioURLForPath(c *u.MinioClient, remotePath string) string {
	return c.URLBase() + remotePath
}

func filesListSpaces() {
	c := newMinioClient()
	files, err := c.ListRemoteFiles(filesRemoteDir)
	must(err)
	for _, f := range files {
		sizeStr := u.FmtSizeHuman(f.Size)
		logf("%s : %s\n", f.Key, sizeStr)
	}
}

func filesList() {
	ensureCanUpload()
	filesListSpaces()
}
