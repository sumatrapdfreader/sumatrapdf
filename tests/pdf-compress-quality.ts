// Compress PDF's Pictures drop-down: Balanced scales a 300 dpi picture down to
// 150 dpi (as JPEG); Same quality, the default, leaves it.
//
// ng only: orig's dialog is a Win32 combo box this test can't pick in.
// Run: bun tests/pdf-compress-quality.ts [--no-build]

import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { photoPdf } from "./cli-pdf-tools.ts";
import { type ControlClient, ControlCommand } from "./control.ts";
import { cmdId, EXE, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const name = "pdf-compress-quality";
const kTool = "pdfcompress";
const kSame = "Same quality";
const kBalanced = "Balanced (150 dpi pictures)";

// a node by its text: a button's label, a Select's value
async function layoutNode(client: ControlClient, label: string): Promise<{ x: number; y: number }> {
  const line = await pollUntil(
    async () => {
      const res = await client.request(ControlCommand.TestToolWindow, ["layout", kTool, "", 0, 0, 0, 0]);
      return String(res[1] ?? "")
        .split("\n")
        .find((l) => l.startsWith("node ") && (l.includes(`label='${label}'`) || l.includes(`value='${label}'`)));
    },
    (l) => !!l,
    { timeoutMs: 10000 * SLOW_BUILD_FACTOR, error: () => `${name}: no '${label}' in the dialog` },
  );
  const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line!)!;
  return { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) };
}

async function layoutText(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolWindow, ["layout", kTool, "", 0, 0, 0, 0]);
  return String(res[1] ?? "");
}

function nodeCount(text: string, label: string): number {
  return text
    .split("\n")
    .filter((l) => l.startsWith("node ") && (l.includes(`label='${label}'`) || l.includes(`value='${label}'`))).length;
}

async function click(client: ControlClient, label: string): Promise<void> {
  const at = await layoutNode(client, label);
  await client.request(ControlCommand.TestToolWindow, ["input", kTool, "click", at.x, at.y, 0, 0]);
}

// The list's row can be in the layout a frame before it takes a click. A miss
// leaves the default, Same quality, which does not scale the picture.
async function chooseBalanced(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 10000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const text = await layoutText(client);
    if (nodeCount(text, kBalanced) === 1 && nodeCount(text, kSame) === 0) {
      return;
    }
    if (nodeCount(text, kBalanced) === 0) {
      await click(client, kSame);
    } else {
      await click(client, kBalanced);
    }
    if (Date.now() > deadline) {
      throw new Error(`${name}: Pictures stayed on Same quality\n${await layoutText(client)}`);
    }
    await sleep(50);
  }
}

function imageWidth(pdf: string, dir: string): number {
  const plain = join(dir, "plain.pdf");
  const p = Bun.spawnSync([EXE, "cli", "decompress", pdf, "-o", plain]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: decompress failed: ${p.stderr.toString()}`);
  }
  return Number(/\/Width (\d+)/.exec(readFileSync(plain, "latin1"))?.[1] ?? 0);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const src = join(dir, "photo.pdf");
  writeFileSync(src, photoPdf());

  const { proc, client, frame } = await launchControlled([src]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdPdfCompress"));
    await chooseBalanced(client);
    await click(client, "Compress PDF");
    const out = await pollUntil(
      async () => readdirSync(dir).find((f) => f !== "photo.pdf" && f !== "plain.pdf" && f.endsWith(".pdf")),
      (f) => !!f && existsSync(join(dir, f)),
      { timeoutMs: 30000 * SLOW_BUILD_FACTOR, error: () => `${name}: no compressed PDF in ${dir}` },
    );
    // written once the app opens it
    await client.waitForRenderIdle();
    const w = imageWidth(join(dir, out!), dir);
    if (Math.abs(w - 1275) > 2) {
      throw new Error(`${name}: Balanced left the picture ${w} pixels wide, want 1275`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
