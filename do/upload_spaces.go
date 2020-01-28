package main

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/u"
)

// we delete old daily and pre-release builds. This defines how many most recent
// builds to retain
const nBuildsToRetainPreRel = 16
const nBuildsToRetainDaily = 64
const nBuildsToRetaininMicro = 128

const (
	buildTypeDaily   = "daily"
	buildTypePreRel  = "prerel"
	buildTypeRel     = "rel"
	buildTypeRaMicro = "ramicro"
)

var (
	rel32Dir   = filepath.Join("out", "rel32")
	rel64Dir   = filepath.Join("out", "rel64")
	rel64RaDir = filepath.Join("out", "rel64ra")
)

func isValidBuildType(buildType string) bool {
	switch buildType {
	case buildTypeDaily, buildTypePreRel, buildTypeRel, buildTypeRaMicro:
		return true
	}
	return false
}

func getRemoteDir(buildType string) string {
	panicIf(!isValidBuildType(buildType), "invalid build type: '%s'", buildType)
	return "software/sumatrapdf/" + buildType + "/"
}

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

func getFileNamesWithPrefix(prefix string) []string {
	files := []string{
		"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix),
		"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix),
		"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix),
		"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix),
		"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix),
	}
	return files
}

func getRemotePaths(buildType string) []string {
	if buildType == buildTypePreRel {
		return []string{
			"software/sumatrapdf/sumatralatest.js",
			"software/sumatrapdf/sumpdf-prerelease-latest.txt",
			"software/sumatrapdf/sumpdf-prerelease-update.txt",
		}
	}

	if buildType == buildTypeDaily {
		return []string{
			"software/sumatrapdf/sumadaily.js",
			"software/sumatrapdf/sumpdf-daily-latest.txt",
			"software/sumatrapdf/sumpdf-daily-update.txt",
		}
	}

	if buildType == buildTypeRaMicro {
		return []string{
			"software/sumatrapdf/ramicrolatest.js",
			"software/sumatrapdf/ramicro-daily-latest.txt",
			"software/sumatrapdf/ramicro-daily-update.txt",
		}
	}

	if buildType == buildTypeRel {
		panic("NYI")
		// TODO: those are not the right paths
		/*
			return []string{
				"software/sumatrapdf/rel.js",
				"software/sumatrapdf/rel-latest.txt",
				"software/sumatrapdf/rel.txt",
			}
		*/
	}

	panicIf(true, "Unkonwn buildType='%s'", buildType)
	return nil
}

// https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func spacesUploadPreReleaseMust(buildType string) {
	if shouldSkipUpload() {
		return
	}
	if !hasSpacesCreds() {
		return
	}

	ver := getPreReleaseVer()
	remoteDir := getRemoteDir(buildType)

	c := newMinioClient()
	timeStart := time.Now()

	prefix := fmt.Sprintf("SumatraPDF-prerelease-%s", ver)
	manifestRemotePath := remoteDir + prefix + "-manifest.txt"

	// ra-micro only needs 64-bit builds
	if buildType == buildTypePreRel {
		files := getFileNamesWithPrefix(prefix)
		err := minioUploadFiles(c, remoteDir, rel32Dir, files)
		fatalIfErr(err)
	}

	dir64 := rel64Dir
	prefix = fmt.Sprintf("SumatraPDF-prerelease-%s-64", ver)
	if buildType == buildTypeRaMicro {
		// must match createSumatraLatestJs
		prefix = fmt.Sprintf("RAMicro-prerelease-%s-64", ver)
		dir64 = rel64RaDir
	}
	files := getFileNamesWithPrefix(prefix)
	err := minioUploadFiles(c, remoteDir, dir64, files)
	fatalIfErr(err)

	manifestLocalPath := filepath.Join(artifactsDir, "manifest.txt")
	err = c.UploadFilePublic(manifestRemotePath, manifestLocalPath)
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s' as '%s'\n", manifestLocalPath, manifestRemotePath)

	remotePaths := getRemotePaths(buildType)

	s := createSumatraLatestJs(buildType)
	remotePath := remotePaths[0]
	err = c.UploadDataPublic(remotePath, []byte(s))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)

	remotePath = remotePaths[1]
	err = c.UploadDataPublic(remotePath, []byte(ver))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)

	//don't set a Stable version for pre-release builds
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	remotePath = remotePaths[2]
	err = c.UploadDataPublic(remotePath, []byte(s))
	fatalIfErr(err)
	logf("Uploaded to spaces: '%s'\n", remotePath)

	logf("Uploaded the build to spaces in %s\n", time.Since(timeStart))
}

// "software/sumatrapdf/prerel/SumatraPDF-prerelease-11290-64-install.exe"
// =>
// 11290
func extractVersionFromName(s string) int {
	parts := strings.Split(s, "/")
	name := parts[len(parts)-1]
	name = strings.TrimPrefix(name, "SumatraPDF-prerelease-")

	name = strings.TrimPrefix(name, "RAMicro-prerelease-")

	// TODO: temporary, for old builds in s3
	name = strings.TrimPrefix(name, "SumatraPDF-prerelase-")
	name = strings.TrimPrefix(name, "manifest-")
	name = strings.TrimPrefix(name, "manifest")
	if name == "" {
		return 0
	}

	parts = strings.Split(name, "-")
	parts = strings.Split(parts[0], ".")
	verStr := parts[0]
	ver, err := strconv.Atoi(verStr)
	panicIf(err != nil, "extractVersionFromName: '%s'\n", s)
	return ver
}

type filesByVer struct {
	ver   int
	files []string
}

func groupFilesByVersion(files []string) []*filesByVer {
	m := map[int]*filesByVer{}
	for _, f := range files {
		ver := extractVersionFromName(f)
		i := m[ver]
		if i == nil {
			i = &filesByVer{
				ver: ver,
			}
			m[ver] = i
		}
		i.files = append(i.files, f)
	}
	res := []*filesByVer{}
	for _, v := range m {
		res = append(res, v)
	}
	sort.Slice(res, func(i, j int) bool {
		return res[i].ver > res[j].ver
	})
	return res
}

func minioDeleteOldBuildsPrefix(buildType string) {
	nBuildsToRetain := nBuildsToRetainDaily
	if buildType == buildTypePreRel {
		nBuildsToRetain = nBuildsToRetainPreRel
	}
	if buildType == buildTypeRaMicro {
		nBuildsToRetain = nBuildsToRetaininMicro
	}
	remoteDir := getRemoteDir(buildType)

	c := newMinioClient()
	files, err := c.ListRemoteFiles(remoteDir)
	must(err)
	fmt.Printf("%d minio files under '%s'\n", len(files), remoteDir)
	var keys []string
	for _, f := range files {
		keys = append(keys, f.Key)
		//fmt.Printf("key: %s\n", f.Key)
	}
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

func minioDeleteOldBuilds() {
	minioDeleteOldBuildsPrefix(buildTypePreRel)
	minioDeleteOldBuildsPrefix(buildTypeDaily)
	minioDeleteOldBuildsPrefix(buildTypeRaMicro)
}
