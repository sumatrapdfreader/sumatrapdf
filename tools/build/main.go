package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

/*
To run:
* install Go
 - download and run latest installer http://golang.org/doc/install
 - restart so that PATH changes take place
 - set GOPATH env variable (e.g. to %USERPROFILE%\src\go)
* go run .\tools\buildgo\main.go

Some notes on the insanity that is setting up command-line build for both
32 and 64 bit executables.

Useful references:
https://msdn.microsoft.com/en-us/library/f2ccy3wt.aspx
https://msdn.microsoft.com/en-us/library/x4d2c09s.aspx
http://www.sqlite.org/src/artifact/60dbf6021d3de0a9 -sqlite's win build script

%VS140COMNTOOLS%\vsvars32.bat is how set basic env for 32bit builds for VS 2015
(it's VS120COMNTOOLS for VS 2013).

That sets VSINSTALLDIR env variable which we can use to setup both 32bit and
64bit builds:
%VCINSTALLDIR%\vcvarsall.bat x86_amd64 : 64bit
%VCINSTALLDIR%\vcvarsall.bat x86 : 32bit

If the OS is 64bit, there are also 64bit compilers that can be selected with:
amd64 (for 64bit builds) and amd64_x86 (for 32bit builds). They generate
the exact same code but can compiler bigger programs (can use more memory).

I'm guessing %VS140COMNTOOLS%\vsvars32.bat is the same as %VSINSTALLDIR%\vcvarsall.bat x86.
*/

// Platform represents a 32bit vs 64bit platform
type Platform int

const (
	Platform32Bit Platform = 1
	Platform64Bit Platform = 2
)

// EnvVar describes an environment variable
type EnvVar struct {
	Name string
	Val  string
}

var (
	cachedVcInstallDir string
)

func (p Platform) is64() bool {
	return p == Platform64Bit
}

func (p Platform) is32() bool {
	return p == Platform32Bit
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	os.Exit(1)
}

func pj(elem ...string) string {
	return filepath.Join(elem...)
}

func strConcat(arr1, arr2 []string) []string {
	var res []string
	for _, s := range arr1 {
		res = append(res, s)
	}
	for _, s := range arr2 {
		res = append(res, s)
	}
	return res
}

func replaceExt(path string, newExt string) string {
	ext := filepath.Ext(path)
	return path[0:len(path)-len(ext)] + newExt
}

func fileExists(path string) bool {
	fi, err := os.Stat(path)
	if err != nil {
		return false
	}
	return fi.Mode().IsRegular()
}

// maps upper-cased name of env variable to Name/Val
func envToMap(env []string) map[string]*EnvVar {
	res := make(map[string]*EnvVar)
	for _, v := range env {
		if len(v) == 0 {
			continue
		}
		parts := strings.SplitN(v, "=", 2)
		if len(parts) != 2 {

		}
		nameUpper := strings.ToUpper(parts[0])
		res[nameUpper] = &EnvVar{
			Name: parts[0],
			Val:  parts[1],
		}
	}
	return res
}

func flattenEnv(env map[string]*EnvVar) []string {
	var res []string
	for _, envVar := range env {
		v := fmt.Sprintf("%s=%s", envVar.Name, envVar.Val)
		res = append(res, v)
	}
	return res
}

func getEnvAfterScript(dir, script string) map[string]*EnvVar {
	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	fmt.Printf("Executing: %s in %s\n", cmd.Args, cmd.Dir)
	resBytes, err := cmd.Output()
	if err != nil {
		fatalf("failed with %s\n", err)
	}
	res := string(resBytes)
	//fmt.Printf("res:\n%s\n", res)
	parts := strings.Split(res, "\n")
	if len(parts) == 1 {
		fmt.Printf("split failed\n")
		fmt.Printf("res:\n%s\n", res)
		os.Exit(1)
	}
	for idx, env := range parts {
		env = strings.TrimSpace(env)
		parts[idx] = env
	}
	return envToMap(parts)
}

