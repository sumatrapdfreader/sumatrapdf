package do

import (
	"bytes"
	"encoding/csv"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/dustin/go-humanize"
	"github.com/gomarkdown/markdown"
	"github.com/gomarkdown/markdown/ast"
	mdhtml "github.com/gomarkdown/markdown/html"
	"github.com/gomarkdown/markdown/parser"

	"github.com/kjk/common/u"
)

var logvf = logf

type MdProcessedInfo struct {
	mdFileName string
	data       []byte
}

// paths are relative to "docs" folder
var (
	mdDocsDir   = path.Join("md")
	mdProcessed = map[string]*MdProcessedInfo{}
	mdToProcess = []string{}
	mdHTMLExt   = true
	fsys        fs.FS
)

const h1BreadcrumbsStart = `
	<div class="breadcrumbs">
		<div><a href="SumatraPDF-documentation.html">SumatraPDF documentation</a></div>
		<div>/</div>
		<div>`
const h1BreadcrumbsEnd = `</div>
</div>
`

func renderFirstH1(w io.Writer, h *ast.Heading, entering bool, seenFirstH1 *bool) {
	if entering {
		io.WriteString(w, h1BreadcrumbsStart)
	} else {
		*seenFirstH1 = true
		io.WriteString(w, h1BreadcrumbsEnd)
	}
}

func genCsvTableHTML(records [][]string, noHeader bool) string {
	if len(records) == 0 {
		return ""
	}
	lines := []string{`<table class="collection-content">`}
	if !noHeader {
		row := records[0]
		records = records[1:]
		push(&lines, "<thead>", "<tr>")
		for _, cell := range row {
			s := fmt.Sprintf(`<th>%s</th>`, cell)
			push(&lines, s)
		}
		push(&lines, "</tr>", "</thead>")
	}

	push(&lines, "<tbody>")
	for len(records) > 0 {
		push(&lines, "<tr>")
		row := records[0]
		records = records[1:]
		for i, cell := range row {
			cell = strings.TrimSpace(cell)
			if cell == "" {
				push(&lines, "<td>", "</td>")
				continue
			}
			inCode := i == 0 || i == 1
			push(&lines, "<td>")
			if inCode {
				// TODO: "Ctrl + W, Ctrl + F4"
				// should be rendered as:
				// <code>Ctrl + W</code>,&nbsp;<code>Ctrl + F4</code>
				s := fmt.Sprintf("<code>%s</code>", cell)
				push(&lines, s)
			} else {
				push(&lines, cell)
			}
			push(&lines, "</td>")
		}
		push(&lines, "</tr>")
	}

	push(&lines, "</tbody>", "</table>")
	return strings.Join(lines, "\n")
}

func renderCodeBlock(w io.Writer, cb *ast.CodeBlock, entering bool) {
	csvContent := bytes.TrimSpace(cb.Literal)
	// os.WriteFile("temp.csv", csvContent, 0644)
	r := csv.NewReader(bytes.NewReader(csvContent))
	records, err := r.ReadAll()
	if err != nil {
		logf("csv:\n%s\n\n", string(csvContent))
		must(err)
	}
	s := genCsvTableHTML(records, false)
	io.WriteString(w, s)
}

func renderColumns(w io.Writer, columns *Columns, entering bool) {
	if entering {
		io.WriteString(w, `<div class="doc-columns">`)
	} else {
		io.WriteString(w, `</div>`)
	}
}

func makeRenderHook(r *mdhtml.Renderer, isMainPage bool) mdhtml.RenderNodeFunc {
	seenFirstH1 := false
	return func(w io.Writer, node ast.Node, entering bool) (ast.WalkStatus, bool) {
		if h, ok := node.(*ast.Heading); ok {
			// first h1 is always a title of the page, turn it into bread-crumbs
			// (except for the main index page)
			if !seenFirstH1 && h.Level == 1 {
				if isMainPage {
					seenFirstH1 = true
					return ast.SkipChildren, true
				}
				renderFirstH1(w, h, entering, &seenFirstH1)
				return ast.GoToNext, true
			}
			// add: <a class="hlink" href="#${id}">#</a>
			if entering {
				r.HeadingEnter(w, h)
			} else {
				href := `<a class="hlink" href="#` + h.HeadingID + `"> # </a>`
				r.Outs(w, href)
				r.HeadingExit(w, h)
			}
			return ast.GoToNext, true
		}
		if cb, ok := node.(*ast.CodeBlock); ok {
			if string(cb.Info) != "commands" {
				return ast.GoToNext, false
			}
			renderCodeBlock(w, cb, entering)
			return ast.GoToNext, true
		}
		if columns, ok := node.(*Columns); ok {
			renderColumns(w, columns, entering)
			return ast.GoToNext, true
		}
		return ast.GoToNext, false
	}
}

