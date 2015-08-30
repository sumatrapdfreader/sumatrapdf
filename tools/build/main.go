package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"mime"
	"os"
	"os/exec"
	"path/filepath"
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
* implement pre-release build
* upload files to s3
* implement release build
* implement buildbot
* implement translation download
* print how much each runCmd took at the end
* log to rel/build-log.txt in addition to stdout (open a log file at the beginning
  and in run*() commands append to the log if log file exists)
*/

type Secrets struct {
	AwsSecret               string
	AwsAccess               string
	CertPwd                 string
	NotifierEmail           string
	NotifierEmailPwd        string
	TranslationUploadSecret string
}

var (
	flgRelease       bool // if doing an official release build
	flgPreRelease    bool // if doing pre-release build
	flgUpload        bool
	flgSmoke         bool
	flgListS3        bool
	flgNoOp          bool
	svnPreReleaseVer int
	gitSha1          string
	sumatraVersion   string
	timeStart        time.Time
	cachedSecrets    *Secrets
)

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

func printTotalTime() {
	revertBuildConfig()
	fmt.Printf("total time: %s\n", time.Since(timeStart))
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printTotalTime()
	os.Exit(1)
}

func fatalif(cond bool, format string, args ...interface{}) {
	if cond {
		fmt.Printf(format, args...)
		printTotalTime()
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
	parts := strings.Split(path, ";")
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
	fmt.Printf("lookExeInEnvPathUncached: exeName=%s\n", exeName)
	path := lookExeInEnvPathUncachedHelper(env, exeName)
	if path == "" && filepath.Ext(exeName) == "" {
		path = lookExeInEnvPathUncachedHelper(env, exeName+".exe")
	}
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

func runCmd(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	if !showProgress {
		return cmd.CombinedOutput()
	}
	out, _ := cmd.StdoutPipe()
	cmd.Start()
	var res []byte
	go func() {
		buf := make([]byte, 1024, 1024)
		for {
			n, err := out.Read(buf)
			if err == io.EOF {
				break
			}
			if n > 0 {
				d := buf[:n]
				res = append(res, d...)
				os.Stdout.Write(d)
			}
		}
	}()
	err := cmd.Wait()
	return res, err
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

func runExeMust(exeName string, args ...string) {
	_, err := runExeInEnv(os.Environ(), exeName, args...)
	fataliferr(err)
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

// TODO: also pass logger io.Writer where the result is written in addition
// to stdout
func runMsbuild(showProgress bool, args ...string) error {
	cmd := getCmdInEnv(getEnvForVS(), "msbuild.exe", args...)
	_, err := runCmdLogged(cmd, showProgress)
	return err
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

func build() {
	err := runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
}

func buildConfigPath() string {
	return pj("src", "utils", "BuildConfig.h")
}

func certPath() string {
	return pj("scripts", "cert.pfx")
}

func setBuildConfig(preRelVer int, sha1 string, verQualifier string) {
	s := fmt.Sprintf("#define SVN_PRE_RELEASE_VER %d\n", preRelVer)
	s += fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	if verQualifier != "" {
		s += fmt.Sprintf("#define VER_QUALIFIER %s\n", verQualifier)
	}
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 644)
	fataliferr(err)
}

func verifyPreReleaseNotInS3Must(preReleaseVer int) {
	// TODO: write me
}

// check we have cert for signing and s3 creds for file uploads
func verifyHasPreReleaseSecretsMust() {
	p := certPath()
	fatalif(!fileExists(p), "verifyHasPreReleaseSecretsMust(): certificate file '%s' doesn't exist\n", p)
	secrets := readSecretsMust()
	fatalif(secrets.CertPwd == "", "CertPwd missing in %s\n", p)
	if flgUpload {
		fatalif(secrets.AwsSecret == "", "AwsSecret missing in %s\n", p)
		fatalif(secrets.AwsAccess == "", "AwsAccess missing in %s\n", p)
	}
}

func buildPreRelease() {
	fmt.Printf("Building pre-release version\n")
	var err error
	verifyGitCleanMsut()
	verifyHasPreReleaseSecretsMust()
	verifyPreReleaseNotInS3Must(svnPreReleaseVer)

	setBuildConfig(svnPreReleaseVer, gitSha1, "")
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	signMust(pj("rel", "SumatraPDF.exe"))
	signMust(pj("rel", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	signMust(pj("rel", "Installer.exe"))

	setBuildConfig(svnPreReleaseVer, gitSha1, "x64")
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	signMust(pj("rel64", "SumatraPDF.exe"))
	signMust(pj("rel64", "SumatraPDF-no-MUPDF.exe"))
	signMust(pj("rel64", "Uninstaller.exe"))
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	signMust(pj("rel64", "Installer.exe"))
	// TODO: build pdb.zip
	createManifestMust()
	s3UploadPreReleaseMust()
}

func buildRelease() {
	fmt.Printf("Building a release version\n")

	// TODO: for now the same as pre-release
	buildPreRelease()

	//s3UploadReleaseMust()
}

// TODO: download translations if necessary
func buildSmoke() {
	fmt.Printf("Smoke build\n")
	err := runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	path := pj("rel", "test_util.exe")
	runExeMust(path)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	err = runMsbuild(true, "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;Uninstaller;test_util", "/p:Configuration=Debug;Platform=x64", "/m")
	fataliferr(err)
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
	s3Obj := s3.New(auth, aws.USEast, aws.RetryingClient)
	return s3Obj.Bucket(s3BucketName)
}

func s3UploadFile(pathLocal string, pathRemote string) error {
	bucket := s3GetBucket()
	d, err := ioutil.ReadFile(pathLocal)
	if err != nil {
		return err
	}
	mimeType := mime.TypeByExtension(filepath.Ext(pathLocal))
	opts := s3.Options{}
	return bucket.Put(pathRemote, d, mimeType, s3.PublicRead, opts)
}

func s3UploadFiles(s3Dir string, dir string, files []string) error {
	n := len(files) / 2
	for i := 0; i < n; i++ {
		pathLocal := filepath.Join(dir, files[2*i])
		pathRemote := files[2*i+1]
		err := s3UploadFile(pathLocal, s3Dir+pathRemote)
		if err != nil {
			return fmt.Errorf("failed to upload %s as %s, err: %s", pathLocal, pathRemote, err)
		}
	}
	return nil
}

func s3List() {
	bucket := s3GetBucket()
	s3Dir := "sumatrapdf/prerel/"
	resp, err := bucket.List(s3Dir, "", "", 10000)
	fataliferr(err)
	fmt.Printf("%d files\n", len(resp.Contents))
	if resp.IsTruncated {
		fmt.Printf("beware: truncated response!\n")
	}
	for _, key := range resp.Contents {
		fmt.Printf("file: %s\n", key.Key)
	}
}

func fileSizeMust(path string) int64 {
	fi, err := os.Stat(path)
	fataliferr(err)
	return fi.Size()
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

func signMust(path string) {
	// the sign tool is finicky, so copy it and cert to the same dir as
	// exe we're signing
	fileDir := filepath.Dir(path)
	fileName := filepath.Base(path)
	secrets := readSecretsMust()
	certPwd := secrets.CertPwd
	certSrc := certPath()
	certDest := pj(fileDir, "cert.pfx")
	if !fileExists(certDest) {
		fileCopyMust(certDest, certSrc)
	}
	env := getEnvForVS()
	cmd := getCmdInEnv(env, "signtool.exe", "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
		"/du", "http://www.sumatrapdfreader.org", "/f", "cert.pfx",
		"/p", certPwd, fileName)
	cmd.Dir = fileDir
	_, err := runCmdLogged(cmd, true)
	fataliferr(err)
}

// TODO: more files
func s3UploadReleaseMust() {
	s3Dir := "sumatrapdf/prerel/"

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
	err = s3UploadFile(manifestPath(), fmt.Sprintf("SumatraPDF-prerelease-%d-manifest.txt", svnPreReleaseVer))
	fataliferr(err)
}

// https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-3.0-install.exe
func s3UploadPreReleaseMust() {
	s3Dir := "sumatrapdf/rel/"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("SumatraPDF-%s.exe", sumatraVersion),
		"Installer.exe", fmt.Sprintf("SumatraPDF-%s-install.exe", sumatraVersion),
	}
	err := s3UploadFiles(s3Dir, "rel", files)
	fataliferr(err)

	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("SumatraPDF-%s-64.exe", sumatraVersion),
		"Installer.exe", fmt.Sprintf("SumatraPDF-%s-install-64.exe", sumatraVersion),
	}
	err = s3UploadFiles(s3Dir, "rel64", files)
	fataliferr(err)
	// TODO: upload
	//sumatrapdf/sumatralatest.js
	//sumatrapdf/sumpdf-prerelease-update.txt
	//sumatrapdf/sumpdf-prerelease-latest.txt
}

func removeDirMust(dir string) {
	err := os.RemoveAll(dir)
	fataliferr(err)
}

func clean() {
	removeDirMust("rel")
	removeDirMust("rel64")
	removeDirMust("relPrefast")
	removeDirMust("dbg")
	removeDirMust("dbg64")
	removeDirMust("dbgPrefast")
}

func detectVersions() {
	svnPreReleaseVer = getGitLinearVersionMust()
	gitSha1 = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	fmt.Printf("svnPreReleaseVer: '%d'\n", svnPreReleaseVer)
	fmt.Printf("gitSha1: '%s'\n", gitSha1)
	fmt.Printf("sumatraVersion: '%s'\n", sumatraVersion)
}

func parseCmdLine() {
	flag.BoolVar(&flgListS3, "list-s3", false, "list files in s3")
	flag.BoolVar(&flgSmoke, "smoke", false, "do a smoke (sanity) build")
	flag.BoolVar(&flgRelease, "release", false, "do a release build")
	flag.BoolVar(&flgPreRelease, "prerelease", false, "do a pre-release build")
	flag.BoolVar(&flgUpload, "upload", false, "upload to s3 for release/prerelease builds")
	flag.BoolVar(&flgNoOp, "noop", false, "compile but do nothing else")
	flag.Parse()
}

func main() {
	timeStart = time.Now()
	parseCmdLine()
	verifyStartedInRightDirectoryMust()
	detectVersions()
	if flgNoOp {
		fatalf("Exiting because -noop\n")
	}
	if flgRelease || flgPreRelease {
		verifyHasPreReleaseSecretsMust()
	}
	clean()
	if flgRelease {
		buildRelease()
	} else if flgPreRelease {
		buildPreRelease()
	} else if flgSmoke {
		buildSmoke()
	} else if flgListS3 {
		s3List()
	} else {
		flag.Usage()
	}
	printTotalTime()
}
