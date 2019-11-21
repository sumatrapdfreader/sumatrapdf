package main

import "path/filepath"

func replaceExt(fileName, newExt string) string {
	ext := filepath.Ext(fileName)
	if ext == "" {
		return fileName
	}
	n := len(fileName)
	s := fileName[:n-len(ext)]
	return s + newExt
}
