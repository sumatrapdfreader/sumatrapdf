package main

import (
	"crypto/sha1"
	"fmt"
	"html"
	"io/ioutil"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/fmthtml"
	"github.com/kjk/notionapi"
	"github.com/kjk/notionapi/caching_downloader"
	"github.com/kjk/notionapi/tohtml"
	"github.com/kjk/u"
)

var (
	cacheDir        = "notion_cache"
	cacheImgDir     = filepath.Join("docs", "img")
	startPageID     = "fed36a5624d443fe9f7be0e410ecd715"
	nDownloadedPage int
)

func sha1OfLink(link string) string {
	link = strings.ToLower(link)
	h := sha1.New()
	h.Write([]byte(link))
	return fmt.Sprintf("%x", h.Sum(nil))
}

var imgFiles []os.FileInfo

func findImageInDir(imgDir string, sha1 string) string {
	if len(imgFiles) == 0 {
		imgFiles, _ = ioutil.ReadDir(imgDir)
	}
	for _, fi := range imgFiles {
		if strings.HasPrefix(fi.Name(), sha1) {
			return filepath.Join(imgDir, fi.Name())
		}
	}
	return ""
}

func guessExt(fileName string, contentType string) string {
	ext := strings.ToLower(filepath.Ext(fileName))
	// TODO: maybe allow every non-empty extension. This
	// white-listing might not be a good idea
	switch ext {
	case ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".tiff", ".svg":
		return ext
	}

	contentType = strings.ToLower(contentType)
	switch contentType {
	case "image/png":
		return ".png"
	case "image/jpeg":
		return ".jpg"
	case "image/svg+xml":
		return ".svg"
	}
	panic(fmt.Errorf("didn't find ext for file '%s', content type '%s'", fileName, contentType))
}

func downloadImage(c *notionapi.Client, uri string) ([]byte, string, error) {
	img, err := c.DownloadFile(uri)
	if err != nil {
		// TODO: getSignedURLs stopped working so we need this workaround
		// should move this logic down to c.DownloadFile()
		uri = "https://www.notion.so/image/" + url.PathEscape(uri)
		img, err = c.DownloadFile(uri)
	}
	if err != nil {
		logf("\n  failed with %s\n", err)
		return nil, "", err
	}
	ext := guessExt(uri, img.Header.Get("Content-Type"))
	return img.Data, ext, nil
}

// return path of cached image on disk
func downloadAndCacheImage(c *notionapi.Client, uri string) (string, error) {
	sha := sha1OfLink(uri)

	//ext := strings.ToLower(filepath.Ext(uri))

	u.CreateDirMust(cacheImgDir)

	cachedPath := findImageInDir(cacheImgDir, sha)
	if cachedPath != "" {
		logf("Image %s already downloaded as %s\n", uri, cachedPath)
		return cachedPath, nil
	}

	timeStart := time.Now()
	logf("Downloading %s ... ", uri)

	imgData, ext, err := downloadImage(c, uri)
	must(err)

	cachedPath = filepath.Join(cacheImgDir, sha+ext)

	err = ioutil.WriteFile(cachedPath, imgData, 0644)
	if err != nil {
		return "", err
	}
	logf("finished in %s. Wrote as '%s'\n", time.Since(timeStart), cachedPath)

	return cachedPath, nil
}

func eventObserver(ev interface{}) {
	switch v := ev.(type) {
	case *caching_downloader.EventError:
		logf(v.Error)
	case *caching_downloader.EventDidDownload:
		nDownloadedPage++
		logf("%03d '%s' : downloaded in %s\n", nDownloadedPage, v.PageID, v.Duration)
	case *caching_downloader.EventDidReadFromCache:
		// TODO: only verbose
		nDownloadedPage++
		logf("%03d '%s' : read from cache in %s\n", nDownloadedPage, v.PageID, v.Duration)
	case *caching_downloader.EventGotVersions:
		logf("downloaded info about %d versions in %s\n", v.Count, v.Duration)
	}
}

