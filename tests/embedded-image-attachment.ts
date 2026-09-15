// A file-attachment annotation whose payload is an image opens it from memory
// into a new tab. That engine has no file path, and EngineImage::GetPropertyTemp()
// (asked for the title when the tab is shown) read the image from FilePath(),
// which fired the empty-path debug report in file::ReadFile().

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { getWindowText } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

// 1x1 red PNG
const PNG_BASE64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==";

function makePdfWithImageAttachment(): string {
  const png = Buffer.from(PNG_BASE64, "base64").toString("latin1");
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [200 400 400 600] /FS 5 0 R /Name /PushPin >>",
    "<< /Type /Filespec /F (image002.png) /UF (image002.png) /EF << /F 6 0 R >> >>",
    `<< /Type /EmbeddedFile /Length ${png.length} /Params << /Size ${png.length} >> >>\nstream\n${png}\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("embedded-image-attachment");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "outer.pdf");
  writeFileSync(pdf, makePdfWithImageAttachment(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    await pollUntil(
      async () => {
        const res = await client.request(ControlCommand.TestMarkupAnnots, ["open-embedded", 0, 0]);
        return { code: res[0], text: String(res[1] ?? "") };
      },
      (r) => r.code === 0 && /OK/.test(r.text),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (r) => `embedded-image-attachment: open-embedded failed: code=${r.code} ${r.text}`,
      },
    );

    const title = await pollUntil(
      () => getWindowText(frame),
      (t) => t.includes("image002.png"),
      {
        timeoutMs: 8000 * SLOW_BUILD_FACTOR,
        error: (t) => `embedded-image-attachment: expected tab image002.png, title='${t}'`,
      },
    );
    console.log(`embedded-image-attachment: opened ${title}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
