package main

import (
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
)

// run optipng in parallel
func optimizeWithOptipng(path string) {
	logf(ctx(), "Optimizing '%s'\n", path)
	cmd := exec.Command("optipng", "-o5", path)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		// it's ok if fails. some jpeg images are saved as .png
		// which trips it
		logf(ctx(), "optipng failed with '%s'\n", err)
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
	nThreads := runtime.NumCPU()
	sem := make(chan bool, nThreads)
	var wg sync.WaitGroup
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
			sem <- true
			wg.Add(1)
			go func(path string) {
				maybeOptimizeImage(path)
				<-sem
				wg.Done()
			}(path)
		}
	}
	wg.Wait()
}

// makes -small.png variant of screenshot file sin website\img
func makeSmallImages() {
	{
		cmd := exec.Command("magick", "-version")
		err := cmd.Run()
		if err != nil {
			logf(ctx(), "ImageMagick doesn't seem to be installed\n")
			logf(ctx(), "You can install it with: choco install -y imagemagick\n")
			os.Exit(1)
		}
	}

	dirsToVisit := []string{filepath.Join("website", "img")}
	for len(dirsToVisit) > 0 {
		dir := dirsToVisit[0]
		dirsToVisit = dirsToVisit[1:]
		files, err := ioutil.ReadDir(dir)
		must(err)
		for _, f := range files {
			if f.IsDir() {
				//dirsToVisit = append(dirsToVisit, path)
				continue
			}
			name := f.Name()
			ext := strings.ToLower(filepath.Ext(name))
			if ext != ".png" {
				continue
			}
			if strings.Contains(name, "-small") {
				continue
			}
			dstName := strings.Split(name, ".")[0] + "-small.png"
			cmd := exec.Command("magick", "convert", name, "-resize", "80x80", dstName)
			logf(ctx(), "> %s\n", cmd.String())
			cmd.Dir = dir
			err := cmd.Run()
			must(err)
		}
	}
}
