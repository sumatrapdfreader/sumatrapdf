package main

import (
	"crypto/sha1"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

// Timing records how long something took to execute
type Timing struct {
	Duration time.Duration
	What     string
}

var (
	timings     []Timing
	inFatal     bool
	logFile     *os.File
	logFileName string // set logFileName to enable loggin
)

func logToFile(s string) {
	if logFileName == "" {
		return
	}

	if logFile == nil {
		var err error
		logFile, err = os.Create(logFileName)
		if err != nil {
			fmt.Printf("logToFile: os.Create('%s') failed with %s\n", logFileName, err)
			os.Exit(1)
		}
	}
	logFile.WriteString(s)
}

func closeLogFile() {
	if logFile != nil {
		logFile.Close()
		logFile = nil
	}
}

// Note: it can say is 32bit on 64bit machine (if 32bit toolset is installed),
// but it'll never say it's 64bit if it's 32bit
func isOS64Bit() bool {
	return runtime.GOARCH == "amd64"
}

func appendTiming(dur time.Duration, what string) {
	t := Timing{
		Duration: dur,
		What:     what,
	}
	timings = append(timings, t)
}

func printTimings() {
	for _, t := range timings {
		fmt.Printf("%s\n    %s\n", t.Duration, t.What)
		logToFile(fmt.Sprintf("%s\n    %s\n", t.Duration, t.What))
	}
}

func printStack() {
	buf := make([]byte, 1024*164)
	n := runtime.Stack(buf, false)
	fmt.Printf("%s", buf[:n])
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	finalizeThings(true)
	os.Exit(1)
}

func fatalIf(cond bool, format string, args ...interface{}) {
	if cond {
		if inFatal {
			os.Exit(1)
		}
		inFatal = true
		fmt.Printf(format, args...)
		printStack()
		finalizeThings(true)
		os.Exit(1)
	}
}

func fatalIfErr(err error) {
	if err != nil {
		fatalf("%s\n", err.Error())
	}
}

func pj(elem ...string) string {
	return filepath.Join(elem...)
}

func replaceExt(path string, newExt string) string {
	ext := filepath.Ext(path)
	return path[0:len(path)-len(ext)] + newExt
}

func fileExists(path string) bool {
	fi, err := os.Stat(path)
	if err != nil {
		return false
	}
	return fi.Mode().IsRegular()
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

func fileSizeMust(path string) int64 {
	fi, err := os.Stat(path)
	fatalIfErr(err)
	return fi.Size()
}

func fileCopyMust(dst, src string) {
	in, err := os.Open(src)
	fatalIfErr(err)
	defer in.Close()

	out, err := os.Create(dst)
	fatalIfErr(err)

	_, err = io.Copy(out, in)
	cerr := out.Close()
	fatalIfErr(err)
	fatalIfErr(cerr)
}

func isNum(s string) bool {
	_, err := strconv.Atoi(s)
	return err == nil
}

func isGitClean() bool {
	out, err := runExe("git", "status", "--porcelain")
	fatalIfErr(err)
	s := strings.TrimSpace(string(out))
	return len(s) == 0
}

func removeDirMust(dir string) {
	err := os.RemoveAll(dir)
	fatalIfErr(err)
}

func removeFileMust(path string) {
	if !fileExists(path) {
		return
	}
	err := os.Remove(path)
	fatalIfErr(err)
}

// Version must be in format x.y.z
func verifyCorrectVersionMust(ver string) {
	parts := strings.Split(ver, ".")
	fatalIf(len(parts) == 0 || len(parts) > 3, "%s is not a valid version number", ver)
	for _, part := range parts {
		fatalIf(!isNum(part), "%s is not a valid version number", ver)
	}
}

func getGitSha1Must() string {
	out, err := runExe("git", "rev-parse", "HEAD")
	fatalIfErr(err)
	s := strings.TrimSpace(string(out))
	fatalIf(len(s) != 40, "getGitSha1Must(): %s doesn't look like sha1\n", s)
	return s
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
	if fileExists(path) {
		sha1File, err := fileSha1Hex(path)
		fatalIfErr(err)
		fatalIf(sha1File != sha1Hex, "file '%s' exists but has sha1 of %s and we expected %s", path, sha1File, sha1Hex)
		return
	}
	fmt.Printf("Downloading '%s'\n", uri)
	d := httpDlMust(uri)
	sha1File := dataSha1Hex(d)
	fatalIf(sha1File != sha1Hex, "downloaded '%s' but it has sha1 of %s and we expected %s", uri, sha1File, sha1Hex)
	err := ioutil.WriteFile(path, d, 0755)
	fatalIfErr(err)
}
