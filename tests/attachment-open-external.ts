// Clicking a FileAttachment annotation whose file SumatraPDF can't open itself
// must do something: the file is extracted to the temp dir and handed to the
// shell, or, for a type the shell isn't allowed to open, shown in the file
// manager. The temp path used to be rejected as a URL, so nothing happened.
//
// TMP points at a fresh dir so the extracted file can be checked. What happened
// to it is read from the log: Explorer is unreliable without an interactive
// desktop. Explorer windows the click opened are closed.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { WM_CLOSE, enumWindows, getClassName, getClientRect, postMessage, sleep } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

// no perceived type, so the shell isn't allowed to run it: it's revealed instead
const kAttachName = "payload.bin";
const kAttachData = "hello attachment\n";

// one page with a FileAttachment icon in its center
function attachmentPdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Annots [4 0 R] >>`,
    `<< /Type /Annot /Subtype /FileAttachment /Rect [90 90 110 110] /FS 5 0 R >>`,
    `<< /Type /Filespec /F (${kAttachName}) /UF (${kAttachName}) /EF << /F 6 0 R >> >>`,
    `<< /Type /EmbeddedFile /Length ${kAttachData.length} >>\nstream\n${kAttachData}\nendstream`,
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

function explorerWindows(): Set<number> {
  const res = new Set<number>();
  enumWindows((hwnd) => {
    if (getClassName(hwnd) === "CabinetWClass") {
      res.add(hwnd);
    }
    return true;
  });
  return res;
}

// explorer windows opened since `before`
function newExplorerWindows(before: Set<number>): number[] {
  return [...explorerWindows()].filter((w) => !before.has(w));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("attachment-open-external");
  rmSync(dir, { recursive: true, force: true });
  const tempDir = join(dir, `attach-tmp-${process.pid}`);
  mkdirSync(tempDir, { recursive: true });
  const pdf = join(dir, "attachment.pdf");
  writeFileSync(pdf, attachmentPdf());

  const logPath = join(dir, "log.txt");
  const env = { TMP: tempDir, TEMP: tempDir };
  const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, pdf], { env });
  const before = explorerWindows();
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdZoomFitPage"));
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    const rc = getClientRect(canvas);
    const click = () => clickAt(canvas, Math.floor(rc.right / 2), Math.floor(rc.bottom / 2));
    await click();

    const extracted = join(tempDir, kAttachName);
    const handedOver = new RegExp(`LaunchEmbeddedDestination: (opened|showing) '.*${kAttachName}'`);
    const deadline = Date.now() + 10_000;
    let lastClick = Date.now();
    let log = "";
    while (Date.now() < deadline) {
      log = existsSync(logPath) ? readFileSync(logPath, "utf8") : "";
      if (handedOver.test(log)) {
        break;
      }
      // a click that didn't reach the link: try again
      if (!log.includes("GotoLink: opening file attachment") && Date.now() - lastClick > 2000) {
        await click();
        lastClick = Date.now();
      }
      await sleep(200);
    }
    if (!existsSync(extracted)) {
      throw new Error(`attachment-open-external: click did not extract ${extracted}`);
    }
    if (!handedOver.test(log)) {
      throw new Error("attachment-open-external: the extracted attachment was neither opened nor shown");
    }
    console.log("attachment-open-external: OK");
  } finally {
    // Explorer opens its window a moment later
    await sleep(1500);
    for (const w of newExplorerWindows(before)) {
      postMessage(w, WM_CLOSE, 0, 0);
    }
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
