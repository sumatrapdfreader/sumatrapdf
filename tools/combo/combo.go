package main

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"path/filepath"
	"strings"
)

type ComboState struct {
	// keeps track of files that have already been included
	includedFiles map[string]struct{}
}

func must(err error) {
	if err != nil {
		panic(err.Error())
	}
}

func normalizeNewlines(d []byte) []byte {
	// replace CR LF (windows) with LF (unix)
	d = bytes.Replace(d, []byte{13, 10}, []byte{10}, -1)
	// replace CF (mac) with LF (unix)
	d = bytes.Replace(d, []byte{13}, []byte{10}, -1)
	return d
}

func processFile(state *ComboState, path string) []byte {
	state.includedFiles[path] = struct{}{}
	d, err := ioutil.ReadFile(path)
	must(err)
	d = normalizeNewlines(d)

	var res []string
	lines := strings.Split(string(d), "\n")
	for _, l := range lines {
		if strings.Contains(l, "#include") {
			fmt.Printf("%s\n", l)
		}
		res = append(res, l)
	}
	return nil
}

func comboFiles(dir string, files []string) []byte {
	state := &ComboState{
		includedFiles: make(map[string]struct{}),
	}

	for _, f := range files {
		path := filepath.Join(dir, f)
		processFile(state, path)

	}
	return nil
}

func comboZlib() {
	dir := filepath.Join("ext", "zlib")
	files := []string{"adler32.c", "compress.c", "crc32.c", "deflate.c", "inffast.c",
		"inflate.c", "inftrees.c", "trees.c", "zutil.c", "gzlib.c",
		"gzread.c", "gzwrite.c", "gzclose.c"}
	f := comboFiles(dir, files)
	fmt.Printf("f:\n%s\n", string(f))
}

func main() {
	fmt.Printf("Combo\n")
	comboZlib()
}
