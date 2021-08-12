package main

import (
	"os"
	"path/filepath"

	"github.com/kjk/u"
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
		u.WriteFileMust(path, []byte(mimeTypeFile))
	}
	{
		path := filepath.Join(metaInfDir, "container.xml")
		u.WriteFileMust(path, []byte(containerXmlFile))
	}

	{
		srcDir := filepath.Join("website", "docs")
		u.DirCopyRecurMust(dir, srcDir, nil)
	}

	// TODO:
	// - generate docs.opf file
	// - generate toc.ncx file

	{
		err = u.CreateZipWithDirContent("docs.epub", dir)
		must(err)
	}
}
