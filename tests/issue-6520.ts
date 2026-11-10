// discussion #6520: Save Trimmed Pages As writes the trim the view shows into
// each page's CropBox, so the saved PDF opens trimmed everywhere.
import { existsSync, rmSync, writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";

const SAVE_MS = 10_000;
// points the saved page may differ from the content box it was trimmed to
const TOLERANCE = 1;

// two pages with text at different places, so each gets its own crop
function makePdf(): Buffer {
  const page = (x: number, y: number) => `BT /F1 24 Tf ${x} ${y} Td (Trimmed) Tj ET`;
  const s1 = page(100, 600);
  const s2 = page(300, 150);
  const body = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 6 0 R >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 7 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${s1.length} >>\nstream\n${s1}\nendstream`,
    `<< /Length ${s2.length} >>\nstream\n${s2}\nendstream`,
  ];
  let out = "%PDF-1.4\n";
  const offsets: number[] = [];
  body.forEach((b, i) => {
    offsets.push(out.length);
    out += `${i + 1} 0 obj\n${b}\nendobj\n`;
  });
  const xrefPos = out.length;
  out += `xref\n0 ${body.length + 1}\n0000000000 65535 f \n`;
  for (const o of offsets) {
    out += `${String(o).padStart(10, "0")} 00000 n \n`;
  }
  out += `trailer\n<< /Size ${body.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(out, "latin1");
}

type Box = { x: number; y: number; dx: number; dy: number };

async function pageBoxes(client: ControlClient, pageNo: number): Promise<string> {
  const deadline = Date.now() + SAVE_MS;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageBoxes, [pageNo]);
    const raw = String(res[1] ?? "").trim();
    if (res[0] === 0) {
      return raw;
    }
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`issue-6520: TestPageBoxes failed: ${raw}`);
    }
    await sleep(100);
  }
}

function boxOf(raw: string, name: string): Box {
  const m = new RegExp(`\\b${name}=([-\\d.]+),([-\\d.]+),([-\\d.]+),([-\\d.]+)`).exec(raw);
  if (!m) {
    throw new Error(`issue-6520: no ${name} in: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  const res = await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  if (res[0] !== 0) {
    throw new Error(`issue-6520: ${cmd} failed: ${String(res[1] ?? "")}`);
  }
}

export async function testit(): Promise<void> {
  const src = tmpPath("issue-6520.pdf");
  const dst = tmpPath("issue-6520 trimmed.pdf");
  writeFileSync(src, makePdf());
  rmSync(dst, { force: true });

  // the content box of each page, which Trim Empty Margins shows
  const content: Box[] = [];
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      for (const pageNo of [1, 2]) {
        content.push(boxOf(await pageBoxes(client, pageNo), "engineContent"));
      }
      await invoke(client, "CmdToggleTrimEmptyMargins");
      await client.request(ControlCommand.TestSaveFileName, [dst]);
      await invoke(client, "CmdSaveTrimmedPagesAs");
      const deadline = Date.now() + SAVE_MS;
      while (!existsSync(dst)) {
        if (Date.now() > deadline) {
          throw new Error("issue-6520: Save Trimmed Pages As wrote nothing");
        }
        await sleep(100);
      }
    },
    [src],
  );

  // the saved pages are the content boxes, each with a CropBox
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      for (const pageNo of [1, 2]) {
        const raw = await pageBoxes(client, pageNo);
        boxOf(raw, "crop");
        const page = boxOf(raw, "engineMedia");
        const want = content[pageNo - 1]!;
        if (Math.abs(page.dx - want.dx) > TOLERANCE || Math.abs(page.dy - want.dy) > TOLERANCE) {
          throw new Error(
            `issue-6520: page ${pageNo} is ${page.dx}x${page.dy}, expected the content ${want.dx}x${want.dy}`,
          );
        }
      }
    },
    [dst],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
