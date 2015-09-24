package main

import (
	"bytes"
	"compress/gzip"
	"crypto/md5"
	"crypto/sha1"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
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

type FileInfo struct {
	Path           string // file path of the original file
	S3PathSha1Part string
	S3FullPath     string // S3PathSha1Part + ext
	Sha1Hex        string
	Md5Hex         string
	ShouldCompress bool
	Size           int    // size of the file, not compressed
	CompressedData []byte // if is compressed, this is gzipped data
}

const (
	s3Bucket = "kjkpub"
	s3Dir    = "files/"
)

var (
	flgPub    bool
	flgPublic bool

	cachedSecrets *Secrets

	wgUploads sync.WaitGroup
	inFatal   bool
	validExts = []string{".pdf", ".mobi", ".epub", ".chm", ".cbz", "cbr"}
	// only try to compress files that are not already compressed
	toMaybeCompressExts = []string{".pdf", ".mobi", ".chm"}
)

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

func fatalif(cond bool, format string, args ...interface{}) {
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

func fataliferr(err error) {
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

func shouldTryCompressFile(fileName string) bool {
	ext := strings.ToLower(filepath.Ext(fileName))
	return strInArr(ext, toMaybeCompressExts)
}

func readSecretsMust() *Secrets {
	if cachedSecrets != nil {
		return cachedSecrets
	}
	path := pj("scripts", "secrets.json")
	d, err := ioutil.ReadFile(path)
	fatalif(err != nil, "readSecretsMust(): error %s reading file '%s'\n", err, path)
	var s Secrets
	err = json.Unmarshal(d, &s)
	fatalif(err != nil, "readSecretsMust(): failed to json-decode file '%s'. err: %s, data:\n%s\n", path, err, string(d))
	fatalif(s.AwsAccess == "", "AwsAccess in secrets.json missing")
	fatalif(s.AwsSecret == "", "AwsSecret in secrets.json missing")
	cachedSecrets = &s
	return cachedSecrets
}

// to minimize number of files per s3 "directory", we use 3 level structure
// xx/yy/zzzzzzzzzzz..
// This gives 256 files in the leaf for 16 million total files
func sha1HexToS3Path(sha1Hex string) string {
	fatalif(len(sha1Hex) != 40, "invalid sha1Hex '%s'", sha1Hex)
	return fmt.Sprintf("%s/%s/%s", sha1Hex[:2], sha1Hex[2:4], sha1Hex[4:])
}

func calcFileInfo(fi *FileInfo) {
	fmt.Printf("calcFileInfo: '%s'\n", fi.Path)
	const BufSize = 16 * 1024
	var buf [BufSize]byte
	r, err := os.Open(fi.Path)
	fataliferr(err)
	defer r.Close()
	sha1 := sha1.New()
	md5Hash := md5.New()
	fi.ShouldCompress = false
	tryCompressFirsBlock := shouldTryCompressFile(fi.Path)
	var gzw *gzip.Writer
	var compressedData bytes.Buffer
	fi.Size = 0
	fi.CompressedData = nil
	for {
		n, err := r.Read(buf[:])
		if err == io.EOF {
			break
		}
		d := buf[:n]
		fataliferr(err)
		fatalif(n == 0, "n is 0")
		fi.Size += n
		_, err = sha1.Write(d)
		fataliferr(err)
		_, err = md5Hash.Write(d)
		fataliferr(err)
		if tryCompressFirsBlock {
			tryCompressFirsBlock = false
			gz, err := gzip.NewWriterLevel(&compressedData, gzip.BestCompression)
			fataliferr(err)
			_, err = gz.Write(d)
			fataliferr(err)
			gz.Close()
			compressedSize := compressedData.Len()
			// only compress if we compress at least 6.25% (16k => 15k)
			fi.ShouldCompress = compressedSize < (BufSize - 1024)
			if fi.ShouldCompress {
				compressedData.Reset()
				gzw, err = gzip.NewWriterLevel(&compressedData, gzip.BestCompression)
				fataliferr(err)
			}
			if gzw != nil {
				_, err = gzw.Write(d)
				fataliferr(err)
			}
		}
	}
	sha1Sum := sha1.Sum(nil)
	fi.Sha1Hex = fmt.Sprintf("%x", sha1Sum)

	if gzw != nil {
		gzw.Close()
		compressedSize := compressedData.Len()
		// only use compressed if compressed by at least 5%
		if compressedSize+(compressedSize/20) < fi.Size {
			fi.CompressedData = compressedData.Bytes()
		}
	}

	md5Sum := md5Hash.Sum(nil)
	fi.Md5Hex = fmt.Sprintf("%x", md5Sum)
	// if compressed, md5 is of the compressed content
	if fi.CompressedData != nil {
		md5Sum2 := md5.Sum(fi.CompressedData)
		fi.Md5Hex = fmt.Sprintf("%x", md5Sum2[:])
	}

	fi.S3PathSha1Part = sha1HexToS3Path(fi.Sha1Hex)
	ext := strings.ToLower(filepath.Ext(fi.Path))
	if fi.CompressedData != nil {
		fi.S3FullPath = fi.S3PathSha1Part + ".gz" + ext
	} else {
		fi.S3FullPath = fi.S3PathSha1Part + ext
	}
	fmt.Printf("  sha1: %s\n", fi.Sha1Hex)
	fmt.Printf("  s3: %s\n", fi.S3FullPath)
	fmt.Printf("  size: %d\n", fi.Size)
	if fi.CompressedData != nil {
		sizedCompressed := len(fi.CompressedData)
		saved := fi.Size - sizedCompressed
		savedPerc := (float64(sizedCompressed) * 100) / float64(fi.Size)
		fmt.Printf("  size compressed: %d (saves %d %.2f%%)\n", sizedCompressed, saved, savedPerc)
	}
}

func uploadFileInfo(fi *FileInfo) {
	// TODO: write me
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

func usageAndExit() {
	fmt.Printf("s3 $fileOrDir ")
}

func main() {
	flag.BoolVar(&flgPub, "pub", false, "should file be made public, short version")
	flag.BoolVar(&flgPublic, "public", false, "should file be made public")
	flag.Parse()
	if 1 == len(flag.Args()) {
		usageAndExit()
	}
	fileOrDir := flag.Arg(0)
	fi, err := os.Stat(fileOrDir)
	fataliferr(err)
	if fi.IsDir() {
		uploadDir(fileOrDir)
	} else if fi.Mode().IsRegular() {
		uploadFile(fileOrDir)
	}
}
