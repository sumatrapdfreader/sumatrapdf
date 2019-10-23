package main

import (
	"os/exec"
	"strings"

	"github.com/kjk/u"
)

func getGitLinearVersionMust() int {
	cmd := exec.Command("git", "log", "--oneline")
	out := u.RunCmdMust(cmd)
	lines := toTrimmedLines([]byte(out))
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) + 1000
	u.PanicIf(n < 10000, "getGitLinearVersion: n is %d (should be > 10000)", n)
	return n
}

func getGitSha1Must() string {
	cmd := exec.Command("git", "rev-parse", "HEAD")
	out := u.RunCmdMust(cmd)
	s := strings.TrimSpace(string(out))
	u.PanicIf(len(s) != 40, "getGitSha1Must(): %s doesn't look like sha1\n", s)
	return s
}

func isGitClean() bool {
	cmd := exec.Command("git", "status", "--porcelain")
	out := u.RunCmdMust(cmd)
	s := strings.TrimSpace(string(out))
	return len(s) == 0
}

func verifyGitCleanMust() {
	if flgNoCleanCheck {
		return
	}
	u.PanicIf(!isGitClean(), "git has unsaved changes\n")
}

func runExeMust(c string, args ...string) []byte {
	cmd := exec.Command(c, args...)
	out := u.RunCmdMust(cmd)
	return []byte(out)
}

/*
Given result of git btranch that looks like:

master
* rel3.1working

Return active branch marked with "*" ('rel3.1working' in this case) or empty
string if no current branch.
*/
func getCurrentBranch(d []byte) string {
	lines := toTrimmedLines(d)
	for _, l := range lines {
		if strings.HasPrefix(l, "* ") {
			return l[2:]
		}
	}
	return ""
}

// When doing a release build, it must be from from a branch rel${ver}working
// e.g. rel3.1working, where ${ver} must match first 2 digits in sumatraVersion
// i.e. we allow 3.1.1 and 3.1.2 from branch 3.1 but not from 3.0 or 3.2
func verifyOnReleaseBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := runExeMust("git", "branch")
	currBranch := getCurrentBranch(out)
	pref := "rel"
	suff := "working"
	u.PanicIf(!strings.HasPrefix(currBranch, pref), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)
	u.PanicIf(!strings.HasSuffix(currBranch, suff), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)

	ver := currBranch[len(pref):]
	ver = ver[:len(ver)-len(suff)]

	u.PanicIf(!strings.HasPrefix(sumatraVersion, ver), "version mismatch, sumatra: '%s', branch: '%s'\n", sumatraVersion, ver)
}

func verifyOnMasterBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := runExeMust("git", "branch")
	currBranch := getCurrentBranch(out)
	u.PanicIf(currBranch != "master", "no on master branch (branch: '%s')\n", currBranch)
}
