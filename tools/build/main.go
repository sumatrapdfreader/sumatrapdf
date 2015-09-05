package main

import (
	"archive/zip"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"mime"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/goamz/goamz/aws"
	"github.com/goamz/goamz/s3"
)

/*
To run:
* install Go
 - download and run latest installer http://golang.org/doc/install
 - restart so that PATH changes take place
 - set GOPATH env variable (e.g. to %USERPROFILE%\src\go)
 - install goamz: go get github.com/goamz/goamz/s3
* go run .\tools\buildgo\main.go
*/

/*
TODO:
* implement release build
* implement buildbot loop
*/

// Secrets defines secrets
type Secrets struct {
	AwsSecret               string
	AwsAccess               string
	CertPwd                 string
	NotifierEmail           string
	NotifierEmailPwd        string
	TranslationUploadSecret string
}

// Timing records how long something took to execute
type Timing struct {
	Duration time.Duration
	What     string
}

const (
	s3PreRelDir = "sumatrapdf/prerel/"
	s3RelDir    = "sumatrapdf/rel/"
	logFileName = "build-log.txt"
)

var (
	flgRelease       bool // if doing an official release build
	flgPreRelease    bool // if doing pre-release build
	flgUpload        bool
	flgSmoke         bool
	flgListS3        bool
	flgAnalyze       bool
	flgNoCleanCheck  bool
	svnPreReleaseVer int
	gitSha1          string
	sumatraVersion   string
	timeStart        time.Time
	cachedSecrets    *Secrets
	timings          []Timing
	logFile          *os.File
	inFatal          bool
)

// Note: it can say is 32bit on 64bit machine (if 32bit toolset is installed),
// but it'll never say it's 64bit if it's 32bit
func isOS64Bit() bool {
	return runtime.GOARCH == "amd64"
}

