package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"mime"
	"net/http"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/kjk/u"
)

func logErrorf(ctx context.Context, format string, args ...interface{}) {
	msg := u.FmtSmart(format, args...)
	fmt.Printf(msg)
}

func addNl(s string) string {
	n := len(s)
	if n == 0 {
		return s
	}
	if s[n-1] == '\n' {
		return s
	}
	return s + "\n"
}

const (
	htmlMimeType     = "text/html; charset=utf-8"
	jsMimeType       = "text/javascript; charset=utf-8"
	markdownMimeType = "text/markdown; charset=UTF-8"
)

var extraMimeTypes = map[string]string{
	".icon": "image-x-icon",
	".ttf":  "application/x-font-ttf",
	".woff": "application/x-font-woff",
	".eot":  "application/vnd.ms-fontobject",
	".svg":  "image/svg+xml",
}

// mimeTypeByExtensionExt is like mime.TypeByExtension but supports more types
// and defaults to text/plain
func mimeTypeByExtensionExt(name string) string {
	ext := strings.ToLower(filepath.Ext(name))
	result := mime.TypeByExtension(ext)
	if result == "" {
		result = extraMimeTypes[ext]
	}
	if result == "" {
		result = "text/plain; charset=utf-8"
	}
	return result
}
func acceptsGzip(r *http.Request) bool {
	// TODO: would be safer to split by ", "
	return r != nil && strings.Contains(r.Header.Get("accept-encoding"), "gzip")
}

func writeHeader(w http.ResponseWriter, code int, contentType string) {
	w.Header().Set("Content-Type", contentType+"; charset=utf-8")
	w.WriteHeader(code)
}

func serveJSONWithCode(w http.ResponseWriter, r *http.Request, code int, v interface{}) {
	d, err := json.Marshal(v)
	if err != nil {
		serveInternalError(w, r, "json.Marshal() failed with '%s'", err)
		return
	}
	writeHeader(w, code, jsMimeType)
	_, err = w.Write(d)
	if err != nil {
		logErrorf(r.Context(), "err: '%s'\n", err)
	}
}

func serveJSON(w http.ResponseWriter, r *http.Request, v interface{}) {
	serveJSONWithCode(w, r, http.StatusOK, v)
}

func servePlainText(w http.ResponseWriter, r *http.Request, code int, format string, args ...interface{}) {
	writeHeader(w, code, "text/plain")
	var err error
	s := format
	if len(args) > 0 {
		s = fmt.Sprintf(format, args...)
	}
	_, err = io.WriteString(w, s)
	if err != nil {
		logErrorf(r.Context(), "err: '%s'\n", err)
	}
}

func serveHTMLTemplate(w http.ResponseWriter, r *http.Request, code int, tmplName string, v interface{}) bool {
	dir := filepath.Join("do")
	templates, err := getTemplatesInDir(dir)
	if err != nil {
		serveSimpleError(w, r, "getTemplatesInDir('%s') failed with '%s'", dir, err)
		return false
	}

	// render template in memory first so that if this fails,
	// we can show an error page
	var buf bytes.Buffer
	err = templates.ExecuteTemplate(&buf, tmplName, v)
	if err != nil {
		serveSimpleError(w, r, "templ.Execute('%s') failed with '%s'", tmplName, err)
		return false
	}

	w.Header().Set("Content-Type", htmlMimeType)
	w.WriteHeader(code)
	_, _ = w.Write(buf.Bytes())
	return true
}

func serve404(w http.ResponseWriter, r *http.Request, format string, args ...interface{}) {
	logErrorf(r.Context(), addNl(format), args...)
	v := map[string]interface{}{
		"URL":      r.URL.String(),
		"ErrorMsg": u.FmtSmart(format, args...),
	}
	serveHTMLTemplate(w, r, http.StatusNotFound, "404.tmpl.html", v)
}

func serveInternalError(w http.ResponseWriter, r *http.Request, format string, args ...interface{}) {
	logErrorf(r.Context(), addNl(format), args...)
	errMsg := u.FmtSmart(format, args...)
	v := map[string]interface{}{
		"URL":      r.URL.String(),
		"ErrorMsg": errMsg,
	}
	serveHTMLTemplate(w, r, http.StatusInternalServerError, "internal.error.tmpl.html", v)
}

// doesn't use templates so can be used when template parsing fails
func serveSimpleError(w http.ResponseWriter, r *http.Request, format string, args ...interface{}) {
	logErrorf(r.Context(), addNl(format), args...)
	servePlainText(w, r, http.StatusInternalServerError, format, args...)
}

func serveMaybeGzippedFile(w http.ResponseWriter, r *http.Request, path string) bool {
	if !u.FileExists(path) {
		serve404(w, r, "file '%s' doesn't exist", path)
		return false
	}
	contentType := mimeTypeByExtensionExt(path)
	usesGzip := acceptsGzip(r)
	if usesGzip {
		if u.FileExists(path + ".gz") {
			path = path + ".gz"
		} else {
			usesGzip = false
		}
	}
	if len(contentType) > 0 {
		w.Header().Set("Content-Type", contentType)
	}
	// https://www.maxcdn.com/blog/accept-encoding-its-vary-important/
	// prevent caching non-gzipped version
	w.Header().Add("Vary", "Accept-Encoding")
	if usesGzip {
		w.Header().Set("Content-Encoding", "gzip")
	}
	d, err := ioutil.ReadFile(path)
	if err != nil {
		serve404(w, r, "ioutil.ReadFile('%s') failed with '%s'", path, err)
		return false
	}
	w.Header().Set("Content-Length", strconv.Itoa(len(d)))
	w.WriteHeader(200)
	_, _ = w.Write(d)
	return true
}

func serveRelativeFile(w http.ResponseWriter, r *http.Request, relativePath string) bool {
	dir := filepath.Join("do")
	path := filepath.Join(dir, relativePath)
	return serveMaybeGzippedFile(w, r, path)
}

// Request.RemoteAddress contains port, which we want to remove i.e.:
// "[::1]:58292" => "[::1]"
func ipAddrFromRemoteAddr(s string) string {
	idx := strings.LastIndex(s, ":")
	if idx == -1 {
		return s
	}
	return s[:idx]
}

// requestGetRemoteAddress returns ip address of the client making the request,
// taking into account http proxies
func requestGetRemoteAddress(r *http.Request) string {
	hdr := r.Header
	hdrRealIP := hdr.Get("x-real-ip")
	hdrForwardedFor := hdr.Get("x-forwarded-for")
	if hdrRealIP == "" && hdrForwardedFor == "" {
		return ipAddrFromRemoteAddr(r.RemoteAddr)
	}
	if hdrForwardedFor != "" {
		// X-Forwarded-For is potentially a list of addresses separated with ","
		parts := strings.Split(hdrForwardedFor, ",")
		for i, p := range parts {
			parts[i] = strings.TrimSpace(p)
		}
		// TODO: should return first non-local address
		return parts[0]
	}
	return hdrRealIP
}

func requestGetHostNoPort(r *http.Request) string {
	h := r.Host
	parts := strings.Split(h, ":")
	return parts[0]
}

// requestGetReferrer returns a referer e.g. "https://codeeval.dev/home"
func requestGetReferrer(r *http.Request) string {
	return r.Header.Get("referer")
}
