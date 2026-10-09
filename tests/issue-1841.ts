// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1841
//
// PageUp / PageDown with the focus in the bookmarks tree paged through the
// tree, and since a selection change navigates, a short outline jumped the
// document to its last entry. Those keys must scroll the document instead,
// as they do from the canvas.
//
// Run: bun tests/issue-1841.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { makeBookmarkedPdf } from "./toc-tree-sent-click.ts";
import { pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { findVisibleChildWindow, postMessage, WM_KEYDOWN } from "./winapi.ts";

const VK_PRIOR = 0x21;
const VK_NEXT = 0x22;

async function pageAfterKey(client: ControlClient, tree: number, vk: number, want: number): Promise<number> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["key", vk, 0]);
  } else {
    postMessage(tree, WM_KEYDOWN, vk, 0);
  }
  return pollUntil(
    async () => (await client.chapterInfo()).page,
    (page) => page === want,
    { error: (page) => `issue-1841: key left document on page ${page}, want ${want}` },
  );
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-1841.pdf");
  writeFileSync(pdfPath, makeBookmarkedPdf());

  // single page + fit page: one PageDown is one page
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fitpage", pdfPath]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    let tree = 0;
    if (USE_NG) {
      // the tree is drawn in the frame; focus it, then the key goes to the frame
      const focused = await client.request(ControlCommand.TestUiState, ["focus"]);
      if (String(focused[1] ?? "") !== "ok") {
        throw new Error(`issue-1841: bookmarks tree not focused (${String(focused[1] ?? "")})`);
      }
    } else {
      tree = findVisibleChildWindow(frame, "SysTreeView32");
      if (!tree) {
        throw new Error("issue-1841: bookmarks tree not found");
      }
    }
    const down = await pageAfterKey(client, tree, VK_NEXT, 2);
    if (down !== 2) {
      throw new Error(`issue-1841: PageDown in the bookmarks tree went to page ${down}, want 2`);
    }
    const up = await pageAfterKey(client, tree, VK_PRIOR, 1);
    if (up !== 1) {
      throw new Error(`issue-1841: PageUp in the bookmarks tree went to page ${up}, want 1`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
