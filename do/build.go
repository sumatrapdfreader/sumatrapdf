package main

import (
	"archive/zip"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/u"
)

var (
	pdbFiles = []string{"libmupdf.pdb", "SumatraPDF-dll.pdb", "SumatraPDF.pdb"}
)

var (
	preReleaseVerCached string
	gitSha1Cached       string
	sumatraVersion      string
)

func getGitSha1() string {
	panicIf(gitSha1Cached == "", "must call detectVersions() first")
	return gitSha1Cached
}

func getPreReleaseVer() string {
	panicIf(preReleaseVerCached == "", "must call detectVersions() first")
	return preReleaseVerCached
}

func isNum(s string) bool {
	_, err := strconv.Atoi(s)
	return err == nil
}

// https://goobar.io/2019/12/07/manually-trigger-a-github-actions-workflow/
// send a webhook POST request to trigger a build
func triggerPreRelBuild() {
	ghtoken := os.Getenv("GITHUB_TOKEN")
	panicIf(ghtoken == "", "need GITHUB_TOKEN env variable")
	data := `{"event_type": "build-pre-rel"}`
	uri := "https://api.github.com/repos/sumatrapdfreader/sumatrapdf/dispatches"
	req, err := http.NewRequest(http.MethodPost, uri, strings.NewReader(data))
	u.Must(err)
	req.Header.Set("Accept", "application/vnd.github.everest-preview+json")
	val := fmt.Sprintf("token %s", ghtoken)
	req.Header.Set("Authorization", val)
	rsp, err := http.DefaultClient.Do(req)
	u.Must(err)
	u.PanicIf(rsp.StatusCode >= 400)
}

// Version must be in format x.y.z
func verifyCorrectVersionMust(ver string) {
	parts := strings.Split(ver, ".")
	u.PanicIf(len(parts) == 0 || len(parts) > 3, "%s is not a valid version number", ver)
	for _, part := range parts {
		u.PanicIf(!isNum(part), "%s is not a valid version number", ver)
	}
}

func extractSumatraVersionMust() string {
	path := filepath.Join("src", "Version.h")
	d := u.ReadFileMust(path)
	lines := toTrimmedLines(d)
	s := "#define CURR_VERSION "
	for _, l := range lines {
		if strings.HasPrefix(l, s) {
			ver := l[len(s):]
			verifyCorrectVersionMust(ver)
			return ver
		}
	}
	panic(fmt.Sprintf("couldn't extract CURR_VERSION from %s\n", path))
}

func detectVersions() {
	ver := getGitLinearVersionMust()
	preReleaseVerCached = strconv.Itoa(ver)
	gitSha1Cached = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	logf("preReleaseVer: '%s'\n", preReleaseVerCached)
	logf("gitSha1: '%s'\n", gitSha1Cached)
	logf("sumatraVersion: '%s'\n", sumatraVersion)
}

func clean() {
	os.RemoveAll("out")
}

func runTestUtilMust(dir string) {
	cmd := exec.Command(`.\test_util.exe`)
	cmd.Dir = dir
	u.RunCmdLoggedMust(cmd)
}

func buildLzsa() {
	// early exit if missing
	detectSigntoolPath()

	defer makePrintDuration("buildLzsa")()
	clean()

	msbuildPath := detectMsbuildPath()
	runExeLoggedMust(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:MakeLZSA`, `/p:Configuration=Release;Platform=Win32`, `/m`)

	path := filepath.Join("out", "rel32", "MakeLZSA.exe")
	signMust(path)
}

// smoke build is meant to be run locally to check that we can build everything
// it does full installer build of 64-bit release build
// We don't build other variants for speed. It takes about 5 mins locally
func smokeBuild() {
	detectSigntoolPath()
	defer makePrintDuration("smoke build")()
	clean()

	lzsa := absPathMust(filepath.Join("bin", "MakeLZSA.exe"))
	u.PanicIf(!u.FileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPath()
	runExeLoggedMust(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:SumatraPDF-dll;test_util`, `/p:Configuration=Release;Platform=x64`, `/m`)
	runTestUtilMust(filepath.Join("out", "rel64"))

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "SumatraPDF-dll.pdb:SumatraPDF-dll.pdb")
		cmd.Dir = filepath.Join("out", "rel64")
		u.RunCmdLoggedMust(cmd)
	}
}

func buildConfigPath() string {
	return filepath.Join("src", "utils", "BuildConfig.h")
}

// writes src/utils/BuildConfig.h to over-ride some of build settings
func setBuildConfig(isDaily bool) {
	sha1 := getGitSha1()
	preRelVer := getPreReleaseVer()
	todayDate := time.Now().Format("2006-01-02")

	s := fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	s += fmt.Sprintf("#define BUILT_ON %s\n", todayDate)
	if preRelVer != "" {
		s += fmt.Sprintf("#define PRE_RELEASE_VER %s\n", preRelVer)
	}
	if isDaily {
		s += "#define IS_DAILY_BUILD 1\n"
	}
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 644)
	fatalIfErr(err)
}

func revertBuildConfig() {
	runExeMust("git", "checkout", buildConfigPath())
}

func addZipFileWithNameMust(w *zip.Writer, path, nameInZip string) {
	fi, err := os.Stat(path)
	fatalIfErr(err)
	fih, err := zip.FileInfoHeader(fi)
	fatalIfErr(err)
	fih.Name = nameInZip
	fih.Method = zip.Deflate
	d, err := ioutil.ReadFile(path)
	fatalIfErr(err)
	fw, err := w.CreateHeader(fih)
	fatalIfErr(err)
	_, err = fw.Write(d)
	fatalIfErr(err)
	// fw is just a io.Writer so we can't Close() it. It's not necessary as
	// it's implicitly closed by the next Create(), CreateHeader()
	// or Close() call on zip.Writer
}

