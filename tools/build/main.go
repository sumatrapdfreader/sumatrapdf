package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"
)

const (
	s3PreRelDir  = "sumatrapdf/prerel/"
	s3RelDir     = "sumatrapdf/rel/"
	maxS3Results = 1000
)

func verifyReleaseNotInS3Must(ver string) {
	if !flgUpload {
		return
	}
	s3Path := s3RelDir + fmt.Sprintf("SumatraPDF-%s-manifest.txt", ver)
	fatalIf(s3Exists(s3Path), "build '%s' already exists in s3 because '%s' existst\n", ver, s3Path)
}

func buildAnalyze() {
	fmt.Printf("Analyze build\n")
	// I assume 64-bit build will catch more issues
	slnPath := filepath.Join(vsVer, "SumatraPDF.sln")
	out, _ := runMsbuildGetOutput(true, slnPath, "/t:Installer", "/p:Configuration=ReleasePrefast;Platform=x64", "/m")

	if true {
		err2 := ioutil.WriteFile("analyze-output.txt", out, 0644)
		fatalIfErr(err2)
	}
	//fatalIfErr(err)

	parseAnalyzeOutput(out)
}

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/rel/SumatraPDF-3.1-install.exe etc.
func s3UploadReleaseMust(ver string) {
	if !flgUpload {
		fmt.Printf("Skipping release upload to s3 because -upload flag not given\n")
		return
	}

	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	manifestRemotePath := s3RelDir + prefix + "-manifest.txt"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := s3UploadFiles(s3RelDir, "rel", files)
	fatalIfErr(err)

	prefix = fmt.Sprintf("SumatraPDF-%s-64", ver)
	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err = s3UploadFiles(s3RelDir, "rel64", files)
	fatalIfErr(err)

	// upload manifest last
	manifestLocalPath := pj("rel", "manifest.txt")
	err = s3UploadFileReader(manifestRemotePath, manifestLocalPath, true)
	fatalIfErr(err)

	// Note: not uploading auto-update version info. We update it separately,
	// a week or so after build is released, so that if there are serious issues,
	// we can create an update and less people will be affected
}
