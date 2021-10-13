package main

import (
	"bytes"
	"context"
	"fmt"
	"io/fs"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"text/template"
	"time"

	"github.com/kjk/common/u"
)

var (
	must              = u.Must
	panicIf           = u.PanicIf
	fatalIf           = panicIf
	urlify            = u.Slug
	fileExists        = u.FileExists
	dirExists         = u.DirExists
	pathExists        = u.PathExists
	normalizeNewlines = u.NormalizeNewlines
	formatSize        = u.FormatSize
	getFileSize       = u.FileSize
	copyFile          = u.CopyFile
	fileSha1Hex       = u.FileSha1Hex
	formatDuration    = u.FormatDuration
	readLinesFromFile = u.ReadLines
	toTrimmedLines    = u.ToTrimmedLines
)

func ctx() context.Context {
	return context.Background()
}

func logf(ctx context.Context, s string, arg ...interface{}) {
	if len(arg) > 0 {
		s = fmt.Sprintf(s, arg...)
	}
	fmt.Print(s)
}

func absPathMust(path string) string {
	res, err := filepath.Abs(path)
	must(err)
	return res
}

func runExeMust(c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	logf(ctx(), "> %s\n", cmd)
	out, err := cmd.CombinedOutput()
	must(err)
	return []byte(out)
}

func runExeLoggedMust(c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	out := runCmdLoggedMust(cmd)
	return []byte(out)
}

func makePrintDuration(name string) func() {
	logf(ctx(), "%s\n", name)
	timeStart := time.Now()
	return func() {
		dur := time.Since(timeStart)
		logf(ctx(), "%s took %s\n", name, formatDuration(dur))
	}
}

// run a .bat script and capture environment variables after
func getEnvAfterScript(path string) []string {
	if !fileExists(path) {
		return nil
	}
	dir, script := filepath.Split(path)

	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	logf(ctx(), "Executing: %s in %s\n", cmd, cmd.Dir)
	resBytes, err := cmd.Output()
	must(err)
	res := string(resBytes)
	parts := strings.Split(res, "\n")
	panicIf(len(parts) == 1, "split failed\nres:\n%s\n", res)
	for idx, env := range parts {
		parts[idx] = strings.TrimSpace(env)
	}
	return parts
}

func fileSizeMust(path string) int64 {
	size := getFileSize(path)
	panicIf(size == -1)
	return size
}

func removeFileMust(path string) {
	if !fileExists(path) {
		return
	}
	err := os.Remove(path)
	must(err)
}

func findFile(dir string, match func(string, os.FileInfo) bool) {
	fn := func(path string, info os.FileInfo, err error) error {
		if match(path, info) {
			logf(ctx(), "Found: '%s'\n", path)
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

func evalTmpl(s string, v interface{}) string {
	tmpl, err := template.New("tmpl").Parse(s)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, v)
	must(err)
	return buf.String()
}

// return true if file in path1 is newer than file in path2
// also returns true if one or both files don't exist
func fileNewerThan(path1, path2 string) bool {
	stat1, err1 := os.Stat(path1)
	stat2, err2 := os.Stat(path2)
	if err1 != nil || err2 != nil {
		return true
	}
	return stat1.ModTime().After(stat2.ModTime())
}

func cmdRunLoggedMust(cmd *exec.Cmd) {
	fmt.Printf("> %s\n", cmd.String())
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	err := cmd.Run()
	must(err)
}

func readFileMust(path string) []byte {
	d, err := ioutil.ReadFile(path)
	must(err)
	return d
}

func writeFileMust(path string, data []byte) {
	err := ioutil.WriteFile(path, data, 0644)
	must(err)
}

func findLargestFileByExt() {
	drive := "x:\\" // on laptop
	drive = "v:\\"  // on desktop
	isWantedExt := func(ext string) bool {
		for _, s := range []string{".pdf", ".cbr", ".cbz", ".epub", "mobi", ".xps", ".djvu", ".pdb", ".prc", ".xps"} {
			if s == ext {
				return true
			}
		}
		return false
	}

	extToSize := map[string]int64{}
	dirs := []string{"comics", "comics read", "books"}
	nFiles := 0
	for _, d := range dirs {
		startDir := filepath.Join(drive, d)
		filepath.WalkDir(startDir, func(path string, d fs.DirEntry, err error) error {
			if !d.Type().IsRegular() {
				return nil
			}
			if false && (nFiles == 0 || nFiles%128 == 0) {
				logf(ctx(), "%s\n", path)
			}
			nFiles++
			ext := strings.ToLower(filepath.Ext(path))
			if !isWantedExt(ext) {
				return nil
			}
			fi, err := d.Info()
			if err != nil {
				return nil
			}
			size := fi.Size()
			if size > extToSize[ext] {
				logf(ctx(), "%s of size %s\n", path, formatSize(size))
				extToSize[ext] = size
			}
			return nil
		})
	}
	logf(ctx(), "processed %d files\n", nFiles)
}

func runCmdLoggedMust(cmd *exec.Cmd) string {
	logf(ctx(), "> %s\n", cmd.String())
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	err := cmd.Run()
	must(err)
	return ""
}

func createDirMust(path string) string {
	err := os.MkdirAll(path, 0755)
	must(err)
	return path
}

func userHomeDirMust() string {
	s, err := os.UserHomeDir()
	must(err)
	return s
}

func createDirForFile(path string) error {
	dir := filepath.Dir(path)
	return os.MkdirAll(dir, 0755)
}

func stringInSlice(a []string, toCheck string) bool {
	for _, s := range a {
		if s == toCheck {
			return true
		}
	}
	return false
}

func fmtCmdShort(cmd exec.Cmd) string {
	cmd.Path = filepath.Base(cmd.Path)
	return cmd.String()
}

func runCmdMust(cmd *exec.Cmd) string {
	fmt.Printf("> %s\n", fmtCmdShort(*cmd))
	canCapture := (cmd.Stdout == nil) && (cmd.Stderr == nil)
	if canCapture {
		out, err := cmd.CombinedOutput()
		if err == nil {
			if len(out) > 0 {
				logf(ctx(), "Output:\n%s\n", string(out))
			}
			return string(out)
		}
		logf(ctx(), "cmd '%s' failed with '%s'. Output:\n%s\n", cmd, err, string(out))
		must(err)
		return string(out)
	}
	err := cmd.Run()
	if err == nil {
		return ""
	}
	logf(ctx(), "cmd '%s' failed with '%s'\n", cmd, err)
	must(err)
	return ""
}

func fmtSmart(format string, args ...interface{}) string {
	if len(args) == 0 {
		return format
	}
	return fmt.Sprintf(format, args...)
}

func currDirAbsMust() string {
	dir, err := filepath.Abs(".")
	must(err)
	return dir
}

// we are executed for do/ directory so top dir is parent dir
func cdUpDir(dirName string) {
	startDir := currDirAbsMust()
	dir := startDir
	for {
		// we're already in top directory
		if filepath.Base(dir) == dirName && dirExists(dir) {
			err := os.Chdir(dir)
			must(err)
			return
		}
		parentDir := filepath.Dir(dir)
		panicIf(dir == parentDir, "invalid startDir: '%s', dir: '%s'", startDir, dir)
		dir = parentDir
	}
}

func execTextTemplate(tmplText string, data interface{}) string {
	tmpl, err := template.New("").Parse(tmplText)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, data)
	must(err)
	return buf.String()
}
