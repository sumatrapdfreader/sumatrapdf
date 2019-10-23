package main

import (
	"flag"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

var (
	flgNoCleanCheck bool
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
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.Parse()
	}

	if flgRegenPremake {
		regenPremake()
		return
	}

	if flgBuildLzsa {
		buildLzsa()
		return
	}

	if flgCIBuild {
		ciBuild()
		return
	}

	detectVersions()

	if flgBuildPreRelease {
		buildPreRelease()
		return
	}

	flag.Usage()
}
