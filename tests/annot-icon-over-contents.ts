// Edit Annotations: selecting a Text annot shows the Icon dropdown. It used to
// appear at (0,0) over Contents, then move with SWP_NOREDRAW and leave a ghost.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

function makePdf(): string {
  const annots = [
    `<< /Type /Annot /Subtype /Highlight /Rect [50 700 200 720] /Contents (hl) >>`,
    `<< /Type /Annot /Subtype /Text /Rect [50 650 70 670] /T (t1) /Contents (note) /Name /Comment >>`,
  ];
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
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

type Layout = {
  sel: number;
  n: number;
  contents: { x: number; y: number; dx: number; dy: number };
  iconVis: number;
  icon: { x: number; y: number; dx: number; dy: number };
  raw: string;
};

function parseRect(s: string): { x: number; y: number; dx: number; dy: number } {
  const p = s.split(",").map((n) => parseInt(n, 10));
  return { x: p[0]!, y: p[1]!, dx: p[2]!, dy: p[3]! };
}

function intersects(
  a: { x: number; y: number; dx: number; dy: number },
  b: { x: number; y: number; dx: number; dy: number },
): boolean {
  return a.x < b.x + b.dx && a.x + a.dx > b.x && a.y < b.y + b.dy && a.y + a.dy > b.y;
}

async function layout(client: ControlClient, selectItem: number): Promise<Layout> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, selectItem]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m =
        /sel=(-?\d+) n=(\d+) selCount=\d+ contents=(-?\d+,-?\d+,-?\d+,-?\d+) iconVis=(\d+) icon=(-?\d+,-?\d+,-?\d+,-?\d+)/.exec(
          raw,
        );
      if (!m) {
        throw new Error(`annot-icon-over-contents: could not parse: ${raw}`);
      }
      return {
        sel: +m[1]!,
        n: +m[2]!,
        contents: parseRect(m[3]!),
        iconVis: +m[4]!,
        icon: parseRect(m[5]!),
        raw,
      };
    }
    if (exitCode !== 2) {
      throw new Error(`annot-icon-over-contents: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-icon-over-contents: editor never ready: ${raw}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("annot-icon-over-contents.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const hl = await layout(client, 1);
    if (hl.n < 2) {
      throw new Error(`annot-icon-over-contents: expected 2 annots, got n=${hl.n}`);
    }
    if (hl.iconVis !== 0) {
      throw new Error(`annot-icon-over-contents: Highlight should hide Icon: ${hl.raw}`);
    }

    const text = await layout(client, 2);
    if (text.iconVis !== 1) {
      throw new Error(`annot-icon-over-contents: Text should show Icon: ${text.raw}`);
    }
    if (text.icon.dx <= 0 || text.icon.dy <= 0) {
      throw new Error(`annot-icon-over-contents: Icon dropdown has no size: ${text.raw}`);
    }
    if (text.icon.x < 8 && text.icon.y < 8) {
      throw new Error(`annot-icon-over-contents: Icon dropdown still at origin over Contents: ${text.raw}`);
    }
    if (intersects(text.icon, text.contents)) {
      throw new Error(`annot-icon-over-contents: Icon dropdown overlaps Contents: ${text.raw}`);
    }
    if (text.icon.y < text.contents.y + text.contents.dy) {
      throw new Error(`annot-icon-over-contents: Icon dropdown is not below Contents: ${text.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("annot-icon-over-contents: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
