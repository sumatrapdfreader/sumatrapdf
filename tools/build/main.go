package main

import (
	"encoding/json"
	"flag"
	"fmt"
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

func printTotalTime() {
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
		fatalf("%s", err.Error())
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

// TODO: show progress as it happens
func runExeInEnv(env []string, exeName string, args ...string) ([]byte, error) {
	exePath := lookExeInEnvPath(env, exeName)
	cmd := exec.Command(exePath, args...)
	cmd.Env = env
	if true {
		fmt.Printf("Running %s\n", cmd.Args)
	}
	return cmd.CombinedOutput()
}

func runExe(exeName string, args ...string) ([]byte, error) {
	return runExeInEnv(os.Environ(), exeName, args...)
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
	n := len(lines)
	fatalif(n < 9000, "getGitLinearVersion: n is %d (should be > 9000)", n)
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
	env := getEnvForVS()
	_, err := runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=x64", "/m")
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

func revertBuildConfigMust() {
	runExeMust("git", "checkout", buildConfigPath())
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
	fmt.Printf("Building pre-release version")
	verifyGitCleanMsut()
	verifyHasPreReleaseSecretsMust()
	verifyPreReleaseNotInS3Must(svnPreReleaseVer)

	env := getEnvForVS()
	setBuildConfig(svnPreReleaseVer, gitSha1, "")
	_, err := runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	// TODO: sign SumatraPDF.exe and SumatraPDF-no-MUPDF.exe
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)

	setBuildConfig(svnPreReleaseVer, gitSha1, "x64")
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:SumatraPDF;SumatraPDF-no-MUPDF;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	// TODO: sign SumatraPDF.exe and SumatraPDF-no-MUPDF.exe
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)

	// TODO: revert
	s3UploadPreReleaseMust()
}

func buildRelease() {
	fmt.Printf("Building a release version\n")

	// TODO: for now the same as smoke
	buildSmoke()
	s3UploadReleaseMust()
}

// TODO: download translations if necessary
func buildSmoke() {
	fmt.Printf("Smoke build\n")
	env := getEnvForVS()
	_, err := runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	path := pj("rel", "test_util.exe")
	runExeMust(path)
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Debug;Platform=x64", "/m")
	fataliferr(err)
}

func s3UploadFile(pathLocal string, pathRemote string) error {
	s3BucketName := "kjkpub"
	auth := aws.Auth{
		AccessKey: "",
		SecretKey: "",
	}
	s3Obj := s3.New(auth, aws.USWest, aws.RetryingClient)
	bucket := s3Obj.Bucket(s3BucketName)
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
	fatalif(true, "fileCopyMust: NYI")
}

/*
def sign(file_path, cert_pwd):
    # not everyone has a certificate, in which case don't sign
    if cert_pwd is None:
        print("Skipping signing %s" % file_path)
        return
    # the sign tool is finicky, so copy it and cert to the same dir as
    # exe we're signing
    file_dir = os.path.dirname(file_path)
    file_name = os.path.basename(file_path)
    cert_src = os.path.join("scripts", "cert.pfx")
    cert_dest = os.path.join(file_dir, "cert.pfx")
    if not os.path.exists(cert_dest):
        shutil.copy(cert_src, cert_dest)
    curr_dir = os.getcwd()
    os.chdir(file_dir)
    run_cmd_throw(
        "signtool.exe", "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
        "/du", "http://blog.kowalczyk.info/software/sumatrapdf/", "/f", "cert.pfx", "/p", cert_pwd, file_name)
    os.chdir(curr_dir)
*/

func sign(path string, certPwd string) {
	// the sign tool is finicky, so copy it and cert to the same dir as
	// exe we're signing
	fileDir := filepath.Dir(path)
	fileName := filepath.Base(path)
	certSrc := certPath()
	certDest := filepath.Join(fileDir, "cert.pfx")
	if !fileExists(certDest) {
		fileCopyMust(certDest, certSrc)
	}
	// TODO: must be able to pass-in curr dir
	runExeMust("signtool", "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
		"/du", "http://blog.kowalczyk.info/software/sumatrapdf/", "/f", "cert.pfx",
		"/p", certPwd, fileName)
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
	err = s3UploadFile(manifestPath(), fmt.Sprintf("manifest-%d.txt", svnPreReleaseVer))
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
	} else {
		buildSmoke()
	}
	printTotalTime()
}
