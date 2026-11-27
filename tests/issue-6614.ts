// #6614: TeX math in markdown ($...$, $$...$$, ```math) is typeset with Temml.
// The page's resource requests show temml.min.js, temml.css and math-run.js
// loaded, which also means the CSP allowed them. A page without math loads
// none of them. The markdown parsing is covered by MarkdownToc_UnitTestMath.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { IS_MAC } from "./host";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import { sleep } from "./winapi";

const MD = `# Math

Inline $\\int_0^\\infty x \\, dx$ and a price of $5.

$$\\left( \\sum_{k=1}^n a_k b_k \\right)^2 \\leq \\left( \\sum_{k=1}^n a_k^2 \\right) \\left( \\sum_{k=1}^n b_k^2 \\right)$$

\`\`\`math
V_{sphere} = \\frac{4}{3}\\pi r^3
\`\`\`
`;

const NO_MATH_MD = `# No math

Costs $5 and $10.
`;

const MATH_URLS = ["temml.min.js", "temml.css", "math-run.js"];

async function served(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkdownServed, []))[1] ?? "");
}

async function waitServed(client: ControlClient, names: string[]): Promise<string> {
  const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await served(client);
    if (names.every((n) => s.includes(n))) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6614: page did not load ${names.join(", ")}; served:\n${s}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6614: orig WebView2 markdown view only");
    return;
  }
  const dir = tmpPath("issue-6614");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const mdPath = join(dir, "math.md");
  writeFileSync(mdPath, MD);
  const noMathPath = join(dir, "no-math.md");
  writeFileSync(noMathPath, NO_MATH_MD);

  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitServed(client, MATH_URLS);
    },
    [mdPath],
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      const s = await waitServed(client, ["no-math"]);
      // give a page that wrongly asked for temml time to request it
      await sleep(1000);
      const after = await served(client);
      const loaded = MATH_URLS.filter((n) => after.includes(n));
      if (loaded.length > 0) {
        throw new Error(`issue-6614: page without math loaded ${loaded.join(", ")}; served:\n${s}`);
      }
    },
    [noMathPath],
  );
  console.log("issue-6614: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
