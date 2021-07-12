package main

import (
	"crypto/md5"
	"crypto/sha1"
	"encoding/base64"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"mime"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"

	"github.com/goamz/goamz/aws"
	"github.com/goamz/goamz/s3"
)

/*
This tool uploads files to s3 so that they are preserved for future testing.

Files are stored by their sha1 + suffix.

Usage:
go run tools/s3/main.go $file [-pub]
go run tools/s3/main.go $dir [-pub]

Providing a file does a recursive upload.
*/

// Secrets defines secrets
type Secrets struct {
	AwsSecret string
	AwsAccess string
}

// FileInfo describes a file
type FileInfo struct {
	Path           string // file path of the original file
	S3PathSha1Part string
	S3FullPath     string // S3PathSha1Part + ext
	Sha1Hex        string
	Md5Hex         string
	Md5B64         string
	Size           int64 // size of the file, not compressed
}

// FileInS3 describes file in S3
type FileInS3 struct {
	S3Path  string // full path in S3 i.e. testfiles/xx/yy/zzzzz.ext
	Name    string // original name of the file
	Size    int64
	Sha1Hex string
	Md5Hex  string
}

const (
	s3Bucket     = "kjkpub"
	s3Dir        = "testfiles/"
	maxS3Results = 1000
)

var (
	flgPub        bool
	flgPublic     bool
	flgList       bool
	cachedSecrets *Secrets

	wgUploads sync.WaitGroup
	inFatal   bool
	validExts = []string{".pdf", ".mobi", ".epub", ".chm", ".cbz", ".cbr", ".xps", ".djvu"}
)

// URL returns url
func (fi *FileInS3) URL() string {
	return "https://kjkpub.s3.amazonaws.com/testfiles/" + fi.S3Path
}

func printStack() {
	buf := make([]byte, 1024*164)
	n := runtime.Stack(buf, false)
	fmt.Printf("%s", buf[:n])
}

func pj(elem ...string) string {
	return filepath.Join(elem...)
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	os.Exit(1)
}

func panicIf(cond bool, format string, args ...interface{}) {
	if cond {
		if inFatal {
			os.Exit(1)
		}
		inFatal = true
		fmt.Printf(format, args...)
		printStack()
		os.Exit(1)
	}
}

func fatalIfErr(err error) {
	if err != nil {
		fatalf("%s\n", err.Error())
	}
}
func isPublic() bool {
	return flgPub || flgPublic
}

func strInArr(s string, a []string) bool {
	for _, s2 := range a {
		if s == s2 {
			return true
		}
	}
	return false
}

func isSupportedFile(fileName string) bool {
	ext := strings.ToLower(filepath.Ext(fileName))
	return strInArr(ext, validExts)
}

func readSecretsMust() *Secrets {
	if cachedSecrets != nil {
		return cachedSecrets
	}
	path := pj("scripts", "secrets.json")
	d, err := ioutil.ReadFile(path)
	panicIf(err != nil, "readSecretsMust(): error %s reading file '%s'\n", err, path)
	var s Secrets
	err = json.Unmarshal(d, &s)
	panicIf(err != nil, "readSecretsMust(): failed to json-decode file '%s'. err: %s, data:\n%s\n", path, err, string(d))
	panicIf(s.AwsAccess == "", "AwsAccess in secrets.json missing")
	panicIf(s.AwsSecret == "", "AwsSecret in secrets.json missing")
	cachedSecrets = &s
	return cachedSecrets
}

// to minimize number of files per s3 "directory", we use 3 level structure
// xx/yy/zzzzzzzzzzz..
// This gives 256 files in the leaf for 16 million total files
func sha1HexToS3Path(sha1Hex string) string {
	panicIf(len(sha1Hex) != 40, "invalid sha1Hex '%s'", sha1Hex)
	return fmt.Sprintf("%s/%s/%s", sha1Hex[:2], sha1Hex[2:4], sha1Hex[4:])
}

