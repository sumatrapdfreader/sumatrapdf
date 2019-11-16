package main

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/kjk/u"
)

func newMinioClient() *u.MinioClient {
	res := &u.MinioClient{
		StorageKey:    os.Getenv("SPACES_KEY"),
		StorageSecret: os.Getenv("SPACES_SECRET"),
		Bucket:        "kjkpubsf",
		Endpoint:      "sfo2.digitaloceanspaces.com",
	}
	res.EnsureConfigured()
	return res
}

func hasSpacesCreds() bool {
	if os.Getenv("SPACES_KEY") == "" {
		logf("Not uploading to do spaces because SPACES_KEY env variable not set\n")
		return false
	}
	if os.Getenv("SPACES_SECRET") == "" {
		logf("Not uploading to do spaces because SPACES_SECRET env variable not set\n")
		return false
	}
	return true
}

func minioUploadFiles(c *u.MinioClient, prefix string, dir string, files []string) error {
	n := len(files) / 2
	for i := 0; i < n; i++ {
		pathLocal := filepath.Join(dir, files[2*i])
		pathRemote := prefix + files[2*i+1]
		err := c.UploadFilePublic(pathRemote, pathLocal)
		if err != nil {
			return fmt.Errorf("failed to upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
		logf("Uploaded to spaces: '%s' as '%s'\n", pathLocal, pathRemote)
	}
	return nil
}

// upload as:
// https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func spacesUploadPreReleaseMust(ver string) {
	if shouldSkipUpload() {
		return
	}
	if !hasSpacesCreds() {
		return
	}
	c := newMinioClient()
	timeStart := time.Now()
	preRelDir := "software/sumatrapdf/prerel/"
	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	manifestRemotePath := preRelDir + prefix + "-manifest.txt"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := minioUploadFiles(c, preRelDir, filepath.Join("out", "rel32"), files)
	fatalIfErr(err)

	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s-64", ver)
	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"Installer.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}

	err = minioUploadFiles(c, preRelDir, filepath.Join("out", "rel64"), files)
	fatalIfErr(err)

	manifestLocalPath := filepath.Join(artifactsDir, "manifest.txt")
	err = c.UploadFilePublic(manifestRemotePath, manifestLocalPath)
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s' as '%s'\n", manifestLocalPath, manifestRemotePath)

	minioUploadDailyInfo(c, ver)

	logf("Uploaded the build to spaces in %s\n", time.Since(timeStart))
}

func minioUploadDailyInfo(c *u.MinioClient, ver string) {
	s := createSumatraLatestJs()
	remotePath := "software/sumatrapdf/sumadaily.js"
	err := c.UploadDataPublic(remotePath, []byte(s))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)

	//sumatrapdf/sumpdf-prerelease-latest.txt
	remotePath = "software/sumatrapdf/sumpdf-daily-latest.txt"
	err = c.UploadDataPublic(remotePath, []byte(ver))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)

	//sumatrapdf/sumpdf-prerelease-update.txt
	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	remotePath = "software/sumatrapdf/sumpdf-daily-update.txt"
	err = c.UploadDataPublic(remotePath, []byte(s))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)
}

func minioSetAsPreRelease(c *u.MinioClient, ver string) {
	s := createSumatraLatestJs()
	err := c.UploadDataPublic("sumatrapdf/sumatralatest.js", []byte(s))
	fatalIfErr(err)

	//sumatrapdf/sumpdf-prerelease-latest.txt
	err = c.UploadDataPublic("sumatrapdf/sumpdf-prerelease-latest.txt", []byte(ver))
	fatalIfErr(err)

	//sumatrapdf/sumpdf-prerelease-update.txt
	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	err = c.UploadDataPublic("sumatrapdf/sumpdf-prerelease-update.txt", []byte(s))
	fatalIfErr(err)
}
