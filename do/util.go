package do

import (
	"bytes"
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"strings"
	"text/template"
	"time"

	"github.com/kjk/common/u"
)

var (
	must              = u.Must
	panicIf           = u.PanicIf
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

func logf(s string, args ...interface{}) {
	if len(args) > 0 {
		s = fmt.Sprintf(s, args...)
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
	logf("> %s\n", cmd)
	out, err := cmd.CombinedOutput()
	must(err)
	return []byte(out)
}

func runExeInDirMust(dir string, c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	logf("> %s\n", cmd)
	cmd.Dir = dir
	out, err := cmd.CombinedOutput()
	must(err)
	return []byte(out)
}

func runExeLoggedMust(c string, args ...string) {
	cmd := exec.Command(c, args...)
	runCmdLoggedMust(cmd)
}

func makePrintDuration(name string) func() {
	logf("%s\n", name)
	timeStart := time.Now()
	return func() {
		dur := time.Since(timeStart)
		logf("%s took %s\n", name, formatDuration(dur))
	}
}

func fileSizeMust(path string) int64 {
	size := getFileSize(path)
	panicIf(size == -1, "getFileSize() for '%s' failed", path)
	return size
}

func evalTmpl(s string, v interface{}) string {
	tmpl, err := template.New("tmpl").Parse(s)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, v)
	must(err)
	return buf.String()
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
	d, err := os.ReadFile(path)
	must(err)
	return d
}

func writeFileMust(path string, data []byte) {
	err := os.WriteFile(path, data, 0644)
	must(err)
}

func writeFileCreateDirMust(path string, data []byte) {
	createDirForFile(path)
	writeFileMust(path, data)
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
				logf("%s\n", path)
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
				logf("%s of size %s\n", path, formatSize(size))
				extToSize[ext] = size
			}
			return nil
		})
	}
	logf("processed %d files\n", nFiles)
}

func fmdCmdShort(cmd *exec.Cmd) string {
	cmd2 := *cmd
	exePath := filepath.Base(cmd.Path)
	cmd2.Path = exePath
	return cmd2.String()
}

func runCmdLoggedMust(cmd *exec.Cmd) {
	logf("> %s\n", fmdCmdShort(cmd))
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	err := cmd.Run()
	must(err)
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

func createDirForFileMust(path string) {
	dir := filepath.Dir(path)
	must(os.MkdirAll(dir, 0755))
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

func push[S ~[]E, E any](s *S, els ...E) {
	*s = append(*s, els...)
}

func measureDuration() func() {
	timeStart := time.Now()
	return func() {
		logf("took %s\n", time.Since(timeStart))
	}
}

func shouldCopyFile(dir string, de fs.DirEntry) bool {
	name := de.Name()

	bannedSuffixes := []string{".go", ".bat"}
	for _, s := range bannedSuffixes {
		if strings.HasSuffix(name, s) {
			return false
		}
	}

	bannedPrefixes := []string{"yarn", "go."}
	for _, s := range bannedPrefixes {
		if strings.HasPrefix(name, s) {
			return false
		}
	}

	doNotCopy := []string{"tests"}
	return !slices.Contains(doNotCopy, name)
}

func copyFilesRecurMust(dstDir, srcDir string) {
	files, err := os.ReadDir(srcDir)
	must(err)

	for _, de := range files {
		if !shouldCopyFile(dstDir, de) {
			continue
		}
		dstPath := filepath.Join(dstDir, de.Name())
		srcPath := filepath.Join(srcDir, de.Name())
		if de.IsDir() {
			copyFilesRecurMust(dstPath, srcPath)
			continue
		}
		copyFileMust(dstPath, srcPath)
	}
}

var copyFileMustOverwrite = false
var copyFilesExtsToNormalizeNL = []string{}

func copyFileMust(dst, src string) {
	copyFile2Must(dst, src, copyFileMustOverwrite)
}

func copyFile2Must(dst, src string, overwrite bool) {
	if !overwrite {
		_, err := os.Stat(dst)
		if err == nil {
			logf("destination '%s' already exists, skipping\n", dst)
			return
		}
	}
	logf("copy %s => %s\n", src, dst)
	dstDir := filepath.Dir(dst)
	err := os.MkdirAll(dstDir, 0755)
	must(err)
	d, err := os.ReadFile(src)
	must(err)
	ext := filepath.Ext(dst)
	ext = strings.ToLower(ext)
	if slices.Contains(copyFilesExtsToNormalizeNL, ext) {
		d = u.NormalizeNewlines(d)
	}
	err = os.WriteFile(dst, d, 0644)
	must(err)
}

func recreateDirMust(dir string) {
	err := os.RemoveAll(dir)
	must(err)
	err = os.MkdirAll(dir, 0755)
	must(err)
}
