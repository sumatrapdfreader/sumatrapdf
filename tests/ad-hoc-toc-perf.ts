// Run with -exe out/rel64/SumatraPDF.exe; TOC_PROFILE_LABEL names the xperf artifacts.
// TOC_PROFILE_PDF optionally replaces the generated large TOC with a real document.
import { strict as assert } from "node:assert";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makeTocPdf } from "./issue-6167";
import { cmdId, ROOT, runStandalone, tmpPath, writeAppdata } from "./util";
import { killAndWait, launchSumatra, sendCommandSync, waitForFrame, waitForTitle } from "./win-automation";
import { findChildWindow, getWindowText, sendMessage, TVM_GETCOUNT } from "./winapi";

const width = 20;
const switches = 100;

function xperf(args: string[]): void {
  const result = Bun.spawnSync(["xperf", ...args], { stdout: "pipe", stderr: "pipe" });
  if (result.exitCode !== 0) {
    throw new Error(`xperf ${args.join(" ")}: ${result.stdout.toString()} ${result.stderr.toString()}`);
  }
}

export async function testit(): Promise<void> {
  if (!Bun.which("xperf")) {
    console.log("SKIP: install xperf from the Windows Performance Toolkit and run from an elevated shell.");
    return;
  }
  const label = process.env.TOC_PROFILE_LABEL || "toc-perf";
  assert.match(label, /^[\w-]+$/);
  const output = join(ROOT, ".work", "toc-profiles");
  mkdirSync(output, { recursive: true });
  const trace = join(output, `${label}.etl`);
  const files = [tmpPath("toc-perf-a.pdf"), tmpPath("toc-perf-b.pdf")];
  const source = process.env.TOC_PROFILE_PDF;
  const pdf = source ? readFileSync(source) : makeTocPdf(width);
  for (const file of files) {
    writeFileSync(file, pdf);
  }
  const appdata = writeAppdata("toc-perf-settings", "NoHomeTab = true\nShowToc = true\n");
  const proc = launchSumatra(["-appdata", appdata, ...files]);
  let recording = false;
  try {
    const frame = await waitForFrame(proc.pid);
    await waitForTitle(frame, (title) => title.includes("toc-perf-b.pdf"));
    const nextTab = cmdId("CmdNextTab");
    for (let i = 0; i < 10; i++) {
      sendCommandSync(frame, nextTab);
    }
    const tree = findChildWindow(frame, "SysTreeView32");
    assert.ok(tree, "TOC tree exists");
    const inserted = Number(sendMessage(tree, TVM_GETCOUNT, 0, 0));
    xperf([
      "-on",
      "PROC_THREAD+LOADER+PROFILE",
      "-stackwalk",
      "Profile",
      "-BufferSize",
      "1024",
      "-MinBuffers",
      "64",
      "-MaxBuffers",
      "256",
      "-f",
      tmpPath(`${label}-raw.etl`),
    ]);
    recording = true;
    const times: number[] = [];
    const started = performance.now();
    for (let i = 0; i < switches; i++) {
      const before = performance.now();
      sendCommandSync(frame, nextTab);
      times.push(performance.now() - before);
      const name = i % 2 === 0 ? "toc-perf-a.pdf" : "toc-perf-b.pdf";
      assert.ok(getWindowText(frame).includes(name), `switch ${i} reached ${name}`);
    }
    const elapsedMs = performance.now() - started;
    xperf(["-d", trace]);
    recording = false;
    const sorted = [...times].sort((a, b) => a - b);
    const result = {
      label,
      pid: proc.pid,
      source,
      nodes: source ? undefined : width + width ** 2 + width ** 3,
      inserted,
      switches,
      elapsedMs,
      medianMs: sorted[Math.floor(sorted.length / 2)],
      p95Ms: sorted[Math.floor(sorted.length * 0.95)],
      times,
      trace,
    };
    writeFileSync(join(output, `${label}-timing.json`), JSON.stringify(result, null, 2));
    console.log(JSON.stringify({ ...result, times: undefined }, null, 2));
  } finally {
    if (recording) {
      xperf(["-d", trace]);
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