// return value of VCINSTALLDIR env variable after running vsvars32.bat
func getVcInstallDir(toolsDir string) string {
	if cachedVcInstallDir == "" {
		env := getEnvAfterScript(toolsDir, "vsvars32.bat")
		val := env["VCINSTALLDIR"]
		if val == nil {
			fmt.Printf("no 'VCINSTALLDIR' variable in %s\n", env)
			os.Exit(1)
		}
		cachedVcInstallDir = val.Val
	}
	return cachedVcInstallDir
}

func genEnvForVS() []string {
	initialEnv := envToMap(os.Environ())
	vsVar := initialEnv["VS140COMNTOOLS"]
	if vsVar == nil {
		fatalf("VS140COMNTOOLS not set; VS 2015 not installed\n")
	}
	env := getEnvAfterScript(vsVar.Val, "vsvars32.bat")
	return flattenEnv(env)
}

func getEnvForVcTools(vcInstallDir, platform string) []string {
	env := getEnvAfterScript(vcInstallDir, "vcvarsall.bat "+platform)
	return flattenEnv(env)
}

func getEnv32(vcInstallDir string) []string {
	return getEnvForVcTools(vcInstallDir, "x86")
}

func getEnv64(vcInstallDir string) []string {
	return getEnvForVcTools(vcInstallDir, "x86_amd64")
}

func dumpEnv(env map[string]*EnvVar) {
	var keys []string
	for k := range env {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		v := env[k]
		fmt.Printf("%s: %s\n", v.Name, v.Val)
	}
}

func getEnv(platform Platform) []string {
	initialEnv := envToMap(os.Environ())
	vsVar := initialEnv["VS140COMNTOOLS"]
	if vsVar == nil {
		fatalf("VS140COMNTOOLS not set; VS 2015 not installed\n")
	}
	vcInstallDir := getVcInstallDir(vsVar.Val)
	switch platform {
	case Platform32Bit:
		return getEnv32(vcInstallDir)
	case Platform64Bit:
		return getEnv64(vcInstallDir)
	default:
		panic("unknown platform")
	}
}

func lookupInEnvPathUncached(exeName string, env []string) string {
	for _, envVar := range env {
		parts := strings.SplitN(envVar, "=", 2)
		name := strings.ToLower(parts[0])
		if name != "path" {
			continue
		}
		parts = strings.Split(parts[1], ";")
		for _, dir := range parts {
			path := filepath.Join(dir, exeName)
			if fileExists(path) {
				return path
			}
		}
		fatalf("didn't find %s in '%s'\n", exeName, parts[1])
	}
	return ""
}

func runExe(env []string, exeName string, args ...string) {
	exePath := lookupInEnvPathUncached(exeName, env)
	cmd := exec.Command(exePath, args...)
	cmd.Env = env
	if true {
		args := cmd.Args
		args[0] = exeName
		fmt.Printf("Running %s\n", args)
		args[0] = exePath
	}
	out, err := cmd.CombinedOutput()
	if err != nil {
		fatalf("%s failed with %s, out:\n%s\n", cmd.Args, err, string(out))
	}
	fmt.Printf("%s\n", out)
}

func main() {
	timeStart := time.Now()
	env := genEnvForVS()
	//runExe(env, "msbuild.exe", "vs2015\\SumatraPDF.vcxproj", "/p:Configuration=rel")
	//runExe(env, "devenv.exe", "vs2015\\SumatraPDF.sln", "/Rebuild", "Release|Win32", "/Project", "vs2015\\SumatraPDF.vcxproj")
  runExe(env, "devenv.exe", "vs2015\\SumatraPDF.sln", "/Rebuild", "Release|Win32", "/Project", "vs2015\\all.vcxproj")
	fmt.Printf("total time: %s\n", time.Since(timeStart))
}
