// Compatibility entry point used by the WSL launcher.
const proc = Bun.spawn(["bun", "cmd/build.ts", "-linux", ...Bun.argv.slice(2)], {
  stdout: "inherit",
  stderr: "inherit",
  stdin: "inherit",
});
process.exitCode = await proc.exited;
