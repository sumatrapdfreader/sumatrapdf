package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

var (
	// vs2017
	vsVer        string
	envForVS     []string
	msbuildPath  string // full path to msbuild.exe
	signtoolPath string // full path to signtool.exe
	devenvPath   string // full path to devenv.exe
)

func cmdToStrLong(cmd *exec.Cmd) string {
	arr := []string{cmd.Path}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

func cmdToStr(cmd *exec.Cmd) string {
	s := filepath.Base(cmd.Path)
	arr := []string{s}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

// run a .bat script and capture environment variables after
func getEnvAfterScript(dir, script string) []string {
	path := filepath.Join(dir, script)
	if !fileExists(path) {
		return nil
	}

	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	fmt.Printf("Executing: %s in %s\n", cmd.Args, cmd.Dir)
	resBytes, err := cmd.Output()
	if err != nil {
		fatalf("failed with %s\n", err)
	}
	res := string(resBytes)
	parts := strings.Split(res, "\n")
	if len(parts) == 1 {
		fatalf("split failed\nres:\n%s\n", res)
	}
	for idx, env := range parts {
		parts[idx] = strings.TrimSpace(env)
	}
	return parts
}

func getEnvValue(env []string, name string) (string, bool) {
	for _, v := range env {
		parts := strings.SplitN(v, "=", 2)
		if len(parts) != 2 {
			continue
		}
		if strings.EqualFold(name, parts[0]) {
			return parts[1], true
		}
	}
	return "", false
}

func getOsEnvValue(name string) (string, bool) {
	return getEnvValue(os.Environ(), name)
}

func getEnvForVS2017() []string {
	if !tryVs2017 {
		fmt.Printf("Not trying vs 2017\n")
		return nil
	}
	dir, ok := getOsEnvValue("ProgramFiles(x86)")
	if !ok {
		dir, ok = getOsEnvValue("ProgramFiles")
	}
	if !ok {
		fmt.Printf("didn't find Program files directory\n")
		return nil
	}

	// TODO: don't know why this doesn't work. The log show I try to execute the
	// right msbuild but it fails to even start
	// also: https://github.com/bazelbuild/bazel/commit/321aa540feb2cd0583b824bbb646c885fda17f0b
	// It must have something with the env. If I use standard env, msbuild.exe starts
	// (but doesn't compile properly). If I pass env from vcvar32
	// TODO: Professional or Enterprise would be in different directories, I assume
	dir = filepath.Join(dir, "Microsoft Visual Studio", "2017", "Community", "VC", "Auxiliary", "Build")
	env := getEnvAfterScript(dir, "vcvars64.bat")
	if len(env) == 0 {
		fmt.Printf("getEnvAfterScript() returned empty, no VS 2017\n")
		return nil
	}
	// TODO: probably should be vcvars32.bat on 32-bit windows
	fmt.Printf("Found VS 2017\n")
	vsVer = "vs2017"
	return env
}

func lookExeInEnvPathUncachedHelper(env []string, exeName string) string {
	var found []string
	paths := getPaths(env)
	for _, dir := range paths {
		path := filepath.Join(dir, exeName)
		if fileExists(path) {
			found = append(found, path)
		}
	}
	if len(found) == 0 {
		return ""
	}
	if len(found) == 1 {
		return found[0]
	}
	// HACK: for msbuild.exe we might find 3 locations, prefer the one for
	// 2015 VS. If we pick up 2013 VS, it'll complain about v140_xp toolset
	// not being installed
	for _, p := range found {
		if strings.Contains(p, "14.0") {
			return p
		}
	}
	return found[0]
}

func lookExeInEnvPathUncached(env []string, exeName string) string {
	var err error
	fmt.Printf("lookExeInEnvPathUncached: exeName=%s\n", exeName)
	path := lookExeInEnvPathUncachedHelper(env, exeName)
	if path == "" && filepath.Ext(exeName) == "" {
		path = lookExeInEnvPathUncachedHelper(env, exeName+".exe")
	}
	//panic(fmt.Sprintf("didn't find %s in %s\n", exeName, getPaths(env)))
	if path == "" {
		path, err = exec.LookPath(exeName)
	}
	fatalIfErr(err)
	fatalIf(path == "", "didn't find %s in %s\n", exeName, getPaths(env))
	fmt.Printf("found %v for %s\n", path, exeName)
	return path
}

func lookExeInEnvPath(env []string, exeName string) string {
	var exePath string
	var err error
	if false {
		exePath, err = exec.LookPath(exeName)
		if err != nil {
			exePath = lookExeInEnvPathUncached(env, exeName)
		}
	} else {
		exePath = lookExeInEnvPathUncached(env, exeName)
	}
	return exePath
}

func detectVisualStudio() {
	fatalIf(envForVS != nil, "called detectVisualStudio() second time")
	envForVS = getEnvForVS2017()
	fatalIf(envForVS == nil, "didn't find Visual Studio")
	msbuildPath = lookExeInEnvPath(envForVS, "msbuild.exe")
	fatalIf(msbuildPath == "", "didn't find msbuild.exe")
	signtoolPath = lookExeInEnvPath(envForVS, "signtool.exe")
	fatalIf(signtoolPath == "", "didnt' find signtool.exe")
}

func getPaths(env []string) []string {
	path, ok := getEnvValue(env, "path")
	fatalIf(!ok, "")
	sep := string(os.PathListSeparator)
	parts := strings.Split(path, sep)
	for i, s := range parts {
		parts[i] = strings.TrimSpace(s)
	}
	return parts
}

func dumpEnv(env []string) {
	for _, e := range env {
		fmt.Printf("%s\n", e)
	}
	paths := getPaths(env)
	fmt.Printf("PATH:\n  %s\n", strings.Join(paths, "\n  "))
}

func logCmdResult(cmd *exec.Cmd, out []byte, err error) {
	var s string
	if err != nil {
		s = fmt.Sprintf("%s failed with %s, out:\n%s\n\n", cmdToStr(cmd), err, string(out))
	} else {
		s = fmt.Sprintf("%s\n%s\n\n", cmdToStr(cmd), string(out))
	}
	logToFile(s)
}

func copyAndCapture(w io.Writer, r io.Reader) []byte {
	var out []byte
	buf := make([]byte, 1024, 1024)
	for {
		n, err := r.Read(buf[:])
		if err != nil {
			break
		}
		if n > 0 {
			d := buf[:n]
			out = append(out, d...)
			os.Stdout.Write(d)
		}
	}
	return out
}

func runCmd(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	cmdTimeStart := time.Now()
	if !showProgress {
		res, err := cmd.CombinedOutput()
		appendTiming(time.Since(cmdTimeStart), cmdToStr(cmd))
		logCmdResult(cmd, res, err)
		return res, err
	}
	var stdout, stderr []byte
	stdoutIn, _ := cmd.StdoutPipe()
	stderrIn, _ := cmd.StderrPipe()
	cmd.Start()

	go func() {
		stdout = copyAndCapture(os.Stdout, stdoutIn)
	}()

	go func() {
		stderr = copyAndCapture(os.Stderr, stderrIn)
	}()

	err := cmd.Wait()
	appendTiming(time.Since(cmdTimeStart), cmdToStr(cmd))
	res := append(stdout, stderr...)
	logCmdResult(cmd, res, err)
	return res, err
}

func runCmdMust(cmd *exec.Cmd, showProgress bool) {
	_, err := runCmd(cmd, showProgress)
	fatalIfErr(err)
}

func runCmdLogged(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	out, err := runCmd(cmd, showProgress)
	if err != nil {
		args := []string{cmd.Path}
		args = append(args, cmd.Args...)
		fmt.Printf("%s failed with %s, out:\n'%s'\n", args, err, string(out))
		return out, err
	}
	fmt.Printf("%s\n", out)
	return out, nil
}

func runExe(exeName string, args ...string) ([]byte, error) {
	cmd := exec.Command(exeName, args...)
	return runCmd(cmd, false)
}

func runExeMust(name string, args ...string) []byte {
	cmd := exec.Command(name, args...)
	out, err := runCmd(cmd, false)
	fatalIfErr(err)
	return out
}

func runMsbuild(showProgress bool, args ...string) error {
	cmd := exec.Command(msbuildPath, args...)
	_, err := runCmdLogged(cmd, showProgress)
	return err
}

func runMsbuildGetOutput(showProgress bool, args ...string) ([]byte, error) {
	cmd := exec.Command(msbuildPath, args...)
	return runCmdLogged(cmd, showProgress)
}
