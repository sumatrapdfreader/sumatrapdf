// #5971: with Document Properties still open, Home/End on the main window
// stopped navigating the PDF. FindAcceleratorsForHwnd treated the owner frame
// as the properties edit, so Home/End (unsafe in an edit) were not translated.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, WM_CLOSE, WM_KEYDOWN } from "./winapi";
import { killAndWait, launchControlled, sendCommandSync, waitForExit } from "./win-automation";

const VK_HOME_KEY = 0x24;

function buildTwoPagePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function currentPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageBoxes, []);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      const m = /OK page=(\d+)/.exec(out);
      if (m) {
        return parseInt(m[1]!, 10);
      }
      throw new Error(`issue-5971: could not parse page: ${out}`);
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5971: TestPageBoxes failed: ${out}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5971: document never ready: ${out}`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

async function waitForPage(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 8_000;
  let last = -1;
  for (;;) {
    last = await currentPage(client);
    if (last === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5971: ${what} (page=${last}, want=${want})`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5971.pdf");
  writeFileSync(pdf, buildTwoPagePdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdGoToLastPage"));
    await waitForPage(client, 2, "did not reach last page");

    sendCommandSync(frame, cmdId("CmdProperties"));
    // posted to the frame, the way a key lands after clicking back on the
    // main window without closing Properties
    postMessage(frame, WM_KEYDOWN, VK_HOME_KEY, 0);
    await waitForPage(client, 1, "Home did not go to the first page while Properties was open");

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5971: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5971: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
