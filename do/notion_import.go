package main

import (
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

func afterPageDownload(page *notionapi.Page) error {
	logf("Downloaded page %sn", page.ID)
	return nil
}

// HTMLConverter renders article as html
type HTMLConverter struct {
	page     *notionapi.Page
	idToPage map[string]*notionapi.Page
	conv     *tohtml.Converter
}

// NewHTMLConverter returns new HTMLGenerator
func NewHTMLConverter(page *notionapi.Page, idToPage map[string]*notionapi.Page) *HTMLConverter {
	res := &HTMLConverter{
		page:     page,
		idToPage: idToPage,
	}

	conv := tohtml.NewConverter(page)
	notionapi.PanicOnFailures = true
	//conv.RenderBlockOverride = res.blockRenderOverride
	//conv.RewriteURL = res.rewriteURL
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
	s := `<p></p>`
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

func fileNameForPage(page *notionapi.Page) string {
	title := page.Root().Title
	return urlify(title) + ".html"
}

func notionToHTML(page *notionapi.Page, idToPage map[string]*notionapi.Page) {
	conv := NewHTMLConverter(page, idToPage)
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
		notionToHTML(page, d.IdToPage)
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
