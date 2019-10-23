package main

import (
	"fmt"
	"path/filepath"

	"github.com/kjk/u"
)

const (
	// https://help.github.com/en/github/automating-your-workflow-with-github-actions/software-in-virtual-environments-for-github-actions#visual-studio-2019-enterprise
	vsPathGitHub = `C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise`
	// because I'm poor
	vsPathLocal = `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community`
)

var (
	msbuilds = []string{
		`C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe`,
		`C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe`,
	}
	vsvars = []string{
		`C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat`,
		`C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat`,
	}
	signtools = []string{
		`C:\Program Files (x86)\Windows Kits\10\bin\10.0.14393.0\x64\signtool.exe`,
		`C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\signtool.exe`,
	}
)

var (
	cachedEnv []string
)

func detectPath(paths []string) string {
	for _, p := range paths {
		if u.FileExists(p) {
			return p
		}
	}
	panic(fmt.Sprintf("Didn't find %s", filepath.Base(paths[0])))
}

func detectMsbuildPath() string {
	return detectPath(msbuilds)
}

func detectSigntoolPath() string {
	return detectPath(signtools)
}

func getVs2019Env() []string {
	if len(cachedEnv) == 0 {
		path := detectPath(vsvars)
		cachedEnv = getEnvAfterScript(path)
		u.PanicIf(len(cachedEnv) == 0, "didn't find env from '%s'", path)
	}
	return cachedEnv
}
