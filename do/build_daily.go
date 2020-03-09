package main

import (
	"fmt"
	"path/filepath"
)

func buildDaily() {
	// early exit if missing
	detectSigntoolPath()

	ver := getPreReleaseVer()
	s := fmt.Sprintf("buidling daily version %s", ver)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnMasterBranchMust()

	clean()

	setBuildConfigDaily()
	defer revertBuildConfig()

	build64()
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-daily")
	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	copyBuiltFiles64(dstDir, prefix)
}