func newMarkdownHTMLRenderer(isMainPage bool) *mdhtml.Renderer {
	htmlFlags := mdhtml.Smartypants |
		mdhtml.SmartypantsFractions |
		mdhtml.SmartypantsDashes |
		mdhtml.SmartypantsLatexDashes
	htmlOpts := mdhtml.RendererOptions{
		Flags:        htmlFlags,
		ParagraphTag: "div",
	}
	r := mdhtml.NewRenderer(htmlOpts)
	r.Opts.RenderNodeHook = makeRenderHook(r, isMainPage)
	return r
}

type Columns struct {
	ast.Container
}

var columns = []byte(":columns\n")

func parseColumns(data []byte) (ast.Node, []byte, int) {
	if !bytes.HasPrefix(data, columns) {
		return nil, nil, 0
	}
	i := len(columns)
	// find empty line
	// TODO: should also consider end of document
	end := bytes.Index(data[i:], columns)
	if end < 0 {
		return nil, data, 0
	}
	inner := data[i : end+i]
	res := &Columns{}
	return res, inner, end + i + i
}

func parserHook(data []byte) (ast.Node, []byte, int) {
	if node, d, n := parseColumns(data); node != nil {
		return node, d, n
	}
	return nil, nil, 0
}

func newMarkdownParser() *parser.Parser {
	extensions := parser.NoIntraEmphasis |
		parser.Tables |
		parser.FencedCode |
		parser.Autolink |
		parser.Strikethrough |
		parser.SpaceHeadings |
		parser.NoEmptyLineBeforeBlock |
		parser.AutoHeadingIDs

	p := parser.NewWithExtensions(extensions)
	p.Opts.ParserHook = parserHook
	return p
}

func getFileExt(s string) string {
	ext := filepath.Ext(s)
	return strings.ToLower(ext)
}

func removeNotionId(s string) string {
	if len(s) <= 32 {
		return s
	}
	isHex := func(c rune) bool {
		if c >= '0' && c <= '9' {
			return true
		}
		if c >= 'a' && c <= 'f' {
			return true
		}
		if c >= 'A' && c <= 'F' {
			return true
		}
		return false
	}
	suffix := s[len(s)-32:]
	for _, c := range suffix {
		if !isHex(c) {
			return s
		}
	}
	return s[:len(s)-32]
}

func getHTMLFileName(mdName string) string {
	mdName, _ = parseMdFileName(mdName)
	parts := strings.Split(mdName, ".")
	panicIf(len(parts) != 2)
	panicIf(parts[1] != "md")
	name := parts[0]
	name = removeNotionId(name)
	name = strings.TrimSpace(name)
	name = strings.Replace(name, " ", "-", -1)
	if mdHTMLExt {
		name += ".html"
	}
	return name
}

func FsFileExistsMust(fsys fs.FS, name string) {
	_, err := fsys.Open(name)
	must(err)
}

func checkMdFileExistsMust(name string) {
	path := path.Join(mdDocsDir, name)
	FsFileExistsMust(fsys, path)
}

// Commands.md#foo => "Commands.md", "foo"
func parseMdFileName(name string) (string, string) {
	parts := strings.Split(name, "#")
	if len(parts) == 1 {
		return name, ""
	}
	panicIf(len(parts) != 2)
	return parts[0], parts[1]
}

