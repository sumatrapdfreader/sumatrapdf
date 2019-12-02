package main

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"text/template"
	"time"
)

const (
	s3RelDir     = "sumatrapdf/rel/"
	maxS3Results = 1000
)

var (
	s3PreRelDir = "sumatrapdf/prerel/"
)

// we should only sign and upload to s3 if this is my repo
// and a push event
func shouldSignOrUpload() bool {
	// https://help.github.com/en/actions/automating-your-workflow-with-github-actions/using-environment-variables

	repo := os.Getenv("GITHUB_REPOSITORY")
	if repo != "sumatrapdfreader/sumatrapdf" {
		return false
	}
	event := os.Getenv("GITHUB_EVENT_NAME")
	// other event is "pull_request"
	return event == "push"
}

func execTextTemplate(tmplText string, data interface{}) string {
	tmpl, err := template.New("").Parse(tmplText)
	must(err)
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, data)
	must(err)
	return buf.String()
}

// sumatrapdf/sumatralatest.js
/*
var sumLatestVer = 10175;
var sumBuiltOn = "2015-07-23";
var sumLatestName = "SumatraPDF-prerelease-10175.exe";
var sumLatestExe = "https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175.exe";
var sumLatestPdb = "https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175.pdb.zip";
var sumLatestInstaller = "https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-10175-install.exe";
*/
func createSumatraLatestJs(dir string) string {
	currDate := time.Now().Format("2006-01-02")
	v := svnPreReleaseVer
	tmplText := `
		var sumLatestVer = {{.Ver}};
		var sumBuiltOn = "{{.CurrDate}}";
		var sumLatestName = "SumatraPDF-prerelease-{{.Ver}}.exe";

		var sumLatestExe = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}.exe";
		var sumLatestPdb = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}.pdb.zip";
		var sumLatestInstaller = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}-install.exe";

		var sumLatestExe64 = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}-64.exe";
		var sumLatestPdb64 = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}-64.pdb.zip";
		var sumLatestInstaller64 = "https://kjkpub.s3.amazonaws.com/sumatrapdf/{{.Dir}}/SumatraPDF-prerelease-{{.Ver}}-64-install.exe";
`
	d := map[string]interface{}{
		"Ver":      v,
		"Dir":      dir,
		"CurrDate": currDate,
	}
	return execTextTemplate(tmplText, d)
}

// list is sorted by Version, biggest first, to make it easy to delete oldest
func s3ListPreReleaseFilesMust(c *S3Client, prefix string) []string {
	bucket := c.GetBucket()
	resp, err := bucket.List(s3PreRelDir, "", "", maxS3Results)
	fatalIfErr(err)
	//fatalIf(resp.IsTruncated, "truncated response! implement reading all the files\n")
	var res []string
	for _, key := range resp.Contents {
		// fmt.Printf("%s\n", key.Key)
		res = append(res, key.Key)
	}
	return res
}

// we shouldn't re-upload files. We upload manifest-${ver}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func verifyPreReleaseNotInS3Must(c *S3Client, ver string) {
	if !flgUpload {
		return
	}
	s3Path := s3PreRelDir + fmt.Sprintf("SumatraPDF-prerelease-%s-manifest.txt", ver)
	fatalIf(c.Exists(s3Path), "build %s already exists in s3 because '%s' exists\n", ver, s3Path)
}

func verifyReleaseNotInS3Must(c *S3Client, ver string) {
	if !flgUpload {
		return
	}
	s3Path := s3RelDir + fmt.Sprintf("SumatraPDF-%s-manifest.txt", ver)
	fatalIf(c.Exists(s3Path), "build '%s' already exists in s3 because '%s' existst\n", ver, s3Path)
}

func dumpEnv() {
	env := os.Environ()
	logf("\nEnv:\n")
	for _, s := range env {
		logf("env: %s\n", s)
	}
	logf("\n")
}

func isMaster() bool {
	ref := os.Getenv("GITHUB_REF")
	return ref == "refs/heads/master"
}

func shouldSkipUpload() bool {
	if flgUpload {
		return false
	}
	logf("shouldSkipUpload: -upload flag not given\n")

	if !isMaster() {
		logf("Skipping pre-release upload to s3 because not on master branch\n")
		logf("GITHUB_REF: '%s'\n", os.Getenv("GITHUB_REF"))
		flgUpload = false
		return true
	}

	if !shouldSignOrUpload() {
		logf("skipping upload beacuse not my repo\n")
		flgUpload = false
		return true
	}
	return false
}

func newS3Client() *S3Client {
	c := &S3Client{
		Access: os.Getenv("AWS_ACCESS"),
		Secret: os.Getenv("AWS_SECRET"),
		Bucket: "kjkpub",
	}
	return c
}

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func s3UploadPreReleaseMust(ver string, dir string) {
	if shouldSkipUpload() {
		return
	}

	s3PreRelDir = "sumtrapdf/" + dir + "/"

	c := newS3Client()
	//dumpEnv()
	c.VerifyHasSecrets()

	timeStart := time.Now()

	verifyPreReleaseNotInS3Must(c, svnPreReleaseVer)

	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	manifestRemotePath := s3PreRelDir + prefix + "-manifest.txt"
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err := c.UploadFiles(s3PreRelDir, filepath.Join("out", "rel32"), files)
	fatalIfErr(err)

	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s-64", ver)
	files = []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	err = c.UploadFiles(s3PreRelDir, filepath.Join("out", "rel64"), files)
	fatalIfErr(err)

	manifestLocalPath := filepath.Join(artifactsDir, "manifest.txt")
	err = c.UploadFileReader(manifestRemotePath, manifestLocalPath, true)
	fatalIfErr(err)

	s3UploadDailyInfo(c, ver, dir)

	logf("Uploaded the build to s3 in %s\n", time.Since(timeStart))
}

func s3UploadDailyInfo(c *S3Client, ver string, dir string) {
	s := createSumatraLatestJs(dir)
	err := c.UploadString("sumatrapdf/sumadaily.js", s, true)
	fatalIfErr(err)

	//sumatrapdf/sumpdf-prerelease-latest.txt
	err = c.UploadString("sumatrapdf/sumpdf-daily-latest.txt", ver, true)
	fatalIfErr(err)

	//sumatrapdf/sumpdf-prerelease-update.txt
	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	err = c.UploadString("sumatrapdf/sumpdf-daily-update.txt", s, true)
	fatalIfErr(err)
}

func s3DeleteOldBuilkdsPrefix(prefix string) {
	c := newS3Client()

	keys := s3ListPreReleaseFilesMust(c, prefix)
	fmt.Printf("%d s3 files under '%s'\n", len(keys), prefix)
	byVer := groupFilesByVersion(keys)
	for i, v := range byVer {
		deleting := (i >= nBuildsToRetain)
		if deleting {
			fmt.Printf("%d, deleting\n", v.ver)
			for _, fn := range v.files {
				fmt.Printf("  %s deleting\n", fn)
				err := c.Delete(fn)
				must(err)
			}
		} else {
			fmt.Printf("%d, not deleting\n", v.ver)
			// for _, fn := range v.files {
			// 	fmt.Printf("  %s not deleting\n", fn)
			// }
		}
	}
}

func s3DeleteOldBuilds() {
	s3DeleteOldBuilkdsPrefix("software/sumatrapdf/prerel/")
	s3DeleteOldBuilkdsPrefix("software/sumatrapdf/daily/")
}
