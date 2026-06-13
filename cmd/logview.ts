import { join } from "node:path";

// runs the logview-web dev server, which lives in a sibling repo:
//   ..\hack\winapps\logview-web\
// equivalent to: cd ..\hack\winapps\logview-web\; go run . -run-dev
const cwd = join(import.meta.dir, "..", "..", "hack", "winapps", "logview-web");

async function main(): Promise<void> {
  console.log(`> go run . -run-dev (in ${cwd})`);
  const proc = Bun.spawn(["go", "run", ".", "-run-dev"], {
    cwd: cwd,
    stdin: "inherit",
    stdout: "inherit",
    stderr: "inherit",
  });
  const exitCode = await proc.exited;
  process.exit(exitCode);
}

if (import.meta.main) {
  await main();
}
