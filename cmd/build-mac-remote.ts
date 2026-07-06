/**
 * Run the macOS dependency build on the remote Mac checkout.
 *
 * Usage:
 *   bun cmd/build-mac-remote.ts
 *   bun cmd/build-mac-remote.ts -debug
 *   bun cmd/build-mac-remote.ts -release
 *   bun cmd/build-mac-remote.ts -debug -clean
 */

const remoteHost = "kjk@100.120.113.17";
const remoteDir = "src/sumatrapdf";

function shellQuote(s: string): string {
  return `'${s.replace(/'/g, `'\\''`)}'`;
}

const args = Bun.argv.slice(2);
const buildArgs = args.length > 0 ? args : ["-debug"];
const quotedArgs = buildArgs.map(shellQuote).join(" ");
const remoteCmd =
  `cd ${shellQuote(remoteDir)} && ` +
  `(command -v bun >/dev/null 2>&1 && bun cmd/build-mac.ts ${quotedArgs} || ~/.bun/bin/bun cmd/build-mac.ts ${quotedArgs})`;

console.log(`> ssh ${remoteHost} ${remoteCmd}`);
const proc = Bun.spawn(["ssh", remoteHost, remoteCmd], {
  stdout: "inherit",
  stderr: "inherit",
  stdin: "inherit",
});

const exitCode = await proc.exited;
if (exitCode !== 0) {
  throw new Error(`remote mac build failed with exit code ${exitCode}`);
}
