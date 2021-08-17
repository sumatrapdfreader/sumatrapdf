package main

import (
	"archive/zip"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
)

const mimeTypeFile = `application/epub+zip`

const containerXmlFile = `<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="docs.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>`

func buildEpubDocs() {
	dir := "docs_epub_tmp"
	os.RemoveAll(dir)

	//defer os.RemoveAll(dir)

	metaInfDir := filepath.Join(dir, "META-INF")
	err := os.MkdirAll(metaInfDir, 0755)
	must(err)

	{
		path := filepath.Join(dir, "mimetype")
		writeFileMust(path, []byte(mimeTypeFile))
	}
	{
		path := filepath.Join(metaInfDir, "container.xml")
		writeFileMust(path, []byte(containerXmlFile))
	}

	{
		srcDir := filepath.Join("website", "docs")
		dirCopyRecurMust(dir, srcDir, nil)
	}

	// TODO:
	// - generate docs.opf file
	// - generate toc.ncx file

	{
		err = CreateZipWithDirContent("docs.epub", dir)
		must(err)
	}
}

func PathIsDir(path string) (isDir bool, err error) {
	fi, err := os.Stat(path)
	if err != nil {
		return false, err
	}
	return fi.IsDir(), nil
}

// CreateZipWithDirContent creates a zip file with the content of a directory.
// The names of files inside the zip file are relative to dirToZip e.g.
// if dirToZip is foo and there is a file foo/bar.txt, the name in the zip
// will be bar.txt
func CreateZipWithDirContent(zipFilePath, dirToZip string) error {
	if isDir, err := PathIsDir(dirToZip); err != nil || !isDir {
		// TODO: should return an error if err == nil && !isDir
		return err
	}
	zf, err := os.Create(zipFilePath)
	if err != nil {
		//fmt.Printf("Failed to os.Create() %s, %s\n", zipFilePath, err.Error())
		return err
	}
	defer zf.Close()
	zipWriter := zip.NewWriter(zf)
	// TODO: is the order of defer here can create problems?
	// TODO: need to check error code returned by Close()
	defer zipWriter.Close()

	//fmt.Printf("Walk root: %s\n", config.LocalDir)
	err = filepath.Walk(dirToZip, func(pathToZip string, info os.FileInfo, err error) error {
		if err != nil {
			//fmt.Printf("WalkFunc() received err %s from filepath.Wath()\n", err.Error())
			return err
		}
		//fmt.Printf("%s\n", path)
		isDir, err := PathIsDir(pathToZip)
		if err != nil {
			//fmt.Printf("PathIsDir() for %s failed with %s\n", pathToZip, err.Error())
			return err
		}
		if isDir {
			return nil
		}
		toZipReader, err := os.Open(pathToZip)
		if err != nil {
			//fmt.Printf("os.Open() %s failed with %s\n", pathToZip, err.Error())
			return err
		}
		defer toZipReader.Close()

		zipName := pathToZip[len(dirToZip)+1:] // +1 for '/' in the path
		inZipWriter, err := zipWriter.Create(zipName)
		if err != nil {
			//fmt.Printf("Error in zipWriter(): %s\n", err.Error())
			return err
		}
		_, err = io.Copy(inZipWriter, toZipReader)
		if err != nil {
			return err
		}
		//fmt.Printf("Added %s to zip file\n", pathToZip)
		return nil
	})
	return err
}

func copyFileMust(dst, src string) {
	// ensure windows-style dir separator
	dst = strings.Replace(dst, "/", "\\", -1)
	src = strings.Replace(src, "/", "\\", -1)

	fdst, err := os.Create(dst)
	must(err)
	defer fdst.Close()
	fsrc, err := os.Open(src)
	must(err)
	defer fsrc.Close()
	_, err = io.Copy(fdst, fsrc)
	must(err)
}

func dirCopyRecur(dstDir, srcDir string, shouldCopyFn func(path string) bool) ([]string, error) {
	err := os.MkdirAll(dstDir, 0755)
	if err != nil {
		return nil, err
	}
	fileInfos, err := ioutil.ReadDir(srcDir)
	if err != nil {
		return nil, err
	}
	var allCopied []string
	for _, fi := range fileInfos {
		name := fi.Name()
		if fi.IsDir() {
			dst := filepath.Join(dstDir, name)
			src := filepath.Join(srcDir, name)
			copied, err := dirCopyRecur(dst, src, shouldCopyFn)
			if err != nil {
				return nil, err
			}
			allCopied = append(allCopied, copied...)
			continue
		}

		src := filepath.Join(srcDir, name)
		dst := filepath.Join(dstDir, name)
		shouldCopy := true
		if shouldCopyFn != nil {
			shouldCopy = shouldCopyFn(src)
		}
		if !shouldCopy {
			continue
		}
		copyFileMust(dst, src)
		allCopied = append(allCopied, src)
	}
	return allCopied, nil
}

func dirCopyRecurMust(dstDir, srcDir string, shouldCopyFn func(path string) bool) []string {
	copied, err := dirCopyRecur(dstDir, srcDir, shouldCopyFn)
	must(err)
	return copied
}
