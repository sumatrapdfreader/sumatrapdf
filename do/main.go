package main

import (
	"path/filepath"
	"strings"

	"github.com/kjk/u"
)

const (
	// https://help.github.com/en/github/automating-your-workflow-with-github-actions/software-in-virtual-environments-for-github-actions#visual-studio-2019-enterprise
	vsPathGitHub = `C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise`
	// because I'm poor
	vsPathLocal = `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community`
)

func must(err error) {
	u.Must(err)
}

func logf(format string, args ...interface{}) {
	u.Logf(format, args...)
}

func listExeFiles(dir string) {
	if !u.DirExists(dir) {
		logf("Directory '%s' doesn't exist\n", dir)
		return
	}
	files := u.ListFilesInDir(dir, true)
	isExe := func(s string) bool {
		ext := strings.ToLower(filepath.Ext(s))
		switch ext {
		case ".bat", ".exe", ".cmd":
			return true
		}
		return false
	}
	shouldRemember := func(s string) bool {
		s = strings.ToLower(s)
		switch {
		case strings.Contains(s, "msbuild.exe"):
			return true
		case strings.Contains(s, "msdev"):
			return true
		}
		return false
	}
	logf("Exe files in '%s'\n", dir)
	var remember []string
	for _, f := range files {
		if !isExe(f) {
			continue
		}
		f = strings.TrimPrefix(f, dir)
		f = strings.TrimPrefix(f, "\\")
		logf("%s\n", f)
		if shouldRemember(f) {
			remember = append(remember, f)
		}
	}
	logf("\n")
	for _, s := range remember {
		logf("%s\n", s)
	}
}

func main() {
	logf("Starting do\n")
	listExeFiles(vsPathLocal)
	listExeFiles(vsPathGitHub)
}
