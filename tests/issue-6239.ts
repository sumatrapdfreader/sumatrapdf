// The selection toolbar waited out a 500 ms debounce even after a mouse drag
// released, so it lagged behind a finished selection (#6239). A finished drag
// shows it at once; only the paint-driven re-show keeps the debounce.
//
// Run: bun tests/issue-6239.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const LINE = "The quick brown fox jumps over the lazy dog";

function makeTextPdf(): string {
  const stream = `BT /F1 18 Tf 72 700 Td (${LINE}) Tj ET`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6239.pdf");
  writeFileSync(pdf, makeTextPdf(), "latin1");
  const dir = writeAppdata(
    "issue-6239-appdata",
    ["RestoreSession = false", "CheckForUpdates = false", "ShowToc = false", "SelectionToolbar = true"].join("\n"),
  );

  const { proc, client } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // an in-process press / move / release over "quick"
    // (its exit code judges rotated glyphs, which this text has none of)
    const drag = String((await client.request(ControlCommand.TestRotatedTextMouseDrag, ["quick"]))[1] ?? "");
    if (!/selected=quick$/m.test(drag)) {
      throw new Error(`issue-6239: mouse drag did not select the word:\n${drag}`);
    }
    // the release is what finished the selection; the toolbar must be up
    // before anything else happens, not after a timer
    const dump = String((await client.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
    if (!/^visible=1$/m.test(dump)) {
      throw new Error(`issue-6239: selection toolbar not shown right after the drag released\n${dump}`);
    }
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  await client.quit();
  await proc.exited;
}

if (import.meta.main) {
  await runStandalone(testit);
}
