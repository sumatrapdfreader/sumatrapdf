package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// AnalyzeLine has info about a warning line from prefast/analyze build
// Given:
//
// c:\users\kjk\src\sumatrapdf\ext\unarr\rar\uncompress-rar.c(171): warning C6011:
// Dereferencing NULL pointer 'code->table'. : Lines: 163, 165, 169, 170,
// 171 [C:\Users\kjk\src\sumatrapdf\vs2015\Installer.vcxproj]
//
// FilePath will be: "ext\unarr\rar\uncompress-rar.c"
// LineNo will be: 171
// Message will be: warning C6011: Dereferencing NULL pointer 'code->table'. : Lines: 163, 165, 169, 170, 171
type AnalyzeLine struct {
	FilePath string
	LineNo   int
	Message  string
	OrigLine string
}

// ByPathLine is to sort AnalyzeLine by file path then by line number
type ByPathLine []*AnalyzeLine

func (s ByPathLine) Len() int {
	return len(s)
}
func (s ByPathLine) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}
func (s ByPathLine) Less(i, j int) bool {
	if s[i].FilePath == s[j].FilePath {
		return s[i].LineNo < s[j].LineNo
	}
	return s[i].FilePath < s[j].FilePath
}

var (
	currDirLenCached int
)

func currDirLen() int {
	if currDirLenCached == 0 {
		dir, err := os.Getwd()
		fataliferr(err)
		currDirLenCached = len(dir)
	}
	return currDirLenCached
}

func pre(s string) string {
	return `<pre style="white-space: pre-wrap;">` + s + `</pre>`
}

func a(url, txt string) string {
	return fmt.Sprintf(`<a href="%s">%s</a>`, url, txt)
}

//https://github.com/sumatrapdfreader/sumatrapdf/blob/c760b1996bec63c0bd9b2910b0811c41ed26db60/premake5.lua
func htmlizeSrcLink(al *AnalyzeLine, gitVersion string) string {
	path := strings.Replace(al.FilePath, "\\", "/", -1)
	lineNo := al.LineNo
	uri := fmt.Sprintf("https://github.com/sumatrapdfreader/sumatrapdf/blob/%s/%s#L%d", gitSha1, path, lineNo)
	txt := fmt.Sprintf("%s(%d)", al.FilePath, lineNo)
	return a(uri, txt)
}

func htmlizeErrorLines(errors []*AnalyzeLine) ([]string, []string, []string) {
	var sumatraErrors, mupdfErrors, extErrors []string
	for _, al := range errors {
		s := htmlizeSrcLink(al, gitSha1) + " : " + al.Message
		path := al.FilePath
		if strings.HasPrefix(path, "src") {
			sumatraErrors = append(sumatraErrors, s)
		} else if strings.HasPrefix(path, "mupdf") {
			mupdfErrors = append(mupdfErrors, s)
		} else if strings.HasPrefix(path, "ext") {
			extErrors = append(extErrors, s)
		} else {
			extErrors = append(extErrors, s)
		}
	}
	return sumatraErrors, mupdfErrors, extErrors
}

func genAnalyzeHTML(errors []*AnalyzeLine) string {
	sumatraErrors, mupdfErrors, extErrors := htmlizeErrorLines(errors)
	nSumatraErrors := len(sumatraErrors)
	nMupdfErrors := len(mupdfErrors)
	nExtErrors := len(extErrors)

	res := []string{"<html>", "<body>"}

	homeLink := a("../index.html", "Home")
	commitLink := a("https://github.com/sumatrapdfreader/sumatrapdf/commit/"+gitSha1, gitSha1)
	s := fmt.Sprintf("%s: commit %s, %d warnings in sumatra code, %d in mupdf, %d in ext:", homeLink, commitLink, nSumatraErrors, nMupdfErrors, nExtErrors)
	res = append(res, s)

	s = pre(strings.Join(sumatraErrors, "\n"))
	res = append(res, s)

	res = append(res, "<p>Warnings in mupdf code:</p>")
	s = pre(strings.Join(mupdfErrors, "\n"))
	res = append(res, s)

	res = append(res, "<p>Warnings in ext code:</p>")
	s = pre(strings.Join(extErrors, "\n"))
	res = append(res, s)

	res = append(res, "</pre>")
	res = append(res, "</body>", "</html>")
	return strings.Join(res, "\n")
}

