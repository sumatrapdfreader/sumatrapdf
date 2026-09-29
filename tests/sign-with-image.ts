// Sign With Image stamps the Annotations.SignatureImage image on the page
// without asking for a file (a file picker would block this test).
//
// Run: bun tests/sign-with-image.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

// 1x1 PNG
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

export async function testit(): Promise<void> {
  const dir = tmpPath("sign-with-image");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const png = join(dir, "signature.png");
  writeFileSync(png, PNG);
  const settings = [
    "UiLanguage = en",
    "CheckForUpdates = false",
    "RestoreSession = false",
    "Annotations [",
    `\tSignatureImage = ${png}`,
    "]",
    "",
  ].join("\n");
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings);
  const pdf = join(dir, "blank.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    ]),
    "latin1",
  );

  const { proc, client } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdSignWithImage"]);
    const deadline = Date.now() + 5000;
    for (;;) {
      const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
      if (/type=Stamp /.test(raw)) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`sign-with-image: no image stamp on the page\n${raw}`);
      }
      await sleep(100);
    }
    console.log("sign-with-image: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