func addZipFileMust(w *zip.Writer, path string) {
	nameInZip := filepath.Base(path)
	addZipFileWithNameMust(w, path, nameInZip)
}

func createExeZipWithGoWithNameMust(dir, nameInZip string) {
	zipPath := filepath.Join(dir, "SumatraPDF.zip")
	os.Remove(zipPath) // called multiple times during upload
	f, err := os.Create(zipPath)
	fatalIfErr(err)
	defer f.Close()
	zw := zip.NewWriter(f)
	path := filepath.Join(dir, "SumatraPDF.exe")
	addZipFileWithNameMust(zw, path, nameInZip)
	err = zw.Close()
	fatalIfErr(err)
}

func createExeZipWithPigz(dir string) {
	srcFile := "SumatraPDF.exe"
	srcPath := filepath.Join(dir, srcFile)
	fatalIf(!u.FileExists(srcPath), "file '%s' doesn't exist\n", srcPath)

	// this is the file that pigz.exe will create
	dstFileTmp := "SumatraPDF.exe.zip"
	dstPathTmp := filepath.Join(dir, dstFileTmp)
	removeFileMust(dstPathTmp)

	// this is the file we want at the end
	dstFile := "SumatraPDF.zip"
	dstPath := filepath.Join(dir, dstFile)
	removeFileMust(dstPath)

	wd, err := os.Getwd()
	fatalIfErr(err)
	pigzExePath := filepath.Join(wd, "bin", "pigz.exe")
	fatalIf(!u.FileExists(pigzExePath), "file '%s' doesn't exist\n", pigzExePath)
	cmd := exec.Command(pigzExePath, "-11", "--keep", "--zip", srcFile)
	// in pigz we don't control the name of the file created inside so
	// so when we run pigz the current directory is the same as
	// the directory with the file we're compressing
	cmd.Dir = dir
	u.RunCmdMust(cmd)

	fatalIf(!u.FileExists(dstPathTmp), "file '%s' doesn't exist\n", dstPathTmp)
	err = os.Rename(dstPathTmp, dstPath)
	fatalIfErr(err)
}

func createPdbZipMust(dir string) {
	path := filepath.Join(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	fatalIfErr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		addZipFileMust(w, filepath.Join(dir, file))
	}

	err = w.Close()
	fatalIfErr(err)
}

func createPdbLzsaMust(dir string) {
	args := []string{"SumatraPDF.pdb.lzsa"}
	args = append(args, pdbFiles...)
	curDir, err := os.Getwd()
	fatalIfErr(err)
	makeLzsaPath := filepath.Join(curDir, "bin", "MakeLZSA.exe")
	cmd := exec.Command(makeLzsaPath, args...)
	cmd.Dir = dir
	u.RunCmdLoggedMust(cmd)
}

// manifest is build for pre-release builds and contains information about file sizes
func createManifestMust() {
	var lines []string
	files := []string{
		"SumatraPDF.exe",
		"SumatraPDF.zip",
		"SumatraPDF-dll.exe",
		"libmupdf.dll",
		"PdfFilter.dll",
		"PdfPreview.dll",
		"SumatraPDF.pdb.zip",
		"SumatraPDF.pdb.lzsa",
	}
	dirs := []string{rel32Dir, rel64Dir}
	// in daily build, there's no 32bit build
	if !pathExists(rel32Dir) {
		dirs = []string{rel64Dir}
	}
	for _, dir := range dirs {
		for _, file := range files {
			path := filepath.Join(dir, file)
			size := fileSizeMust(path)
			line := fmt.Sprintf("%s: %d", path, size)
			lines = append(lines, line)
		}
	}

	s := strings.Join(lines, "\n")
	u.CreateDirIfNotExistsMust(artifactsDir)
	path := filepath.Join(artifactsDir, "manifest.txt")
	u.WriteFileMust(path, []byte(s))
}

// https://lucasg.github.io/2018/01/15/Creating-an-appx-package/
// https://docs.microsoft.com/en-us/windows/win32/appxpkg/make-appx-package--makeappx-exe-#to-create-a-package-using-a-mapping-file
// https://github.com/lucasg/Dependencies/tree/master/DependenciesAppx
// https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-packaging-dot-net
func makeAppx() {
	appExePath := detectMakeAppxPath()
	fmt.Printf("makeAppx: '%s'\n", appExePath)
}

const (
	artifactsDir = "artifacts"
)

var (
	artifactFiles = []string{
		"SumatraPDF.exe",
		"SumatraPDF.zip",
		"SumatraPDF-dll.exe",
		"SumatraPDF.pdb.lzsa",
		"SumatraPDF.pdb.zip",
	}
)

func signFilesMust(dir string) {
	if !shouldSignAndUpload() {
		logf("Skipping signing in dir '%s'\n", dir)
	}
	signMust(filepath.Join(dir, "SumatraPDF.exe"))
	signMust(filepath.Join(dir, "libmupdf.dll"))
	signMust(filepath.Join(dir, "PdfFilter.dll"))
	signMust(filepath.Join(dir, "PdfPreview.dll"))
	signMust(filepath.Join(dir, "SumatraPDF-dll.exe"))
}