func astWalk(doc ast.Node) {
	ast.WalkFunc(doc, func(node ast.Node, entering bool) ast.WalkStatus {
		if img, ok := node.(*ast.Image); ok && entering {
			uri := string(img.Destination)
			if strings.HasPrefix(uri, "https://") {
				return ast.GoToNext
			}
			logf("  img.Destination:  %s\n", string(uri))
			fileName := strings.Replace(uri, "%20", " ", -1)
			checkMdFileExistsMust(fileName)
			img.Destination = []byte(fileName)
			return ast.GoToNext
		}

		if link, ok := node.(*ast.Link); ok && entering {
			uri := string(link.Destination)
			isExternalURI := func(uri string) bool {
				return (strings.HasPrefix(uri, "https://") || strings.HasPrefix(uri, "http://")) && !strings.Contains(uri, "sumatrapdfreader.org")
			}
			if isExternalURI(string(link.Destination)) {
				link.AdditionalAttributes = append(link.AdditionalAttributes, `target="_blank"`)
			}

			if strings.HasPrefix(uri, "https://") {
				return ast.GoToNext
			}
			// TODO: change to https://
			if strings.HasPrefix(uri, "http://") {
				return ast.GoToNext
			}
			if strings.HasPrefix(uri, "mailto:") {
				return ast.GoToNext
			}
			logvf("  link.Destination: %s\n", uri)
			fileNameWithHash := strings.Replace(uri, "%20", " ", -1)
			logvf("  mdName          : %s\n", fileNameWithHash)
			if strings.HasPrefix(fileNameWithHash, "Untitled Database") {
				fileNameWithHash = strings.Replace(fileNameWithHash, ".md", ".csv", -1)
				logvf("  mdName          : %s\n", fileNameWithHash)
				return ast.GoToNext
			}

			fileName, hash := parseMdFileName(fileNameWithHash)
			checkMdFileExistsMust(fileName)
			ext := getFileExt(fileName)
			if ext == ".png" || ext == ".jpg" || ext == ".jpeg" {
				return ast.GoToNext
			}
			if ext == ".csv" {
				return ast.GoToNext
			}
			panicIf(ext != ".md")
			push(&mdToProcess, fileName)
			dest := getHTMLFileName(fileName)
			if hash != "" {
				dest = dest + "#" + hash
			}
			link.Destination = []byte(dest)
		}

		return ast.GoToNext
	})
}

var (
	muMdToHTML sync.Mutex
)

func mdToHTML(name string, force bool) ([]byte, error) {
	name = strings.TrimPrefix(name, "docs-md/")
	logvf("mdToHTML: '%s', force: %v\n", name, force)
	isMainPage := name == "SumatraPDF-documentation.md"

	// called from http goroutines so needs to be thread-safe
	muMdToHTML.Lock()
	defer muMdToHTML.Unlock()

	mdInfo := mdProcessed[name]
	if mdInfo != nil && !force {
		logvf("mdToHTML: skipping '%s' because already processed\n", name)
		return mdInfo.data, nil
	}
	logvf("mdToHTML: processing '%s'\n", name)
	mdInfo = &MdProcessedInfo{
		mdFileName: name,
	}
	mdProcessed[name] = mdInfo

	filePath := path.Join(mdDocsDir, name)
	md, err := fs.ReadFile(fsys, filePath)
	if err != nil {
		return nil, err
	}
	logf("read:  %s size: %s\n", filePath, u.FormatSize(int64(len(md))))
	parser := newMarkdownParser()
	renderer := newMarkdownHTMLRenderer(isMainPage)
	doc := parser.Parse(md)
	astWalk(doc)
	res := markdown.Render(doc, renderer)
	innerHTML := string(res)

	innerHTML = `<div class="notion-page">` + innerHTML + `</div>`
	innerHTML += `<hr>`
	editLink := `<center><a href="https://github.com/sumatrapdfreader/sumatrapdf/blob/master/docs/md/{name}" target="_blank" class="suggest-change">edit</a></center>`
	editLink = strings.Replace(editLink, "{name}", name, -1)
	innerHTML += editLink
	filePath = "manual.tmpl.html"
	tmplManual, err := fs.ReadFile(fsys, filePath)
	must(err)
	s := strings.Replace(string(tmplManual), "{{InnerHTML}}", innerHTML, -1)
	title := getHTMLFileName(name)
	title = strings.Replace(title, ".html", "", -1)
	title = strings.Replace(title, "-", " ", -1)
	s = strings.Replace(s, "{{Title}}", title, -1)

	panicIf(searchJS == "")
	if name == "Commands.md" {
		s = strings.Replace(s, `<div>:search:</div>`, searchHTML, -1)
		toReplace := "</body>"
		s = strings.Replace(s, toReplace, searchJS+toReplace, 1)
	}
	mdInfo.data = []byte(s)
	return mdInfo.data, nil
}

