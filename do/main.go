package main

import (
	"flag"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

func regenPremake() {
	premakePath := filepath.Join("bin", "premake5.exe")
	{
		cmd := exec.Command(premakePath, "vs2017")
		u.RunCmdLoggedMust(cmd)
	}
	{
		cmd := exec.Command(premakePath, "vs2019")
		u.RunCmdLoggedMust(cmd)
	}
}

func clean() {
	os.RemoveAll("rel32")
	os.RemoveAll("rel64")
	os.RemoveAll("dbg32")
	os.RemoveAll("dbg64")
}

func buildLzsa() {
	//getCertPwd() // early exit if missing
	defer makePrintDuration("buildLzsa")()
	clean()

	msbuildPath := detectMsbuildPath()
	cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:MakeLZSA`, `/p:Configuration=Release;Platform=Win32`, `/m`)
	u.RunCmdMust(cmd)

	path := filepath.Join("rel32", "MakeLZSA.exe")
	signMust(path)
}

// the things we do on GitHub Actions CI
func ciBuild() {
	//getCertPwd() // early exit if missing
	defer makePrintDuration("ciBuild")()
	clean()

	lzsa, err := filepath.Abs(filepath.Join("bin", "MakeLZSA.exe"))
	must(err)
	u.PanicIf(!u.FileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPath()
	{
		cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:all;Installer`, `/p:Configuration=Release;Platform=Win32`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:SumatraPDF;Installer;test_util`, `/p:Configuration=Release;Platform=x64`, `/m`)
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command("test_util.exe")
		cmd.Dir = "rel32"
		u.RunCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command("test_util.exe")
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
	// TODO: finish me
	defer makePrintDuration("buildPreRelease")()
	clean()
}

func main() {
	u.CdUpDir("sumatrapdf")
	logf("Current directory: %s\n", u.CurrDirAbsMust())

	if false {
		findSigntool()
		//listExeFiles(vsPathLocal)
		//listExeFiles(vsPathGitHub)
		return
	}

	var (
		flgRegenPremake    bool
		flgCIBuild         bool
		flgBuildLzsa       bool
		flgBuildPreRelease bool
	)

	{
		flag.BoolVar(&flgRegenPremake, "regen-premake", false, "regenerage premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-release", false, "build pre-release")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.Parse()
	}

	if flgRegenPremake {
		regenPremake()
		return
	}

	if flgBuildPreRelease {
		buildPreRelease()
		return
	}

	if flgCIBuild {
		ciBuild()
		return
	}

	if flgBuildLzsa {
		buildLzsa()
		return
	}

	flag.Usage()

	findSigntool()
}
