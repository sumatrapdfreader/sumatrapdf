package main

import (
	"html/template"
	"io/ioutil"
	"path/filepath"
	"strings"
	"sync"
)

var (
	dirToTemplates = map[string]*template.Template{}
	muTemplates    sync.Mutex
)

func loadTemplatesInDir(dir string) (*template.Template, error) {
	files, err := ioutil.ReadDir(dir)
	if err != nil {
		return nil, err
	}

	var paths []string
	for _, fi := range files {
		if fi.Mode().IsRegular() && strings.Contains(fi.Name(), ".tmpl.") {
			path := filepath.Join(dir, fi.Name())
			paths = append(paths, path)
		}
	}
	templates, err := template.ParseFiles(paths...)
	if err != nil {
		return nil, err
	}
	return templates, nil
}

func reloadTemplatesInDirAndCache(dir string) (*template.Template, error) {
	templates, err := loadTemplatesInDir(dir)
	if err != nil {
		return nil, err
	}
	muTemplates.Lock()
	dirToTemplates[dir] = templates
	muTemplates.Unlock()
	return templates, nil
}

func getCachedTemplatesForDir(dir string) *template.Template {
	muTemplates.Lock()
	res := dirToTemplates[dir]
	muTemplates.Unlock()
	return res
}

func getTemplatesInDir(dir string) (*template.Template, error) {
	// in dev mode, we always reload templates for easier debugging
	templates := getCachedTemplatesForDir(dir)
	if isRunningDev() || templates == nil {
		return reloadTemplatesInDirAndCache(dir)
	}
	return templates, nil
}

func isRunningDev() bool {
	return true
}
