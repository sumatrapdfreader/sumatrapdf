// #6380: raw HTML in a markdown file (e.g. <span style="color: red">) was dropped.
// It is now kept, but the page's Content-Security-Policy stops it running script,
// while the Mermaid bootstrap still runs.
//
// The page's resource requests show what happened: a span's background image
// loads only if raw HTML and its style= survived, an <img onerror> handler would
// request pwned.png, and a mermaid fence requests mermaid.min.js and mermaid-run.js.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { IS_MAC } from "./host";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import { sleep } from "./winapi";

const MD = `# Raw HTML

Now: <span style="color: red">This should **be in red**</span>.

<span style="display:inline-block;width:8px;height:8px;background-image:url(styled.png)"></span>

<img src="missing.png" onerror="this.onerror=null;this.src='pwned.png'">

\`\`\`mermaid
flowchart TD
    A --> B
\`\`\`
`;

async function served(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkdownServed, []);
  return String(res[1] ?? "");
}

async function waitServed(client: ControlClient, names: string[]): Promise<string> {
  const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await served(client);
    if (names.every((n) => s.includes(n))) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6380: page did not load ${names.join(", ")}; served:\n${s}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6380: orig WebView2 markdown view only");
    return;
  }
  const dir = tmpPath("issue-6380");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const mdPath = join(dir, "raw-html.md");
  writeFileSync(mdPath, MD);
  // contents don't matter, only whether they get requested
  writeFileSync(join(dir, "styled.png"), "styled");
  writeFileSync(join(dir, "pwned.png"), "pwned");

  await withControlledSumatra(
    EXE,
    async (client) => {
      await waitServed(client, ["styled.png", "mermaid.min.js", "mermaid-run.js"]);
      // give a blocked handler time to show itself
      await sleep(1000);
      const s = await served(client);
      if (s.includes("pwned.png")) {
        throw new Error(`issue-6380: an onerror handler in the markdown ran; served:\n${s}`);
      }
    },
    [mdPath],
  );
  console.log("issue-6380: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