func logToFile(s string) {
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

func cmdToStrLong(cmd *exec.Cmd) string {
	arr := []string{cmd.Path}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

func cmdToStr(cmd *exec.Cmd) string {
	s := filepath.Base(cmd.Path)
	arr := []string{s}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

func readSecretsMust() *Secrets {
	if cachedSecrets != nil {
		return cachedSecrets
	}
	path := pj("scripts", "secrets.json")
	d, err := ioutil.ReadFile(path)
	fatalif(err != nil, "readSecretsMust(): error %s reading file '%s'\n", err, path)
	var s Secrets
	err = json.Unmarshal(d, &s)
	fatalif(err != nil, "readSecretsMust(): failed to json-decode file '%s'. err: %s, data:\n%s\n", path, err, string(d))
	cachedSecrets = &s
	return cachedSecrets
}

func revertBuildConfig() {
	runExe("git", "checkout", buildConfigPath())
}

func finalizeThings(crashed bool) {
	revertBuildConfig()
	if !crashed {
		printTimings()
		fmt.Printf("total time: %s\n", time.Since(timeStart))
		logToFile(fmt.Sprintf("total time: %s\n", time.Since(timeStart)))
	}
	closeLogFile()
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	finalizeThings(true)
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
		finalizeThings(true)
		os.Exit(1)
	}
}

func fataliferr(err error) {
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

func getEnvAfterScript(dir, script string) []string {
	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	fmt.Printf("Executing: %s in %s\n", cmd.Args, cmd.Dir)
	resBytes, err := cmd.Output()
	if err != nil {
		fatalf("failed with %s\n", err)
	}
	res := string(resBytes)
	parts := strings.Split(res, "\n")
	if len(parts) == 1 {
		fatalf("split failed\nres:\n%s\n", res)
	}
	for idx, env := range parts {
		parts[idx] = strings.TrimSpace(env)
	}
	return parts
}

func getEnvValue(env []string, name string) (string, bool) {
	for _, v := range env {
		parts := strings.SplitN(v, "=", 2)
		if len(parts) != 2 {
			continue
		}
		if strings.EqualFold(name, parts[0]) {
			return parts[1], true
		}
	}
	return "", false
}

func getOsEnvValue(name string) (string, bool) {
	return getEnvValue(os.Environ(), name)
}

func getEnvForVSUncached() []string {
	val, ok := getOsEnvValue("VS140COMNTOOLS")
	if !ok {
		fatalf("VS140COMNTOOLS not set; VS 2015 not installed?\n")
	}
	return getEnvAfterScript(val, "vsvars32.bat")
}

var (
	envForVSCached []string
)

func getPaths(env []string) []string {
	path, ok := getEnvValue(env, "path")
	fatalif(!ok, "")
	sep := string(os.PathListSeparator)
	parts := strings.Split(path, sep)
	for i, s := range parts {
		parts[i] = strings.TrimSpace(s)
	}
	return parts
}

func dumpEnv(env []string) {
	for _, e := range env {
		fmt.Printf("%s\n", e)
	}
	paths := getPaths(env)
	fmt.Printf("PATH:\n  %s\n", strings.Join(paths, "\n  "))
}

func getEnvForVS() []string {
	if envForVSCached == nil {
		envForVSCached = getEnvForVSUncached()
		//dumpEnv(envForVSCached)
	}
	return envForVSCached
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
	fataliferr(err)
	return fi.Size()
}

func fileCopyMust(dst, src string) {
	in, err := os.Open(src)
	fataliferr(err)
	defer in.Close()

	out, err := os.Create(dst)
	fataliferr(err)

	_, err = io.Copy(out, in)
	cerr := out.Close()
	fataliferr(err)
	fataliferr(cerr)
}

func lookExeInEnvPathUncachedHelper(env []string, exeName string) string {
	var found []string
	paths := getPaths(env)
	for _, dir := range paths {
		path := filepath.Join(dir, exeName)
		if fileExists(path) {
			found = append(found, path)
		}
	}
	if len(found) == 0 {
		return ""
	}
	if len(found) == 1 {
		return found[0]
	}
	// HACK: for msbuild.exe we might find 3 locations, prefer the one for
	// 2015 VS. If we pick up 2013 VS, it'll complain about v140_xp toolset
	// not being installed
	for _, p := range found {
		if strings.Contains(p, "14.0") {
			return p
		}
	}
	return found[0]
}

func lookExeInEnvPathUncached(env []string, exeName string) string {
	var err error
	fmt.Printf("lookExeInEnvPathUncached: exeName=%s\n", exeName)
	path := lookExeInEnvPathUncachedHelper(env, exeName)
	if path == "" && filepath.Ext(exeName) == "" {
		path = lookExeInEnvPathUncachedHelper(env, exeName+".exe")
	}
	//panic(fmt.Sprintf("didn't find %s in %s\n", exeName, getPaths(env)))
	if path == "" {
		path, err = exec.LookPath(exeName)
	}
	fataliferr(err)
	fatalif(path == "", "didn't find %s in %s\n", exeName, getPaths(env))
	fmt.Printf("found %v for %s\n", path, exeName)
	return path
}

func lookExeInEnvPath(env []string, exeName string) string {
	var exePath string
	var err error
	if false {
		exePath, err = exec.LookPath(exeName)
		if err != nil {
			exePath = lookExeInEnvPathUncached(env, exeName)
		}
	} else {
		exePath = lookExeInEnvPathUncached(env, exeName)
	}
	return exePath
}

func getCmdInEnv(env []string, exeName string, args ...string) *exec.Cmd {
	if env == nil {
		env = os.Environ()
	}
	exePath := lookExeInEnvPath(env, exeName)
	cmd := exec.Command(exePath, args...)
	cmd.Env = env
	if true {
		fmt.Printf("Running %s\n", cmd.Args)
	}
	return cmd
}

func getCmd(exeName string, args ...string) *exec.Cmd {
	return getCmdInEnv(nil, exeName, args...)
}

func logCmdResult(cmd *exec.Cmd, out []byte, err error) {
	var s string
	if err != nil {
		s = fmt.Sprintf("%s failed with %s, out:\n%s\n\n", cmdToStr(cmd), err, string(out))
	} else {
		s = fmt.Sprintf("%s\n%s\n\n", cmdToStr(cmd), string(out))
	}
	logToFile(s)
}

func runCmd(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	timeStart := time.Now()
	if !showProgress {
		res, err := cmd.CombinedOutput()
		appendTiming(time.Since(timeStart), cmdToStr(cmd))
		logCmdResult(cmd, res, err)
		return res, err
	}
	var resOut, resErr []byte
	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()
	cmd.Start()

	go func() {
		buf := make([]byte, 1024, 1024)
		for {
			n, err := stdout.Read(buf)
			if err != nil {
				break
			}
			if n > 0 {
				d := buf[:n]
				resOut = append(resOut, d...)
				os.Stdout.Write(d)
			}
		}
	}()

	go func() {
		buf := make([]byte, 1024, 1024)
		for {
			n, err := stderr.Read(buf)
			if err != nil {
				break
			}
			if n > 0 {
				d := buf[:n]
				resErr = append(resErr, d...)
				os.Stderr.Write(d)
			}
		}
	}()
	err := cmd.Wait()
	appendTiming(time.Since(timeStart), cmdToStr(cmd))
	resOut = append(resOut, resErr...)
	logCmdResult(cmd, resOut, err)
	return resOut, err
}

func runCmdMust(cmd *exec.Cmd, showProgress bool) {
	_, err := runCmd(cmd, showProgress)
	fataliferr(err)
}

func runCmdLogged(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	out, err := runCmd(cmd, showProgress)
	if err != nil {
		args := []string{cmd.Path}
		args = append(args, cmd.Args...)
		fmt.Printf("%s failed with %s, out:\n%s\n", args, err, string(out))
		return out, err
	}
	fmt.Printf("%s\n", out)
	return out, nil
}

func runExe(exeName string, args ...string) ([]byte, error) {
	cmd := getCmd(exeName, args...)
	return runCmd(cmd, false)
}

func runExeInEnv(env []string, exeName string, args ...string) ([]byte, error) {
	cmd := getCmdInEnv(env, exeName, args...)
	return runCmd(cmd, false)
}

func runExeMust(exeName string, args ...string) []byte {
	out, err := runExeInEnv(os.Environ(), exeName, args...)
	fataliferr(err)
	return out
}

func runExeLogged(env []string, exeName string, args ...string) ([]byte, error) {
	out, err := runExeInEnv(env, exeName, args...)
	if err != nil {
		fmt.Printf("%s failed with %s, out:\n%s\n", args, err, string(out))
		return out, err
	}
	fmt.Printf("%s\n", out)
	return out, nil
}

func runMsbuild(showProgress bool, args ...string) error {
	cmd := getCmdInEnv(getEnvForVS(), "msbuild.exe", args...)
	_, err := runCmdLogged(cmd, showProgress)
	return err
}

func runMsbuildGetOutput(showProgress bool, args ...string) ([]byte, error) {
	cmd := getCmdInEnv(getEnvForVS(), "msbuild.exe", args...)
	return runCmdLogged(cmd, showProgress)
}

func isNum(s string) bool {
	_, err := strconv.Atoi(s)
	return err == nil
}

// Version must be in format x.y.z
func verifyCorrectVersionMust(ver string) {
	parts := strings.Split(ver, ".")
	fatalif(len(parts) == 0 || len(parts) > 3, "%s is not a valid version number", ver)
	for _, part := range parts {
		fatalif(!isNum(part), "%s is not a valid version number", ver)
	}
}

func extractSumatraVersionMust() string {
	path := pj("src", "Version.h")
	d, err := ioutil.ReadFile(path)
	fataliferr(err)
	lines := toTrimmedLines(d)
	s := "#define CURR_VERSION "
	for _, l := range lines {
		if strings.HasPrefix(l, s) {
			ver := l[len(s):]
			verifyCorrectVersionMust(ver)
			return ver
		}
	}
	fatalf("couldn't extract CURR_VERSION from %s\n", path)
	return ""
}

func getGitLinearVersionMust() int {
	out, err := runExe("git", "log", "--oneline")
	fataliferr(err)
	lines := toTrimmedLines(out)
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) + 1000
	fatalif(n < 10000, "getGitLinearVersion: n is %d (should be > 10000)", n)
	return n
}

func isGitCleanMust() bool {
	out, err := runExe("git", "status", "--porcelain")
	fataliferr(err)
	s := strings.TrimSpace(string(out))
	return len(s) == 0
}

func verifyGitCleanMsut() {
	fatalif(!isGitCleanMust(), "git has unsaved changes\n")
}

func getGitSha1Must() string {
	out, err := runExe("git", "rev-parse", "HEAD")
	fataliferr(err)
	s := strings.TrimSpace(string(out))
	fatalif(len(s) != 40, "getGitSha1Must(): %s doesn't look like sha1\n", s)
	return s
}

func verifyStartedInRightDirectoryMust() {
	path := buildConfigPath()
	fatalif(!fileExists(path), "started in wrong directory (%s doesn't exist)\n", path)
}

func buildConfigPath() string {
	return pj("src", "utils", "BuildConfig.h")
}

func certPath() string {
	return pj("scripts", "cert.pfx")
}

func setBuildConfig(preRelVer int, sha1 string) {
	s := fmt.Sprintf("#define SVN_PRE_RELEASE_VER %d\n", preRelVer)
	s += fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 644)
	fataliferr(err)
}

// we shouldn't re-upload files. We upload manifest-${rel}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func verifyPreReleaseNotInS3Must(preReleaseVer int) {
	s3Path := fmt.Sprintf("%smanifest-%d.txt", s3PreRelDir, preReleaseVer)
	fatalif(s3Exists(s3Path), "build %d already exists in s3 because '%s' exists\n", preReleaseVer, s3Path)
}

