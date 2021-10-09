package main

import (
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
