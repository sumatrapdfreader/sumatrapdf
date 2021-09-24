package main

import (
	"html"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/kjk/notionapi"
	"github.com/kjk/notionapi/tohtml"
)

type NotionID = notionapi.NotionID

var (
	newNotionID = notionapi.NewNotionID

	cacheDir       = "notion_cache"
	imagesCacheDIr = filepath.Join("docs", "img")
	startPageID    = "fed36a5624d443fe9f7be0e410ecd715"
)

func fileNameFromTitle(title string) string {
	return urlify(title) + ".html"
}

func fileNameForPage(page *notionapi.Page) string {
	title := page.Root().Title
	return fileNameFromTitle(title)
}

func afterPageDownload(di *notionapi.DownloadInfo) error {
	//logf(ctx(), "Downloaded page %sn\n", page.ID)
	return nil
}

// HTMLConverter renders article as html
type HTMLConverter struct {
	client   *notionapi.CachingClient
	page     *notionapi.Page
	idToPage map[string]*notionapi.Page
	conv     *tohtml.Converter
}

// PageByID returns a page given its id
func (c *HTMLConverter) PageByID(nid *NotionID) *notionapi.Page {
	page := c.idToPage[nid.NoDashID]
	if page != nil {
		return page
	}
	page = c.idToPage[nid.DashID]
	if page == nil {
		logf(ctx(), "Didn't find page for id %s' ('%s') in %d pages\n", nid.NoDashID, nid.DashID, len(c.idToPage))
		for id := range c.idToPage {
			logf(ctx(), "%s\n", id)
		}
	}
	return page
}

func (c *HTMLConverter) getURLAndTitleForBlock(block *notionapi.Block) (string, string) {
	id := newNotionID(block.ID)
	page := c.PageByID(id)
	if page == nil {
		title := block.Title
		logf(ctx(), "No page for id %s %s\n", id.NoDashID, title)
		pageURL := "https://notion.so/" + notionapi.ToNoDashID(c.page.ID)
		logf(ctx(), "Link from page: %s\n", pageURL)
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
	rsp, err := c.client.DownloadFile(link, block)
	if err != nil {
		logf(ctx(), "genImage: downloadAndCacheImage('%s') from page https://notion.so/%s failed with '%s'\n", link, normalizeID(c.page.ID), err)
		must(err)
	}
	path := rsp.CacheFilePath
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
	nid := newNotionID(id)
	page := c.PageByID(nid)
	// this might happen when I link to some-one else's public notion pages
	// but we don't do that for Sumatra docs
	if page == nil {
		logf(ctx(), "Didn't find page for url '%s', id '%s'\n", uri, id)
		os.Exit(0)
	}
	panicIf(page == nil)
	return fileNameForPage(page)
}

// NewHTMLConverter returns new HTMLGenerator
func NewHTMLConverter(page *notionapi.Page, pages []*notionapi.Page) *HTMLConverter {
	idToPage := map[string]*notionapi.Page{}
	for _, p := range pages {
		nid := notionapi.NewNotionID(p.ID)
		idToPage[nid.NoDashID] = p
	}

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
	s = strings.Replace(s, `<details open="">`, `<details>`, -1)
	d := []byte(s)
	return d
}

func notionToHTML(client *notionapi.CachingClient, page *notionapi.Page, pages []*notionapi.Page) {
	conv := NewHTMLConverter(page, pages)
	conv.client = client
	html := conv.GenerateHTML()
	name := fileNameForPage(page)
	path := filepath.Join("docs", name)
	logf(ctx(), "Writing '%s' for title '%s'\n", path, page.Root().Title)
	writeFileMust(path, html)
}

func checkPrettierExist() {
	cmd := exec.Command("prettier", "-v")
	err := cmd.Run()
	if err != nil {
		logf(ctx(), "prettier doesn't seem to be installed. Install with:\n")
		logf(ctx(), "npm i -g prettier\n")
		os.Exit(1)
	}
}

func newNotionClient() *notionapi.CachingClient {
	//token := os.Geenv("NOTION_TOKEN")
	//panicIf(token == "", "NOTION_TOKEN env variable not set, needed for downloading images\n")
	// Note: public page, no need for a token
	token := ""
	client := &notionapi.Client{
		AuthToken: token,
		DebugLog:  true,
	}
	// if true, shows http requests sent to notion
	if false {
		client.Logger = os.Stdout
	}
	d, err := notionapi.NewCachingClient(cacheDir, client)
	must(err)
	d.CacheDirFiles = imagesCacheDIr
	if flgNoCache {
		d.Policy = notionapi.PolicyDownloadAlways
	}
	return d
}

func websiteImportNotion() {
	logf(ctx(), "websiteImportNotion() started\n")
	checkPrettierExist()
	must(os.Chdir("website"))
	d := newNotionClient()
	pages, err := d.DownloadPagesRecursively(startPageID, afterPageDownload)
	must(err)
	{
		// delete .html files as they might be stale
		// we are in a
		files, err := filepath.Glob(filepath.Join("docs", "*.html"))
		if err == nil {
			for _, f := range files {
				err = os.Remove(f)
				if err != nil {
					logf(ctx(), "Failed to remove file '%s'\n", f)
				} else {
					logf(ctx(), "Removed file '%s'\n", f)
				}
			}
		} else {
			logf(ctx(), "filepath.Glob() failed with '%s'\n", err)
		}
	}
	for _, page := range pages {
		notionToHTML(d, page, pages)
	}

	if false {
		// run formatting in background to get to preview sooner
		go func() {
			// to install prettier: npm i -g prettier
			// TODO: automatically install if not installed
			cmd := exec.Command("prettier", "--html-whitespace-sensitivity", "strict", "--write", `*.html`)
			cmd.Dir = "docs" // only imported pages from notion
			runCmdLoggedMust(cmd)
		}()
	}

	if true {
		websiteRunLocally(".")
	}

	if false {
		//err = os.Chdir("website")
		//must(err)
		openBrowser("free-pdf-reader.html")
	}
}