func verifyReleaseNotInS3Must(sumatraVersion string) {
	s3Path := fmt.Sprintf("%sSuamtraPDF-%sinstall.exe", s3RelDir, sumatraVersion)
	fatalif(s3Exists(s3Path), "build '%s' already exists in s3 because '%s' existst\n", sumatraVersion, s3Path)
}

// check we have cert for signing and s3 creds for file uploads
func verifyHasReleaseSecretsMust() {
	p := certPath()
	fatalif(!fileExists(p), "verifyHasPreReleaseSecretsMust(): certificate file '%s' doesn't exist\n", p)
	secrets := readSecretsMust()
	fatalif(secrets.CertPwd == "", "CertPwd missing in %s\n", p)

	if flgUpload {
		fatalif(secrets.AwsSecret == "", "AwsSecret missing in %s\n", p)
		fatalif(secrets.AwsAccess == "", "AwsAccess missing in %s\n", p)
	}
}

func runTestUtilMust(dir string) {
	timeStart := time.Now()
	cmd := exec.Command(".\\test_util.exe")
	cmd.Dir = "rel"
	out, err := cmd.CombinedOutput()
	logCmdResult(cmd, out, err)
	fataliferr(err)
	appendTiming(time.Since(timeStart), cmdToStr(cmd))
}

var (
	pdbFiles = []string{"libmupdf.pdb", "Installer.pdb",
		"SumatraPDF-no-MuPDF.pdb", "SumatraPDF.pdb"}
)

func createPdbZipMust(dir string) {
	path := pj(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	fataliferr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		path = pj(dir, file)
		d, err := ioutil.ReadFile(path)
		fataliferr(err)
		f, err := w.Create(file)
		fataliferr(err)
		_, err = f.Write(d)
		fataliferr(err)
	}

	err = w.Close()
	fataliferr(err)
}

func createPdbLzsaMust(dir string) {
	args := []string{"SumatraPDF.pdb.lzsa"}
	args = append(args, pdbFiles...)
	// refer to rel\MakeLZSA.exe using absolute path so that we always
	// use 32-bit version and to avoid issues with running it in different
	// directories when name is relative
	curDir, err := os.Getwd()
	fataliferr(err)
	makeLzsaPath := pj(curDir, "rel", "MakeLZSA.exe")
	cmd := getCmd(makeLzsaPath, args...)
	cmd.Dir = dir
	_, err = runCmdLogged(cmd, true)
	fataliferr(err)
}

