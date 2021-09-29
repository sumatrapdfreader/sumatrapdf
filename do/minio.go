package main

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/atomicfile"
	"github.com/minio/minio-go/v7"
	"github.com/minio/minio-go/v7/pkg/credentials"
)

type MinioClient struct {
	c *minio.Client

	bucket string
}

func (c *MinioClient) URLBase() string {
	url := c.c.EndpointURL()
	return fmt.Sprintf("https://%s.%s/", c.bucket, url.Host)
}

func minioURLForPath(c *MinioClient, remotePath string) string {
	return c.URLBase() + strings.TrimPrefix(remotePath, "/")
}

func minioSetPublicObjectMetadata(opts *minio.PutObjectOptions) {
	if opts.UserMetadata == nil {
		opts.UserMetadata = map[string]string{}
	}
	opts.UserMetadata["x-amz-acl"] = "public-read"
}

func minioUploadDataPublic(mc *MinioClient, remotePath string, data []byte) error {
	contentType := mimeTypeFromFileName(remotePath)
	opts := minio.PutObjectOptions{
		ContentType: contentType,
	}
	minioSetPublicObjectMetadata(&opts)
	r := bytes.NewBuffer(data)
	_, err := mc.c.PutObject(ctx(), mc.bucket, remotePath, r, int64(len(data)), opts)
	return err
}

func minioUploadFilePublic(mc *MinioClient, remotePath string, path string) error {
	contentType := mimeTypeFromFileName(remotePath)
	opts := minio.PutObjectOptions{
		ContentType: contentType,
	}
	minioSetPublicObjectMetadata(&opts)
	_, err := mc.c.FPutObject(ctx(), mc.bucket, remotePath, path, opts)
	return err
}

func minioExists(mc *MinioClient, remotePath string) bool {
	_, err := mc.c.StatObject(ctx(), mc.bucket, remotePath, minio.StatObjectOptions{})
	return err == nil
}

func minioRemove(mc *MinioClient, remotePath string) error {
	opts := minio.RemoveObjectOptions{}
	err := mc.c.RemoveObject(ctx(), mc.bucket, remotePath, opts)
	return err
}

func minioListObjects(mc *MinioClient, prefix string) <-chan minio.ObjectInfo {
	opts := minio.ListObjectsOptions{
		Prefix:    prefix,
		Recursive: true,
	}
	return mc.c.ListObjects(ctx(), mc.bucket, opts)
}

func minioUploadDir(mc *MinioClient, dirRemote string, dirLocal string) error {
	files, err := ioutil.ReadDir(dirLocal)
	must(err)
	for _, f := range files {
		fname := f.Name()
		pathLocal := filepath.Join(dirLocal, fname)
		pathRemote := path.Join(dirRemote, fname)
		uri := minioURLForPath(mc, pathRemote)
		logf(ctx(), "Uploading '%s' as '%s' ", pathLocal, uri)
		timeStart := time.Now()
		err := minioUploadFilePublic(mc, pathRemote, pathLocal)
		if err != nil {
			logf(ctx(), "Failed with '%s'\n", err)
			return fmt.Errorf("upload of '%s' as '%s' failed with '%s'", pathLocal, pathRemote, err)
		}
		logf(ctx(), "%s\n", time.Since(timeStart))
	}
	return nil
}

func minioDownloadFileAtomically(mc *MinioClient, dstPath string, remotePath string) error {
	opts := minio.GetObjectOptions{}
	obj, err := mc.c.GetObject(ctx(), mc.bucket, remotePath, opts)
	if err != nil {
		return err
	}
	defer obj.Close()

	// ensure there's a dir for destination file
	dir := filepath.Dir(dstPath)
	err = os.MkdirAll(dir, 0755)
	if err != nil {
		return err
	}

	f, err := atomicfile.New(dstPath)
	if err != nil {
		return err
	}
	defer f.Close()
	_, err = io.Copy(f, obj)
	if err != nil {
		return err
	}
	return f.Close()
}

// retry 3 times because processing on GitHub actions often fails due to
// "An existing connection was forcibly closed by the remote host"
func minioDownloadAtomicallyRetry(mc *MinioClient, path string, key string) {
	var err error
	for i := 0; i < 3; i++ {
		err = minioDownloadFileAtomically(mc, path, key)
		if err == nil {
			return
		}
		logf(ctx(), "Downloading '%s' to '%s' failed with '%s'\n", key, path, err)
		time.Sleep(time.Millisecond * 500)
	}
	panicIf(true, "mc.DownloadFileAtomically('%s', '%s') failed with '%s'", path, key, err)
}

func newMinioSpacesClient() *MinioClient {
	bucket := "kjkpubsf"
	mc, err := minio.New("sfo2.digitaloceanspaces.com", &minio.Options{
		Creds:  credentials.NewStaticV4(os.Getenv("SPACES_KEY"), os.Getenv("SPACES_SECRET"), ""),
		Secure: true,
	})
	must(err)
	found, err := mc.BucketExists(ctx(), bucket)
	must(err)
	panicIf(!found, "bucket '%s' doesn't exist", bucket)
	return &MinioClient{
		c:      mc,
		bucket: bucket,
	}
}

func newMinioS3Client() *MinioClient {
	bucket := "kjkpub"
	mc, err := minio.New("s3.amazonaws.com", &minio.Options{
		Creds:  credentials.NewStaticV4(os.Getenv("AWS_ACCESS"), os.Getenv("AWS_SECRET"), ""),
		Secure: true,
	})
	must(err)
	found, err := mc.BucketExists(ctx(), bucket)
	must(err)
	panicIf(!found, "bucket '%s' doesn't exist", bucket)
	return &MinioClient{
		c:      mc,
		bucket: bucket,
	}
}

func newMinioWasabiClient() *MinioClient {
	bucket := "kjksoft"
	mc, err := minio.New("s3.us-west-1.wasabisys.com", &minio.Options{
		Creds:  credentials.NewStaticV4(os.Getenv("WASABI_ACCESS"), os.Getenv("WASABI_SECRET"), ""),
		Secure: true,
	})
	must(err)
	found, err := mc.BucketExists(ctx(), bucket)
	must(err)
	panicIf(!found, "bucket '%s' doesn't exist", bucket)
	return &MinioClient{
		c:      mc,
		bucket: bucket,
	}
}
