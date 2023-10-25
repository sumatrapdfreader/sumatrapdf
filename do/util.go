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
	"runtime"
	"strconv"
	"strings"
	"text/template"
	"time"

	"github.com/kjk/common/u"
)

var (
	must              = u.Must
	panicIf           = u.PanicIf
	panicIfErr        = u.PanicIfErr
	urlify            = u.Slug
	fileExists        = u.FileExists
	dirExists         = u.DirExists
	pathExists        = u.PathExists
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

func getCallstackFrames(skip int) []string {
	var callers [32]uintptr
	n := runtime.Callers(skip+1, callers[:])
	frames := runtime.CallersFrames(callers[:n])
	var cs []string
	for {
		frame, more := frames.Next()
		if !more {
			break
		}
		s := frame.File + ":" + strconv.Itoa(frame.Line)
		cs = append(cs, s)
	}
	return cs
}

func getCallstack(skip int) string {
	frames := getCallstackFrames(skip + 1)
	return strings.Join(frames, "\n")
}

func logErrorf(ctx context.Context, s string, args ...interface{}) {
	if len(args) > 0 {
		s = fmt.Sprintf(s, args...)
	}
	cs := getCallstack(1)
	fmt.Printf("%s\n%s\n", s, cs)
}

// return true if there was an error
func logIfError(ctx context.Context, err error) bool {
	if err == nil {
		return false
	}
	logErrorf(ctx, "err.Error(): %s", err.Error())
	return true
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

func runExeInDirMust(dir string, c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	logf(ctx(), "> %s\n", cmd)
	cmd.Dir = dir
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

func fileSizeMust(path string) int64 {
	size := getFileSize(path)
	panicIf(size == -1, "getFileSize() for '%s' failed", path)
	return size
}

func removeFileMust(path string) {
	if !fileExists(path) {
		return
	}
	err := os.Remove(path)
	must(err)
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
			must(err)
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

func fmdCmdShort(cmd *exec.Cmd) string {
	cmd2 := *cmd
	exePath := filepath.Base(cmd.Path)
	cmd2.Path = exePath
	return cmd2.String()
}

func runCmdLoggedMust(cmd *exec.Cmd) string {
	logf(ctx(), ">2 %s\n", fmdCmdShort(cmd))
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

func execTextTemplate(tmplText string, data interface{}) string {
	tmpl, err := template.New("").Parse(tmplText)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, data)
	must(err)
	return buf.String()
}
