package main

import (
	"bufio"
	"bytes"
	"context"
	"crypto/sha1"
	"fmt"
	"io"
	"io/fs"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"text/template"
	"time"
)

var (
	fatalIf = panicIf
)

func must(err error) {
	if err != nil {
		panic(err)
	}
}

func ctx() context.Context {
	return context.Background()
}

func logf(ctx context.Context, s string, arg ...interface{}) {
	if len(arg) > 0 {
		s = fmt.Sprintf(s, arg...)
	}
	fmt.Print(s)
}

func isWindows() bool {
	return strings.Contains(runtime.GOOS, "windows")
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

func removeDirMust(dir string) {
	err := os.RemoveAll(dir)
	must(err)
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
	must(err)
	d, err := ioutil.ReadAll(res.Body)
	res.Body.Close()
	must(err)
	return d
}

func httpDlToFileMust(uri string, path string, sha1Hex string) {
	if fileExists(path) {
		sha1File, err := fileSha1Hex(path)
		must(err)
		fatalIf(sha1File != sha1Hex, "file '%s' exists but has sha1 of %s and we expected %s", path, sha1File, sha1Hex)
		return
	}
	logf(ctx(), "Downloading '%s'\n", uri)
	d := httpDlMust(uri)
	sha1File := dataSha1Hex(d)
	fatalIf(sha1File != sha1Hex, "downloaded '%s' but it has sha1 of %s and we expected %s", uri, sha1File, sha1Hex)
	err := ioutil.WriteFile(path, d, 0755)
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

// whitelisted characters valid in url
func validateRune(c rune) byte {
	if c >= 'a' && c <= 'z' {
		return byte(c)
	}
	if c >= 'A' && c <= 'Z' {
		return byte(c)
	}
	if c >= '0' && c <= '9' {
		return byte(c)
	}
	if c == '-' || c == '_' || c == '.' {
		return byte(c)
	}
	if c == ' ' {
		return '-'
	}
	return 0
}

func charCanRepeat(c byte) bool {
	if c >= 'a' && c <= 'z' {
		return true
	}
	if c >= 'A' && c <= 'Z' {
		return true
	}
	if c >= '0' && c <= '9' {
		return true
	}
	return false
}

// urlify generates safe url from tile by removing hazardous characters
func urlify(s string) string {
	s = strings.TrimSpace(s)
	var res []byte
	for _, r := range s {
		c := validateRune(r)
		if c == 0 {
			continue
		}
		// eliminate duplicate consecutive characters
		var prev byte
		if len(res) > 0 {
			prev = res[len(res)-1]
		}
		if c == prev && !charCanRepeat(c) {
			continue
		}
		res = append(res, c)
	}
	s = string(res)
	if len(s) > 128 {
		s = s[:128]
	}
	return s
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

func panicIf(cond bool, args ...interface{}) {
	if !cond {
		return
	}
	s := "condition failed"
	if len(args) > 0 {
		s = fmt.Sprintf("%s", args[0])
		if len(args) > 1 {
			s = fmt.Sprintf(s, args[1:]...)
		}
	}
	panic(s)
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

func fileExists(path string) bool {
	st, err := os.Lstat(path)
	return err == nil && st.Mode().IsRegular()
}

func dirExists(path string) bool {
	st, err := os.Lstat(path)
	return err == nil && st.IsDir()
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

func mimeTypeFromFileName(path string) string {
	var mimeTypes = map[string]string{
		// this is a list from go's mime package
		".css":  "text/css; charset=utf-8",
		".gif":  "image/gif",
		".htm":  "text/html; charset=utf-8",
		".html": "text/html; charset=utf-8",
		".jpg":  "image/jpeg",
		".js":   "application/javascript",
		".wasm": "application/wasm",
		".pdf":  "application/pdf",
		".png":  "image/png",
		".svg":  "image/svg+xml",
		".xml":  "text/xml; charset=utf-8",

		// those are my additions
		".txt":  "text/plain",
		".exe":  "application/octet-stream",
		".json": "application/json",
	}

	ext := strings.ToLower(filepath.Ext(path))
	mt := mimeTypes[ext]
	if mt != "" {
		return mt
	}
	// if not given, default to this
	return "application/octet-stream"
}

func normalizeNewlines(d []byte) []byte {
	// replace CR LF (windows) with LF (unix)
	d = bytes.Replace(d, []byte{13, 10}, []byte{10}, -1)
	// replace CF (mac) with LF (unix)
	d = bytes.Replace(d, []byte{13}, []byte{10}, -1)
	return d
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

func formatSize(n int64) string {
	sizes := []int64{1024 * 1024 * 1024, 1024 * 1024, 1024}
	suffixes := []string{"GB", "MB", "kB"}
	for i, size := range sizes {
		if n >= size {
			s := fmt.Sprintf("%.2f", float64(n)/float64(size))
			return strings.TrimSuffix(s, ".00") + " " + suffixes[i]
		}
	}
	return fmt.Sprintf("%d bytes", n)
}

func getFileSize(path string) int64 {
	st, err := os.Lstat(path)
	if err == nil {
		return st.Size()
	}
	return -1
}

func sha1OfFile(path string) ([]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		//fmt.Printf("os.Open(%s) failed with %s\n", path, err.Error())
		return nil, err
	}
	defer f.Close()
	h := sha1.New()
	_, err = io.Copy(h, f)
	if err != nil {
		//fmt.Printf("io.Copy() failed with %s\n", err.Error())
		return nil, err
	}
	return h.Sum(nil), nil
}

func sha1HexOfFile(path string) (string, error) {
	sha1, err := sha1OfFile(path)
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%x", sha1), nil
}

// time.Duration with a better string representation
type FormattedDuration time.Duration

func (d FormattedDuration) String() string {
	return formatDuration(time.Duration(d))
}

// formats duration in a more human friendly way
// than time.Duration.String()
func formatDuration(d time.Duration) string {
	s := d.String()
	if strings.HasSuffix(s, "µs") {
		// for µs we don't want fractions
		parts := strings.Split(s, ".")
		if len(parts) > 1 {
			return parts[0] + " µs"
		}
		return strings.ReplaceAll(s, "µs", " µs")
	} else if strings.HasSuffix(s, "ms") {
		// for ms we only want 2 digit fractions
		parts := strings.Split(s, ".")
		//fmt.Printf("fmtDur: '%s' => %#v\n", s, parts)
		if len(parts) > 1 {
			s2 := parts[1]
			if len(s2) > 4 {
				// 2 for "ms" and 2+ for fraction
				res := parts[0] + "." + s2[:2] + " ms"
				//fmt.Printf("fmtDur: s2: '%s', res: '%s'\n", s2, res)
				return res
			}
		}
		return strings.ReplaceAll(s, "ms", " ms")
	}
	return s
}

func copyFile(dstPath, srcPath string) error {
	d, err := os.ReadFile(srcPath)
	if err != nil {
		return err
	}
	err = os.MkdirAll(filepath.Dir(dstPath), 0755)
	if err != nil {
		return err
	}
	return os.WriteFile(dstPath, d, 0644)
}

func createDirForFile(path string) error {
	dir := filepath.Dir(path)
	return os.MkdirAll(dir, 0755)
}

func pathExists(path string) bool {
	_, err := os.Lstat(path)
	return err == nil
}

func readLinesFromFile(filePath string) ([]string, error) {
	file, err := os.OpenFile(filePath, os.O_RDONLY, 0666)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	scanner := bufio.NewScanner(file)
	res := make([]string, 0)
	for scanner.Scan() {
		line := scanner.Bytes()
		res = append(res, string(line))
	}
	if err = scanner.Err(); err != nil {
		return nil, err
	}
	return res, nil
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

// from https://gist.github.com/hyg/9c4afcd91fe24316cbf0
func openBrowser(url string) {
	var err error

	switch runtime.GOOS {
	case "linux":
		err = exec.Command("xdg-open", url).Start()
	case "windows":
		err = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
	case "darwin":
		err = exec.Command("open", url).Start()
	default:
		err = fmt.Errorf("unsupported platform")
	}
	if err != nil {
		log.Fatal(err)
	}
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
