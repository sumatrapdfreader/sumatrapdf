package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

/*
To run:
* install Go
 - download and run latest installer http://golang.org/doc/install
 - restart so that PATH changes take place
 - set GOPATH env variable (e.g. to %USERPROFILE%\src\go)
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

func getEnvForVS() []string {
	if envForVSCached == nil {
		envForVSCached = getEnvForVSUncached()
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

func lookExeInEnvPathUncached(env []string, exeName string) string {
	for _, envVar := range env {
		parts := strings.SplitN(envVar, "=", 2)
		name := strings.ToLower(parts[0])
		if name != "path" {
			continue
		}
		parts = strings.Split(parts[1], ";")
		for _, dir := range parts {
			path := filepath.Join(dir, exeName)
			if fileExists(path) {
				return path
			}
		}
		fatalf("didn't find %s in '%s'\n", exeName, parts[1])
	}
	return ""
}

// TODO: show progress as it happens
func runExeInEnv(env []string, exeName string, args ...string) ([]byte, error) {
	exePath, err := exec.LookPath(exeName)
	if err != nil {
		exePath = lookExeInEnvPathUncached(env, exeName)
	}
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

func uploadPreReleaseToS3Must() {

}

func verifyPreReleaseNotInS3Must(preReleaseVer int) {
	// TODO: write me
}

// check we have cert for signing and s3 creds for file uploads
func verifyHasPreReleaseSecretsMust() {
	p := certPath()
	fatalif(!fileExists(p), "verifyHasPreReleaseSecretsMust(): certificate file '%s' doesn't exist\n", p)
	secrets := readSecretsMust()
	fatalif(secrets.AwsSecret == "", "AwsSecret in %s is empty\n", p)
	fatalif(secrets.AwsAccess == "", "AwsAccess in %s is empty\n", p)
}

func buildPreRelease() {
	fmt.Printf("Building pre-release version")
	verifyGitCleanMsut()
	verifyHasPreReleaseSecretsMust()
	verifyPreReleaseNotInS3Must(svnPreReleaseVer)
	env := getEnvForVS()
	setBuildConfig(svnPreReleaseVer, gitSha1, "")
	_, err := runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
	fataliferr(err)
	setBuildConfig(svnPreReleaseVer, gitSha1, "x64")
	_, err = runExeLogged(env, "msbuild.exe", "vs2015\\SumatraPDF.sln", "/t:Installer;SumatraPDF;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	fataliferr(err)
	uploadPreReleaseToS3Must()
}

func buildRelease() {
	fmt.Printf("Building a release version\n")
	fatalf("NYI")
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
	flag.Parse()
}

func main() {
	timeStart = time.Now()
	parseCmdLine()
	verifyStartedInRightDirectoryMust()
	detectVersions()
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
