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

// Test describes a single test
type Test struct {
	// values from parsing test file
	CmdUnparsed    string
	FileSha1Hex    string
	FileURL        string
	ExpectedOutput string

	// computed values
	CmdName  string // e.g. SumatraPDF.exe
	CmdPath  string // e.g. rel64\SumatraPDF.exe
	CmdArgs  []string
	FilePath string
	Error    error
	Output   string
}

// TestFile describes as test file
type TestFile struct {
	Path    string
	Sha1Hex string
}

var (
	inFatal         bool
	testFilesBySha1 map[string]*TestFile
)

func init() {
	testFilesBySha1 = make(map[string]*TestFile)
}

func errStr(err error) string {
	if err == nil {
		return ""
	}
	return err.Error()
}

func dumpTest(t *Test) {
	fmt.Printf(`CmdUnparsed: '%s'
FileSha1Hex: %s
FileURL: '%s'
ExpectedOutput: '%s'
CmdName: '%s'
CmdPath: '%s'
CmdArgs: %v
FilePath: '%s'
Error: '%s'
Output: '%s'

`, t.CmdUnparsed, t.FileSha1Hex, t.FileURL, t.ExpectedOutput, t.CmdName, t.CmdPath, t.CmdArgs, t.FilePath, errStr(t.Error), t.Output)
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
		fatalIfErr(err)
		cacheDir = d
	}
	return cacheDir
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	os.Exit(1)
}

