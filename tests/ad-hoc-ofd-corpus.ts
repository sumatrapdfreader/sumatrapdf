// Private samples stay outside the suite. Run: bun tests/ad-hoc-ofd-corpus.ts --no-build

import { mkdirSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

export async function testit(): Promise<void> {
  const dir = join(ROOT, "demo");
  const files = readdirSync(dir).filter((name) => name.toLowerCase().endsWith(".ofd"));
  if (!files.length) {
    throw new Error("No OFD samples in demo/");
  }
  await withControlledSumatra(EXE, async (client) => {
    for (const [index, name] of files.entries()) {
      const result = await client.request(ControlCommand.TestRenderPageColors, [join(dir, name), 1, 100]);
      const raw = String(result[1] ?? "");
      if (result[0] !== 0 || !/nonwhite=[1-9]\d*/.test(raw)) {
        throw new Error(`Sample ${index + 1} failed: ${raw}`);
      }
      console.log(`Sample ${index + 1}/${files.length}: ${raw.trim()}`);
    }
  });

  const output = tmpPath("ofd-corpus");
  mkdirSync(output, { recursive: true });
  const samples = [files.find((name) => !name.startsWith("dzfp")), files.find((name) => name.startsWith("dzfp"))];
  for (const [index, name] of samples.entries()) {
    if (!name) {
      continue;
    }
    await withControlledSumatra(
      EXE,
      async (client) => {
        await client.waitForRenderIdle();
        const result = await client.request(ControlCommand.TestConvertToImages, [
          join(output, `sample-${index + 1}-<N>.png`),
          "all",
          144,
        ]);
        if (result[0] !== 0 || !String(result[1]).startsWith("OK")) {
          throw new Error(`Sample export failed: ${result}`);
        }
      },
      [join(dir, name)],
    );
  }
  console.log(`Previews: ${output}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
