package main

import (
	"crypto/md5"
	"encoding/base64"
	"fmt"
	"io/ioutil"
	"mime"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/goamz/goamz/aws"
	"github.com/goamz/goamz/s3"
)

type S3Client struct {
	Access string
	Secret string
	Bucket string
}

func md5B64OfBytes(d []byte) string {
	md5Sum := md5.Sum(d)
	return base64.StdEncoding.EncodeToString(md5Sum[:])
}

func md5B64OfFile(path string) string {
	d, err := ioutil.ReadFile(path)
	fatalIfErr(err)
	return md5B64OfBytes(d)
}

// must be called before any other call
func (c *S3Client) VerifyHasSecrets() {
	fatalIf(c.Access == "", "invalid Access\n")
	fatalIf(c.Secret == "", "invalid Secret\n")
	fatalIf(c.Secret == c.Access, "Secret == Access")
}

// Note: http.DefaultClient is more robust than aws.RetryingClient
// (which fails for me with a timeout for large files e.g. ~6MB)
func (c *S3Client) GetClient() *http.Client {
	// return aws.RetryingClient
	return http.DefaultClient
}

func (c *S3Client) GetBucket() *s3.Bucket {
	c.VerifyHasSecrets()
	auth := aws.Auth{
		AccessKey: c.Access,
		SecretKey: c.Secret,
	}
	// Note: it's important that region is aws.USEast. This is where my bucket
	// is and giving a different region will fail
	// TODO: make aws.USEast a variable s3BucketRegion, to allow over-ride
	s3Obj := s3.New(auth, aws.USEast, c.GetClient())
	return s3Obj.Bucket(c.Bucket)
}

func (c *S3Client) UploadFileReader(pathRemote, pathLocal string, public bool) error {
	logf("Uploading '%s' as '%s'. ", pathLocal, pathRemote)
	start := time.Now()
	opts := s3.Options{}
	opts.ContentMD5 = md5B64OfFile(pathLocal)
	bucket := c.GetBucket()
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	fileSize := fileSizeMust(pathLocal)
	perm := s3.Private
	if public {
		perm = s3.PublicRead
	}
	f, err := os.Open(pathLocal)
	if err != nil {
		return err
	}
	defer f.Close()
	err = bucket.PutReader(pathRemote, f, fileSize, mimeType, perm, opts)
	//appendTiming(time.Since(start), fmt.Sprintf("Upload of %s, size: %d", pathRemote, fileSize))
	if err != nil {
		logf("Failed with %s\n", err)
	} else {
		logf("Done in %s\n", time.Since(start))
	}
	return err
}

func (c *S3Client) UploadFile(pathRemote, pathLocal string, public bool) error {
	logf("Uploading '%s' as '%s'\n", pathLocal, pathRemote)
	bucket := c.GetBucket()
	d, err := ioutil.ReadFile(pathLocal)
	if err != nil {
		return err
	}
	perm := s3.Private
	if public {
		perm = s3.PublicRead
	}
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	opts := s3.Options{}
	opts.ContentMD5 = md5B64OfBytes(d)
	return bucket.Put(pathRemote, d, mimeType, perm, opts)
}

func (c *S3Client) UploadString(pathRemote string, s string, public bool) error {
	logf("Uploading string of length %d  as '%s'\n", len(s), pathRemote)
	bucket := c.GetBucket()
	d := []byte(s)
	mimeType := mime.TypeByExtension(filepath.Ext(pathRemote))
	opts := s3.Options{}
	opts.ContentMD5 = md5B64OfBytes([]byte(s))
	perm := s3.Private
	if public {
		perm = s3.PublicRead
	}
	return bucket.Put(pathRemote, d, mimeType, perm, opts)
}

func (c *S3Client) UploadFiles(s3Dir string, dir string, files []string) error {
	n := len(files) / 2
	for i := 0; i < n; i++ {
		pathLocal := filepath.Join(dir, files[2*i])
		pathRemote := files[2*i+1]
		err := c.UploadFileReader(s3Dir+pathRemote, pathLocal, true)
		if err != nil {
			return fmt.Errorf("failed to upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
	}
	return nil
}

func (c *S3Client) Delete(path string) error {
	bucket := c.GetBucket()
	return bucket.Del(path)
}

func (c *S3Client) Exists(s3Path string) bool {
	bucket := c.GetBucket()
	exists, err := bucket.Exists(s3Path)
	if err != nil {
		logf("bucket.Exists('%s') failed with %s\n", s3Path, err)
		return false
	}
	return exists
}
