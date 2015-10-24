package main

import (
	"archive/zip"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
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
 - install goamz: go get github.com/goamz/goamz/s3
* see .\scripts\build-release.bat for how to run it
*/

/*
TODO:
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

const (
	s3PreRelDir  = "sumatrapdf/prerel/"
	s3RelDir     = "sumatrapdf/rel/"
	maxS3Results = 1000
)

var (
	flgRelease       bool // if doing an official release build
	flgPreRelease    bool // if doing pre-release build
	flgUpload        bool
	flgSmoke         bool
	flgListS3        bool
	flgAnalyze       bool
	flgNoCleanCheck  bool
	svnPreReleaseVer string
	gitSha1          string
	sumatraVersion   string
	timeStart        time.Time
	cachedSecrets    *Secrets
)

func finalizeThings(crashed bool) {
	revertBuildConfig()
	if !crashed {
		printTimings()
		fmt.Printf("total time: %s\n", time.Since(timeStart))
		logToFile(fmt.Sprintf("total time: %s\n", time.Since(timeStart)))
	}
	closeLogFile()
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

func verifyGitCleanMust() {
	if flgNoCleanCheck {
		return
	}
	fatalif(!isGitClean(), "git has unsaved changes\n")
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

func setBuildConfig(preRelVer string, sha1 string) {
	s := fmt.Sprintf("#define SVN_PRE_RELEASE_VER %s\n", preRelVer)
	s += fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 644)
	fataliferr(err)
}

// we shouldn't re-upload files. We upload manifest-${ver}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func verifyPreReleaseNotInS3Must(ver string) {
	if !flgUpload {
		return
	}
	s3Path := s3PreRelDir + fmt.Sprintf("SumatraPDF-prerelease-%s-manifest.txt", ver)
	fatalif(s3Exists(s3Path), "build %d already exists in s3 because '%s' exists\n", ver, s3Path)
}

func verifyReleaseNotInS3Must(ver string) {
	if !flgUpload {
		return
	}
	s3Path := s3RelDir + fmt.Sprintf("SumatraPDF-%s-manifest.txt", ver)
	fatalif(s3Exists(s3Path), "build '%s' already exists in s3 because '%s' existst\n", ver, s3Path)
}

// check we have cert for signing and s3 creds for file uploads
func verifyHasReleaseSecretsMust() {
	p := certPath()
	fatalif(!fileExists(p), "verifyHasPreReleaseSecretsMust(): certificate file '%s' doesn't exist\n", p)
	secrets := readSecretsMust()
	fatalif(secrets.CertPwd == "", "CertPwd missing in %s\n", p)

	if flgUpload {
		s3SetSecrets(secrets.AwsAccess, secrets.AwsSecret)
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

func addZipFileMust(w *zip.Writer, path string) {
	d, err := ioutil.ReadFile(path)
	fataliferr(err)
	f, err := w.Create(filepath.Base(path))
	fataliferr(err)
	_, err = f.Write(d)
	fataliferr(err)
	// no need to close f. It's implicitly closed by the next Create() or Close() call
}

func createExeZipMust(dir string) {
	path := pj(dir, "SumatraPDF.zip")
	f, err := os.Create(path)
	fataliferr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	addZipFileMust(w, pj(dir, "SumatraPDF.exe"))

	err = w.Close()
	fataliferr(err)
}

func createPdbZipMust(dir string) {
	path := pj(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	fataliferr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		addZipFileMust(w, pj(dir, file))
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

	fmt.Printf("Building pre-release version %s\n", svnPreReleaseVer)
	verifyGitCleanMust()
	verifyOnMasterBranchMust()
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
	s3DeleteOldestPreRel()
	s3UploadPreReleaseMust(svnPreReleaseVer)
}

func buildRelease() {
	var err error

	fmt.Printf("Building release version %s\n", sumatraVersion)
	verifyGitCleanMust()
	verifyOnReleaseBranchMust()
	verifyReleaseNotInS3Must(sumatraVersion)

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

	createExeZipMust("rel")
	createExeZipMust("rel64")

	createPdbZipMust("rel")
	createPdbZipMust("rel64")

	createPdbLzsaMust("rel")
	createPdbLzsaMust("rel64")

	createManifestMust()
	s3UploadReleaseMust(sumatraVersion)
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
		var sumLatestVer = %s;
		var sumBuiltOn = "%s";
		var sumLatestName = "SumatraPDF-prerelease-%s.exe";

		var sumLatestExe = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s.exe";
		var sumLatestPdb = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s.pdb.zip";
		var sumLatestInstaller = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s-install.exe";

		var sumLatestExe64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s-64.exe";
		var sumLatestPdb64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s-64.pdb.zip";
		var sumLatestInstaller64 = "http://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-%s-install-64.exe";
`, v, currDate, v, v, v, v, v, v, v)
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

// ByVerFilesForVer sorts by version
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
	resp, err := bucket.List(s3PreRelDir, "", "", maxS3Results)
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
	if !flgUpload {
		return
	}
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

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func s3UploadPreReleaseMust(ver string) {
	if !flgUpload {
		fmt.Printf("Skipping pre-release upload to s3 because -upload flag not given\n")
		return
	}

	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	manifestRemotePath := s3PreRelDir + prefix + "-manifest.txt"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := s3UploadFiles(s3PreRelDir, "rel", files)
	fataliferr(err)

	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s-64", ver)
	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err = s3UploadFiles(s3PreRelDir, "rel64", files)
	fataliferr(err)

	manifestLocalPath := pj("rel", "manifest.txt")
	err = s3UploadFileReader(manifestRemotePath, manifestLocalPath, true)
	fataliferr(err)

	s := createSumatraLatestJs()
	err = s3UploadString("sumatrapdf/sumatralatest.js", s, true)
	fataliferr(err)

	//sumatrapdf/sumpdf-prerelease-latest.txt
	err = s3UploadString("sumatrapdf/sumpdf-prerelease-latest.txt", ver, true)
	fataliferr(err)

	//sumatrapdf/sumpdf-prerelease-update.txt
	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	err = s3UploadString("sumatrapdf/sumpdf-prerelease-update.txt", s, true)
	fataliferr(err)
}

/*
Given result of git btranch that looks like:

master
* rel3.1working

Return active branch marked with "*" ('rel3.1working' in this case) or empty
string if no current branch.
*/
func getCurrentBranch(d []byte) string {
	lines := toTrimmedLines(d)
	for _, l := range lines {
		if strings.HasPrefix(l, "* ") {
			return l[2:]
		}
	}
	return ""
}

// When doing a release build, it must be from from a branch rel${ver}working
// e.g. rel3.1working, where ${ver} must much sumatraVersion
func verifyOnReleaseBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := runExeMust("git", "branch")
	currBranch := getCurrentBranch(out)
	pref := "rel"
	suff := "working"
	fatalif(!strings.HasPrefix(currBranch, pref), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)
	fatalif(!strings.HasSuffix(currBranch, suff), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)

	ver := currBranch[len(pref):]
	ver = ver[:len(ver)-len(suff)]
	fatalif(ver != sumatraVersion, "version mismatch, sumatra: '%s', branch: '%s'\n", sumatraVersion, ver)
}

func verifyOnMasterBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := runExeMust("git", "branch")
	currBranch := getCurrentBranch(out)
	fatalif(currBranch != "master", "no on master branch (branch: '%s')\n", currBranch)
}

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-3.1-install.exe etc.
func s3UploadReleaseMust(ver string) {
	if !flgUpload {
		fmt.Printf("Skipping release upload to s3 because -upload flag not given\n")
		return
	}

	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	manifestRemotePath := s3RelDir + prefix + "-manifest.txt"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := s3UploadFiles(s3RelDir, "rel", files)
	fataliferr(err)

	prefix = fmt.Sprintf("SumatraPDF-%s-64", ver)
	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err = s3UploadFiles(s3RelDir, "rel64", files)
	fataliferr(err)

	// upload manifest last
	manifestLocalPath := pj("rel", "manifest.txt")
	err = s3UploadFileReader(manifestRemotePath, manifestLocalPath, true)
	fataliferr(err)

	// Note: not uploading auto-update version info. We update it separately,
	// a week or so after build is released, so that if there are serious issues,
	// we can create an update and less people will be affected
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
	svnPreReleaseVer = strconv.Itoa(getGitLinearVersionMust())
	gitSha1 = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	fmt.Printf("svnPreReleaseVer: '%s'\n", svnPreReleaseVer)
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
	err := s3UploadFile(dst, src, true)
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
	logFileName = "build-log.txt"
	compilePreRelNameRegexpsMust()
}

func main() {
	//testBuildLzsa()
	//testS3Upload()

	// TODO: temporary
	if true {
		err := os.Chdir(pj("..", "sumatrapdf-3.1"))
		fataliferr(err)
	}

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
