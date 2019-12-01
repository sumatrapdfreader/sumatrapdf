package main

import (
	"flag"
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

	/*
		{
			cmd := exec.Command(premakePath, "vs2017")
			u.RunCmdLoggedMust(cmd)
		}
	*/

	{
		cmd := exec.Command(premakePath, "vs2019")
		u.RunCmdLoggedMust(cmd)
	}
}

func main() {
	u.CdUpDir("sumatrapdf")
	logf("Current directory: %s\n", u.CurrDirAbsMust())

	var (
		flgRegenPremake            bool
		flgCIBuild                 bool
		flgUploadCiBuild           bool
		flgBuildLzsa               bool
		flgBuildPreRelease         bool
		flgSmoke                   bool
		flgClangFormat             bool
		flgFormat                  bool
		flgWc                      bool
		flgDownloadTranslations    bool
		flgRegenerateTranslattions bool
		flgUploadTranslations      bool
		flgClean                   bool
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-release", false, "build pre-release")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3")
		flag.BoolVar(&flgClangFormat, "clang-format", false, "format source files with clang-format")
		flag.BoolVar(&flgClangFormat, "format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgDownloadTranslations, "trans-dl", false, "download translations and re-generate C code")
		flag.BoolVar(&flgRegenerateTranslattions, "trans-regen", false, "regenerate .cpp translations files from strings/translations.txt")
		flag.BoolVar(&flgUploadTranslations, "trans-ul", false, "upload translations to apptranslators.org if changed")
		flag.BoolVar(&flgClean, "clean", false, "clean the build")
		flag.Parse()
	}

	if flgWc {
		doLineCount()
		return
	}

	if flgClean {
		clean()
		return
	}

	if flgClangFormat || flgFormat {
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

	if flgRegenerateTranslattions {
		regenerateLangs()
		return
	}

	if flgUploadTranslations {
		uploadStringsIfChanged()
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

	if flgCIBuild {
		// ci build does the same thing as pre-release
		detectVersions()
		buildPreRelease()
		return
	}

	if flgUploadCiBuild {
		flgUpload = true
		detectVersions()
		s3UploadPreReleaseMust(svnPreReleaseVer)
		spacesUploadPreReleaseMust(svnPreReleaseVer)
		return
	}

	if flgBuildPreRelease {
		detectVersions()
		buildPreRelease()
		return
	}

	flag.Usage()
}
