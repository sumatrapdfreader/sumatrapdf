package main

import (
	"archive/zip"
	"fmt"
	"io/ioutil"
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

// Version must be in format x.y.z
func verifyCorrectVersionMust(ver string) {
	parts := strings.Split(ver, ".")
	u.PanicIf(len(parts) == 0 || len(parts) > 3, "%s is not a valid version number", ver)
	for _, part := range parts {
		u.PanicIf(!isNum(part), "%s is not a valid version number", ver)
	}
}
func getFileNamesWithPrefix(prefix string) [][]string {
	files := [][]string{
		{"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix)},
		{"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix)},
		{"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix)},
		{"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix)},
		{"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix)},
	}
	return files
}

func copyBuiltFiles(dstDir string, srcDir string, prefix string) {
	files := getFileNamesWithPrefix(prefix)
	for _, f := range files {
		srcName := f[0]
		srcPath := filepath.Join(srcDir, srcName)
		dstName := f[1]
		dstPath := filepath.Join(dstDir, dstName)
		u.CreateDirForFileMust(dstPath)
		if u.FileExists(srcPath) {
			u.CopyFileMust(dstPath, srcPath)
		} else {
			logf("Skipping copying '%s'\n", srcPath)
		}
	}
}

func copyBuiltManifest(dstDir string, prefix string) {
	srcPath := filepath.Join(artifactsDir, "manifest.txt")
	dstName := prefix + "-manifest.txt"
	dstPath := filepath.Join(dstDir, dstName)
	u.CopyFileMust(dstPath, srcPath)
}

func build(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util`, p, `/m`)
	runTestUtilMust(dir)

	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview`, p, `/m`)
	signFilesMust(dir)
	createPdbZipMust(dir)
	createPdbLzsaMust(dir)
}

func buildJustInstaller(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF-dll;PdfFilter;PdfPreview`, p, `/m`)
	signFilesMust(dir)
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

func getBuildConfigCommon() string {
	sha1 := getGitSha1()
	s := fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	todayDate := time.Now().Format("2006-01-02")
	s += fmt.Sprintf("#define BUILT_ON %s\n", todayDate)
	return s
}

// writes src/utils/BuildConfig.h to over-ride some of build settings
func setBuildConfigDaily() {
	s := getBuildConfigCommon()
	// daily are also pre-release builds
	preRelVer := getPreReleaseVer()
	panicIf(preRelVer == "")
	s += fmt.Sprintf("#define PRE_RELEASE_VER %s\n", preRelVer)
	s += "#define IS_DAILY_BUILD 1\n"
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 0644)
	panicIfErr(err)
}

func setBuildConfigPreRelease() {
	s := getBuildConfigCommon()
	preRelVer := getPreReleaseVer()
	s += fmt.Sprintf("#define PRE_RELEASE_VER %s\n", preRelVer)
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 0644)
	panicIfErr(err)
}

func setBuildConfigRelease() {
	s := getBuildConfigCommon()
	s += "#define SUMATRA_UPDATE_INFO_URL L\"https://www.sumatrapdfreader.org/update-check-rel.txt\"\n"
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 0644)
	panicIfErr(err)
}

func revertBuildConfig() {
	runExeMust("git", "checkout", buildConfigPath())
}

func addZipFileWithNameMust(w *zip.Writer, path, nameInZip string) {
	fi, err := os.Stat(path)
	panicIfErr(err)
	fih, err := zip.FileInfoHeader(fi)
	panicIfErr(err)
	fih.Name = nameInZip
	fih.Method = zip.Deflate
	d, err := ioutil.ReadFile(path)
	panicIfErr(err)
	fw, err := w.CreateHeader(fih)
	panicIfErr(err)
	_, err = fw.Write(d)
	panicIfErr(err)
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
	panicIfErr(err)
	defer f.Close()
	zw := zip.NewWriter(f)
	path := filepath.Join(dir, "SumatraPDF.exe")
	addZipFileWithNameMust(zw, path, nameInZip)
	err = zw.Close()
	panicIfErr(err)
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
	panicIfErr(err)
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
	panicIfErr(err)
}

func createPdbZipMust(dir string) {
	path := filepath.Join(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	panicIfErr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		addZipFileMust(w, filepath.Join(dir, file))
	}

	err = w.Close()
	panicIfErr(err)
}

func createPdbLzsaMust(dir string) {
	args := []string{"SumatraPDF.pdb.lzsa"}
	args = append(args, pdbFiles...)
	curDir, err := os.Getwd()
	panicIfErr(err)
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

var (
	artifactsDir = filepath.Join("out", "artifacts")
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
	if u.FileExists(filepath.Join(dir, "SumatraPDF.exe")) {
		signMust(filepath.Join(dir, "SumatraPDF.exe"))
	}
	signMust(filepath.Join(dir, "libmupdf.dll"))
	signMust(filepath.Join(dir, "PdfFilter.dll"))
	signMust(filepath.Join(dir, "PdfPreview.dll"))
	signMust(filepath.Join(dir, "SumatraPDF-dll.exe"))
}