var searchJS = ``
var searchHTML = ``

func loadSearchJS() {
	{
		path := filepath.Join("do", "gen_docs.search.js")
		d, err := os.ReadFile(path)
		must(err)
		searchJS = `<script>` + string(d) + `</script>`
	}
	{
		path := filepath.Join("do", "gen_docs.search.html")
		d, err := os.ReadFile(path)
		must(err)
		searchHTML = string(d)
	}
}

func removeHTMLFilesInDir(dir string) {
	files, err := os.ReadDir(dir)
	must(err)
	for _, fi := range files {
		if fi.IsDir() {
			continue
		}
		name := fi.Name()
		if strings.HasSuffix(name, ".html") {
			path := filepath.Join(dir, name)
			must(os.Remove(path))
		}
	}
}

func getWWWOutDir() string {
	return filepath.Join("docs", "www")
}

func writeDocsHtmlFiles() {
	wwwOutDir := getWWWOutDir()
	imgOutDir := filepath.Join(wwwOutDir, "img")
	// images are copied from docs/md/img so remove potentially stale images
	must(os.RemoveAll(imgOutDir))
	must(os.MkdirAll(filepath.Join(wwwOutDir, "img"), 0755))
	// remove potentially stale .html files
	// can't just remove the directory because has .css and .ico files
	removeHTMLFilesInDir(wwwOutDir)
	for name, info := range mdProcessed {
		name = strings.ReplaceAll(name, ".md", ".html")
		path := filepath.Join(wwwOutDir, name)
		err := os.WriteFile(path, info.data, 0644)
		logf("wrote '%s', len: %d\n", path, len(info.data))
		must(err)
	}
	{
		// copy image files
		copyFileMustOverwrite = true
		dstDir := filepath.Join(wwwOutDir, "img")
		srcDir := filepath.Join("docs", "md", "img")
		copyFilesRecurMust(dstDir, srcDir)
	}
}

func genHTMLDocsFromMarkdown() {
	logf("genHTMLDocsFromMarkdown starting\n")
	loadSearchJS()
	fsys = os.DirFS("docs")

	mdToHTML("SumatraPDF-documentation.md", false)
	for len(mdToProcess) > 0 {
		name := mdToProcess[0]
		mdToProcess = mdToProcess[1:]
		_, err := mdToHTML(name, false)
		must(err)
	}
	writeDocsHtmlFiles()
}

func extractCommandsFromMarkdown() []string {
	// CmdHelpOpenManual,,Help: Manual
	// =>
	// CmdHelpOpenManual
	// or "" if not found
	extractCommandFromMarkdownLine := func(s string) string {
		if !strings.HasPrefix(s, "Cmd") {
			return ""
		}
		idx := strings.Index(s, ",")
		panicIf(idx < 0)
		s = s[:idx]
		return s
	}

	path := filepath.Join("docs", "md", "Commands.md")
	lines, err := u.ReadLines(path)
	must(err)
	var res []string
	for _, l := range lines {
		s := extractCommandFromMarkdownLine(l)
		if s != "" {
			push(&res, s)
		}
	}
	panicIf(len(res) < 20)
	return res
}

func extractCommandFromSource() []string {
	//	V(CmdCreateAnnotCaret, "Create Caret Annotation")                     \
	//
	// =>
	// CmdCreateAnnotCaret
	// or "" if not found
	extractCommandFromSourceLine := func(s string) string {
		commentIdx := strings.Index(s, "//")
		idx := strings.Index(s, "V(Cmd")
		if idx < 0 {
			return ""
		}
		if commentIdx >= 0 && commentIdx < idx {
			// this is a commented-out line
			return ""
		}
		s = s[idx+2:]
		panicIf(!strings.HasPrefix(s, "Cmd"))
		idx = strings.Index(s, ",")
		panicIf(idx < 0)
		s = s[:idx]
		return s
	}

	path := filepath.Join("src", "Commands.h")
	lines, err := u.ReadLines(path)
	must(err)
	var res []string
	for _, l := range lines {
		s := extractCommandFromSourceLine(l)
		if s != "" {
			push(&res, s)
		}
	}
	panicIf(len(res) < 20)
	return res
}