func newNotionClient() *notionapi.Client {
	token := os.Getenv("NOTION_TOKEN")
	panicIf(token == "", "NOTION_TOKEN env variable not set, needed for downloading images\n")
	// TODO: verify token still valid, somehow
	client := &notionapi.Client{
		AuthToken: token,
	}
	// if true, shows http requests sent to notion
	if false {
		client.Logger = os.Stdout
	}
	return client
}

func fileNameFromTitle(title string) string {
	return urlify(title) + ".html"
}

func fileNameForPage(page *notionapi.Page) string {
	title := page.Root().Title
	return fileNameFromTitle(title)
}

func afterPageDownload(page *notionapi.Page) error {
	logf("Downloaded page %sn\n", page.ID)
	return nil
}

// HTMLConverter renders article as html
type HTMLConverter struct {
	client   *notionapi.Client
	page     *notionapi.Page
	idToPage map[string]*notionapi.Page
	conv     *tohtml.Converter
}

// PageByID returns a page given its id
func (c *HTMLConverter) PageByID(id string) *notionapi.Page {
	page := c.idToPage[id]
	if page != nil {
		return page
	}
	id2 := notionapi.ToNoDashID(id)
	page = c.idToPage[id2]
	if page == nil {
		logf("Didn't find page for id %s' ('%s') in %d pages\n", id, id2, len(c.idToPage))
		for id := range c.idToPage {
			logf("%s\n", id)
		}
	}
	return page
}

func (c *HTMLConverter) getURLAndTitleForBlock(block *notionapi.Block) (string, string) {
	id := block.ID
	page := c.PageByID(id)
	if page == nil {
		title := block.Title
		logf("No page for id %s %s\n", id, title)
		pageURL := "https://notion.so/" + notionapi.ToNoDashID(c.page.ID)
		logf("Link from page: %s\n", pageURL)
		url := fileNameFromTitle(title)
		return url, title
	}
	url := fileNameForPage(page)

	return url, page.Root().Title
}

// RenderPage renders BlockPage
func (c *HTMLConverter) RenderPage(block *notionapi.Block) bool {
	if c.conv.Page.IsRoot(block) {
		c.conv.Printf(`<div class="notion-page" id="%s">`, block.ID)
		c.conv.RenderChildren(block)
		c.conv.Printf(`</div>`)
		c.conv.Printf(`<hr>`)
		uri := "https://notion.so/" + notionapi.ToNoDashID(block.ID)
		c.conv.Printf(`<center><a href="%s" target="_blank" class="suggest-change">suggest change to this page</a></center>`, uri)
		return true
	}

	cls := "page-link"
	if block.IsSubPage() {
		cls = "page"
	}

	c.conv.Printf(`<div class="%s">`, cls)
	url, title := c.getURLAndTitleForBlock(block)
	title = html.EscapeString(title)
	c.conv.Printf(`<a href="%s">%s</a>`, url, title)
	c.conv.Printf(`</div>`)
	return true
}

func normalizeID(id string) string {
	return notionapi.ToNoDashID(id)
}

// RenderImage downloads and renders an image
func (c *HTMLConverter) RenderImage(block *notionapi.Block) bool {
	link := block.Source
	path, err := downloadAndCacheImage(c.client, link)
	if err != nil {
		logf("genImage: downloadAndCacheImage('%s') from page https://notion.so/%s failed with '%s'\n", link, normalizeID(c.page.ID), err)
		must(err)
	}
	relURL := "img/" + filepath.Base(path)
	// TODO: add explicit width / height
	c.conv.Printf(`<img src="%s">`, relURL)
	return true
}

// if returns false, the block will be rendered with default
func (c *HTMLConverter) blockRenderOverride(block *notionapi.Block) bool {
	switch block.Type {
	case notionapi.BlockPage:
		return c.RenderPage(block)
	case notionapi.BlockImage:
		return c.RenderImage(block)
	}
	return false
}

