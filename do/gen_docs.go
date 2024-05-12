package main

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
	must(err)
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
		if !seenFirstH1 {
			if h, ok := node.(*ast.Heading); ok && h.Level == 1 {
				if isMainPage {
					seenFirstH1 = true
					return ast.SkipChildren, true
				}
				renderFirstH1(w, h, entering, &seenFirstH1)
				return ast.GoToNext, true
			}
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
			fileName := strings.Replace(uri, "%20", " ", -1)
			logvf("  mdName          : %s\n", fileName)
			if strings.HasPrefix(fileName, "Untitled Database") {
				fileName = strings.Replace(fileName, ".md", ".csv", -1)
				logvf("  mdName          : %s\n", fileName)
				return ast.GoToNext
			}

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
			link.Destination = []byte(getHTMLFileName(fileName))
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

	if name == "Commands.md" {
		s = strings.Replace(s, `<div>:search:</div>`, searchHTML, -1)
		toReplace := "</body>"
		s = strings.Replace(s, toReplace, searchJS+toReplace, 1)
	}
	mdInfo.data = []byte(s)
	return mdInfo.data, nil
}

func mdToHTMLAll() {
	logf("mdToHTMLAll starting\n")
	timeStart := time.Now()
	fsys = os.DirFS("docs")

	mdToHTML("SumatraPDF-documentation.md", false)
	for len(mdToProcess) > 0 {
		name := mdToProcess[0]
		mdToProcess = mdToProcess[1:]
		_, err := mdToHTML(name, false)
		must(err)
	}
	writeDocsHtmlFiles()
	//u.OpenBrowser(filepath.Join("docs", "www", "SumatraPDF-documentation.html"))
	logf("mdToHTMLAll finished in %s\n", time.Since(timeStart))
}

func writeDocsHtmlFiles() {
	for name, info := range mdProcessed {
		name = strings.ReplaceAll(name, ".md", ".html")
		path := filepath.Join("docs", "www", name)
		err := os.WriteFile(path, info.data, 0644)
		logf("wrote '%s', len: %d\n", path, len(info.data))
		must(err)
	}
	{
		// copy image files
		copyFileMustOverwrite = true
		dstDir := filepath.Join("docs", "www", "img")
		srcDir := filepath.Join("docs", "md", "img")
		copyFilesRecurMust(dstDir, srcDir)
	}
	{
		// create lzsa archive
		makeLzsa := filepath.Join("bin", "MakeLZSA.exe")
		archive := filepath.Join("docs", "manual.dat")
		os.Remove(archive)
		docsDir := filepath.Join("docs", "www")
		cmd := exec.Command(makeLzsa, archive, docsDir)
		runCmdLoggedMust(cmd)
	}
}

const (
	searchHTML = `
	<h3 style="margin-top: 1em;">Find command:</h3>
	<table style="margin-bottom: 2em;" class="collection-content">
		<thead>
			<tr>
				<th width="0">Command IDs</th>
				<th width="0">Keyboard shortcuts</th>
				<th width="0">Command Palette</th>
			</tr>
		</thead>
		<tbody>
			<tr>
				<td width="0"><input type="text" id="cmd_ids" /></td>
				<td width="0"><input type="text" id="key_sht" /></td>
				<td width="0"><input type="text" id="cmd_plt" /></td>
			</tr>
		</tbody>
	</table>
`

	searchJS = `
  <script>
    function driver() {
      let selectors = ["input#cmd_ids", "input#key_sht", "input#cmd_plt"];
      let q = "//table[contains(@class,'collection-content')]/tbody/tr[not(./td/input)]";
      let rows = getElementByXpath(q);
      console.log("rows:", rows.length);
      let lists = [], inputs = [];
      selectors.forEach((x, y) => { inputs[y] = document.querySelector(x); });
      for (let i = 1; i <= selectors.length; i++) {
        q = "//table[contains(@class,'collection-content')]/tbody/tr/td[(not(./input))][position()=" + i + "]";
        let els = getElementByXpath(q);
        els = els.map(x => x.innerText);
        lists[i - 1] = els;
      }

      lists[1] = lists[1].map(x => x.replace(/(?:(?<!\+)|(?<=\+\+))\,/g, ""));//removing commas b/w shortcuts
      function tableFilter() {
        let regexs = [
          getRegex_cmdids(inputs[0]),
          getRegex_keysht(inputs[1]),
          getRegex_cmdplt(inputs[2])
        ];
        rows.forEach(row => row.setAttribute("style", "display: none;"));
        let shortlist = (new Array(rows.length)).fill(undefined);
        regexs.forEach((regex, list_index) => {
          if (!!regex)
            lists[list_index].forEach((item, row_index) => {
              if (shortlist[row_index] === undefined)
                shortlist[row_index] = regex.test(item);
              else if (shortlist[row_index])
                shortlist[row_index] = regex.test(item);
            });
        });
        if (!regexs.some(x => !!x))
          rows.forEach(row => row.removeAttribute("style"));
        else
          shortlist.forEach((flag, index) => {
            if (flag)
              rows[index].removeAttribute("style");
          });
      };
      inputs.forEach(ele => setEvent(ele, tableFilter));
    };

    function setEvent(target, callback) {
      target.addEventListener("keyup", callback);
    };

    function getElementByXpath(xpathToExecute) {
      let result = [];
      let snapshotNodes = document.evaluate(xpathToExecute, document, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
      for (let i = 0; i < snapshotNodes.snapshotLength; i++)
        result.push(snapshotNodes.snapshotItem(i));
      return result;
    };

    function getRegex_cmdids(ele) {
      let ip_val = ele.value.replace(/([^\w\s])/g, "").replace(/\s+$/, "");
      if (ip_val.length == 0) return false;
      return new RegExp(
        ip_val
          .replace(/\s+(\w+)/g, "(?=.*$1)")
        , "i");
    };

    function getRegex_keysht(ele) {
      let ip_val = ele.value.replace(/\s+$/, "");
      if (ip_val.length == 0) return false;
      return new RegExp(
        "(?:(?=\\W)(?<=\\w)|(?<!\\w))(" +
        ip_val.replace(/([^\w\s])/g, "\\$1")
          .replace(/([^\s]+)/g, "($1)")
          .replace(/\s+/g, "\|")
        + ")(?:(?<=\\W)(?=\\w)|(?!\\w))"
        , "i");
    };

    function getRegex_cmdplt(ele) {
      let ip_val = ele.value.replace(/\s+$/, "");
      if (ip_val.length == 0) return false;
      return new RegExp(
        "(?:(?=\\W)(?<=\\w)|(?<!\\w))" +
        ip_val
          .replace(/([^\w\s])/g, "\\$1")
          .replace(/\s+([\w\W]+)/g, "(?=.*\\b$1)")
        , "i");
    };
    driver();
  </script>`
)
