package main

import (
	"strings"

	"github.com/kjk/u"
)

func excludeFiles(s string) bool {
	if strings.Contains(s, "Trans_sumatra_txt.cpp") {
		return false
	}
	return true
}

var srcFiles = u.MakeAllowedFileFilterForExts(".go", ".cpp", ".h")
var excludeDirs = u.MakeExcludeDirsFilter("ext")
var allFiles = u.MakeFilterAnd(excludeDirs, excludeFiles, srcFiles)

func doLineCount() int {
	stats := u.NewLineStats()
	err := stats.CalcInDir("src", allFiles, true)
	if err != nil {
		logf("doWordCount: stats.wcInDir() failed with '%s'\n", err)
		return 1
	}
	err = stats.CalcInDir("do", allFiles, true)
	if err != nil {
		logf("doWordCount: stats.wcInDir() failed with '%s'\n", err)
		return 1
	}
	u.PrintLineStats(stats)
	return 0
}
