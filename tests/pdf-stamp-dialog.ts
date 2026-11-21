// Add Watermark and Add Page Numbers (CmdPdfWatermark, CmdPdfAddPageNumbers):
// the dialog opens with its text selected, typing replaces it, Enter writes a
// new PDF next to the document with the text on its pages and opens it; the
// document itself stays as it was.
//
// ng only: orig's dialogs are win32 ones this doesn't drive.
// Run: bun tests/pdf-stamp-dialog.ts [--no-build]

import { copyFileSync, mkdirSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { EXE, pollUntil, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const name = "pdf-stamp-dialog";
const VK_RETURN = 0x0d;

async function tool(client: ControlClient, args: (string | number)[]): Promise<string> {
  return String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
}

// the PDFs in dir other than the original
function newPdfs(dir: string): string[] {
  return readdirSync(dir).filter((f) => f.endsWith(".pdf") && f !== "doc.pdf");
}

function cliText(pdf: string): string {
  return Bun.spawnSync([EXE, "cli", "text", pdf], { stdout: "pipe" }).stdout.toString();
}

async function runDialog(cmd: string, window: string, typed: string | null, dir: string): Promise<string> {
  const src = join(dir, "doc.pdf");
  const { proc, client } = await launchControlled([src]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, [cmd]);
    // keys sent before the dialog's first frame are lost
    await pollUntil(
      async () => tool(client, ["layout", window]),
      (l) => l.includes("pdftool-text") || l.includes("label='Cancel'"),
      { timeoutMs: 5000 * SLOW_BUILD_FACTOR, error: (l) => `${name}: ${window} never drew:\n${l}` },
    );
    if (typed !== null) {
      // the text starts selected: typing replaces it
      for (const ch of typed) {
        await tool(client, ["input", window, "char", ch.charCodeAt(0), 0, 0, 0]);
      }
    }
    await tool(client, ["input", window, "key", VK_RETURN, 0, 0, 0]);
    const files = await pollUntil(
      async () => newPdfs(dir),
      (f) => f.length === 1,
      { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (f) => `${name}: ${cmd} wrote ${JSON.stringify(f)}` },
    );
    return join(dir, files[0]!);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const original = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const before = readFileSync(original);
  const dir = tmpPath(name);

  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  copyFileSync(original, join(dir, "doc.pdf"));
  let out = await runDialog("CmdPdfWatermark", "pdfwatermark", null, dir);
  let text = cliText(out);
  expect(`the default watermark is on both pages of ${out}`, text.split("DRAFT").length - 1 === 2, text);

  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  copyFileSync(original, join(dir, "doc.pdf"));
  out = await runDialog("CmdPdfAddPageNumbers", "pdfpagenumbers", "Page {n} of {total}", dir);
  text = cliText(out);
  expect("page numbers", text.includes("Page 1 of 2") && text.includes("Page 2 of 2"), text);

  expect("the document stays as it was", readFileSync(join(dir, "doc.pdf")).equals(before), "");
  console.log(`${name}: OK`);
}

function expect(what: string, ok: boolean, detail: string): void {
  if (!ok) {
    throw new Error(`${name}: ${what}:\n${detail.slice(0, 1500)}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
