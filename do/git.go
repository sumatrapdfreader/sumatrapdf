package main

import (
	"os"
	"strings"
)

func getGitLinearVersionMust() int {
	out := runExeMust("git", "log", "--oneline")
	lines := toTrimmedLines(out)
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) + 1000
	panicIf(n < 10000, "getGitLinearVersion: n is %d (should be > 10000)", n)
	return n
}

func getGitSha1Must() string {
	out := runExeMust("git", "rev-parse", "HEAD")
	s := strings.TrimSpace(string(out))
	panicIf(len(s) != 40, "getGitSha1Must(): %s doesn't look like sha1\n", s)
	return s
}

func isGitClean(dir string) bool {
	out := runExeInDirMust(dir, "git", "status", "--porcelain")
	s := strings.TrimSpace(string(out))
	if len(s) > 0 {
		logf("git status --porcelain returned:\n'%s'\n", s)
	}
	return len(s) == 0
}

/*
Given result of git btranch that looks like:

master
* rel3.1working

Return active branch marked with "*" ('rel3.1working' in this case) or empty
string if no current branch.
*/
func getCurrentBranchMust() string {
	d := runExeMust("git", "branch")
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
	currBranch := getCurrentBranchMust()
	prefix := "rel"
	suffix := "working"
	panicIf(!strings.HasPrefix(currBranch, prefix), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)
	panicIf(!strings.HasSuffix(currBranch, suffix), "running on branch '%s' which is not 'rel${ver}working' branch\n", currBranch)

	ver := currBranch[len(prefix):]
	ver = ver[:len(ver)-len(suffix)]

	panicIf(!strings.HasPrefix(sumatraVersion, ver), "version mismatch, sumatra: '%s', branch: '%s'\n", sumatraVersion, ver)
}

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
