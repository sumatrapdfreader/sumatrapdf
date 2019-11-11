package main

import (
	"flag"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

var (
	flgNoCleanCheck bool
	flgUpload       bool
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

func main() {
	u.CdUpDir("sumatrapdf")
	logf("Current directory: %s\n", u.CurrDirAbsMust())

	if false {
		testEscape()
		os.Exit(0)
	}

	if false {
		findSigntool()
		//listExeFiles(vsPathLocal)
		//listExeFiles(vsPathGitHub)
		return
	}

	if false {
		cmd := exec.Command(".\\test_util.exe")
		cmd.Dir = "rel32"
		u.RunCmdLoggedMust(cmd)
		return
	}

	var (
		flgRegenPremake         bool
		flgCIBuild              bool
		flgBuildLzsa            bool
		flgBuildPreRelease      bool
		flgSmoke                bool
		flgClangFormat          bool
		flgWc                   bool
		flgDownloadTranslations bool
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-release", false, "build pre-release")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3")
		flag.BoolVar(&flgClangFormat, "clang-format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgDownloadTranslations, "trans-dl", false, "download translations and re-generate C code")
		flag.Parse()
	}

	if flgWc {
		doLineCount()
		return
	}

	if flgClangFormat {
		clangFormatFiles()
		return
	}

	if flgRegenPremake {
		regenPremake()
		return
	}

	if flgDownloadTranslations {
		downloadTranslationsMain()
		return
	}

	if flgBuildLzsa {
		buildLzsa()
		return
	}

	if flgSmoke {
		smokeBuild()
		return
	}

	detectVersions()

	if flgCIBuild {
		// ci build does the same thing as pre-release
		buildPreRelease()
		return
	}

	if flgBuildPreRelease {
		buildPreRelease()
		return
	}

	flag.Usage()
}
