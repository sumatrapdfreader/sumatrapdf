// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3744
//
// "Go to Next/Previous Favorite" jump to the nearest favorite (bookmark) page
// after / before the current page (no wrap-around).
//
// Driven entirely via -dbg-control (TestFavoriteNav): no GUI automation,
// dialogs, or screenshots.
//
// Run:  bun tests/issue-3744.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// minimal N-page PDF (content doesn't matter; we assert CurrentPageNo).
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
    const stream = `BT /F1 24 Tf 72 720 Td (Page ${i + 1}) Tj ET`;
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

function parsePage(raw: string): number {
  const m = /page=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`missing page= in control response: ${raw.trim()}`);
  }
  return Number(m[1]);
}

async function favNav(client: ControlClient, action: string, pageNo?: number): Promise<number> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const args: (string | number)[] = pageNo === undefined ? [action] : [action, pageNo];
    const res = await client.request(ControlCommand.TestFavoriteNav, args);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "");
    if (exitCode === 0) {
      return parsePage(raw);
    }
    if (exitCode === 2 && raw.includes("NOTREADY")) {
      if (Date.now() > deadline) {
        throw new Error(`issue-3744: timed out waiting for doc: ${raw.trim()}`);
      }
      await new Promise((r) => setTimeout(r, 50));
      continue;
    }
    throw new Error(`issue-3744: TestFavoriteNav ${action} failed: ${raw.trim()}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-3744");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-3744.pdf");
  writeFileSync(pdf, makePdf(25));

  // isolated appdata so favorites can't touch the tester's settings
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "ReuseInstance = false\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      await favNav(client, "add", 10);
      await favNav(client, "add", 20);

      const on1 = await favNav(client, "goto", 1);
      if (on1 !== 1) {
        throw new Error(`issue-3744: expected page 1 after goto, got ${on1}`);
      }

      const to10 = await favNav(client, "next");
      if (to10 !== 10) {
        throw new Error(`issue-3744: next from 1 expected 10, got ${to10}`);
      }

      const to20 = await favNav(client, "next");
      if (to20 !== 20) {
        throw new Error(`issue-3744: next from 10 expected 20, got ${to20}`);
      }

      const stay20 = await favNav(client, "next");
      if (stay20 !== 20) {
        throw new Error(`issue-3744: next at last favorite should stay on 20, got ${stay20}`);
      }

      const back10 = await favNav(client, "prev");
      if (back10 !== 10) {
        throw new Error(`issue-3744: prev from 20 expected 10, got ${back10}`);
      }

      console.log("PASS: Go to Next/Previous Favorite works (issue #3744)");
    },
    ["-appdata", appdata, pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
