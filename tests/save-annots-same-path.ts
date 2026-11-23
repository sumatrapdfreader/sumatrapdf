// Save Annotations As onto the open file, spelled in other case: Windows paths
// are case-insensitive, so it must save in place and keep the tab's path.
import { readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kComboFlag = 1 << 17;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] /DA (/Helv 0 Tf 0 g) >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Widget /FT /Ch /Ff ${kComboFlag} /T (Choice) /Opt [(A) (B)] ` +
      "/V (A) /I [0] /Rect [100 600 300 630] /F 4 /DA (/Helv 12 Tf 0 g) /P 3 0 R >>",
  ]);
}

async function currentPath(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const [code, raw] = await client.request(ControlCommand.TestCurrentTab, []);
    const m = /path=(.*?) page=/.exec(String(raw));
    if (code === 0 && m) return m[1]!;
    if (Date.now() > deadline) throw new Error(`save-annots-same-path: TestCurrentTab: ${raw}`);
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("save-annots-same-path.pdf");
  rmSync(pdf, { force: true });
  writeFileSync(pdf, makePdf(), "latin1");
  const original = readFileSync(pdf, "latin1");
  const upper = join(dirname(pdf), basename(pdf).toUpperCase());

  await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      await client.waitForRenderIdle();
      const before = await currentPath(client);
      const [code, raw] = await client.request(ControlCommand.TestSetFormChoice, [1, 0, "B"]);
      if (code !== 0) throw new Error(`save-annots-same-path: TestSetFormChoice: ${raw}`);

      await client.request(ControlCommand.TestSaveFileName, [upper]);
      const [c2, r2] = await client.request(ControlCommand.TestInvokeCommand, ["CmdSaveAnnotationsNewFile"]);
      if (c2 !== 0) throw new Error(`save-annots-same-path: CmdSaveAnnotationsNewFile: ${r2}`);

      const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
      while (readFileSync(pdf, "latin1") === original) {
        if (Date.now() > deadline) throw new Error("save-annots-same-path: saving didn't change the file");
        await sleep(100);
      }
      await client.waitForRenderIdle();
      const after = await currentPath(client);
      if (after !== before) {
        throw new Error(`save-annots-same-path: tab path changed from '${before}' to '${after}'`);
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
