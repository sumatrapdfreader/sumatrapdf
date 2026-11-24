// Set while the suite is inside the block of tests that share one process.
// win-automation.ts reads it; this file stays free of that import.

let wanted = false;

export function setProcessReuse(on: boolean): void {
  wanted = on;
}

export function processReuseWanted(): boolean {
  return wanted;
}
