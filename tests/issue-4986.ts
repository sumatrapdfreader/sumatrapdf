// Favorites restore the scroll position on the page (issue #4986).
//
// Add a favorite part-way down a zoomed page, jump to the top, then jump to
// the favorite: the view must come back down the page, not only to page 1.
//
// Run: bun tests/issue-4986.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, launchControlled, killAndWait } from "./win-automation.ts";
import { sendMessage, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DOWN = 0xff880000n;

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const stream = "BT /F1 24 Tf 72 720 Td (page 1 top) Tj ET BT /F1 24 Tf 72 72 Td (page 1 bottom) Tj ET";
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= 5; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 6\n0000000000 65535 f \n`;
  for (let n = 1; n <= 5; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

type FavPos = { page: number; y: number };

async function favNav(client: ControlClient, action: string, pageNo?: number): Promise<FavPos> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const args: (string | number)[] = pageNo === undefined ? [action] : [action, pageNo];
    const res = await client.request(ControlCommand.TestFavoriteNav, args);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "");
    const m = /page=(\d+)(?: y=(-?\d+))?/.exec(raw);
    if (exitCode === 0 && m) {
      return { page: Number(m[1]), y: m[2] !== undefined ? Number(m[2]) : -1 };
    }
    if (exitCode === 2 && raw.includes("NOTREADY")) {
      if (Date.now() > deadline) {
        throw new Error(`issue-4986: timed out waiting for doc: ${raw.trim()}`);
      }
      await sleep(50);
      continue;
    }
    throw new Error(`issue-4986: TestFavoriteNav ${action} failed: ${raw.trim()}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-4986");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-4986.pdf");
  writeFileSync(pdf, makePdf());
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "ReuseInstance = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "SmoothScroll = false",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "400",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-4986: no canvas");
    }

    const atTop = await favNav(client, "page");
    if (atTop.page !== 1) {
      throw new Error(`issue-4986: expected page 1, got ${atTop.page}`);
    }

    for (let i = 0; i < 12; i++) {
      sendMessage(canvas, WM_MOUSEWHEEL, WHEEL_DOWN, 0n);
      await sleep(40);
    }
    await client.waitForRenderIdle();

    const scrolled = await favNav(client, "page");
    if (scrolled.y <= 20) {
      throw new Error(`issue-4986: expected to scroll down, y=${scrolled.y}`);
    }

    await favNav(client, "add", 1);
    await favNav(client, "goto", 1);
    await client.waitForRenderIdle();
    const afterTop = await favNav(client, "page");
    if (afterTop.y > 20) {
      throw new Error(`issue-4986: goto page 1 should be at the top, y=${afterTop.y}`);
    }

    await favNav(client, "goto-fav", 1);
    await client.waitForRenderIdle();
    const restored = await favNav(client, "page");
    if (restored.page !== 1) {
      throw new Error(`issue-4986: favorite jumped to page ${restored.page}`);
    }
    if (Math.abs(restored.y - scrolled.y) > 40) {
      throw new Error(`issue-4986: favorite y=${restored.y}, want about ${scrolled.y}`);
    }

    console.log(`PASS: favorite restored scroll y=${restored.y} (was ${scrolled.y}) (issue #4986)`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
