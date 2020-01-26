package main

import (
	"bytes"
	"crypto/sha1"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"text/template"
	"time"

	"github.com/kjk/u"
)

func must(err error) {
	u.Must(err)
}

func logf(format string, args ...interface{}) {
	u.Logf(format, args...)
}

func fatalf(format string, args ...interface{}) {
	s := fmt.Sprintf(format, args...)
	panic(s)
}

func fatalIf(cond bool, args ...interface{}) {
	u.PanicIf(cond, args...)
}

func panicIf(cond bool, args ...interface{}) {
	u.PanicIf(cond, args...)
}

func fatalIfErr(err error) {
	if err != nil {
		panic(err)
	}
}

func absPathMust(path string) string {
	res, err := filepath.Abs(path)
	must(err)
	return res
}

func runExeMust(c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	logf("> %s\n", cmd)
	out, err := cmd.CombinedOutput()
	must(err)
	return []byte(out)
}

func runExeLoggedMust(c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	out := u.RunCmdLoggedMust(cmd)
	return []byte(out)
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

func fileSizeMust(path string) int64 {
	size, err := u.GetFileSize(path)
	must(err)
	return size
}

func pathExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func removeDirMust(dir string) {
	err := os.RemoveAll(dir)
	fatalIfErr(err)
}

func removeFileMust(path string) {
	if !u.FileExists(path) {
		return
	}
	err := os.Remove(path)
	fatalIfErr(err)
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

func toTrimmedLines(d []byte) []string {
	lines := strings.Split(string(d), "\n")
	i := 0
	for _, l := range lines {
		l = strings.TrimSpace(l)
		// remove empty lines
		if len(l) > 0 {
			lines[i] = l
			i++
		}
	}
	return lines[:i]
}

func dataSha1Hex(d []byte) string {
	sha1 := sha1.Sum(d)
	return fmt.Sprintf("%x", sha1[:])
}

func fileSha1Hex(path string) (string, error) {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return "", err
	}
	sha1 := sha1.Sum(d)
	return fmt.Sprintf("%x", sha1[:]), nil
}

func httpDlMust(uri string) []byte {
	res, err := http.Get(uri)
	fatalIfErr(err)
	d, err := ioutil.ReadAll(res.Body)
	res.Body.Close()
	fatalIfErr(err)
	return d
}

func httpDlToFileMust(uri string, path string, sha1Hex string) {
	if u.FileExists(path) {
		sha1File, err := fileSha1Hex(path)
		fatalIfErr(err)
		fatalIf(sha1File != sha1Hex, "file '%s' exists but has sha1 of %s and we expected %s", path, sha1File, sha1Hex)
		return
	}
	logf("Downloading '%s'\n", uri)
	d := httpDlMust(uri)
	sha1File := dataSha1Hex(d)
	fatalIf(sha1File != sha1Hex, "downloaded '%s' but it has sha1 of %s and we expected %s", uri, sha1File, sha1Hex)
	err := ioutil.WriteFile(path, d, 0755)
	fatalIfErr(err)
}

func evalTmpl(s string, v interface{}) string {
	tmpl, err := template.New("tmpl").Parse(s)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, v)
	must(err)
	return buf.String()
}
