package main

// This program is for testing natural sorting of files in comic archives

import (
	"archive/zip"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
)

const (
	filesDir = `C:\Users\kjk\Downloads\SumatraPeter_3.2.11061_Filename_Sort_Failure_Test_Archives`
)

func must(err error) {
	if err != nil {
		panic(err.Error())
	}
}

func printZipFiles(zipPath string) {
	st, err := os.Stat(zipPath)
	must(err)
	fileSize := st.Size()
	f, err := os.Open(zipPath)
	must(err)
	defer f.Close()

	zr, err := zip.NewReader(f, fileSize)
	must(err)

	for _, fi := range zr.File {
		if fi.FileInfo().IsDir() {
			continue
		}
		fmt.Printf("  %s\n", fi.Name)
	}
}

func printFiles(path string) {
	fmt.Printf("File: %s\n", path)
	ext := strings.ToLower(filepath.Ext(path))
	switch ext {
	//case ".cbr":
	case ".cbz":
		printZipFiles(path)
	default:
		fmt.Printf("  extension %s not supported\n", filepath.Ext(path))
	}

}

func main() {
	files, err := ioutil.ReadDir(filesDir)
	must(err)
	for _, fi := range files {
		if fi.IsDir() {
			continue
		}
		path := filepath.Join(filesDir, fi.Name())
		printFiles(path)
	}
}
