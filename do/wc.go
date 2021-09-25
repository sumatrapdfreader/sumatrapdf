package main

import (
	"fmt"
	"io/ioutil"
	"path/filepath"
	"sort"
	"strings"
)

// LineCount describes line count for a file
type LineCount struct {
	Name      string
	Ext       string
	LineCount int
}

// LineStats gathers line count info for files
type LineStats struct {
	FileToCount map[string]*LineCount
}

// NewLineStats returns new LineStats
func NewLineStats() *LineStats {
	return &LineStats{
		FileToCount: map[string]*LineCount{},
	}
}

func statsPerExt(fileToCount map[string]*LineCount) []*LineCount {
	extToCount := map[string]*LineCount{}
	for _, wc := range fileToCount {
		ext := wc.Ext
		extWc := extToCount[ext]
		if extWc == nil {
			extWc = &LineCount{
				Ext: ext,
			}
			extToCount[ext] = extWc
		}
		extWc.LineCount += wc.LineCount
	}
	var res []*LineCount
	for _, wc := range extToCount {
		res = append(res, wc)
	}
	sort.Slice(res, func(i, j int) bool {
		wc1 := res[i]
		wc2 := res[j]
		return wc1.LineCount < wc2.LineCount
	})
	return res
}

type FilterFunc func(string) bool

func MakeAllowedFileFilterForExts(exts ...string) FilterFunc {
	for i, ext := range exts {
		exts[i] = strings.ToLower(ext)
	}

	return func(path string) bool {
		fext := strings.ToLower(filepath.Ext(path))
		for _, ext := range exts {
			if ext == fext {
				return true
			}
		}
		return false
	}
}

func MakeExcludeDirsFilter(dirs ...string) FilterFunc {
	return func(path string) bool {
		// path starts as a file path
		// we only compare directory names
		path = filepath.Dir(path)
		for len(path) > 0 {
			if path == "." {
				return true
			}
			name := filepath.Base(path)
			for _, dir := range dirs {
				if name == dir {
					return false
				}
			}
			path = filepath.Dir(path)
		}
		return true
	}
}

func MakeFilterOr(filters ...FilterFunc) FilterFunc {
	return func(name string) bool {
		for _, f := range filters {
			if f(name) {
				return true
			}
		}
		return false
	}
}

func MakeFilterAnd(filters ...FilterFunc) FilterFunc {
	return func(name string) bool {
		for _, f := range filters {
			if !f(name) {
				return false
			}
		}
		return true
	}
}

// FileLineCount returns number of lines in a file
func FileLineCount(path string) (int, error) {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return 0, err
	}
	if len(d) == 0 {
		return 0, nil
	}
	d = normalizeNewlines(d)
	nLines := 1
	n := len(d)
	for i := 0; i < n; i++ {
		if d[i] == 10 {
			nLines++
		}
	}
	return nLines, nil
}

func (s *LineStats) CalcInDir(dir string, allowedFileFilter func(name string) bool, recur bool) error {
	files, err := ioutil.ReadDir(dir)
	if err != nil {
		return err
	}
	for _, fi := range files {
		name := fi.Name()
		path := filepath.Join(dir, name)
		if fi.IsDir() {
			if recur {
				s.CalcInDir(path, allowedFileFilter, recur)
			}
			continue
		}
		if !fi.Mode().IsRegular() {
			continue
		}
		if !allowedFileFilter(path) {
			continue
		}
		lineCount, err := FileLineCount(path)
		if err != nil {
			return err
		}
		s.FileToCount[path] = &LineCount{
			Name:      fi.Name(),
			Ext:       strings.ToLower(filepath.Ext(name)),
			LineCount: lineCount,
		}
	}
	return nil
}

func PrintLineStats(stats *LineStats) {
	var files []string
	for k := range stats.FileToCount {
		files = append(files, k)
	}
	sort.Strings(files)
	total := 0
	for _, f := range files {
		wc := stats.FileToCount[f]
		fmt.Printf("% 6d %s\n", wc.LineCount, f)
		total += wc.LineCount
	}
	fmt.Printf("\nPer extension:\n")
	wcPerExt := statsPerExt(stats.FileToCount)
	for _, wc := range wcPerExt {
		fmt.Printf("%d %s\n", wc.LineCount, wc.Ext)
	}
	fmt.Printf("\ntotal: %d\n", total)
}

// -----------------------------------------------------

// return false to exclude a file
func excludeFiles(s string) bool {
	return true
}

var srcFiles = MakeAllowedFileFilterForExts(".go", ".cpp", ".h")
var excludeDirs = MakeExcludeDirsFilter("ext")
var allFiles = MakeFilterAnd(excludeDirs, excludeFiles, srcFiles)

func doLineCount() int {
	stats := NewLineStats()
	err := stats.CalcInDir("src", allFiles, true)
	if err != nil {
		logf(ctx(), "doWordCount: stats.wcInDir() failed with '%s'\n", err)
		return 1
	}
	err = stats.CalcInDir("do", allFiles, true)
	if err != nil {
		logf(ctx(), "doWordCount: stats.wcInDir() failed with '%s'\n", err)
		return 1
	}
	PrintLineStats(stats)
	return 0
}
