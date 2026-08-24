// Saving annotations to the current PDF and file-watcher reloads refresh the
// existing annotation editor instead of destroying and recreating its HWND.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowText,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_COMMAND,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
ReloadModifiedDocuments = true
`;

function makePdf(count: number): string {
  const annots: string[] = [];
  for (let i = 0; i < count; i++) {
    const x = 50 + i * 30;
    annots.push(`<< /Type /Annot /Subtype /Text /Rect [${x} 700 ${x + 20} 720] /Contents (note ${i + 1}) >>`);
  }
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  body += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

type AnnotState = { selected: number; count: number; ignoreReload: boolean; raw: string };

async function annotState(client: ControlClient, selectItem = 0): Promise<AnnotState | null> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, selectItem]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "").trim();
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`annotation-editor-reload: TestAnnotEditorLayout failed: ${raw}`);
  }
  const m = / sel=(-?\d+) n=(\d+)/.exec(raw);
  const flags = / ignoreReload=(\d) reloadOnFocus=(\d)/.exec(raw);
  if (!m || !flags) {
    throw new Error(`annotation-editor-reload: could not parse: ${raw}`);
  }
  return { selected: +m[1]!, count: +m[2]!, ignoreReload: flags[1] === "1", raw };
}

async function waitForAnnotCount(client: ControlClient, want: number): Promise<AnnotState> {
  const deadline = Date.now() + 10_000;
  let last: AnnotState | null = null;
  while (Date.now() < deadline) {
    last = await annotState(client);
    if (last?.count === want) {
      return last;
    }
    await sleep(50);
  }
  throw new Error(`annotation-editor-reload: expected ${want} annotations, got ${last?.raw ?? "not ready"}`);
}

async function waitForOwnSaveReloadToClear(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 10_000;
  let last: AnnotState | null = null;
  while (Date.now() < deadline) {
    last = await annotState(client);
    if (last && !last.ignoreReload) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`annotation-editor-reload: save watcher event was not consumed: ${last?.raw ?? "not ready"}`);
}

function findAnnotWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd !== frame && getWindowPid(hwnd) === pid && getWindowText(hwnd).startsWith("Annotations")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function assertSameEditor(pid: number, expected: number, marker: string, action: string): void {
  const actualPid = getWindowPid(expected);
  const actualTitle = getWindowText(expected);
  if (actualPid !== pid || actualTitle !== marker) {
    throw new Error(
      `annotation-editor-reload: editor window changed after ${action}: hwnd=${expected} pid=${actualPid} title=${actualTitle}`,
    );
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annotation-editor-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "watched.pdf");
  writeFileSync(pdf, makePdf(1), "latin1");
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await waitForAnnotCount(client, 1);
    const editor = findAnnotWindow(proc.pid!, frame);
    if (!editor) {
      throw new Error("annotation-editor-reload: annotation editor did not open");
    }
    // A newly created editor may receive the same recycled HWND. A custom
    // title survives only when this exact native window remains alive.
    const editorMarker = "annotation-editor-reload marker";
    sendText(editor, editorMarker);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(100, 100));
    await waitForAnnotCount(client, 2);
    const selectedBeforeSave = await annotState(client, 1);
    if (!selectedBeforeSave || selectedBeforeSave.selected !== 0) {
      throw new Error(`annotation-editor-reload: could not select first annotation: ${selectedBeforeSave?.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdSaveAnnotations"));
    assertSameEditor(proc.pid!, editor, editorMarker, "saving");
    const afterSave = await waitForAnnotCount(client, 2);
    if (afterSave.selected !== 0) {
      throw new Error(`annotation-editor-reload: selection was not restored after saving: ${afterSave.raw}`);
    }

    await waitForOwnSaveReloadToClear(client);
    writeFileSync(pdf, makePdf(3), "latin1");
    const afterExternalReload = await waitForAnnotCount(client, 3);
    assertSameEditor(proc.pid!, editor, editorMarker, "external reload");
    if (afterExternalReload.selected !== 0) {
      throw new Error(
        `annotation-editor-reload: selection was not restored after external reload: ${afterExternalReload.raw}`,
      );
    }

    console.log("annotation-editor-reload: OK");
  } catch (error) {
    const exitCode = await Promise.race([proc.exited, sleep(200).then(() => null)]);
    throw new Error(`${String(error)}; process exit=${exitCode ?? "still running"}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
