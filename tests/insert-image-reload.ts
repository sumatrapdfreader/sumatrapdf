// Insert Image: the document reloads (file changed on disk) while the image
// file dialog is open. Placing the stamp must not use the DisplayModel freed
// by the reload (crashed most runs).
import { appendFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  isWindowVisible,
  postMessage,
  sendText,
  sleep,
  WM_COMMAND,
} from "./winapi.ts";

const IDOK = 1;

// 1x1 red PNG
const kPngBase64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";

function onePagePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] >>",
  ];
  let out = "%PDF-1.4\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = out.length;
  out += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    out += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return out;
}

async function waitForFileDialog(pid: number, timeoutMs = 8000): Promise<number> {
  for (const deadline = Date.now() + timeoutMs; Date.now() < deadline;) {
    let dlg = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getClassName(hwnd) === "#32770") {
        dlg = hwnd;
        return false;
      }
      return true;
    });
    if (dlg) {
      return dlg;
    }
    await sleep(100);
  }
  return 0;
}

// the file name edit (inside the cmb13 combo box)
function findFileNameEdit(dlg: number): number {
  let edit = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    edit = hwnd;
    return false;
  });
  return edit;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("  skipped: original Windows file dialog only");
    return;
  }

  const dir = tmpPath("insert-image-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  const png = join(dir, "stamp.png");
  writeFileSync(pdf, onePagePdf());
  writeFileSync(png, Buffer.from(kPngBase64, "base64"));

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdInsertImage"));
    const dlg = await waitForFileDialog(proc.pid!);
    if (!dlg) {
      throw new Error("insert-image-reload: image file dialog did not open");
    }

    // change the file so the document auto-reloads under the dialog
    appendFileSync(pdf, "\n");
    await sleep(2500);

    const edit = findFileNameEdit(dlg);
    if (!edit) {
      throw new Error("insert-image-reload: no file name edit in the dialog");
    }
    sendText(edit, png);
    postMessage(dlg, WM_COMMAND, IDOK, 0);

    await sleep(1500);
    if (proc.exitCode !== null) {
      throw new Error(`insert-image-reload: SumatraPDF exited (code ${proc.exitCode}) after inserting the image`);
    }
    await client.waitForRenderIdle();
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("insert-image-reload: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
