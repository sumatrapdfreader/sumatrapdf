// Make PDF From Images (CmdImagesToPdf): the images picked become a PDF with a
// page per image, in name order (scan2 before scan10), saved where the Save As
// question says and opened. The dialogs are answered through -dbg-control.
//
// Run: bun tests/images-to-pdf.ts [--no-build] [-ng]

import { copyFileSync, existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, EXE, ROOT, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { getWindowText } from "./winapi.ts";

const name = "images-to-pdf";

type Info = { pages: number; pageSize: { width: number; height: number } };

function pdfInfo(path: string): Info {
  const p = Bun.spawnSync([EXE, "cli", "info", path, "--json"], { stdout: "pipe", stderr: "pipe" });
  return JSON.parse(p.stdout.toString());
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  // scan2 is 400 x 400, scan10 400 x 2000: page 1's size tells the order
  const scan2 = join(dir, "scan2.jpg");
  const scan10 = join(dir, "scan10.jpg");
  copyFileSync(join(ROOT, "tests", "issue-6229.jpg"), scan2);
  copyFileSync(join(ROOT, "tests", "issue-6225.jpg"), scan10);
  const dest = join(dir, "scans.pdf");

  const { proc, client, frame } = await launchControlled([]);
  try {
    // picked in the "wrong" order: the PDF goes by name
    await client.request(ControlCommand.TestOpenFileNames, [`${scan10}\n${scan2}`]);
    await client.request(ControlCommand.TestSaveFileName, [dest]);
    sendCommand(frame, cmdId("CmdImagesToPdf"));

    await pollUntil(
      () => existsSync(dest) && getWindowText(frame).includes("scans.pdf"),
      (ok) => ok,
      { error: () => `${name}: not made and opened: '${getWindowText(frame)}' exists=${existsSync(dest)}` },
    );
    const info = pdfInfo(dest);
    if (info.pages !== 2) {
      throw new Error(`${name}: ${info.pages} pages, want 2`);
    }
    if (info.pageSize.width !== 400 || info.pageSize.height !== 400) {
      throw new Error(`${name}: page 1 is ${JSON.stringify(info.pageSize)}, want scan2's 400 x 400`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
