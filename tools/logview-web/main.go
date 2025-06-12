package main

import (
	"embed"
)

//go:embed all:frontend/dist
var assets embed.FS

func emitEvent(name string, data ...interface{}) {
	// This function is a placeholder for emitting events.
	// In a real application, you would use the Wails runtime to emit events.
	// For example: runtime.EventsEmit(gApp.ctx, name, data...)
}

func main() {
	go pipeThread()

}
