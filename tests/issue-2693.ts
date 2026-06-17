// Test for issue #2693: "Middle click scrolling is not smooth".
//
// The fix (src/Canvas.cpp) drives middle-click auto-scroll from a high-frequency
// timer and computes the scroll speed as a float with sub-pixel accumulation,
// instead of a 20ms timer with an integer pixels-per-tick speed.
//
// Auto-scroll speed is (cursorOffsetY / 10) pixels per 20ms. The test parks the
// cursor at offset 9: the OLD integer code computed 9/10 = 0, so the document
// would not move at all; the NEW float code computes 0.9 and accumulates it, so
// the document keeps scrolling. We measure how far it scrolls in a fixed window
// and require a clearly non-zero amount -- which fails if the fix is reverted.
//
// Drives the app via PostMessage and reads the scroll position with a
// cross-process GetScrollInfo (see project memory env-gui-automation).

import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, tmpPath } from "./util";

// minimal, valid N-page PDF (Letter-size blank pages), ASCII only so string
// length == byte length (keeps the xref offsets correct).
function makePdf(nPages: number): string {
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R >>`);
  const kids = Array.from({ length: nPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`);
  for (let i = 0; i < nPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`);
  }
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

const SETTINGS = [
  `DefaultDisplayMode = continuous`,
  `DefaultZoom = fit page`,
  `Scrollbars = windows`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2693.pdf");
  writeFileSync(pdf, makePdf(12), "latin1");

  const appdata = tmpPath("issue-2693-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const ps1 = join(ROOT, "tests", "issue-2693.verify.ps1");
  const r = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1, "-Exe", EXE, "-Pdf", pdf, "-AppData", appdata],
    { encoding: "utf8", timeout: 90_000 },
  );
  const out = (r.stdout || "") + (r.stderr || "");
  const m = out.match(/RESULT fracStart=(\d+) fracEnd=(\d+)/);
  if (!m) {
    throw new Error(`could not read scroll position; output:\n${out}`);
  }
  const start = parseInt(m[1], 10);
  const end = parseInt(m[2], 10);
  const moved = end - start;
  console.log(`  at cursor offset 9: scrolled ${moved}px in 1.5s (pos ${start} -> ${end})`);

  // new float code scrolls ~0.9px/20ms (~45px/s nominal) -> tens of px over 1.5s;
  // old integer code truncates the speed to 0 and the document stays put.
  if (moved < 15) {
    throw new Error(
      `middle-click auto-scroll did not move at fractional speed (moved ${moved}px); ` +
        `with integer speed (the un-fixed behavior) it would be ~0`,
    );
  }
  console.log(`  fractional-speed auto-scroll works (moved ${moved}px) ✓`);
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