func removeExt(s string) string {
	ext := filepath.Ext(s)
	if len(ext) == 0 {
		return s
	}
	return s[:len(s)-len(ext)]
}

// reverse of sha1HexToS3Path
func s3PathToSha1Hex(s string) string {
	s = removeExt(s)
	s = s[len(s)-42:]
	panicIf(s[2] != '/', "s[2] is '%c', should be '/'", s[2])
	s = strings.Replace(s, "/", "", -1)
	panicIf(len(s) != 40, "len(s) is %d, should be 40", len(s))
	return s
}

func perc(total, n int) float64 {
	return float64(n) * 100 / float64(total)
}

func calcFileInfo(fi *FileInfo) {
	fmt.Printf("calcFileInfo: '%s'\n", fi.Path)
	var buf [16 * 1024]byte
	r, err := os.Open(fi.Path)
	fatalIfErr(err)
	defer r.Close()
	sha1 := sha1.New()
	md5Hash := md5.New()
	fi.Size = 0
	for {
		n, err := r.Read(buf[:])
		if err == io.EOF {
			break
		}
		d := buf[:n]
		fatalIfErr(err)
		panicIf(n == 0, "n is 0")
		fi.Size += int64(n)
		_, err = sha1.Write(d)
		fatalIfErr(err)
		_, err = md5Hash.Write(d)
		fatalIfErr(err)
	}
	sha1Sum := sha1.Sum(nil)
	fi.Sha1Hex = fmt.Sprintf("%x", sha1Sum)

	md5Sum := md5Hash.Sum(nil)
	fi.Md5Hex = fmt.Sprintf("%x", md5Sum)
	fi.Md5B64 = base64.StdEncoding.EncodeToString(md5Sum)

	fi.S3PathSha1Part = sha1HexToS3Path(fi.Sha1Hex)
	ext := strings.ToLower(filepath.Ext(fi.Path))
	fi.S3FullPath = s3Dir + fi.S3PathSha1Part + ext
	fmt.Printf("  sha1: %s\n", fi.Sha1Hex)
	fmt.Printf("   md5: %s\n", fi.Md5Hex)
	fmt.Printf("   md5: %s\n", fi.Md5B64)
	fmt.Printf("    s3: %s\n", fi.S3FullPath)
	fmt.Printf("  size: %d\n", fi.Size)
}

// TODO: speed it up by getting all files once at startup and using hashtable
// to check for existence
func sha1ExistsInS3Must(sha1 string) bool {
	// must do a list, because we add file extension at the end
	prefix := s3Dir + sha1[:2] + "/" + sha1[2:4] + "/" + sha1[4:]
	bucket := s3GetBucket()
	resp, err := bucket.List(prefix, "", "", maxS3Results)
	fatalIfErr(err)
	keys := resp.Contents
	panicIf(len(keys) > 1, "len(keys) == %d (should be 0 or 1)", len(keys))
	return len(keys) == 1
}

func uploadFileInfo(fi *FileInfo) {
	timeStart := time.Now()
	pathRemote := fi.S3FullPath
	pathLocal := fi.Path
	fmt.Printf("uploadFileInfo(): %s => %s, pub: %v, %d bytes", pathLocal, pathRemote, isPublic(), fi.Size)
	if sha1ExistsInS3Must(fi.Sha1Hex) {
		// TODO: if different permissions (public vs. private), change the perms
		// TODO: for extra paranoia could verify that fi.Size matches size in S3
		fmt.Printf("  skipping because already exists\n")
		return
	}
	bucket := s3GetBucket()
	d, err := ioutil.ReadFile(pathLocal)
	fatalIfErr(err)
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	perm := s3.Private
	if isPublic() {
		perm = s3.PublicRead
	}
	opts := s3.Options{}
	opts.Meta = make(map[string][]string)
	opts.Meta["name"] = []string{filepath.Base(pathLocal)}
	opts.ContentMD5 = fi.Md5B64
	err = bucket.Put(pathRemote, d, mimeType, perm, opts)
	fatalIfErr(err)
	fmt.Printf(" took %s\n", time.Since(timeStart))
}

