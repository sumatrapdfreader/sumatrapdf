package main

import (
	"fmt"
	"path/filepath"
)

func buildRaMicroPreRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypePreRel)
	s := fmt.Sprintf("buidling ramicro pre-release version %s", ver)
	defer makePrintDuration(s)()

	verifyGitCleanMust()
	verifyOnMasterBranchMust()
	//verifyTranslationsMust()

	clean()
	setBuildConfigPreRelease()
	defer revertBuildConfig()

	build(rel64RaDir, "Release", "x64_ramicro")
	nameInZip := fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", ver)
	createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)

	//createManifestMust()

	// note: manifest won't be for the right files but we don't care
	dstDir := filepath.Join("out", "final-ramicro")
	prefix := fmt.Sprintf("RAMicroPDFViewer-prerel-%s", ver)
	copyBuiltFiles(dstDir, rel64RaDir, prefix+"-64")
	//copyBuiltManifest(dstDir, prefix)
}
