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
	// s3RelDir     = "sumatrapdf/rel/"
	maxS3Results = 1000
)

// we should only sign and upload to s3 if this is my repo
// and a push event
func shouldSignAndUpload() bool {
	// https://help.github.com/en/actions/automating-your-workflow-with-github-actions/using-environment-variables

	repo := os.Getenv("GITHUB_REPOSITORY")
	if repo != "sumatrapdfreader/sumatrapdf" {
		return false
	}
	event := os.Getenv("GITHUB_EVENT_NAME")
	// other event is "pull_request"
	return event == "push" || event == "repository_dispatch"
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
func createSumatraLatestJs(buildType string) string {
	appName := "SumatraPDF-prerelease"
	if buildType == buildTypeRaMicro {
		// must match name in spacesUploadPreReleaseMust
		appName = "RAMicro-prerelease"
	}
	currDate := time.Now().Format("2006-01-02")
	v := getPreReleaseVer()
	tmplText := `
var sumLatestVer = {{.Ver}};
var sumBuiltOn = "{{.CurrDate}}";
var sumLatestName = "{{.AppName}}-{{.Ver}}.exe";

var sumLatestExe         = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}.exe";
var sumLatestExeZip      = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}.zip";
var sumLatestPdb         = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}.pdb.zip";
var sumLatestInstaller   = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}-install.exe";

var sumLatestExe64       = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}-64.exe";
var sumLatestExeZip64    = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}-64.zip";
var sumLatestPdb64       = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}-64.pdb.zip";
var sumLatestInstaller64 = "{{.Host}}{{.Dir}}/{{.AppName}}-{{.Ver}}-64-install.exe";
`
	d := map[string]interface{}{
		"Host":     "https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/",
		"Ver":      v,
		"Dir":      buildType,
		"CurrDate": currDate,
		"AppName":  appName,
	}
	return execTextTemplate(tmplText, d)
}

// list is sorted by Version, biggest first, to make it easy to delete oldest
func s3ListPreReleaseFilesMust(c *S3Client, prefix string) []string {
	bucket := c.GetBucket()
	resp, err := bucket.List(prefix, "", "", maxS3Results)
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
func verifyPreReleaseNotInS3Must(c *S3Client, remoteDir string, ver string) {
	if !flgUpload {
		return
	}
	s3Path := remoteDir + fmt.Sprintf("SumatraPDF-prerelease-%s-manifest.txt", ver)
	fatalIf(c.Exists(s3Path), "build %s already exists in s3 because '%s' exists\n", ver, s3Path)
}

func verifyReleaseNotInS3Must(c *S3Client, remoteDir string, ver string) {
	if !flgUpload {
		return
	}
	s3Path := remoteDir + fmt.Sprintf("SumatraPDF-%s-manifest.txt", ver)
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
		return true
	}

	if !shouldSignAndUpload() {
		logf("skipping upload beacuse not my repo\n")
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

func s3UploadFiles(c *S3Client, s3Dir string, dir string, files []string) error {
	n := len(files) / 2
	for i := 0; i < n; i++ {
		pathLocal := filepath.Join(dir, files[2*i])
		pathRemote := files[2*i+1]
		err := c.UploadFileReader(s3Dir+pathRemote, pathLocal, true)
		if err != nil {
			return fmt.Errorf("failed to upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
		logf("Uploaded to s3: '%s' as '%s'\n", pathLocal, pathRemote)
	}
	return nil
}

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func s3UploadPreReleaseMust(buildType string) {
	if shouldSkipUpload() {
		return
	}

	ver := getPreReleaseVer()
	isDaily := buildType == buildTypeDaily
	panicIf(buildType == buildTypeRaMicro, "only uploading ramicro to spaces")

	remoteDir := getRemoteDir(buildType)

	c := newS3Client()
	c.VerifyHasSecrets()

	timeStart := time.Now()

	verifyPreReleaseNotInS3Must(c, remoteDir, ver)

	var prefix string
	var files []string
	var err error

	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	manifestRemotePath := remoteDir + prefix + "-manifest.txt"

	if !isDaily {
		files = getFileNamesWithPrefix(prefix)
		err = s3UploadFiles(c, remoteDir, filepath.Join("out", "rel32"), files)
		fatalIfErr(err)
	}

	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s-64", ver)
	files = getFileNamesWithPrefix(prefix)
	err = s3UploadFiles(c, remoteDir, filepath.Join("out", "rel64"), files)
	fatalIfErr(err)

	manifestLocalPath := filepath.Join(artifactsDir, "manifest.txt")
	err = c.UploadFileReader(manifestRemotePath, manifestLocalPath, true)
	fatalIfErr(err)
	logf("Uploaded to s3: '%s' as '%s'\n", manifestLocalPath, manifestRemotePath)

	remotePaths := []string{
		"software/sumatrapdf/sumatralatest.js",
		"software/sumatrapdf/sumpdf-prerelease-latest.txt",
		"software/sumatrapdf/sumpdf-prerelease-update.txt",
	}
	if buildType == buildTypeDaily {
		remotePaths = []string{
			"software/sumatrapdf/sumadaily.js",
			"software/sumatrapdf/sumpdf-daily-latest.txt",
			"software/sumatrapdf/sumpdf-daily-update.txt",
		}
	}

	s := createSumatraLatestJs(buildType)
	remotePath := remotePaths[0]
	err = c.UploadString(remotePath, s, true)
	fatalIfErr(err)
	logf("Uploaded to s3: '%s'\n", remotePath)

	remotePath = remotePaths[1]
	err = c.UploadString(remotePath, ver, true)
	fatalIfErr(err)
	logf("Uploaded to s3: '%s'\n", remotePath)

	// don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	remotePath = remotePaths[2]
	err = c.UploadString(remotePath, s, true)
	fatalIfErr(err)
	logf("Uploaded to s3: '%s'\n", remotePath)

	logf("Uploaded the build to s3 in %s\n", time.Since(timeStart))
}

func s3DeleteOldBuildsPrefix(buildType string) {
	c := newS3Client()

	nBuildsToRetain := nBuildsToRetainDaily
	if buildType == buildTypePreRel {
		nBuildsToRetain = nBuildsToRetainPreRel
	}
	remoteDir := getRemoteDir(buildType)

	keys := s3ListPreReleaseFilesMust(c, remoteDir)
	fmt.Printf("%d s3 files under '%s'\n", len(keys), remoteDir)
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
			//fmt.Printf("%d, not deleting\n", v.ver)
			// for _, fn := range v.files {
			// 	fmt.Printf("  %s not deleting\n", fn)
			// }
		}
	}
}

func s3DeleteOldBuilds() {
	s3DeleteOldBuildsPrefix(buildTypePreRel)
	s3DeleteOldBuildsPrefix(buildTypeDaily)
}
