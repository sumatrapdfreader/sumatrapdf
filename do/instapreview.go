package main

import (
	"archive/zip"
	"bytes"
	"compress/flate"
	"io"
	"os"
	"time"

	"github.com/kjk/common/httputil"
)

// upload to https://www.instantpreview.dev

func uploadCrashesFilesToInstantPreviewMust(files map[string][]byte) string {
	zipData, err := zipCreateFromContent(files)
	must(err)
	timeStart := time.Now()
	uri := "https://sumcrashes.instantpreview.dev/upload?" + os.Getenv("INSTA_PREV_CRASHES_PWD")
	res, err := httputil.Post(uri, zipData)
	must(err)
	uri = string(res)
	sizeStr := formatSize(int64(len(zipData)))
	logf(ctx(), "uploaded under: %s, %d files, zip file size: %s in: %s\n", uri, len(files), sizeStr, time.Since(timeStart))
	return uri
}

func zipCreateFromContent(files map[string][]byte) ([]byte, error) {
	var buf bytes.Buffer
	zw := zip.NewWriter(&buf)
	zw.RegisterCompressor(zip.Deflate, func(out io.Writer) (io.WriteCloser, error) {
		return flate.NewWriter(out, flate.BestCompression)
	})

	zipWriteContent := func(zw *zip.Writer, files map[string][]byte) error {
		for name, data := range files {
			fw, err := zw.Create(name)
			if err != nil {
				return err
			}
			_, err = fw.Write(data)
			if err != nil {
				return err
			}
		}
		return zw.Close()
	}

	err := zipWriteContent(zw, files)
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}
