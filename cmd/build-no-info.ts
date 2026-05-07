import { $ } from "bun";

const out = await $`git log --oneline`.text();
const lines = out.split("\n").filter((l) => l.trim() !== "");

function buildNoForIndex(i: number): number {
  // we add 1000 to create a version that is larger than the svn version
  // from the time we used svn
  return lines.length - i + 1000;
}

const buildNo = parseInt(process.argv[2], 10);
if (!buildNo || buildNo <= 0) {
  // no args: print last 32 checkins
  const count = Math.min(32, lines.length);
  for (let i = 0; i < count; i++) {
    console.log(`${buildNoForIndex(i)} ${lines[i]}`);
  }
} else {
  const n = lines.length - (buildNo - 1000);
  if (n < 0 || n >= lines.length) {
    console.error(`build number ${buildNo} is out of range`);
    process.exit(1);
  }
  console.log(`${buildNo} ${lines[n]}`);
}
