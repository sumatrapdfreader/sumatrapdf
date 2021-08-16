package main

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"text/template"
	"time"
)

const (
	// s3RelDir     = "sumatrapdf/rel/"
	maxS3Results = 1000
)

// we should only sign and upload to s3 if this is my repo and a push event
// or building locally
// don't sign if it's a fork or pull requests
func isGithubMyMasterBranch() bool {
	// https://help.github.com/en/actions/automating-your-workflow-with-github-actions/using-environment-variables
	repo := os.Getenv("GITHUB_REPOSITORY")
	if repo != "sumatrapdfreader/sumatrapdf" {
		return false
	}
	ref := os.Getenv("GITHUB_REF")
	if ref != "refs/heads/master" {
		logf("GITHUB_REF: '%s'\n", ref)
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

// list is sorted by Version, biggest first, to make it easy to delete oldest
func s3ListPreReleaseFilesMust(c *S3Client, prefix string) []string {
	bucket := c.GetBucket()
	resp, err := bucket.List(prefix, "", "", maxS3Results)
	must(err)
	//fatalIf(resp.IsTruncated, "truncated response! implement reading all the files\n")
	var res []string
	for _, key := range resp.Contents {
		// fmt.Printf("%s\n", key.Key)
		res = append(res, key.Key)
	}
	return res
}

// only check one file we know will be uploaded
func verifyBuildNotInS3ShortMust(buildType string) {
	dirRemote := getRemoteDir(buildType)
	ver := getVerForBuildType(buildType)
	fname := fmt.Sprintf("SumatraPDF-prerelease-%s-manifest.txt", ver)
	remotePath := path.Join(dirRemote, fname)
	c := newS3Client()
	fatalIf(c.Exists(remotePath), "build of type '%s' for ver '%s' already exists in s3 because file '%s' exists\n", buildType, ver, remotePath)
}

// we shouldn't re-upload files. We upload manifest-${ver}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func verifyBuildNotInS3Must(c *S3Client, buildType string) {
	dirRemote := getRemoteDir(buildType)
	dirLocal := getFinalDirForBuildType(buildType)
	files, err := ioutil.ReadDir(dirLocal)
	must(err)
	for _, f := range files {
		fname := f.Name()
		remotePath := path.Join(dirRemote, fname)
		fatalIf(c.Exists(remotePath), "build from dir %s already exists in s3 because file '%s' exists\n", dirLocal, remotePath)
	}
}

func newS3Client() *S3Client {
	c := &S3Client{
		Access: os.Getenv("AWS_ACCESS"),
		Secret: os.Getenv("AWS_SECRET"),
		Bucket: "kjkpub",
	}
	return c
}

func hasS3Creds() bool {
	if os.Getenv("AWS_ACCESS") == "" {
		logf("Not uploading to s3 because AWS_ACCESS env variable not set\n")
		return false
	}
	if os.Getenv("AWS_SECRET") == "" {
		logf("Not uploading to s3 because AWS_SECRET env variable not set\n")
		return false
	}
	return true
}

func s3UploadDir(c *S3Client, dirRemote string, dirLocal string) error {
	files, err := ioutil.ReadDir(dirLocal)
	must(err)
	for _, f := range files {
		fname := f.Name()
		pathLocal := filepath.Join(dirLocal, fname)
		pathRemote := path.Join(dirRemote, fname)
		err := c.UploadFileReader(pathRemote, pathLocal, true)
		if err != nil {
			return fmt.Errorf("failed s3 upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
	}
	return nil
}

func s3UploadFilePublic(c *S3Client, dstRemotePath string, srcPath string) error {
	return c.UploadFileReader(dstRemotePath, srcPath, true)
}

func getFinalDirForBuildType(buildType string) string {
	var dir string
	switch buildType {
	case buildTypeRel:
		dir = "final-rel"
	case buildTypePreRel:
		dir = "final-prerel"
	default:
		panicIf(true, "invalid buildType '%s'", buildType)
	}
	return filepath.Join("out", dir)
}

// this returns version to be used in uploaded file names
func getVerForBuildType(buildType string) string {
	switch buildType {
	case buildTypePreRel, buildTypeDaily:
		// this is linear build number like "12223"
		return getPreReleaseVer()
	case buildTypeRel:
		// this is program version like "3.2"
		return sumatraVersion
	}
	panicIf(true, "invalid buildType '%s'", buildType)
	return ""
}

// upload as:
// https://kjkpub.s3.amazonaws.com/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func s3UploadBuildMust(buildType string) {
	timeStart := time.Now()
	defer func() {
		logf("Uploaded the build to s3 in %s\n", time.Since(timeStart))
	}()
	c := newS3Client()
	c.VerifyHasSecrets()

	dirRemote := getRemoteDir(buildType)
	dirLocal := getFinalDirForBuildType(buildType)
	verifyBuildNotInS3Must(c, buildType)

	err := s3UploadDir(c, dirRemote, dirLocal)
	must(err)

	// for release build we don't upload files with version info
	if buildType == buildTypeRel {
		return
	}

	s3UploadBuildUpdateInfoMust := func(buildType string) {
		files := getVersionFilesForLatestInfo("s3", buildType)
		for _, f := range files {
			remotePath := f[0]
			err := c.UploadString(remotePath, f[1], true)
			must(err)
			logf("Uploaded to s3: '%s'\n", remotePath)
		}
	}

	s3UploadBuildUpdateInfoMust(buildType)
	// TODO: for now, we also update daily version
	// to get people to switch to pre-release
	if buildType == buildTypePreRel {
		s3UploadBuildUpdateInfoMust(buildTypeDaily)
	}
}

func s3DeleteOldBuildsPrefix(buildType string) {
	panicIf(buildType == buildTypeRel, "can't delete release builds")
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
	// TODO: we can remove them completely
	//s3DeleteOldBuildsPrefix(buildTypeDaily)
}