func buildPreRelease() {
	var err error

	fmt.Printf("Building pre-release version\n")
	if !flgNoCleanCheck {
		verifyGitCleanMsut()
	}
	verifyPreReleaseNotInS3Must(svnPreReleaseVer)

	downloadTranslations()

	setBuildConfig(svnPreReleaseVer, gitSha1)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	runTestUtilMust("rel")
	signMust(pj("rel", "SumatraPDF.exe"))
	signMust(pj("rel", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	signMust(pj("rel", "Installer.exe"))

	setBuildConfig(svnPreReleaseVer, gitSha1)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)

	if isOS64Bit() {
		runTestUtilMust("rel64")
	}
	signMust(pj("rel64", "SumatraPDF.exe"))
	signMust(pj("rel64", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel64", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	signMust(pj("rel64", "Installer.exe"))

	createPdbZipMust("rel")
	createPdbZipMust("rel64")

	createPdbLzsaMust("rel")
	createPdbLzsaMust("rel64")

	createManifestMust()
	if flgUpload {
		s3DeleteOldestPreRel()
		s3UploadPreReleaseMust()
	}
}

func buildRelease() {
	var err error

	fmt.Printf("Building release version %s\n", sumatraVersion)
	verifyGitCleanMsut()
	verifyOnReleaseBranchMust()

	verifyReleaseNotInS3Must(sumatraVersion)

	//TODO: not sure if should download translations
	downloadTranslations()

	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	runTestUtilMust("rel")
	signMust(pj("rel", "SumatraPDF.exe"))
	signMust(pj("rel", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	signMust(pj("rel", "Installer.exe"))

	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)

	if isOS64Bit() {
		runTestUtilMust("rel64")
	}
	signMust(pj("rel64", "SumatraPDF.exe"))
	signMust(pj("rel64", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel64", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	signMust(pj("rel64", "Installer.exe"))

	createPdbZipMust("rel")
	createPdbZipMust("rel64")

	createPdbLzsaMust("rel")
	createPdbLzsaMust("rel64")

	createManifestMust()
	if flgUpload {
		s3UploadReleaseMust()
	}
}

// AnalyzeLine has info about a warning line from prefast/analyze build
// Given:
//
// c:\users\kjk\src\sumatrapdf\ext\unarr\rar\uncompress-rar.c(171): warning C6011:
// Dereferencing NULL pointer 'code->table'. : Lines: 163, 165, 169, 170,
// 171 [C:\Users\kjk\src\sumatrapdf\vs2015\Installer.vcxproj]
//
// FilePath will be: "ext\unarr\rar\uncompress-rar.c"
// LineNo will be: 171
// Message will be: warning C6011: Dereferencing NULL pointer 'code->table'. : Lines: 163, 165, 169, 170, 171
type AnalyzeLine struct {
	FilePath string
	LineNo   int
	Message  string
	OrigLine string
}

// ByPathLine is to sort AnalyzeLine by file path then by line number
type ByPathLine []*AnalyzeLine

func (s ByPathLine) Len() int {
	return len(s)
}
func (s ByPathLine) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}
func (s ByPathLine) Less(i, j int) bool {
	if s[i].FilePath == s[j].FilePath {
		return s[i].LineNo < s[j].LineNo
	}
	return s[i].FilePath < s[j].FilePath
}

var (
	currDirLenCached int
)

func currDirLen() int {
	if currDirLenCached == 0 {
		dir, err := os.Getwd()
		fataliferr(err)
		currDirLenCached = len(dir)
	}
	return currDirLenCached
}

func pre(s string) string {
	return `<pre style="white-space: pre-wrap;">` + s + `</pre>`
}

func a(url, txt string) string {
	return fmt.Sprintf(`<a href="%s">%s</a>`, url, txt)
}

//https://github.com/sumatrapdfreader/sumatrapdf/blob/c760b1996bec63c0bd9b2910b0811c41ed26db60/premake5.lua
func htmlizeSrcLink(al *AnalyzeLine, gitVersion string) string {
	path := strings.Replace(al.FilePath, "\\", "/", -1)
	lineNo := al.LineNo
	uri := fmt.Sprintf("https://github.com/sumatrapdfreader/sumatrapdf/blob/%s/%s#L%d", gitSha1, path, lineNo)
	txt := fmt.Sprintf("%s(%d)", al.FilePath, lineNo)
	return a(uri, txt)
}

func htmlizeErrorLines(errors []*AnalyzeLine) ([]string, []string, []string) {
	var sumatraErrors, mupdfErrors, extErrors []string
	for _, al := range errors {
		s := htmlizeSrcLink(al, gitSha1) + " : " + al.Message
		path := al.FilePath
		if strings.HasPrefix(path, "src") {
			sumatraErrors = append(sumatraErrors, s)
		} else if strings.HasPrefix(path, "mupdf") {
			mupdfErrors = append(mupdfErrors, s)
		} else if strings.HasPrefix(path, "ext") {
			extErrors = append(extErrors, s)
		} else {
			extErrors = append(extErrors, s)
		}
	}
	return sumatraErrors, mupdfErrors, extErrors
}

func genAnalyzeHTML(errors []*AnalyzeLine) string {
	sumatraErrors, mupdfErrors, extErrors := htmlizeErrorLines(errors)
	nSumatraErrors := len(sumatraErrors)
	nMupdfErrors := len(mupdfErrors)
	nExtErrors := len(extErrors)

	res := []string{"<html>", "<body>"}

	homeLink := a("../index.html", "Home")
	commitLink := a("https://github.com/sumatrapdfreader/sumatrapdf/commit/"+gitSha1, gitSha1)
	s := fmt.Sprintf("%s: commit %s, %d warnings in sumatra code, %d in mupdf, %d in ext:", homeLink, commitLink, nSumatraErrors, nMupdfErrors, nExtErrors)
	res = append(res, s)

	s = pre(strings.Join(sumatraErrors, "\n"))
	res = append(res, s)

	res = append(res, "<p>Warnings in mupdf code:</p>")
	s = pre(strings.Join(mupdfErrors, "\n"))
	res = append(res, s)

	res = append(res, "<p>Warnings in ext code:</p>")
	s = pre(strings.Join(extErrors, "\n"))
	res = append(res, s)

	res = append(res, "</pre>")
	res = append(res, "</body>", "</html>")
	return strings.Join(res, "\n")
}

func parseAnalyzeLine(s string) AnalyzeLine {
	sOrig := s
	// remove " [C:\Users\kjk\src\sumatrapdf\vs2015\Installer.vcxproj]" from the end
	end := strings.LastIndex(s, " [")
	fatalif(end == -1, "invalid line '%s'\n", sOrig)
	s = s[:end]
	parts := strings.SplitN(s, "): ", 2)
	fatalif(len(parts) != 2, "invalid line '%s'\n", sOrig)
	res := AnalyzeLine{
		OrigLine: sOrig,
		Message:  parts[1],
	}
	s = parts[0]
	end = strings.LastIndex(s, "(")
	fatalif(end == -1, "invalid line '%s'\n", sOrig)
	// change
	// c:\users\kjk\src\sumatrapdf\ext\unarr\rar\uncompress-rar.c
	// =>
	// ext\unarr\rar\uncompress-rar.c
	path := s[:end]
	// sometimes the line starts with:
	// 11>c:\users\kjk\src\sumatrapdf\ext\bzip2\bzlib.c(238)
	start := strings.Index(path, ">")
	if start != -1 {
		path = path[start+1:]
	}
	start = currDirLen() + 1
	res.FilePath = path[start:]
	n, err := strconv.Atoi(s[end+1:])
	fataliferr(err)
	res.LineNo = n
	return res
}

func isSrcFile(name string) bool {
	ext := strings.ToLower(filepath.Ext(name))
	switch ext {
	case ".cpp", ".c", ".h":
		return true
	}
	return false
}

// the compiler prints file names lower cased, we want real name in file system
// (otherwise e.g. links to github break)
func fixFileNames(a []*AnalyzeLine) {
	fmt.Printf("fixFileNames\n")
	files := make(map[string]string)
	filepath.Walk(".", func(path string, fi os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if !isSrcFile(path) {
			return nil
		}
		//fmt.Printf("path: '%s', name: '%s'\n", path, fi.Name())
		pathLower := strings.ToLower(path)
		files[pathLower] = path
		return nil
	})
	for _, al := range a {
		if sub := files[al.FilePath]; sub != "" {
			al.FilePath = sub
		}
	}
}

func parseAnalyzeOutput(d []byte) {
	lines := toTrimmedLines(d)
	var warnings []string
	for _, line := range lines {
		if strings.Contains(line, ": warning C") {
			warnings = append(warnings, line)
		}
	}

	seen := make(map[string]bool)
	var deDuped []*AnalyzeLine
	for _, s := range warnings {
		al := parseAnalyzeLine(s)
		full := fmt.Sprintf("%s, %d, %s\n", al.FilePath, al.LineNo, al.Message)
		if !seen[full] {
			deDuped = append(deDuped, &al)
			seen[full] = true
			//fmt.Print(full)
		}
	}

	sort.Sort(ByPathLine(deDuped))
	fixFileNames(deDuped)

	if false {
		for _, al := range deDuped {
			fmt.Printf("%s, %d, %s\n", al.FilePath, al.LineNo, al.Message)
		}
	}
	fmt.Printf("\n\n%d warnings\n", len(deDuped))

	s := genAnalyzeHTML(deDuped)
	err := ioutil.WriteFile("analyze-errors.html", []byte(s), 0644)
	fataliferr(err)
	// TODO: open a browser with analyze-errors.html
}

func parseSavedAnalyzeOuptut() {
	d, err := ioutil.ReadFile("analyze-output.txt")
	fataliferr(err)
	parseAnalyzeOutput(d)
}

func buildAnalyze() {
	fmt.Printf("Analyze build\n")
	// I assume 64-bit build will catch more issues
	out, _ := runMsbuildGetOutput(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=ReleasePrefast;Platform=x64", "/m")

	if true {
		err2 := ioutil.WriteFile("analyze-output.txt", out, 0644)
		fataliferr(err2)
	}
	//fataliferr(err)

	parseAnalyzeOutput(out)
}

func buildSmoke() {
	fmt.Printf("Smoke build\n")
	downloadTranslations()

	err := runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	path := pj("rel", "test_util.exe")
	runExeMust(path)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Debug;Platform=x64", "/m")
	fataliferr(err)
}

func manifestPath() string {
	return filepath.Join("rel", "manifest.txt")
}

// manifest is build for pre-release builds and contains build stats
func createManifestMust() {
	var lines []string
	files := []string{
		"SumatraPDF.exe",
		"SumatraPDF-no-MUPDF.exe",
		"Installer.exe",
		"libmupdf.dll",
		"PdfFilter.dll",
		"PdfPreview.dll",
		"Uninstaller.exe",
		"SumatraPDF.pdb.zip",
		"SumatraPDF.pdb.lzsa",
	}
	dirs := []string{"rel", "rel64"}
	for _, dir := range dirs {
		for _, file := range files {
			path := filepath.Join(dir, file)
			size := fileSizeMust(path)
			line := fmt.Sprintf("%s: %d", path, size)
			lines = append(lines, line)
		}
	}
	s := strings.Join(lines, "\n")
	err := ioutil.WriteFile(manifestPath(), []byte(s), 0644)
	fataliferr(err)
}

func signMust(path string) {
	// the sign tool is finicky, so copy the cert to the same dir as
	// the exe we're signing
	fileDir := filepath.Dir(path)
	fileName := filepath.Base(path)
	secrets := readSecretsMust()
	certPwd := secrets.CertPwd
	certSrc := certPath()
	certDest := pj(fileDir, "cert.pfx")
	if !fileExists(certDest) {
		fileCopyMust(certDest, certSrc)
	}
	cmd := getCmdInEnv(getEnvForVS(), "signtool.exe", "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
		"/du", "http://www.sumatrapdfreader.org", "/f", "cert.pfx",
		"/p", certPwd, fileName)
	cmd.Dir = fileDir
	_, err := runCmdLogged(cmd, true)
	fataliferr(err)
}

// sumatrapdf/sumatralatest.js
/*
var sumLatestVer = 10175;
var sumBuiltOn = "2015-07-23";
var sumLatestName = "SumatraPDF-prerelease-10175.exe";
var sumLatestExe = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175.exe";
var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175.pdb.zip";
var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175-install.exe";
*/
func createSumatraLatestJs() string {
	currDate := time.Now().Format("2006-01-02")
	v := svnPreReleaseVer
	return fmt.Sprintf(`
		var sumLatestVer = %d;
		var sumBuiltOn = "%s";
		var sumLatestName = "SumatraPDF-prerelease-%d.exe";

		var sumLatestExe = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d.exe";
		var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d.pdb.zip";
		var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d-install.exe";

		var sumLatestExe64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d-64.exe";
		var sumLatestPdb64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d-64.pdb.zip";
		var sumLatestInstaller64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%d-install-64.exe";
`, v, currDate, v, v, v, v, v, v, v)
}

//Note: http.DefaultClient is more robust than aws.RetryingClient
//(which fails for me with a timeout for large files e.g. ~6MB)
func getS3Client() *http.Client {
	// return aws.RetryingClient
	return http.DefaultClient
}

func s3GetBucket() *s3.Bucket {
	s3BucketName := "kjkpub"
	secrets := readSecretsMust()
	auth := aws.Auth{
		AccessKey: secrets.AwsAccess,
		SecretKey: secrets.AwsSecret,
	}
	// Note: it's important that region is aws.USEast. This is where my bucket
	// is and giving a different region will fail
	s3Obj := s3.New(auth, aws.USEast, getS3Client())
	return s3Obj.Bucket(s3BucketName)
}

func s3UploadFile(pathRemote, pathLocal string) error {
	fmt.Printf("Uploading '%s' as '%s'. ", pathLocal, pathRemote)
	start := time.Now()
	bucket := s3GetBucket()
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	fileSize := fileSizeMust(pathLocal)
	perm := s3.PublicRead
	f, err := os.Open(pathLocal)
	if err != nil {
		return err
	}
	defer f.Close()
	opts := s3.Options{}
	//opts.ContentMD5 =
	err = bucket.PutReader(pathRemote, f, fileSize, mimeType, perm, opts)
	appendTiming(time.Since(start), fmt.Sprintf("Upload of %s, size: %d", pathRemote, fileSize))
	if err != nil {
		fmt.Printf("Failed with %s\n", err)
	} else {
		fmt.Printf("Done in %s\n", time.Since(start))
	}
	return err
}

func s3UploadString(pathRemote string, s string) error {
	fmt.Printf("Uploading string of length %d  as '%s'\n", len(s), pathRemote)
	bucket := s3GetBucket()
	d := []byte(s)
	mimeType := mime.TypeByExtension(filepath.Ext(pathRemote))
	opts := s3.Options{}
	//opts.ContentMD5 =
	return bucket.Put(pathRemote, d, mimeType, s3.PublicRead, opts)
}

func s3UploadFile2(pathRemote, pathLocal string) error {
	fmt.Printf("Uploading '%s' as '%s'\n", pathLocal, pathRemote)
	bucket := s3GetBucket()
	d, err := ioutil.ReadFile(pathLocal)
	if err != nil {
		return err
	}
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	opts := s3.Options{}
	//opts.ContentMD5 =
	return bucket.Put(pathRemote, d, mimeType, s3.PublicRead, opts)
}

func s3UploadFiles(s3Dir string, dir string, files []string) error {
	n := len(files) / 2
	for i := 0; i < n; i++ {
		pathLocal := filepath.Join(dir, files[2*i])
		pathRemote := files[2*i+1]
		err := s3UploadFile(s3Dir+pathRemote, pathLocal)
		if err != nil {
			return fmt.Errorf("failed to upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
	}
	return nil
}

// FilesForVer describes pre-release files in s3 for a given version
type FilesForVer struct {
	Version    int      // pre-release version as int
	VersionStr string   // pre-release version as string
	Names      []string // relative to sumatrapdf/prerel/
	Paths      []string // full key path in S3
}

/*
Recognize the following files:
SumatraPDF-prerelease-10169-install.exe
SumatraPDF-prerelease-10169.exe
SumatraPDF-prerelease-10169.pdb.lzsa
SumatraPDF-prerelease-10169.pdb.zip
SumatraPDF-prerelease-10169-install-64.exe
SumatraPDF-prerelease-10169-64.exe
SumatraPDF-prerelease-10169.pdb-64.lzsa
SumatraPDF-prerelease-10169.pdb-64.zip
manifest-10169.txt
*/

var (
	preRelNameRegexps []*regexp.Regexp
	regexps           = []string{
		`SumatraPDF-prerelease-(\d+)-install-64.exe`,
		`SumatraPDF-prerelease-(\d+)-64.exe`,
		`SumatraPDF-prerelease-(\d+).pdb-64.lzsa`,
		`SumatraPDF-prerelease-(\d+).pdb-64.zip`,

		`SumatraPDF-prerelease-(\d+)-install.exe`,
		`SumatraPDF-prerelease-(\d+).exe`,
		`SumatraPDF-prerelease-(\d+).pdb.lzsa`,
		`SumatraPDF-prerelease-(\d+).pdb.zip`,

		`manifest-(\d+).txt`,
	}
)

func compilePreRelNameRegexpsMust() {
	fatalif(preRelNameRegexps != nil, "preRelNameRegexps != nil")
	for _, s := range regexps {
		r := regexp.MustCompile(s)
		preRelNameRegexps = append(preRelNameRegexps, r)
	}
}

func preRelFileVer(name string) string {
	for _, r := range preRelNameRegexps {
		res := r.FindStringSubmatch(name)
		if len(res) == 2 {
			return res[1]
		}
	}
	return ""
}

func addToFilesForVer(path, name, verStr string, files []*FilesForVer) []*FilesForVer {
	ver, err := strconv.Atoi(verStr)
	fataliferr(err)
	for _, fi := range files {
		if fi.Version == ver {
			fi.Names = append(fi.Names, name)
			fi.Paths = append(fi.Paths, path)
			return files
		}
	}

	fi := FilesForVer{
		Version:    ver,
		VersionStr: verStr,
		Names:      []string{name},
		Paths:      []string{path},
	}
	return append(files, &fi)
}

type ByVerFilesForVer []*FilesForVer

func (s ByVerFilesForVer) Len() int {
	return len(s)
}
func (s ByVerFilesForVer) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}
func (s ByVerFilesForVer) Less(i, j int) bool {
	return s[i].Version > s[j].Version
}

// list is sorted by Version, biggest first, to make it easy to delete oldest
func s3ListPreReleaseFilesMust(dbg bool) []*FilesForVer {
	fatalif(preRelNameRegexps == nil, "preRelNameRegexps == nil")
	var res []*FilesForVer
	bucket := s3GetBucket()
	resp, err := bucket.List(s3PreRelDir, "", "", 10000)
	fataliferr(err)
	fatalif(resp.IsTruncated, "truncated response! implement reading all the files\n")
	if dbg {
		fmt.Printf("%d files\n", len(resp.Contents))
	}
	var unrecognizedFiles []string
	for _, key := range resp.Contents {
		path := key.Key
		name := path[len(s3PreRelDir):]
		verStr := preRelFileVer(name)
		if dbg {
			fmt.Printf("path: '%s', name: '%s', ver: '%s', \n", path, name, verStr)
		}
		if verStr == "" {
			unrecognizedFiles = append(unrecognizedFiles, path)
		} else {
			res = addToFilesForVer(path, name, verStr, res)
		}
	}
	sort.Sort(ByVerFilesForVer(res))
	for _, s := range unrecognizedFiles {
		fmt.Printf("Unrecognized pre-relase file in s3: '%s'\n", s)
	}

	if true || dbg {
		for _, fi := range res {
			fmt.Printf("Ver: %s (%d)\n", fi.VersionStr, fi.Version)
			fmt.Printf("  names: %s\n", fi.Names)
			fmt.Printf("  paths: %s\n", fi.Paths)
		}
	}
	return res
}

func s3DeleteOldestPreRel() {
	maxToRetain := 10
	files := s3ListPreReleaseFilesMust(false)
	if len(files) < maxToRetain {
		return
	}
	toDelete := files[maxToRetain:]
	for _, fi := range toDelete {
		for _, s3Path := range fi.Paths {
			// don't delete manifest files
			if strings.Contains(s3Path, "manifest-") {
				continue
			}
			err := s3Delete(s3Path)
			if err != nil {
				// it's ok if fails, we'll try again next time
				fmt.Printf("Failed to delete '%s' in s3\n", s3Path)
			}
		}
	}
}

func s3Delete(path string) error {
	bucket := s3GetBucket()
	return bucket.Del(path)
}

func s3Exists(s3Path string) bool {
	bucket := s3GetBucket()
	exists, err := bucket.Exists(s3Path)
	if err != nil {
		fmt.Printf("bucket.Exists('%s') failed with %s\n", s3Path, err)
		return false
	}
	return exists
}

// https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe
func s3UploadPreReleaseMust() {
	prefix := fmt.Sprintf("SumatraPDF-prerelease-%d", svnPreReleaseVer)
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := s3UploadFiles(s3PreRelDir, "rel", files)
	fataliferr(err)

	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s-64.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install-64.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb-64.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb-64.lzsa", prefix),
	}
	err = s3UploadFiles(s3PreRelDir, "rel64", files)
	fataliferr(err)

	manifestRemotePath := s3PreRelDir + fmt.Sprintf("manifest-%d.txt", svnPreReleaseVer)
	manifestLocalPath := pj("rel", "manifest.txt")
	err = s3UploadFile(manifestRemotePath, manifestLocalPath)
	fataliferr(err)

	s := createSumatraLatestJs()
	err = s3UploadString("sumatrapdf/sumatralatest.js", s)
	fataliferr(err)

	//sumatrapdf/sumpdf-prerelease-latest.txt
	s = fmt.Sprintf("%d\n", svnPreReleaseVer)
	err = s3UploadString("sumatrapdf/sumpdf-prerelease-latest.txt", s)
	fataliferr(err)

	//sumatrapdf/sumpdf-prerelease-update.txt
	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %d\n", svnPreReleaseVer)
	err = s3UploadString("sumatrapdf/sumpdf-prerelease-update.txt", s)
	fataliferr(err)
}

// When doing a release build, it must be from from a branch rel${ver}working
// e.g. rel3.1working, where ${ver} must much sumatraVersion
func verifyOnReleaseBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := strings.TrimSpace(string(runExeMust("git", "branch")))
	pref := "* rel"
	suff := "working"
	fatalif(!strings.HasPrefix(out, pref), "running on branch '%s' which is not 'rel${ver}working' branch\n", out)
	fatalif(!strings.HasSuffix(out, suff), "running on branch '%s' which is not 'rel${ver}working' branch\n", out)

	ver := out[len(pref):]
	ver = out[:len(out)-len(suff)]
	fatalif(ver != sumatraVersion, "version mismatch, sumatra: '%s', branch: '%s'", sumatraVersion, ver)
}

// https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-3.0-install.exe
// TODO: more files
func s3UploadReleaseMust() {
	s3Dir := "sumatrapdf/rel/"

	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("SumatraPDF-prerelease-%d.exe", svnPreReleaseVer),
		"Installer.exe", fmt.Sprintf("SumatraPDF-prerelease-%d-install.exe", svnPreReleaseVer),
	}
	err := s3UploadFiles(s3Dir, "rel", files)
	fataliferr(err)

	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("SumatraPDF-prerelease-%d-64.exe", svnPreReleaseVer),
		"Installer.exe", fmt.Sprintf("SumatraPDF-prerelease-%d-install-64.exe", svnPreReleaseVer),
	}
	err = s3UploadFiles(s3Dir, "rel64", files)
	fataliferr(err)
	// write manifest last
	s3Path := s3Dir + fmt.Sprintf("SumatraPDF-prerelease-%d-manifest.txt", svnPreReleaseVer)
	err = s3UploadFile(s3Path, manifestPath())
	fataliferr(err)
}

func removeDirMust(dir string) {
	err := os.RemoveAll(dir)
	fataliferr(err)
}

func clean() {
	if flgAnalyze {
		removeDirMust("relPrefast")
		removeDirMust("dbgPrefast")
		return
	}

	removeDirMust("rel")
	removeDirMust("rel64")
	//removeDirMust("dbg")
	//removeDirMust("dbg64")
}

func detectVersions() {
	svnPreReleaseVer = getGitLinearVersionMust()
	gitSha1 = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	fmt.Printf("svnPreReleaseVer: '%d'\n", svnPreReleaseVer)
	fmt.Printf("gitSha1: '%s'\n", gitSha1)
	fmt.Printf("sumatraVersion: '%s'\n", sumatraVersion)
}

func translationsPath() string {
	return pj("strings", "translations.txt")
}

func translationsSha1HexMust(d []byte) string {
	lines := toTrimmedLines(d)
	sha1 := lines[1]
	fatalif(len(sha1) != 40, "lastTranslationsSha1HexMust: '%s' doesn't look like sha1", sha1)
	return sha1
}

func lastTranslationsSha1HexMust() string {
	d, err := ioutil.ReadFile(translationsPath())
	fataliferr(err)
	return translationsSha1HexMust(d)
}

func saveTranslationsMust(d []byte) {
	err := ioutil.WriteFile(translationsPath(), d, 0644)
	fataliferr(err)
}

func httpDlMust(uri string) []byte {
	res, err := http.Get(uri)
	fataliferr(err)
	d, err := ioutil.ReadAll(res.Body)
	res.Body.Close()
	fataliferr(err)
	return d
}

func downloadTranslations() {
	sha1 := lastTranslationsSha1HexMust()
	url := fmt.Sprintf("http://www.apptranslator.org/dltrans?app=SumatraPDF&sha1=%s", sha1)
	d := httpDlMust(url)
	lines := toTrimmedLines(d)
	if lines[1] == "No change" {
		fmt.Printf("translations didn't change\n")
		return
	}
	saveTranslationsMust(d)
	fmt.Printf("\nTranslations have changed! You must checkin before continuing!\n")
	os.Exit(1)
}

func parseCmdLine() {
	flag.BoolVar(&flgListS3, "list-s3", false, "list files in s3")
	flag.BoolVar(&flgSmoke, "smoke", false, "do a smoke (sanity) build")
	flag.BoolVar(&flgRelease, "release", false, "do a release build")
	flag.BoolVar(&flgPreRelease, "prerelease", false, "do a pre-release build")
	flag.BoolVar(&flgAnalyze, "analyze", false, "run analyze (prefast) and create summary of bugs as html file")
	flag.BoolVar(&flgUpload, "upload", false, "upload to s3 for release/prerelease builds")
	// -no-clean-check is useful when testing changes to this build script
	flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
	flag.Parse()
	// must provide an action to perform
	if flgListS3 || flgSmoke || flgRelease || flgPreRelease || flgAnalyze {
		return
	}
	flag.Usage()
	os.Exit(1)
}

func testS3Upload() {
	dst := "temp2.txt"
	src := pj("rel", "SumatraPDF.exe")
	//src := pj("rel", "buildcmap.exe")
	err := s3UploadFile2(dst, src)
	if err != nil {
		fmt.Printf("upload failed with %s\n", err)
	} else {
		fmt.Printf("upload ok!\n")
	}
	os.Exit(0)
}

func testBuildLzsa() {
	createPdbLzsaMust("rel")
	os.Exit(0)
}

func init() {
	timeStart = time.Now()
	compilePreRelNameRegexpsMust()
}

func main() {
	//testBuildLzsa()
	//testS3Upload()

	if false {
		detectVersions()
		parseSavedAnalyzeOuptut()
		os.Exit(0)
	}

	parseCmdLine()

	if flgListS3 {
		s3ListPreReleaseFilesMust(true)
		return
	}

	os.Remove(logFileName)
	verifyStartedInRightDirectoryMust()
	detectVersions()
	clean()
	if flgRelease || flgPreRelease {
		verifyHasReleaseSecretsMust()
	}
	if flgRelease {
		buildRelease()
	} else if flgPreRelease {
		buildPreRelease()
	} else if flgSmoke {
		buildSmoke()
	} else if flgAnalyze {
		buildAnalyze()
	} else {
		flag.Usage()
		os.Exit(1)
	}
	finalizeThings(false)
}
