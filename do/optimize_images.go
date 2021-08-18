package main

import (
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

// run optipng in parallel
func optimizeWithOptipng(path string) {
	logf("Optimizing '%s'\n", path)
	cmd := exec.Command("optipng", "-o5", path)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		// it's ok if fails. some jpeg images are saved as .png
		// which trips it
		logf("optipng failed with '%s'\n", err)
	}
}

func maybeOptimizeImage(path string) {
	ext := strings.ToLower(filepath.Ext(path))
	switch ext {
	// TODO: for .gif requires -snip
	case ".png", ".tiff", ".tif", "bmp":
		optimizeWithOptipng(path)
	}
}

func optimizeAllImages() {
	// verify we have optipng installed
	cmd := exec.Command("optipng", "-h")
	err := cmd.Run()
	panicIf(err != nil, "optipng is not installed")

	dirsToVisit := []string{filepath.Join("website", "img"), filepath.Join("website", "docs", "img")}
	for len(dirsToVisit) > 0 {
		dir := dirsToVisit[0]
		dirsToVisit = dirsToVisit[1:]
		files, err := ioutil.ReadDir(dir)
		must(err)
		for _, f := range files {
			name := f.Name()
			path := filepath.Join(dir, name)
			if f.IsDir() {
				//dirsToVisit = append(dirsToVisit, path)
				continue
			}
			maybeOptimizeImage(path)
		}
	}
}
