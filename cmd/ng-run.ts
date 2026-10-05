// Build and run a single target. Arguments match cmd/ng-build.ts except -run is implied.

process.argv.splice(2, 0, "-run");
await import("./ng-build");
