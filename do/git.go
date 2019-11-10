package main

import (
	"strings"

	"github.com/kjk/u"
)

func getGitLinearVersionMust() int {
	out := runExeMust("git", "log", "--oneline")
	lines := toTrimmedLines(out)
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) + 1000
	u.PanicIf(n < 10000, "getGitLinearVersion: n is %d (should be > 10000)", n)
	return n
}

func getGitSha1Must() string {
	out := runExeMust("git", "rev-parse", "HEAD")
	s := strings.TrimSpace(string(out))
	u.PanicIf(len(s) != 40, "getGitSha1Must(): %s doesn't look like sha1\n", s)
	return s
}

func isGitClean() bool {
	out := runExeMust("git", "status", "--porcelain")
	s := strings.TrimSpace(string(out))
	if len(s) > 0 {
		logf("git status --porcelain returned:\n'%s'\n", s)
	}
	return len(s) == 0
}

func verifyGitCleanMust() {
	if flgNoCleanCheck {
		return
	}
	u.PanicIf(!isGitClean(), "git has unsaved changes\n")
}

/*
Given result of git btranch that looks like:

master
* rel3.1working

Return active branch marked with "*" ('rel3.1working' in this case) or empty
string if no current branch.
*/
func getCurrentBranch(d []byte) string {
	// "(HEAD detached at b5adf8738)" is what we get on GitHub CI
	s := string(d)
	if strings.Contains(s, "(HEAD detached") {
		return "master"
	}
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
	prefix := "rel"
	suffix := "working"
	u.PanicIf(!strings.HasPrefix(currBranch, prefix), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)
	u.PanicIf(!strings.HasSuffix(currBranch, suffix), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)

	ver := currBranch[len(prefix):]
	ver = ver[:len(ver)-len(suffix)]

	u.PanicIf(!strings.HasPrefix(sumatraVersion, ver), "version mismatch, sumatra: '%s', branch: '%s'\n", sumatraVersion, ver)
}

func verifyOnMasterBranchMust() {
	// 'git branch' return branch name in format: '* master'
	out := runExeMust("git", "branch")
	currBranch := getCurrentBranch(out)
	u.PanicIf(currBranch != "master", "not on master branch. out: '%s', currBranch: '%s'\n", string(out), currBranch)
}
