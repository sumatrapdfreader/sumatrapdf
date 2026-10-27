// A markdown page must not reach files outside its folder. WebView2 collapses
// "../" in a url, but not "%2e%2e%5c": decoded, that is "..\", and joining it
// to the folder used to serve (or open) the file above it.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { IS_MAC } from "./host";
import { EXE, makeMinimalPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import { sleep } from "./winapi";

const kUp = "%2e%2e%5c";

async function served(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkdownServed, []);
  return String(res[1] ?? "");
}

async function tabsDump(client: ControlClient, href: string, follow: number): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkdownFollowLink, [href, follow]);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP markdown-path-traversal: orig WebView2 markdown view only");
    return;
  }
  const dir = tmpPath("markdown-path-traversal");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const mdPath = join(dir, "sub", "page.md");
  writeFileSync(mdPath, `# Page\n\n![ok](ok.png)\n\n![up](${kUp}secret.png)\n`);
  // contents don't matter, only whether they get served
  writeFileSync(join(dir, "sub", "ok.png"), "ok");
  writeFileSync(join(dir, "secret.png"), "secret");
  writeFileSync(join(dir, "secret.pdf"), makeMinimalPdf("secret"));

  await withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 15_000 * SLOW_BUILD_FACTOR;
      let s = await served(client);
      while (!s.includes("ok.png") && Date.now() < deadline) {
        await sleep(100);
        s = await served(client);
      }
      if (!s.includes("ok.png")) {
        throw new Error(`markdown-path-traversal: page did not load ok.png; served:\n${s}`);
      }
      // both images are requested together, so the other one had its chance
      await sleep(500);
      s = await served(client);
      if (s.includes("secret.png")) {
        throw new Error(`markdown-path-traversal: served a file outside the folder:\n${s}`);
      }

      // a link out of the folder must not open the file there; opening is
      // queued, so give it time before reading the tabs back
      await tabsDump(client, `${kUp}secret.pdf`, 1);
      await sleep(1000);
      const tabs = await tabsDump(client, "", 0);
      if (/file=.*secret\.pdf/i.test(tabs)) {
        throw new Error(`markdown-path-traversal: a link opened a file outside the folder:\n${tabs}`);
      }
    },
    [mdPath],
  );
  console.log("markdown-path-traversal: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
