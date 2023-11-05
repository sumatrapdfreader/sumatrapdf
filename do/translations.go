package main

import (
	"bytes"
	"io/ioutil"
	"path/filepath"
	"regexp"
	"strings"
)

func verifyTranslationsMust() {
	d := downloadTranslationsMust()
	curr := readFileMust(translationsTxtPath)
	panicIf(!bytes.Equal(d, curr), "Translations did change!!!\nRun:\n.\\doit.bat -trans-dl\nto update translations\n")
}

// Translation describes a single translated text
type Translation struct {
	Text        string
	Lang        string
	Translation string
}

func trimEmptyLinesFromEnd(a []string) []string {
	for len(a) > 0 {
		lastIdx := len(a) - 1
		s := strings.TrimSpace(a[lastIdx])
		if len(s) > 0 {
			return a
		}
		a = a[:lastIdx]
	}
	return a
}

func parseTranslations(s string) map[string][]*Translation {
	res := map[string][]*Translation{}
	lines := strings.Split(s, "\n")[2:]
	// strip empty lines from the end
	lines = trimEmptyLinesFromEnd(lines)
	currStr := ""
	var currTranslations []*Translation
	for _, l := range lines {
		if len(l) == 0 {
			continue
		}
		// TODO: looks like apptranslator doesn't deal well with strings that
		// have newlines in them. Newline at the end ends up as an empty line
		// apptranslator should escape newlines and tabs etc. but for now
		// skip those lines as harmless
		if l[0] == ':' {
			if currStr != "" {
				panicIf(len(currTranslations) == 0)
				res[currStr] = currTranslations
			}
			currStr = l[1:]
			currTranslations = nil
		} else {
			parts := strings.SplitN(l, ":", 2)
			panicIf(len(parts) != 2, "Invalid line: '%s'", l)
			lang, trans := parts[0], parts[1]
			tr := &Translation{
				Text:        currStr,
				Lang:        lang,
				Translation: trans,
			}
			currTranslations = append(currTranslations, tr)
		}
	}

	if currStr != "" {
		panicIf(len(currTranslations) == 0)
		res[currStr] = currTranslations
	}

	return res
}

var (
	dirsToProcess = []string{"src"}
)

func getFilesToProcess() []string {

	shouldTranslate := func(path string) bool {
		ext := strings.ToLower(filepath.Ext(path))
		return ext == ".cpp"
	}

	var res []string
	for _, dir := range dirsToProcess {
		files, err := ioutil.ReadDir(dir)
		must(err)
		for _, f := range files {
			path := filepath.Join(dir, f.Name())
			if shouldTranslate(path) {
				res = append(res, path)
			}
		}
	}
	return res
}

var (
	translationPattern = regexp.MustCompile(`\b_TR[AN]?\("(.*?)"\)`)
)

func extractTranslations(s string) []string {
	var res []string
	a := translationPattern.FindAllStringSubmatch(s, -1)
	for _, el := range a {
		res = append(res, el[1])
	}
	return res
}

func extractStringsFromCFile(path string) []string {
	d := readFileMust(path)
	return extractTranslations(string(d))
}

func uniquifyStrings(a []string) []string {
	m := map[string]bool{}
	for _, s := range a {
		m[s] = true
	}
	var res []string
	for k := range m {
		res = append(res, k)
	}
	return res
}

func extractStringsFromCFilesNoPaths() []string {
	filesToProcess := getFilesToProcess()
	logf("Files to process: %d\n", len(filesToProcess))
	var res []string
	for _, path := range filesToProcess {
		a := extractStringsFromCFile(path)
		res = append(res, a...)
	}
	res = uniquifyStrings(res)
	logf("%d strings to translate\n", len(res))
	return res
}
