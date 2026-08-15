// discussion #5924: a relative link to a PDF in a markdown document
// ([the manual](./manual.pdf)) has to open that PDF in SumatraPDF. It used to be
// navigated to inside the document's WebView2, which served the raw PDF bytes as
// text/html and showed garbage.
//
// Links are followed through MarkdownModel::OnBeforeNavigate(), which is what
// WebView2 calls for a click, so the test drives that entry point directly (a
// real click inside the webview can't be synthesized here) and checks both the
// verdict it gives the browser and which tabs the app ended up with.
//
// The url is passed the way each caller passes it: WebView2 hands over the
// document-relative path ("sub/deep.pdf") because the virtual host is stripped
// before the callback, while a TOC destination carries the full virtual url.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control";
import { EXE, makeMinimalPdf, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { sleep, waitForTopWindow } from "./winapi";

const HOST = "https://sumatrapdf.markdown/";

// 1x1 transparent png
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYGD4DwABBAEAX+aBpQAAAABJRU5ErkJggg==",
  "base64",
);

function makeFolder(): string {
  const dir = tmpPath("issue-5924");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  writeFileSync(
    join(dir, "index.md"),
    [
      "# Index",
      "",
      "- [the manual](./doc.pdf)",
      "- [in a subdir](sub/deep.pdf)",
      "- [gone](./missing.pdf)",
      "- [another page](./other.md)",
      "- [a picture](./pic.png)",
      "",
    ].join("\n"),
  );
  writeFileSync(join(dir, "other.md"), "# Other\n\ntext\n");
  writeFileSync(join(dir, "doc.pdf"), makeMinimalPdf("doc"));
  writeFileSync(join(dir, "sub", "deep.pdf"), makeMinimalPdf("deep"));
  writeFileSync(join(dir, "pic.png"), PNG);
  return dir;
}

type Tab = { win: number; current: boolean; pageNo: number; file: string };
// `tabs` are the tabs holding a document; the app also has an empty start tab
type Result = { navigate: boolean; tabs: Tab[]; dump: string };

function parse(dump: string): Result {
  const nav = /^OK navigate=(-?\d)$/m.exec(dump);
  if (!nav) {
    throw new Error(`issue-5924: unexpected TestMarkdownFollowLink output:\n${dump}`);
  }
  const tabs: Tab[] = [];
  for (const line of dump.split("\n")) {
    const m = /^tab win=(\d+) current=(\d) pageNo=(\d+) file=(.*)$/.exec(line);
    if (m) {
      tabs.push({ win: +m[1]!, current: m[2] === "1", pageNo: +m[3]!, file: m[4]! });
    }
  }
  return { navigate: nav[1] === "1", tabs: tabs.filter((t) => t.file !== ""), dump };
}

export async function testit(): Promise<void> {
  const dir = makeFolder();

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("issue-5924: no frame window");
      }
      await client.waitForRenderIdle();

      // click the link, then read the tabs back: opening a linked document is
      // queued on the UI thread, and by the time it ran the current tab is no
      // longer the markdown one, so the second request must not follow anything
      const follow = async (href: string): Promise<Result> => {
        const res = await client.request(ControlCommand.TestMarkdownFollowLink, [href, 1]);
        const clicked = parse(String(res[1] ?? ""));
        const deadline = Date.now() + 4000;
        let settled = clicked;
        while (Date.now() < deadline) {
          const res2 = await client.request(ControlCommand.TestMarkdownFollowLink, ["", 0]);
          settled = parse(String(res2[1] ?? ""));
          if (settled.tabs.length >= clicked.tabs.length) {
            break;
          }
          await sleep(30);
        }
        return { navigate: clicked.navigate, tabs: settled.tabs, dump: `${clicked.dump}${settled.dump}` };
      };
      const fail = (msg: string, r: Result) => {
        throw new Error(`issue-5924: ${msg}\noutput:\n${r.dump}`);
      };

      // a link to another page of the document stays in the browser view
      let r = await follow("other.html");
      if (!r.navigate || r.tabs.length !== 1) {
        fail("a link to another .md page must stay in the browser view", r);
      }

      // so does a link to a resource the browser renders itself
      r = await follow("pic.png");
      if (!r.navigate || r.tabs.length !== 1) {
        fail("a link to an image must stay in the browser view", r);
      }

      // a link to a PDF that isn't there is still ours: the app reports the
      // error (a notification) instead of the webview showing an empty page
      r = await follow("missing.pdf");
      if (r.navigate || r.tabs.length !== 1) {
        fail("a link to a missing .pdf must not be navigated to in the browser view", r);
      }

      // the actual report: the linked PDF opens in SumatraPDF
      r = await follow("sub/deep.pdf");
      const deep = r.tabs.find((t) => t.file.toLowerCase().endsWith("\\sub\\deep.pdf"));
      if (r.navigate || !deep || !deep.current) {
        fail("a link to sub/deep.pdf must open it as the current tab", r);
      }
      if (r.tabs.length !== 2) {
        fail(`opening sub/deep.pdf should have left 2 tabs, there are ${r.tabs.length}`, r);
      }
    },
    [join(dir, "index.md")],
  );

  // deep.pdf became the current tab, so a second run for the other url form: a
  // full virtual url, as a TOC destination or a new-window request has it
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("issue-5924: no frame window");
      }
      await client.waitForRenderIdle();

      const res = await client.request(ControlCommand.TestMarkdownFollowLink, [`${HOST}doc.pdf`, 1]);
      const clicked = parse(String(res[1] ?? ""));
      const deadline = Date.now() + 4000;
      let settled = clicked;
      while (Date.now() < deadline) {
        const res2 = await client.request(ControlCommand.TestMarkdownFollowLink, ["", 0]);
        settled = parse(String(res2[1] ?? ""));
        if (settled.tabs.some((t) => t.file.toLowerCase().endsWith("\\doc.pdf"))) {
          break;
        }
        await sleep(30);
      }
      const doc = settled.tabs.find((t) => t.file.toLowerCase().endsWith("\\doc.pdf"));
      if (clicked.navigate || !doc || !doc.current || settled.tabs.length !== 2) {
        throw new Error(
          `issue-5924: a link to ./doc.pdf must open it as the current tab\noutput:\n${clicked.dump}${settled.dump}`,
        );
      }
    },
    [join(dir, "index.md")],
  );

  console.log("issue-5924: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
