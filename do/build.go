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

	"github.com/kjk/u"
)

var (
	pdbFiles = []string{"libmupdf.pdb", "Installer.pdb",
		"SumatraPDF-no-MuPDF.pdb", "SumatraPDF.pdb"}
)

var (
	svnPreReleaseVer string
	gitSha1          string
	sumatraVersion   string
)

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
	svnPreReleaseVer = strconv.Itoa(ver)
	gitSha1 = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	fmt.Printf("svnPreReleaseVer: '%s'\n", svnPreReleaseVer)
	fmt.Printf("gitSha1: '%s'\n", gitSha1)
	fmt.Printf("sumatraVersion: '%s'\n", sumatraVersion)
}

func clean() {
	os.RemoveAll("rel32")
	os.RemoveAll("rel64")
	os.RemoveAll("dbg32")
	os.RemoveAll("dbg64")
}

func runTestUtilMust(dir string) {
	cmd := exec.Command(`.\test_util.exe`)
	cmd.Dir = dir
	u.RunCmdLoggedMust(cmd)
}

func buildLzsa() {
	// early exit if missing
	detectSigntoolPath()
	getCertPwd()

	defer makePrintDuration("buildLzsa")()
	clean()

	msbuildPath := detectMsbuildPath()
	cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:MakeLZSA`, `/p:Configuration=Release;Platform=Win32`, `/m`)
	u.RunCmdLoggedMust(cmd)

	path := filepath.Join("rel32", "MakeLZSA.exe")
	signMust(path)
}

// the things we do on GitHub Actions CI
func ciBuild() {
	// early exit if missing
	detectSigntoolPath()
	getCertPwd()

	defer makePrintDuration("ciBuild")()
	clean()

	lzsa := absPathMust(filepath.Join("bin", "MakeLZSA.exe"))
	u.PanicIf(!u.FileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPath()
	{
		cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:all;Installer`, `/p:Configuration=Release;Platform=Win32`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	runTestUtilMust("rel32")

	{
		cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:SumatraPDF;Installer;test_util`, `/p:Configuration=Release;Platform=x64`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	runTestUtilMust("rel64")

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "Installer.pdb:Installer.pdb", "SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb", "SumatraPDF.pdb:SumatraPDF.pdb")
		cmd.Dir = "rel32"
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "Installer.pdb:Installer.pdb", "SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb", "SumatraPDF.pdb:SumatraPDF.pdb")
		cmd.Dir = "rel64"
		u.RunCmdLoggedMust(cmd)
	}
}

// TOOD: alternatively, just puts pigz.exe in the repo
func downloadPigzMust() {
	uri := "https://kjkpub.s3.amazonaws.com/software/pigz/2.3.1-149/pigz.exe"
	path := pj("bin", "pigz.exe")
	sha1 := "10a2d3e3cafbad083972d6498fee4dc7df603c04"
	httpDlToFileMust(uri, path, sha1)
}

func buildConfigPath() string {
	return pj("src", "utils", "BuildConfig.h")
}

// writes src/utils/BuildConfig.h to over-ride some of build settings
func setBuildConfig(sha1, preRelVer string) {
	fatalIf(sha1 == "", "sha1 must be set")
	s := fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	if preRelVer != "" {
		s += fmt.Sprintf("#define SVN_PRE_RELEASE_VER %s\n", preRelVer)
	}
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 644)
	fatalIfErr(err)
}

func addZipFileMust(w *zip.Writer, path string) {
	fi, err := os.Stat(path)
	fatalIfErr(err)
	fih, err := zip.FileInfoHeader(fi)
	fatalIfErr(err)
	fih.Name = filepath.Base(path)
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

// createExeZipWithGoMust() is faster, createExeZipWithPigz() generates slightly
// smaller files
func createExeZipMust(dir string) {
	//createExeZipWithGoMust(dir)
	createExeZipWithPigz(dir)
}

func createPdbZipMust(dir string) {
	path := pj(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	fatalIfErr(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		addZipFileMust(w, pj(dir, file))
	}

	err = w.Close()
	fatalIfErr(err)
}

func createPdbLzsaMust(dir string) {
	args := []string{"SumatraPDF.pdb.lzsa"}
	args = append(args, pdbFiles...)
	curDir, err := os.Getwd()
	fatalIfErr(err)
	makeLzsaPath := pj(curDir, "bin", "MakeLZSA.exe")
	cmd := exec.Command(makeLzsaPath, args...)
	cmd.Dir = dir
	u.RunCmdLoggedMust(cmd)
}

func manifestPath() string {
	return filepath.Join("rel32", "manifest.txt")
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
	dirs := []string{"rel32", "rel64"}
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
	fatalIfErr(err)
}
func buildPreRelease() {
	// early exit if missing
	detectSigntoolPath()
	getCertPwd()

	s := fmt.Sprintf("buidling pre-release version %s", svnPreReleaseVer)
	defer makePrintDuration(s)()
	clean()

	verifyGitCleanMust()
	verifyOnMasterBranchMust()
	//verifyPreReleaseNotInS3Must(svnPreReleaseVer)

	verifyTranslationsMust()

	downloadPigzMust()

	setBuildConfig(gitSha1, svnPreReleaseVer)

	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	{
		cmd := exec.Command(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-no-MUPDF;PdfFilter;PdfPreview;Uninstaller;test_util`, `/p:Configuration=Release;Platform=Win32`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	dir := "rel32"
	runTestUtilMust(dir)
	{
		signMust(pj(dir, "SumatraPDF.exe"))
		signMust(pj(dir, "libmupdf.dll"))
		signMust(pj(dir, "PdfFilter.dll"))
		signMust(pj(dir, "PdfPreview.dll"))
		signMust(pj(dir, "SumatraPDF-no-MUPDF.exe"))
		signMust(pj(dir, "Uninstaller.exe"))
	}

	{
		cmd := exec.Command(msbuildPath, slnPath, "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
		u.RunCmdLoggedMust(cmd)
	}
	signMust(pj(dir, "Installer.exe"))

	{
		cmd := exec.Command(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-no-MUPDF;PdfFilter;PdfPreview;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
		u.RunCmdLoggedMust(cmd)
	}

	dir = "rel64"
	runTestUtilMust(dir)
	signMust(pj(dir, "SumatraPDF.exe"))
	signMust(pj(dir, "libmupdf.dll"))
	signMust(pj(dir, "SumatraPDF-no-MUPDF.exe"))
	signMust(pj(dir, "Uninstaller.exe"))
	signMust(pj("rel32", "PdfFilter.dll"))
	signMust(pj("rel32", "PdfPreview.dll"))

	{
		cmd := exec.Command(msbuildPath, slnPath, "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
		u.RunCmdLoggedMust(cmd)
	}
	signMust(pj("rel64", "Installer.exe"))

	createPdbZipMust("rel32")
	createPdbZipMust("rel64")

	createPdbLzsaMust("rel32")
	createPdbLzsaMust("rel64")

	createManifestMust()

	/*
		s3UploadPreReleaseMust(svnPreReleaseVer)
	*/
}
