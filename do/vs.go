package do

import (
	"fmt"
	"os/exec"
	"path/filepath"
)

var (
	// from ls "c:\Program Files (x86)\Windows Kits\10\bin"
	// TODO: those get constantly updated, need to scan the dir
	// and pick the latest
	sdkVersions = []string{
		"10.0.26100.0",
		"10.0.22621.0",
		"10.0.22000.0",
		"10.0.20348.0",
		"10.0.19041.0",
		"10.0.18362.0",
		"10.0.17134.0",
		"10.0.16299.0",
		"10.0.15063.0",
		"10.0.14393.0",
	}

	vsBasePaths = []string{
		// https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md
		`C:\Program Files\Microsoft Visual Studio\2022\Enterprise`,
		`C:\Program Files\Microsoft Visual Studio\2022\Preview`,
		`C:\Program Files\Microsoft Visual Studio\2022\Community`,
		`C:\Program Files\Microsoft Visual Studio\2022\Professional`,
	}
)

func detectPathMust(paths []string, name string) string {
	if path, ok := checkAvailbleInPATH(name); ok {
		return path
	}
	for _, path := range paths {
		p := filepath.Join(path, name)
		if fileExists(p) {
			return p
		}
	}
	logf("Didn't find '%s' in vs paths\n", name)
	panic("didn't find " + name)
}

// if can execute without an error, then is in path
func checkAvailbleInPATH(path string) (string, bool) {
	name := filepath.Base(path)
	cmd := exec.Command(name)
	_, err := cmd.CombinedOutput()
	if err != nil {
		logf("%s NOT in PATH (from '%s')\n", name, path)
		return "", false
	}
	logf("%s found in PATH (from '%s')\n", name, path)
	return name, true
}

func detectPathInSDKMust(name string) string {
	if path, ok := checkAvailbleInPATH(name); ok {
		return path
	}
	for _, sdkVer := range sdkVersions {
		path := filepath.Join(`C:\Program Files (x86)\Windows Kits\10\bin`, sdkVer, name)
		if fileExists(path) {
			return path
		}
	}
	panic(fmt.Sprintf("Didn't find %s", name))
}

var didPrintMsbuildPath bool

func detectMsbuildPathMust() string {
	msBuildName := `MSBuild\Current\Bin\MSBuild.exe`
	if path, ok := checkAvailbleInPATH(msBuildName); ok {
		logf("%s found in PATH\n", path)
		return path
	}
	path := detectPathMust(vsBasePaths, msBuildName)
	panicIf(path == "", fmt.Sprintf("didn't find %s", msBuildName))
	if !didPrintMsbuildPath {
		logf("msbuild.exe: %s\n", path)
		didPrintMsbuildPath = true
	}
	return path
}

func detectSigntoolPathMust() string {
	return detectPathInSDKMust(filepath.Join(`x64`, `signtool.exe`))
}