// change https://www.notion.so/Advanced-web-spidering-with-Puppeteer-ea07db1b9bff415ab180b0525f3898f6
// =>
// Advanced-web-spidering-with-Puppeteer.url
func (c *HTMLConverter) rewriteURL(uri string) string {
	id := notionapi.ExtractNoDashIDFromNotionURL(uri)
	if id == "" {
		return uri
	}
	page := c.PageByID(id)
	// this might happen when I link to some-one else's public notion pages
	// but we don't do that for Sumatra docs
	if page == nil {
		logf("Didn't find page for url '%s', id '%s'\n", uri, id)
		os.Exit(0)
	}
	panicIf(page == nil)
	return fileNameForPage(page)
}

// NewHTMLConverter returns new HTMLGenerator
func NewHTMLConverter(page *notionapi.Page, pages []*notionapi.Page, idToPage map[string]*notionapi.Page) *HTMLConverter {
	res := &HTMLConverter{
		page:     page,
		idToPage: idToPage,
	}

	conv := tohtml.NewConverter(page)
	conv.PageByIDProvider = tohtml.NewPageByIDFromPages(pages)
	notionapi.PanicOnFailures = true
	conv.RenderBlockOverride = res.blockRenderOverride
	conv.RewriteURL = res.rewriteURL
	res.conv = conv
	return res
}

var tmpl = `<!doctype html>
<html>

<head>
	<meta http-equiv="Content-Language" content="en-us">
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
	<title>{{Title}}</title>
	<link rel="stylesheet" href="../sumatra.css" type="text/css" />
	<link rel="stylesheet" href="../notion.css" type="text/css" />
</head>

<body>
	<script type="text/javascript" src="../sumatra.js"></script>

	<div id="container">
		<div id="banner">
			<h1 style="display:inline;">Sumatra PDF
				<font size="-1">is a PDF, ePub, MOBI, CHM, XPS, DjVu, CBZ, CBR reader for Windows</font>
			</h1>
			<script type="text/javascript">
				document.write(navHtml());
			</script>
			<a id="donate" href="/backers.html">Donate</a>
		</div>
		<br/>
		<div id="center">
			<div class="content">
			{{InnerHTML}}
			</div>
		</div>
	</div>
</body>
</html>
`

// GenerateHTML returns generated HTML
func (c *HTMLConverter) GenerateHTML() []byte {
	inner, err := c.conv.ToHTML()
	must(err)
	page := c.page.Root()
	f := page.FormatPage()
	isMono := f != nil && f.PageFont == "mono"
	s := ``
	if isMono {
		s += `<div style="font-family: monospace">`
	}
	s += string(inner)
	if isMono {
		s += `</div>`
	}
	title := page.Title
	s = strings.Replace(tmpl, "{{InnerHTML}}", s, 1)
	s = strings.Replace(s, "{{Title}}", title, 1)
	d := []byte(s)
	if false {
		d = fmthtml.Format(d)
	}
	return d
}

func notionToHTML(client *notionapi.Client, page *notionapi.Page, pages []*notionapi.Page, idToPage map[string]*notionapi.Page) {
	conv := NewHTMLConverter(page, pages, idToPage)
	conv.client = client
	html := conv.GenerateHTML()
	name := fileNameForPage(page)
	path := filepath.Join("docs", name)
	logf("Writing '%s' for title '%s'\n", path, page.Root().Title)
	u.WriteFileMust(path, html)
}

func websiteImportNotion() {
	logf("websiteImportNotion() started\n")
	must(os.Chdir("website"))
	client := newNotionClient()
	cache, err := caching_downloader.NewDirectoryCache(cacheDir)
	must(err)
	d := caching_downloader.New(cache, client)
	d.EventObserver = eventObserver
	d.RedownloadNewerVersions = true
	//d.NoReadCache = flgNoCache
	pages, err := d.DownloadPagesRecursively(startPageID, afterPageDownload)
	must(err)
	for _, page := range pages {
		notionToHTML(client, page, pages, d.IdToPage)
	}

	if false {
		// using https://github.com/netlify/cli
		cmd := exec.Command("netlify", "dev", "--dir", "website")
		u.RunCmdLoggedMust(cmd)
	}

	if false {
		err = os.Chdir("website")
		must(err)
		u.OpenBrowser("free-pdf-reader.html")
	}
}