func panicIf(cond bool, format string, args ...interface{}) {
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

func fatalIfErr(err error) {
	if err != nil {
		fatalf("%s\n", err.Error())
	}
}

func toTrimmedLines(d []byte) []string {
	var res []string
	for _, l := range strings.Split(string(d), "\n") {
		l = strings.TrimSpace(l)
		res = append(res, l)
	}
	return res
}

func collapseMultipleEmptyLines(lines []string) []string {
	var res []string
	prevWasEmpty := false
	for _, l := range lines {
		if l == "" && prevWasEmpty {
			continue
		}
		prevWasEmpty = l == ""
		res = append(res, l)
	}
	return res
}

func parseTest(lines []string) (*Test, []string) {
	t := &Test{}
	//fmt.Printf("parseTest: %d lines\n", len(lines))
	if len(lines) == 0 {
		return nil, nil
	}
	for len(lines) > 0 {
		l := lines[0]
		lines = lines[1:]
		// skip comments
		if strings.HasPrefix(l, "#") {
			continue
		}
		//fmt.Printf("lt: '%s'\n", l)
		// empty line separates tests
		if l == "" {
			break
		}

		parts := strings.SplitN(l, ":", 2)
		panicIf(len(parts) != 2, "invalid line: '%s'", l)
		name := strings.ToLower(parts[0])
		val := strings.TrimSpace(parts[1])
		switch name {
		case "url":
			t.FileURL = val
		case "sha1":
			panicIf(len(val) != 40, "len(val) != 40 (%d)", len(val))
			t.FileSha1Hex = val
		case "cmd":
			t.CmdUnparsed = val
		case "out":
			t.ExpectedOutput = val
		}
	}
	panicIf(t.FileURL == "", "Url: filed missing")
	panicIf(t.FileSha1Hex == "", "Sha1: field missing")
	panicIf(t.CmdUnparsed == "", "Cmd: field missing")
	panicIf(t.ExpectedOutput == "", "Out: field missing")

	parts := strings.Split(t.CmdUnparsed, " ")
	t.CmdName = parts[0]
	t.CmdArgs = parts[1:]
	return t, lines
}

func parseTestsMust(path string) []*Test {
	var res []*Test
	var test *Test
	d, err := os.ReadFile(path)
	fatalIfErr(err)
	lines := toTrimmedLines(d)
	lines = collapseMultipleEmptyLines(lines)
	for {
		test, lines = parseTest(lines)
		if test == nil {
			break
		}
		res = append(res, test)
	}
	fmt.Printf("%d tests\n", len(res))
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
	// TODO: normalize newlines
	s1 = strings.TrimSpace(s1)
	s2 = strings.TrimSpace(s2)
	return s1 == s2
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
	t.Output = strings.TrimSpace(string(res))
	if err != nil {
		t.Error = err
		fmt.Printf("Failed test:\n")
		dumpTest(t)
		return
	}
	if !isOutputEqual(t.Output, t.ExpectedOutput) {
		fmt.Printf("Failed test:\n")
		dumpTest(t)
		return
	}
	fmt.Printf("test passed, output: %s\n", res)
}

func isFailedTest(t *Test) bool {
	if t.Error != nil {
		return true
	}
	return !isOutputEqual(t.Output, t.ExpectedOutput)
}

func dumpFailedTest(t *Test) {
	args := strings.Join(t.CmdArgs, " ")
	fmt.Printf("Test %s %s failed\n", t.CmdPath, args)
	dumpTest(t)
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
	fmt.Printf("Internal error: unknown reason\n")
}

func dumpFailedTests(tests []*Test) int {
	nFailed := 0
	for _, test := range tests {
		if !isFailedTest(test) {
			continue
		}
		nFailed++
		dumpFailedTest(test)
	}
	if nFailed == 0 {
		fmt.Printf("All tests passed!\n")
	} else {
		fmt.Printf("Failed %d out of %d tests\n", nFailed, len(tests))
	}
	return nFailed
}

func sha1OfBytes(data []byte) []byte {
	res := sha1.Sum(data)
	return res[:]
}

func sha1HexOfBytes(data []byte) string {
	return fmt.Sprintf("%x", sha1OfBytes(data))
}

func sha1OfFile(path string) ([]byte, error) {
	f, err := os.Open(path)
	fatalIfErr(err)
	defer f.Close()
	h := sha1.New()
	_, err = io.Copy(h, f)
	fatalIfErr(err)
	return h.Sum(nil), nil
}

func sha1HexOfFile(path string) (string, error) {
	sha1, err := sha1OfFile(path)
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%x", sha1), nil
}

func httpDlMust(uri string) []byte {
	res, err := http.Get(uri)
	fatalIfErr(err)
	d, err := ioutil.ReadAll(res.Body)
	res.Body.Close()
	fatalIfErr(err)
	return d
}

func testFileExists(sha1Hex string) bool {
	return nil != testFilesBySha1[sha1Hex]
}

func dlIfNotExistsMust(uri, sha1Hex string) {
	if testFileExists(sha1Hex) {
		return
	}
	fmt.Printf("downloading '%s'...", uri)
	d := httpDlMust(uri)
	realSha1Hex := sha1HexOfBytes(d)
	panicIf(sha1Hex != realSha1Hex, "sha1Hex != realSha1Hex (%s != %s)", sha1Hex, realSha1Hex)
	ext := filepath.Ext(uri)
	fileName := sha1Hex + ext
	path := filepath.Join(getCacheDirMust(), fileName)
	err := ioutil.WriteFile(path, d, 0644)
	fatalIfErr(err)
	fmt.Printf(" saved to '%s'\n", path)
	testFilesBySha1[sha1Hex] = &TestFile{
		Path:    path,
		Sha1Hex: sha1Hex,
	}
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
	files, err := os.ReadDir(d)
	fatalIfErr(err)
	for _, fi := range files {
		path := filepath.Join(d, fi.Name())
		sha1HexFromName := removeExt(fi.Name())
		panicIf(len(sha1HexFromName) != 40, "len(sha1HexFromName) != 40 (%d)", len(sha1HexFromName))
		sha1Hex, err := sha1HexOfFile(path)
		fatalIfErr(err)
		panicIf(sha1Hex != sha1HexFromName, "sha1Hex != sha1HexFromName (%s != %s)", sha1Hex, sha1HexFromName)
		testFilesBySha1[sha1Hex] = &TestFile{
			Path:    path,
			Sha1Hex: sha1Hex,
		}
	}
	fmt.Printf("%d test files locally\n", len(testFilesBySha1))
}

func isOS64Bit() bool {
	return runtime.GOARCH == "amd64"
}

func dirExists(path string) bool {
	fi, err := os.Stat(path)
	if err != nil {
		return false
	}
	return fi.IsDir()
}

func fileExists(path string) bool {
	fi, err := os.Stat(path)
	if err != nil {
		return false
	}
	return fi.Mode().IsRegular()
}

func verifyCommandsMust(tests []*Test) {
	var dirsToCheck []string
	cmds := make(map[string]bool)
	if isOS64Bit() && dirExists("rel64") {
		dirsToCheck = append(dirsToCheck, "rel64")
	}
	if dirExists("rel") {
		dirsToCheck = append(dirsToCheck, "rel")
	}
	// TODO: also check dbg64 and dbg?
	panicIf(len(dirsToCheck) == 0, "there is no rel or rel64 directory with executables")
	for _, test := range tests {
		cmds[test.CmdName] = true
	}
	var dirWithCommands string
	for _, dir := range dirsToCheck {
		var cmdsFound []string
		for cmd := range cmds {
			path := filepath.Join(dir, cmd)
			if fileExists(path) {
				cmdsFound = append(cmdsFound, cmd)
			}
		}
		if len(cmdsFound) == len(cmds) {
			fmt.Printf("found all test commands in '%s'\n", dir)
			dirWithCommands = dir
			break
		} else {
			fmt.Printf("dir '%s' has only %d out of %d commands\n", dir, len(cmdsFound), len(cmds))
		}
	}
	panicIf(dirWithCommands == "", "didn't find a directory with all tests commands %v\n", cmds)
	for _, test := range tests {
		test.CmdPath = filepath.Join(dirWithCommands, test.CmdName)
	}
}

func dumpTests(tests []*Test) {
	for _, test := range tests {
		dumpTest(test)
	}
}

func substFileVar(s, filePath string) string {
	return strings.Replace(s, "$file", filePath, -1)
}

func substFileVarAll(tests []*Test) {
	for _, test := range tests {
		sha1Hex := test.FileSha1Hex
		tf := testFilesBySha1[sha1Hex]
		panicIf(tf == nil, "no test file for '%s'\n", sha1Hex)
		test.FilePath = tf.Path
		test.ExpectedOutput = substFileVar(test.ExpectedOutput, tf.Path)
		for i, arg := range test.CmdArgs {
			test.CmdArgs[i] = substFileVar(arg, tf.Path)
		}
	}
}

func main() {
	fmt.Printf("regress, 64-bit os: %v\n", isOS64Bit())

	verifyTestFiles()
	p := filepath.Join("tools", "regress", "tests.txt")
	tests := parseTestsMust(p)
	verifyCommandsMust(tests)
	downloadTestFilesMust(tests)
	substFileVarAll(tests)
	//dumpTests(tests)

	runTests(tests)
	os.Exit(dumpFailedTests(tests))
}
