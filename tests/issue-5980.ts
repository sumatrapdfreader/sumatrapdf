// #5980: Show Links must outline only links, not annotation/comment elements.
// The PDF has one invisible-border link and one large red underline annotation
// with comment text. The old non-image filter drew a second blue box around the
// underline; the fixed link-kind filter draws exactly one blue component.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";
import { findCanvas, launchControlled, killAndWait } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
ReuseInstance = false
ShowLinks = true
`;

function makePdf(): Buffer {
  const content = "";
  const appearance = "1 0 0 RG 3 w 0 2 m 500 2 l S";
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Annots [5 0 R 6 0 R] >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /Annot /Subtype /Link /Rect [60 600 140 640] /Border [0 0 0] ` +
      `/A << /S /URI /URI (https://example.com/) >> >>`,
    `<< /Type /Annot /Subtype /Underline /Rect [50 100 550 230] ` +
      `/QuadPoints [50 230 550 230 50 100 550 100] /Contents (not a link) /T (Tester) ` +
      `/C [1 0 0] /AP << /N 7 0 R >> >>`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 500 130] /Resources << >> ` +
      `/Length ${appearance.length} >>\nstream\n${appearance}\nendstream`,
  ];

  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

type Pixels = NonNullable<ReturnType<typeof captureWindowPixels>>;

function blueComponents(px: Pixels): { count: number; sizes: number[] } {
  const n = px.w * px.h;
  const blue = new Uint8Array(n);
  for (let p = 0, i = 0; p < n; p++, i += 4) {
    // captureWindowPixels is BGRA; the Show Links pen is pure blue.
    if (px.data[i] >= 180 && px.data[i + 1] <= 80 && px.data[i + 2] <= 80) {
      blue[p] = 1;
    }
  }

  const seen = new Uint8Array(n);
  const sizes: number[] = [];
  for (let start = 0; start < n; start++) {
    if (!blue[start] || seen[start]) {
      continue;
    }
    seen[start] = 1;
    const stack = [start];
    let size = 0;
    while (stack.length > 0) {
      const p = stack.pop()!;
      size++;
      const x = p % px.w;
      const neighbors = [p - px.w, p + px.w];
      if (x > 0) neighbors.push(p - 1);
      if (x + 1 < px.w) neighbors.push(p + 1);
      for (const next of neighbors) {
        if (next >= 0 && next < n && blue[next] && !seen[next]) {
          seen[next] = 1;
          stack.push(next);
        }
      }
    }
    if (size >= 20) {
      sizes.push(size);
    }
  }
  return { count: sizes.length, sizes };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5980");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "link-and-underline.pdf");
  writeFileSync(pdf, makePdf());

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
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5980: no canvas");
    }

    let components = { count: 0, sizes: [] as number[] };
    const deadline = Date.now() + 3000;
    while (Date.now() < deadline) {
      const px = captureWindowPixels(canvas);
      if (px) {
        components = blueComponents(px);
        if (components.count > 0) {
          break;
        }
      }
      await sleep(50);
    }
    if (components.count !== 1) {
      throw new Error(
        `issue-5980: Show Links drew ${components.count} blue components, expected only the link ` +
          `(sizes: ${components.sizes.join(", ")})`,
      );
    }
    console.log(`issue-5980: one link outline (${components.sizes[0]} blue pixels), underline ignored`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
