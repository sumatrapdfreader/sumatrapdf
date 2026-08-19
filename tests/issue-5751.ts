// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5751
//
// Custom shortcut Key = Right must bind to VK_RIGHT so it overrides the built-in
// CmdScrollRight accelerator.
//
// Run: bun tests/issue-5751.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, pressKey, sendCommandSync, killAndWait } from "./win-automation.ts";
import { sleep, VK_RIGHT } from "./winapi.ts";
import { ControlClient, ControlCommand } from "./control.ts";

function makePdf(nPages: number): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  const kids: string[] = [];
  for (let i = 0; i < nPages; i++) {
    kids.push(`${4 + i * 2} 0 R`);
  }
  body[2] = enc(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${nPages} >>`);
  body[3] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  for (let i = 0; i < nPages; i++) {
    const po = 4 + i * 2;
    const co = po + 1;
    const stream = `BT /F1 80 Tf 180 400 Td (Page ${i + 1}) Tj ET`;
    body[po] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 3 0 R >> >> /Contents ${co} 0 R >>`,
    );
    body[co] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  }
  const maxN = 3 + nPages * 2;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

const SETTINGS = [
  "ReuseInstance = false",
  "RestoreSession = false",
  "ShowStartPage = false",
  "CheckForUpdates = false",
  "DefaultDisplayMode = continuous",
  "Zoom = 400",
  "Shortcuts = [",
  "  [",
  "    Cmd = CmdGoToNextPage",
  "    Key = Right",
  "  ]",
  "]",
].join("\n");

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const raw = String(res[1] ?? "");
  const m = /OK page=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-5751: could not read current page: ${raw.trim()}`);
  }
  return parseInt(m[1], 10);
}

async function waitForPage(client: ControlClient, want: number): Promise<void> {
  const deadline = Date.now() + 3000;
  let page = 0;
  while (Date.now() < deadline) {
    page = await currentPage(client);
    if (page === want) {
      return;
    }
    await sleep(20);
  }
  throw new Error(`issue-5751: current page is ${page}, want ${want}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5751");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-5751.pdf");
  writeFileSync(pdf, makePdf(5));
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await waitForPage(client, 1);
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await waitForPage(client, 2);

    sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
    await waitForPage(client, 1);
    await pressKey(frame, VK_RIGHT, 80);
    await waitForPage(client, 2);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("PASS: Right keyboard shortcut rebind reaches the same page as CmdGoToNextPage (issue #5751)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
