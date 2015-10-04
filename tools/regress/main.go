package main

import (
	"crypto/sha1"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

/*
The big idea is to start collecting regression tests for bugs we fixed.

Each test is a command we run on a file. A command can be any executable but
for simplicity we'll probably just add more functionality to SumatraPDF.exe.

Test succeeds if the output is same as expected.
*/

type Test struct {
	// values from parsing test file
	CmdUnparsed    string
	FileSha1Hex    string
	FileURL        string
	ExpectedOutput string

	// computed values
	CmdPath  string
	CmdArgs  []string
	FilePath string
	Error    error
	Output   string
}

type TestFile struct {
	Path    string
	Sha1Hex string
}

var (
	inFatal     bool
	failedTests []*Test
	testFiles   map[string]*TestFile
)

func init() {
	testFiles = make(map[string]*TestFile)
}

func printStack() {
	buf := make([]byte, 1024*164)
	n := runtime.Stack(buf, false)
	fmt.Printf("%s", buf[:n])
}

var (
	cacheDir string
)

func getCacheDirMust() string {
	if cacheDir == "" {
		d := filepath.Join("..", "sumatra-test-files")
		err := os.MkdirAll(d, 0755)
		fataliferr(err)
		cacheDir = d
	}
	return cacheDir
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	os.Exit(1)
}

func fatalif(cond bool, format string, args ...interface{}) {
	if cond {
		if inFatal {
			os.Exit(1)
		}
		inFatal = true
		fmt.Printf(format, args...)
		printStack()
		os.Exit(1)
	}
}

func fataliferr(err error) {
	if err != nil {
		fatalf("%s\n", err.Error())
	}
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

func parseTestsMust(path string) []*Test {
	var res []*Test
	d, err := ioutil.ReadFile(path)
	fataliferr(err)
	lines := toTrimmedLines(d)
	// TODO: finish me
	fmt.Printf("%d lines\n", len(lines))
	return res
}

func cmdToStrLong2(cmd *exec.Cmd) string {
	arr := []string{`"` + cmd.Path + `"`}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

func cmdToStrLong(cmd *exec.Cmd) string {
	return strings.Join(cmd.Args, " ")
}

func isOutputEqual(s1, s2 string) bool {
	// TODO: normalize whitespace
	return s1 != s2
}

func runTest(t *Test) {
	for i, arg := range t.CmdArgs {
		if arg == "$file" {
			t.CmdArgs[i] = t.FilePath
		}
	}
	cmd := exec.Command(t.CmdPath, t.CmdArgs...)
	fmt.Printf("Running: %s\n", cmdToStrLong(cmd))
	res, err := cmd.Output()
	t.Output = string(res)
	if err != nil {
		t.Error = err
		failedTests = append(failedTests, t)
		return
	}
	if !isOutputEqual(t.Output, t.ExpectedOutput) {
		failedTests = append(failedTests, t)
		return
	}
	fmt.Printf("test passed, output: %s\n", res)
}

func dumpFailedTest(t *Test) {
	// TODO: write me
	args := strings.Join(t.CmdArgs, " ")
	fmt.Printf("Test %s %s failed\n", t.CmdPath, args)
	if t.Error != nil {
		fmt.Printf("Reason: process exited with error '%s'\n", t.Error)
		return
	}
	if !isOutputEqual(t.Output, t.ExpectedOutput) {
		fmt.Printf(`
Reason: got output:
-----
%s
-----
expected:
-----
%s
-----
`, t.Output, t.ExpectedOutput)
		return
	}
	fmt.Printf("Internal rror: unknown reason\n")
}

func dumpFailedTests() int {
	if len(failedTests) == 0 {
		fmt.Printf("All tests passed!\n")
		return 0
	}
	for _, t := range failedTests {
		dumpFailedTest(t)
	}
	return len(failedTests)
}

func Sha1OfBytes(data []byte) []byte {
	res := sha1.Sum(data)
	return res[:]
}

func Sha1HexOfBytes(data []byte) string {
	return fmt.Sprintf("%x", Sha1OfBytes(data))
}

func Sha1OfFile(path string) ([]byte, error) {
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

func Sha1HexOfFile(path string) (string, error) {
	sha1, err := Sha1OfFile(path)
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%x", sha1), nil
}

func httpDlMust(uri string) []byte {
	res, err := http.Get(uri)
	fataliferr(err)
	d, err := ioutil.ReadAll(res.Body)
	res.Body.Close()
	fataliferr(err)
	return d
}

func testFileExists(sha1Hex string) bool {
	return nil != testFiles[sha1Hex]
}

func dlIfNotExistsMust(uri, sha1Hex string) {
	if testFileExists(sha1Hex) {
		return
	}
	fmt.Printf("downloading '%s'...", uri)
	d := httpDlMust(uri)
	realSha1Hex := Sha1HexOfBytes(d)
	fatalif(sha1Hex != realSha1Hex, "sha1Hex != realSha1Hex (%s != %s)", sha1Hex, realSha1Hex)
	ext := filepath.Ext(uri)
	fileName := sha1Hex + ext
	path := filepath.Join(getCacheDirMust(), fileName)
	err := ioutil.WriteFile(path, d, 0644)
	fataliferr(err)
	fmt.Printf(" saved to '%s'\n", path)
}

func downloadTestFilesMust(tests []*Test) {
	for _, test := range tests {
		dlIfNotExistsMust(test.FileURL, test.FileSha1Hex)
	}
}

func runTests(tests []*Test) {
	for _, test := range tests {
		runTest(test)
	}
}

func removeExt(s string) string {
	ext := filepath.Ext(s)
	if len(ext) == 0 {
		return s
	}
	return s[:len(s)-len(ext)]
}

func verifyTestFiles() {
	d := getCacheDirMust()
	files, err := ioutil.ReadDir(d)
	fataliferr(err)
	for _, fi := range files {
		path := filepath.Join(d, fi.Name())
		sha1HexFromName := removeExt(fi.Name())
		fatalif(len(sha1HexFromName) != 40, "len(sha1HexFromName) != 40 (%d)", len(sha1HexFromName))
		sha1Hex, err := Sha1HexOfFile(path)
		fataliferr(err)
		fatalif(sha1Hex != sha1HexFromName, "sha1Hex != sha1HexFromName (%s != %s)", sha1Hex, sha1HexFromName)
		testFiles[sha1Hex] = &TestFile{
			Path:    path,
			Sha1Hex: sha1Hex,
		}
	}
	fmt.Printf("%d test files locally\n", len(testFiles))
}

func main() {
	fmt.Printf("regress\n")
	verifyTestFiles()
	p := filepath.Join("tools", "regress", "tests.txt")
	tests := parseTestsMust(p)
	t := &Test{
		FileURL:        "https://kjkpub.s3.amazonaws.com/testfiles/6f/d3/89a36816f1ab490d46c0c7a6b34b678f72bf.pdf",
		FileSha1Hex:    "6fd389a36816f1ab490d46c0c7a6b34b678f72bf",
		CmdPath:        "dbg/SumatraPDF.exe",
		CmdArgs:        []string{"-render", "2", "-zoom", "5", "$file"},
		FilePath:       "89a36816f1ab490d46c0c7a6b34b678f72bf.pdf",
		ExpectedOutput: "rendering page 1 for '89a36816f1ab490d46c0c7a6b34b678f72bf.pdf', zoom: 5.00",
	}
	//runTest(t)
	tests = []*Test{t}
	downloadTestFilesMust(tests)

	//runTests(tests)
	os.Exit(dumpFailedTests())
}
