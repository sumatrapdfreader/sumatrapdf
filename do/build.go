package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/kjk/u"
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

	{
		cmd := exec.Command(`.\test_util.exe`)
		cmd.Dir = "rel32"
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:SumatraPDF;Installer;test_util`, `/p:Configuration=Release;Platform=x64`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command(`.\test_util.exe`)
		cmd.Dir = "rel64"
		u.RunCmdLoggedMust(cmd)
	}

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

	// verifyTranslationsMust()

	/*

		downloadPigzMust()

		setBuildConfig(gitSha1, svnPreReleaseVer)
		slnPath := filepath.Join(vsVer, "SumatraPDF.sln")
		err = runMsbuild(true, slnPath, "/t:SumatraPDF;SumatraPDF-no-MUPDF;PdfFilter;PdfPreview;Uninstaller;test_util", "/p:Configuration=Release;Platform=Win32", "/m")
		fatalIfErr(err)
		runTestUtilMust("rel")
		signMust(pj("rel", "SumatraPDF.exe"))
		signMust(pj("rel", "libmupdf.dll"))
		signMust(pj("rel", "PdfFilter.dll"))
		signMust(pj("rel", "PdfPreview.dll"))
		signMust(pj("rel", "SumatraPDF-no-MUPDF.exe"))
		signMust(pj("rel", "Uninstaller.exe"))
		err = runMsbuild(true, slnPath, "/t:Installer", "/p:Configuration=Release;Platform=Win32", "/m")
		fatalIfErr(err)
		signMust(pj("rel", "Installer.exe"))

		err = runMsbuild(true, slnPath, "/t:SumatraPDF;SumatraPDF-no-MUPDF;PdfFilter;PdfPreview;Uninstaller;test_util", "/p:Configuration=Release;Platform=x64", "/m")
		fatalIfErr(err)

		if isOS64Bit() {
			runTestUtilMust("rel64")
		}
		signMust(pj("rel64", "SumatraPDF.exe"))
		signMust(pj("rel64", "libmupdf.dll"))
		signMust(pj("rel", "PdfFilter.dll"))
		signMust(pj("rel", "PdfPreview.dll"))
		signMust(pj("rel64", "SumatraPDF-no-MUPDF.exe"))
		signMust(pj("rel64", "Uninstaller.exe"))
		err = runMsbuild(true, slnPath, "/t:Installer", "/p:Configuration=Release;Platform=x64", "/m")
		fatalIfErr(err)
		signMust(pj("rel64", "Installer.exe"))

		createPdbZipMust("rel")
		createPdbZipMust("rel64")

		createPdbLzsaMust("rel")
		createPdbLzsaMust("rel64")

		createManifestMust()
		s3UploadPreReleaseMust(svnPreReleaseVer)
	*/
}
