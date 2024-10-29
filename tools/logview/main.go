package main

import (
	"embed"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

//go:embed all:frontend/dist
var assets embed.FS

var gApp *App

func main() {
	go pipeThread()

	gApp = NewApp()

	err := wails.Run(&options.App{
		Title:  "Logview",
		Width:  1024,
		Height: 800,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 27, G: 38, B: 54, A: 1},
		OnStartup:        gApp.startup,
		Bind: []interface{}{
			gApp,
		},
	})

	if err != nil {
		println("Error:", err.Error())
	}
}
