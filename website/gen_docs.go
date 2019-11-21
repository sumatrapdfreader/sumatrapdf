package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"github.com/kjk/u"
	"github.com/russross/blackfriday"
)

const (
	htmlTmpl = `<!doctype html>
<html>

<head>
	<meta http-equiv="Content-Language" content="en-us">
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
	<meta name="keywords" content="pdf, epub, mobi, chm, cbr, cbz, xps, djvu, reader, viewer" />
	<meta name="description" content="Sumatra PDF reader and viewer for Windows" />
	<title>Sumatra PDF Documentation</title>
	<link rel="stylesheet" href="/sumatra.css" type="text/css" />
</head>

<body>
	<script type="text/javascript" src="/sumatra.js"></script>

	<div id="container">
		<div id="banner">
			<h1 style="display:inline;">Sumatra PDF
				<font size="-1">is a PDF, ePub, MOBI, CHM, XPS, DjVu, CBZ, CBR reader for Windows</font>
			</h1>
			<script type="text/javascript">
				document.write(navHtml());
			</script>
		</div>

		<br/>

		<div id="center">
			<div class="content">
				{{ body }}
			</div>
		</div>
	</div>

	<hr>
	<center><a href="https://blog.kowalczyk.info">Krzysztof Kowalczyk</a></center>
	<script>
		window.ga = window.ga || function() {
			(ga.q = ga.q || []).push(arguments)
		};
		ga.l = +new Date;
		ga('create', 'UA-194516-5', 'auto');
		ga('send', 'pageview');
	</script>
	<script async src="//www.google-analytics.com/analytics.js"></script>
</body>
</html>
`
)

// SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715.md
// =>
// SumatraPDF-documentation.html
func shortHTMLNameFromMdName(s string) string {
	name := filepath.Base(s)
	name = replaceExt(name, "")
	// SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715
	idx := strings.LastIndex(name, "-")
	if idx == -1 {
		return name + ".html"
	}
	rest := name[idx+1:]
	if len(rest) == 32 {
		// SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715
		// =>
		// // SumatraPDF-documentation
		name = name[:idx]
	}
	return name + ".html"
}

func isMdFile(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".md"
}

func isHTMLFile(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".html"
}

func getDocsMdFiles() []string {
	var res []string
	dir := filepath.Join("docs_md")
	fileInfos, err := ioutil.ReadDir(dir)
	u.PanicIfErr(err)
	for _, fi := range fileInfos {
		if fi.IsDir() || !fi.Mode().IsRegular() {
			continue
		}
		if !isMdFile(fi.Name()) {
			continue
		}

		path := filepath.Join(dir, fi.Name())
		res = append(res, path)
	}
	return res
}

func removeDocsHTML() {
	dir := filepath.Join("www", "docs")
	fileInfos, err := ioutil.ReadDir(dir)
	u.PanicIfErr(err)
	for _, fi := range fileInfos {
		if fi.IsDir() || !fi.Mode().IsRegular() {
			continue
		}
		if !isHTMLFile(fi.Name()) {
			continue
		}
		path := filepath.Join(dir, fi.Name())
		err = os.Remove(path)
		u.PanicIfErr(err)
	}
}

func docsHTMLPath(mdPath string) string {
	dir := filepath.Join("www", "docs")
	name := shortHTMLNameFromMdName(mdPath)
	return filepath.Join(dir, name)
}

func addDocsRedirects(mdFilePaths []string) {
	// for SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715.md
	// add redirect
	// /docs/SumatraPDF-documentation-*
	// =>
	// /docs/SumatraPDF-documentation.html
	for _, name := range mdFilePaths {
		name = shortHTMLNameFromMdName(name)
		base := replaceExt(name, "")
		from := fmt.Sprintf("/docs/%s-*", base)
		to := fmt.Sprintf("/docs/%s.html", base)
		netflifyAddTempRedirect(from, to)
		//fmt.Printf("%s => %s\n", from, to)
	}
}

func genDocs() {
	removeDocsHTML()
	files := getDocsMdFiles()
	addDocsRedirects(files)

	for _, mdFile := range files {
		d, err := ioutil.ReadFile(mdFile)
		u.PanicIfErr(err)

		htmlInner := blackfriday.MarkdownCommon(d)
		html := strings.Replace(string(htmlTmpl), "{{ body }}", string(htmlInner), -1)
		htmlPath := docsHTMLPath(mdFile)
		err = ioutil.WriteFile(htmlPath, []byte(html), 0644)
		u.PanicIfErr(err)
		//fmt.Printf("%s => %s\n", mdFile, htmlPath)
	}
}
