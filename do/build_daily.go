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
	verifyOnMasterBranchMust()

	clean()
	setBuildConfigDaily()
	defer revertBuildConfig()

	build64()
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-daily")
	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	copyBuiltFiles64(dstDir, prefix)
}
