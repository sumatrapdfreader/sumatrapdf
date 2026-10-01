// Test for discussion #6163: CmdCopyLocationToClipboard copies the current
// view as the cmd-line args that re-open it, e.g.
//   -page 3 -zoom "400%" -scroll 0,0 "C:\dir\file.pdf"
//
// Run:  bun tests/issue-6163.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommandSync } from "./win-automation.ts";
import { getClipboardText } from "./winapi.ts";

function makePdf(nPages: number): Buffer {
  const kids: string[] = [];
  const pages: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kids.push(`${3 + i} 0 R`);
    pages.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>");
  }
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${nPages} >>`,
    ...pages,
  ];
  return Buffer.from(assemblePdf(objs, { header: "%PDF-1.7\n" }), "latin1");
}

// split "-page 3 -zoom "400%" -scroll 0,12 "c:\a b\f.pdf"" into argv
function splitArgs(s: string): string[] {
  const args = s.match(/"[^"]*"|\S+/g) ?? [];
  return args.map((a) => (a.startsWith('"') ? a.slice(1, -1) : a));
}

// launch with `args`, scroll down `nScrolls` times, return what the command copies
async function copyLocation(args: string[], nScrolls: number): Promise<string> {
  await killProcessesNamed("SumatraPDF.exe");
  const { proc, client, frame } = await launchControlled(args);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();

    for (let i = 0; i < nScrolls; i++) {
      sendCommandSync(frame, cmdId("CmdScrollDown"));
    }
    await client.waitForRenderIdle();

    sendCommandSync(frame, cmdId("CmdCopyLocationToClipboard"));
    return (await getClipboardText()).trim();
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6163");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "loc.pdf");
  writeFileSync(pdf, makePdf(5));

  const got = await copyLocation(["-page", "3", "-zoom", "400%", pdf], 10);
  console.log(`clipboard: ${got}`);

  const rx = /^-page 3 -zoom "400%" -scroll (-?\d+),(-?\d+) "(.*)"$/;
  const m = rx.exec(got);
  if (!m) {
    throw new Error(`clipboard doesn't look like a location: '${got}'`);
  }
  if (m[3].toLowerCase() !== pdf.toLowerCase()) {
    throw new Error(`wrong path in '${got}', expected '${pdf}'`);
  }

  // re-opening with the copied args must land on the same location. -scroll is
  // whole points, so the position comes back within a point or two
  const got2 = await copyLocation(splitArgs(got), 0);
  console.log(`clipboard after re-open: ${got2}`);
  const m2 = rx.exec(got2);
  if (!m2) {
    throw new Error(`re-opening with the copied args gave: '${got2}'`);
  }
  for (const i of [1, 2]) {
    if (Math.abs(parseInt(m[i], 10) - parseInt(m2[i], 10)) > 2) {
      throw new Error(`re-opening with the copied args scrolled elsewhere:\n  ${got}\n  ${got2}`);
    }
  }
  console.log("PASS: location copied to clipboard and round-trips");
}

if (import.meta.main) {
  await runStandalone(testit);
}