func parseAnalyzeLine(s string) AnalyzeLine {
	sOrig := s
	// remove " [C:\Users\kjk\src\sumatrapdf\vs2015\Installer.vcxproj]" from the end
	end := strings.LastIndex(s, " [")
	fatalif(end == -1, "invalid line '%s'\n", sOrig)
	s = s[:end]
	parts := strings.SplitN(s, "): ", 2)
	fatalif(len(parts) != 2, "invalid line '%s'\n", sOrig)
	res := AnalyzeLine{
		OrigLine: sOrig,
		Message:  parts[1],
	}
	s = parts[0]
	end = strings.LastIndex(s, "(")
	fatalif(end == -1, "invalid line '%s'\n", sOrig)
	// change
	// c:\users\kjk\src\sumatrapdf\ext\unarr\rar\uncompress-rar.c
	// =>
	// ext\unarr\rar\uncompress-rar.c
	path := s[:end]
	// sometimes the line starts with:
	// 11>c:\users\kjk\src\sumatrapdf\ext\bzip2\bzlib.c(238)
	start := strings.Index(path, ">")
	if start != -1 {
		path = path[start+1:]
	}
	start = currDirLen() + 1
	res.FilePath = path[start:]
	n, err := strconv.Atoi(s[end+1:])
	fataliferr(err)
	res.LineNo = n
	return res
}

func isSrcFile(name string) bool {
	ext := strings.ToLower(filepath.Ext(name))
	switch ext {
	case ".cpp", ".c", ".h":
		return true
	}
	return false
}

// the compiler prints file names lower cased, we want real name in file system
// (otherwise e.g. links to github break)
func fixFileNames(a []*AnalyzeLine) {
	fmt.Printf("fixFileNames\n")
	files := make(map[string]string)
	filepath.Walk(".", func(path string, fi os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if !isSrcFile(path) {
			return nil
		}
		//fmt.Printf("path: '%s', name: '%s'\n", path, fi.Name())
		pathLower := strings.ToLower(path)
		files[pathLower] = path
		return nil
	})
	for _, al := range a {
		if sub := files[al.FilePath]; sub != "" {
			al.FilePath = sub
		}
	}
}

func parseAnalyzeOutput(d []byte) {
	lines := toTrimmedLines(d)
	var warnings []string
	for _, line := range lines {
		if strings.Contains(line, ": warning C") {
			warnings = append(warnings, line)
		}
	}

	seen := make(map[string]bool)
	var deDuped []*AnalyzeLine
	for _, s := range warnings {
		al := parseAnalyzeLine(s)
		full := fmt.Sprintf("%s, %d, %s\n", al.FilePath, al.LineNo, al.Message)
		if !seen[full] {
			deDuped = append(deDuped, &al)
			seen[full] = true
			//fmt.Print(full)
		}
	}

	sort.Sort(ByPathLine(deDuped))
	fixFileNames(deDuped)

	if false {
		for _, al := range deDuped {
			fmt.Printf("%s, %d, %s\n", al.FilePath, al.LineNo, al.Message)
		}
	}
	fmt.Printf("\n\n%d warnings\n", len(deDuped))

	s := genAnalyzeHTML(deDuped)
	err := ioutil.WriteFile("analyze-errors.html", []byte(s), 0644)
	fataliferr(err)
	// TODO: open a browser with analyze-errors.html
}

func parseSavedAnalyzeOuptut() {
	d, err := ioutil.ReadFile("analyze-output.txt")
	fataliferr(err)
	parseAnalyzeOutput(d)
}
