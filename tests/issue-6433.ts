// #6433: markdown code blocks that name a language are syntax colored
// (microlighter.min.js, through the CSS Custom Highlight API), and every code
// block gets a copy button (code-run.js). The page's resource requests show
// both scripts loaded, which also means the CSP allowed them.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { IS_MAC } from "./host";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import { sleep } from "./winapi";

const MD = `# Code blocks

Text with \`inline code\`.

\`\`\`python
def greet(name):
    print(f"Hello, {name}!")
\`\`\`

\`\`\`powershell
Write-Host "Hello"
\`\`\`

\`\`\`
plain block
\`\`\`
`;

async function waitServed(client: ControlClient, names: string[]): Promise<string> {
  const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = String((await client.request(ControlCommand.TestMarkdownServed, []))[1] ?? "");
    if (names.every((n) => s.includes(n))) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6433: page did not load ${names.join(", ")}; served:\n${s}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6433: orig WebView2 markdown view only");
    return;
  }
  const dir = tmpPath("issue-6433");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const mdPath = join(dir, "code.md");
  writeFileSync(mdPath, MD);

  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitServed(client, ["microlighter.min.js", "code-run.js"]);
    },
    [mdPath],
  );
  console.log("issue-6433: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
