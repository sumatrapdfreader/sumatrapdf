package main

import (
	"fmt"
	"path/filepath"
)

func buildPreRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypePreRel)
	s := fmt.Sprintf("buidling pre-release version %s", ver)
	defer makePrintDuration(s)()

	verifyGitCleanMust()
	verifyOnMasterBranchMust()
	verifyTranslationsMust()

	clean()
	setBuildConfigPreRelease()
	defer revertBuildConfig()

	build32()
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", ver)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)

	build64()
	nameInZip = fmt.Sprintf("SumatraPDF-prerel-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	buildRa64()
	nameInZip = fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", ver)
	createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-prerel")
	prefix := fmt.Sprintf("SumatraPDF-%s", sumatraVersion)
	copyBuiltFilesAll(dstDir, prefix)

	// note: manifest won't be for the right files but we don't care
	dstDir = filepath.Join("out", "final-ramicro")
	prefix = fmt.Sprintf("RAMicroPDFViewer-prerelease-%s", ver)
	copyBuiltFilesRa64(dstDir, prefix)
}