func checkComandsAreDocumented() {
	logf("checkCommandsAreDocumented\n")
	commandsInSource := extractCommandFromSource()
	logf("%d commands in Commands.h\n", len(commandsInSource))
	commandsInDocs := extractCommandsFromMarkdown()
	logf("%d commands in Commands.md\n", len(commandsInDocs))
	mDocs := map[string]bool{}
	for _, c := range commandsInDocs {
		mDocs[c] = true
	}
	mSrc := map[string]bool{}
	for _, c := range commandsInSource {
		if mDocs[c] {
			delete(mDocs, c)
			continue
		}
		mSrc[c] = true
	}
	if len(mSrc) > 0 {
		logf("%d in Commands.h but not Commands.md:\n", len(mSrc))
		for c := range mSrc {
			logf("  %s\n", c)
		}
	}
	if len(mDocs) > 0 {
		logf("%d in Commands.md but not in Commands.h:\n", len(mDocs))
		for c := range mDocs {
			logf("  %s\n", c)
		}
	}
}

func copyDocsToWebsite() {
	logf("copyDocsToWebsite()\n")
	updateSumatraWebsite()
	srcDir := filepath.Join("docs", "md")
	websiteDir := getWebsiteDir()
	dstDir := filepath.Join(websiteDir, "server", "www", "docs-md")
	must(os.RemoveAll(dstDir))

	copyFilesExtsToNormalizeNL = []string{".md", ".css"}
	copyFileMustOverwrite = true
	copyFilesRecurMust(dstDir, srcDir)
	files := []string{"notion.css", "sumatra.css", "gen_toc.js"}
	for _, name := range files {
		srcPath := filepath.Join("docs", "www", name)
		dstPath := filepath.Join(websiteDir, "server", "www", name)
		copyFileMust(dstPath, srcPath)
	}

	files = []string{"gen_docs.search.js", "gen_docs.search.html"}
	for _, name := range files {
		srcPath := filepath.Join("do", name)
		dstPath := filepath.Join(websiteDir, "server", "www", name)
		copyFileMust(dstPath, srcPath)
	}

	d := runExeInDirMust(websiteDir, "git", "status")
	logf("\n%s\n", string(d))
}

// TODO: for now we just copy .md files to sumatra-website repo and use
// the existing md => html generation, which is duplicate of what we do here
// if we improve html generation here a lot, we'll switch to generating
// html files for sumatra-website here
func genHTMLDocsForWebsite() {
	logf("genHTMLDocsForWebsite starting\n")
	dir := updateSumatraWebsite()
	currBranch := getCurrentBranchMust(dir)
	panicIf(currBranch != "master")
	// don't use .html extension in links to generated .html files
	// for docs we need them because they are shown from file system
	// for website we prefer "clean" links because they are served via web server
	mdHTMLExt = false
	copyDocsToWebsite()
}

func genHTMLDocsForApp() {
	logf("genHTMLDocsFromMarkdown starting\n")
	timeStart := time.Now()
	defer func() {
		logf("genHTMLDocsFromMarkdown finished in %s\n", time.Since(timeStart))
	}()

	genHTMLDocsFromMarkdown()
	wwwOutDir := getWWWOutDir()
	{
		// create lzsa archive
		makeLzsa := filepath.Join("bin", "MakeLZSA.exe")
		archive := filepath.Join("docs", "manual.dat")
		os.Remove(archive)
		cmd := exec.Command(makeLzsa, archive, wwwOutDir)
		runCmdLoggedMust(cmd)
		size := u.FileSize(archive)
		sizeH := humanize.Bytes(uint64(size))
		logf("size of '%s': %s\n", archive, sizeH)
	}
	{
		dir, err := filepath.Abs(wwwOutDir)
		must(err)
		url := "file://" + filepath.Join(dir, "SumatraPDF-documentation.html")
		logf("To view, open:\n%s\n", url)
	}
	checkComandsAreDocumented()
}
