package main

import (
	"fmt"
	"path/filepath"
)

func buildDaily() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeDaily)
	s := fmt.Sprintf("buidling daily version %s", ver)
	defer makePrintDuration(s)()

	verifyGitCleanMust()

	clean()
	setBuildConfigDaily()
	defer revertBuildConfig()

	build(rel64Dir, "Release", "x64")
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)
	createManifestMust()

	dstDir := filepath.Join("out", "final-daily")
	prefix := fmt.Sprintf("SumatraPDF-prerel-%s", ver)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}