func uploadWorker(files chan *FileInfo) {
	for fi := range files {
		uploadFileInfo(fi)
		wgUploads.Done()
	}
}

func uploadDir(dir string) {
	fmt.Printf("Uploading dir '%s'\n", dir)
	ch := make(chan *FileInfo)
	// only one uploader because I'm most likely limited by bandwidth
	go uploadWorker(ch)
	filepath.Walk(dir, func(path string, fi os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if !isSupportedFile(path) {
			return nil
		}
		fileInfo := &FileInfo{
			Path: path,
		}
		calcFileInfo(fileInfo)
		return nil
	})
	close(ch)
	wgUploads.Wait()
}

func uploadFile(filePath string) {
	fmt.Printf("Uploading file '%s'\n", filePath)
	fi := &FileInfo{
		Path: filePath,
	}
	calcFileInfo(fi)
	uploadFileInfo(fi)
}

//Note: http.DefaultClient is more robust than aws.RetryingClient
//(which fails for me with a timeout for large files e.g. ~6MB)
func getS3Client() *http.Client {
	// return aws.RetryingClient
	return http.DefaultClient
}

func s3GetBucket() *s3.Bucket {
	s3BucketName := "kjkpub"
	secrets := readSecretsMust()
	auth := aws.Auth{
		AccessKey: secrets.AwsAccess,
		SecretKey: secrets.AwsSecret,
	}
	// Note: it's important that region is aws.USEast. This is where my bucket
	// is and giving a different region will fail
	s3Obj := s3.New(auth, aws.USEast, getS3Client())
	return s3Obj.Bucket(s3BucketName)
}

// ETag gives the hex-encoded MD5 sum of the contents,
// surrounded with double-quotes.
func keyEtagToMd5Hex(s string) string {
	res := strings.Trim(s, `"`)
	panicIf(len(res) != 32, "len(res) = %d (should be 32)", len(res))
	return res
}

func s3ListFilesMust() []*FileInS3 {
	var res []*FileInS3
	bucket := s3GetBucket()
	delim := ""
	marker := ""
	for {
		resp, err := bucket.List(s3Dir, delim, marker, maxS3Results)
		fatalIfErr(err)
		for _, key := range resp.Contents {
			fi := &FileInS3{}
			fi.S3Path = key.Key
			fi.Size = key.Size
			fi.Md5Hex = keyEtagToMd5Hex(key.ETag)
			fi.Sha1Hex = s3PathToSha1Hex(key.Key)
			// TODO: get filename using func (b *Bucket) Head(path string, headers map[string][]string) (*http.Response, error)
			// and x-amz-meta-name header.
			// Cache locally because doing network call for each file will be
			// expensive
			res = append(res, fi)
		}
		if !resp.IsTruncated {
			break
		}
		marker = resp.NextMarker
	}
	return res
}

func s3List() {
	files := s3ListFilesMust()
	fmt.Printf("%d files\n", len(files))
	for _, fi := range files {
		fmt.Printf("  %s, %d, '%s'\n", fi.S3Path, fi.Size, fi.Name)
	}
}

func usageAndExit() {
	fmt.Printf("s3 $fileOrDir ")
}

func main() {
	flag.BoolVar(&flgPub, "pub", false, "should file be made public, short version")
	flag.BoolVar(&flgPublic, "public", false, "should file be made public")
	flag.BoolVar(&flgList, "list", false, "list files in s3")
	flag.Parse()
	if flgList {
		s3List()
		return
	}
	if 1 == len(flag.Args()) {
		usageAndExit()
	}
	fileOrDir := flag.Arg(0)
	fi, err := os.Stat(fileOrDir)
	fatalIfErr(err)
	if fi.IsDir() {
		uploadDir(fileOrDir)
	} else if fi.Mode().IsRegular() {
		uploadFile(fileOrDir)
	}
}
