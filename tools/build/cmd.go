package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

func getEnvAfterScript(dir, script string) []string {
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

func getEnvForVSUncached() []string {
	val, ok := getOsEnvValue("VS140COMNTOOLS")
	if !ok {
		fatalf("VS140COMNTOOLS not set; VS 2015 not installed?\n")
	}
	return getEnvAfterScript(val, "vsvars32.bat")
}

var (
	envForVSCached []string
)

func getPaths(env []string) []string {
	path, ok := getEnvValue(env, "path")
	fatalif(!ok, "")
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

func getEnvForVS() []string {
	if envForVSCached == nil {
		envForVSCached = getEnvForVSUncached()
		//dumpEnv(envForVSCached)
	}
	return envForVSCached
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
	fataliferr(err)
	fatalif(path == "", "didn't find %s in %s\n", exeName, getPaths(env))
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

func getCmdInEnv(env []string, exeName string, args ...string) *exec.Cmd {
	if env == nil {
		env = os.Environ()
	}
	exePath := lookExeInEnvPath(env, exeName)
	cmd := exec.Command(exePath, args...)
	cmd.Env = env
	if true {
		fmt.Printf("Running %s\n", cmd.Args)
	}
	return cmd
}

func getCmd(exeName string, args ...string) *exec.Cmd {
	return getCmdInEnv(nil, exeName, args...)
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

func runCmd(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	cmdTimeStart := time.Now()
	if !showProgress {
		res, err := cmd.CombinedOutput()
		appendTiming(time.Since(cmdTimeStart), cmdToStr(cmd))
		logCmdResult(cmd, res, err)
		return res, err
	}
	var resOut, resErr []byte
	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()
	cmd.Start()

	go func() {
		buf := make([]byte, 1024, 1024)
		for {
			n, err := stdout.Read(buf)
			if err != nil {
				break
			}
			if n > 0 {
				d := buf[:n]
				resOut = append(resOut, d...)
				os.Stdout.Write(d)
			}
		}
	}()

	go func() {
		buf := make([]byte, 1024, 1024)
		for {
			n, err := stderr.Read(buf)
			if err != nil {
				break
			}
			if n > 0 {
				d := buf[:n]
				resErr = append(resErr, d...)
				os.Stderr.Write(d)
			}
		}
	}()
	err := cmd.Wait()
	appendTiming(time.Since(cmdTimeStart), cmdToStr(cmd))
	resOut = append(resOut, resErr...)
	logCmdResult(cmd, resOut, err)
	return resOut, err
}

func runCmdMust(cmd *exec.Cmd, showProgress bool) {
	_, err := runCmd(cmd, showProgress)
	fataliferr(err)
}

func runCmdLogged(cmd *exec.Cmd, showProgress bool) ([]byte, error) {
	out, err := runCmd(cmd, showProgress)
	if err != nil {
		args := []string{cmd.Path}
		args = append(args, cmd.Args...)
		fmt.Printf("%s failed with %s, out:\n%s\n", args, err, string(out))
		return out, err
	}
	fmt.Printf("%s\n", out)
	return out, nil
}

func runExe(exeName string, args ...string) ([]byte, error) {
	cmd := getCmd(exeName, args...)
	return runCmd(cmd, false)
}

func runExeInEnv(env []string, exeName string, args ...string) ([]byte, error) {
	cmd := getCmdInEnv(env, exeName, args...)
	return runCmd(cmd, false)
}

func runExeMust(exeName string, args ...string) []byte {
	out, err := runExeInEnv(os.Environ(), exeName, args...)
	fataliferr(err)
	return out
}

func runExeLogged(env []string, exeName string, args ...string) ([]byte, error) {
	out, err := runExeInEnv(env, exeName, args...)
	if err != nil {
		fmt.Printf("%s failed with %s, out:\n%s\n", args, err, string(out))
		return out, err
	}
	fmt.Printf("%s\n", out)
	return out, nil
}

func runMsbuild(showProgress bool, args ...string) error {
	cmd := getCmdInEnv(getEnvForVS(), "msbuild.exe", args...)
	_, err := runCmdLogged(cmd, showProgress)
	return err
}

func runMsbuildGetOutput(showProgress bool, args ...string) ([]byte, error) {
	cmd := getCmdInEnv(getEnvForVS(), "msbuild.exe", args...)
	return runCmdLogged(cmd, showProgress)
}

func runDevenvGetOutput(showProgress bool, args ...string) ([]byte, error) {
	cmd := getCmdInEnv(getEnvForVS(), "devenv.exe", args...)
	return runCmdLogged(cmd, showProgress)
}
