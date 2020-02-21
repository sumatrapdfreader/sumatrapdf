package main

import (
	"html"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/kjk/fmthtml"
	"github.com/kjk/notionapi"
	"github.com/kjk/notionapi/caching_downloader"
	"github.com/kjk/notionapi/tohtml"
	"github.com/kjk/u"
)

var (
	cacheDir        = "notion_cache"
	startPageID     = "fed36a5624d443fe9f7be0e410ecd715"
	nDownloadedPage int
)

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
	if token == "" {
		logf("NOTION_TOKEN env variable not set, only public pages accessible\n")
	}
	// TODO: verify token still valid, somehow
	client := &notionapi.Client{
		AuthToken: token,
	}
	if true {
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
	id2 := notionapi.ToDashID(id)
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
	page := c.PageByID(block.ID)
	if page == nil {
		title := block.Title
		logf("No page for id %s %s\n", block.ID, title)
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

// if returns false, the block will be rendered with default
func (c *HTMLConverter) blockRenderOverride(block *notionapi.Block) bool {
	switch block.Type {
	case notionapi.BlockPage:
		return c.RenderPage(block)
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
	d := fmthtml.Format([]byte(s))
	return d
}

func notionToHTML(page *notionapi.Page, pages []*notionapi.Page, idToPage map[string]*notionapi.Page) {
	conv := NewHTMLConverter(page, pages, idToPage)
	html := conv.GenerateHTML()
	name := fileNameForPage(page)
	path := filepath.Join("www", "docs", name)
	logf("Writing '%s' for title '%s'\n", path, page.Root().Title)
	u.WriteFileMust(path, html)
}

func websiteImportNotion() {
	cdWebsiteDir()
	logf("websiteImportNotion() started\n")
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
		notionToHTML(page, pages, d.IdToPage)
	}

	if false {
		// using https://github.com/netlify/cli
		cmd := exec.Command("netlify", "dev", "--dir", "www")
		u.RunCmdLoggedMust(cmd)
	}

	err = os.Chdir("www")
	must(err)
	u.OpenBrowser("free-pdf-reader.html")
}
