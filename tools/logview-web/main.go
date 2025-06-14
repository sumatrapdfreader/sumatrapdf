package main

import (
	"embed"
	"logview-web/server"
)

var (
	// go:embed dist/*
	distFS embed.FS
)

func main() {
	// This is the main entry point for the application.
	// It will call the Main function from the server package.
	server.DistFS = distFS
	server.Main()
}
