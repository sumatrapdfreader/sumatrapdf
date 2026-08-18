// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5969
//
// ScrollbarInSinglePage + Fit Single Page + zoom in hung: SetScrollInfo hid
// the page-number bar (range does not need scrolling), ShowScrollBar showed
// it again, WM_SIZE re-entered UpdateScrollbars.
//
// Repro needs Windows scrollbars and a maximized window (issue comments).
//
// Run:  bun tests/issue-5969.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, makeMinimalPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
ShowToc = false
UseTabs = false
Scrollbars = windows
ScrollbarInSinglePage = true
WindowState = 2
WindowPos = 200 100 1000 800
`;

function makeTwoPagePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function withTimeout<T>(p: Promise<T>, ms: number, msg: string): Promise<T> {
  let t: ReturnType<typeof setTimeout> | undefined;
  try {
    return await Promise.race([
      p,
      new Promise<never>((_, reject) => {
        t = setTimeout(() => reject(new Error(msg)), ms);
      }),
    ]);
  } finally {
    if (t !== undefined) {
      clearTimeout(t);
    }
  }
}

async function queryZoom(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestDisplayMode, ["get"]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK mode=.+ zoom=(.+)$/.exec(raw);
      if (!m) {
        throw new Error(`issue-5969: could not parse: ${raw}`);
      }
      return m[1]!.trim();
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5969: TestDisplayMode failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5969: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

// The hang is a UI-thread loop in UpdateScrollbars. A control command that
// runs on that thread is enough to prove we did not hang. Do not wait for
// tiles: a maximized 400–800% view often stays WaitRenderIdle timeout-busy
// (q=0, a tile still in flight) even though the app is responding.
async function zoomInOrHang(client: ControlClient, frame: number, prevZoom: string): Promise<string> {
  sendCommand(frame, cmdId("CmdZoomIn"));
  const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
  const hangMsg = "issue-5969: app stopped responding after Zoom In";
  for (;;) {
    const left = deadline - Date.now();
    if (left <= 0) {
      throw new Error(`${hangMsg} (still ${prevZoom})`);
    }
    const z = await withTimeout(queryZoom(client), left, hangMsg);
    if (z !== prevZoom) {
      return z;
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

async function zoomInSinglePage(client: ControlClient, proc: { pid?: number }, label: string): Promise<void> {
  const frame = await waitForFrame(proc.pid!);
  if (!frame) {
    throw new Error(`${label}: no frame`);
  }
  sendCommand(frame, cmdId("CmdZoomFitPageAndSinglePage"));
  // Fit-page tiles are small; this also fails the test if Fit Single Page hangs.
  await withTimeout(
    client.waitForRenderIdle(),
    15_000 * SLOW_BUILD_FACTOR,
    `${label}: app stopped responding after Fit Single Page`,
  );
  let zoom = await queryZoom(client);
  const before = zoom;
  for (let i = 0; i < 8; i++) {
    zoom = await zoomInOrHang(client, frame, zoom);
  }
  if (zoom === before) {
    throw new Error(`${label}: zoom did not change after Zoom In (was ${before})`);
  }
  console.log(`  ${label}: fit-page ${before} -> ${zoom} without hanging ✓`);
}

export async function testit(): Promise<void> {
  const appDataDir = tmpPath("issue-5969-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  writeFileSync(join(appDataDir, "SumatraPDF-settings.txt"), SETTINGS);

  const twoPage = tmpPath("issue-5969-two.pdf");
  writeFileSync(twoPage, makeTwoPagePdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await zoomInSinglePage(client, proc, "issue-5969 two-page");
    },
    ["-appdata", appDataDir, twoPage],
    { defaultWindowPos: true },
  );

  const onePage = tmpPath("issue-5969-one.pdf");
  writeFileSync(onePage, makeMinimalPdf("issue-5969"));
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await zoomInSinglePage(client, proc, "issue-5969 one-page");
    },
    ["-appdata", appDataDir, onePage],
    { defaultWindowPos: true },
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
