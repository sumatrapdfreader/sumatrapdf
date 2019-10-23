package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/u"
)

func must(err error) {
	u.Must(err)
}

func logf(format string, args ...interface{}) {
	u.Logf(format, args...)
}

func makePrintDuration(name string) func() {
	logf("%s\n", name)
	timeStart := time.Now()
	return func() {
		dur := time.Since(timeStart)
		logf("%s took %s\n", name, u.FormatDuration(dur))
	}
}

// run a .bat script and capture environment variables after
func getEnvAfterScript(path string) []string {
	if !u.FileExists(path) {
		return nil
	}
	dir, script := filepath.Split(path)

	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	logf("Executing: %s in %s\n", cmd, cmd.Dir)
	resBytes, err := cmd.Output()
	must(err)
	res := string(resBytes)
	parts := strings.Split(res, "\n")
	u.PanicIf(len(parts) == 1, "split failed\nres:\n%s\n", res)
	for idx, env := range parts {
		parts[idx] = strings.TrimSpace(env)
	}
	return parts
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
		//case strings.Contains(s, "msbuild"):
		//	return true
		case strings.Contains(s, "signtool.exe"):
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

func findFile(dir string, match func(string, os.FileInfo) bool) {
	fn := func(path string, info os.FileInfo, err error) error {
		if match(path, info) {
			logf("Found: '%s'\n", path)
		}
		return nil
	}
	filepath.Walk(dir, fn)
}

func findSigntool() {
	isSigntool := func(path string, fi os.FileInfo) bool {
		s := strings.ToLower(fi.Name())
		return s == "signtool.exe"
	}
	findFile(`C:\Program Files (x86)`, isSigntool)
}
