// PR #6167: collapsed TOC branches stay unloaded across tab switches.
import { strict as assert } from "node:assert";
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG, writeAppdata } from "./util";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation";
import {
  countVisibleTreeRows,
  findChildWindow,
  sendMessage,
  treeExpand,
  treeExpandRecursively,
  treeGetNextItem,
  treeGetRoot,
  TVE_COLLAPSE,
  TVE_EXPAND,
  TVGN_NEXT,
  TVGN_CHILD,
  TVM_GETCOUNT,
  TVM_SELECTITEM,
  TVGN_CARET,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi";

// Three levels; negative /Count keeps each branch initially collapsed.
export function makeTocPdf(width = 3): Buffer {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R /PageMode /UseOutlines >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "",
  ];
  function addLevel(parent: number, depth: number): string {
    const ids = Array.from({ length: width }, () => objects.push(""));
    for (let i = 0; i < width; i++) {
      const id = ids[i];
      const links = `${i ? `/Prev ${ids[i - 1]} 0 R` : ""} ${i + 1 < width ? `/Next ${ids[i + 1]} 0 R` : ""}`;
      const children = depth < 3 ? addLevel(id, depth + 1) : "";
      const page = depth === 1 ? 3 : 4;
      objects[id - 1] = `<< /Title (Node ${id}) /Parent ${parent} 0 R ${links} ${children} /Dest [${page} 0 R /Fit] >>`;
    }
    return `/First ${ids[0]} 0 R /Last ${ids[width - 1]} 0 R /Count -${width}`;
  }
  objects[4] = `<< /Type /Outlines ${addLevel(5, 1)} >>`;
  return Buffer.from(assemblePdf(objects), "latin1");
}

async function tocRows(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestUiState, ["count"]))[1] ?? "");
  const m = /tocRows=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6167: ${raw}`);
  }
  return +m[1]!;
}

async function tocOp(client: ControlClient, op: string, arg?: number) {
  const raw = String((await client.request(ControlCommand.TestUiState, arg === undefined ? [op] : [op, arg]))[1] ?? "");
  if (raw.startsWith("ERR")) {
    throw new Error(`issue-6167: ${op} ${raw}`);
  }
}

// ng has no SysTreeView32. Visible rows are the expanded TocItem branches.
async function testNg(pdf: string, other: string, appdata: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf, other]);
  try {
    await client.waitForRenderIdle();
    const rows = () => tocRows(client);
    const switchBack = () => {
      sendCommandSync(frame, cmdId("CmdNextTab"));
      sendCommandSync(frame, cmdId("CmdNextTab"));
    };
    assert.equal(await rows(), 3, "initially only the collapsed roots are visible");
    await tocOp(client, "sel", 0);
    await tocOp(client, "expand");
    assert.equal(await rows(), 6, "expand inserts one level");
    await tocOp(client, "sel", 1);
    await tocOp(client, "expand");
    assert.equal(await rows(), 9, "nested expansion inserts its children");
    await tocOp(client, "sel", 0);
    await tocOp(client, "collapse");

    switchBack();
    assert.equal(await rows(), 3, "tab switch keeps collapsed branches hidden");
    switchBack();
    assert.equal(await rows(), 3, "saving expansion state does not show collapsed branches");
    await tocOp(client, "sel", 0);
    await tocOp(client, "expand");
    assert.equal(await rows(), 9, "hidden child keeps its saved expansion");
    await tocOp(client, "sel", 1);
    await tocOp(client, "collapse");
    await tocOp(client, "expand");
    assert.equal(await rows(), 9, "repeated expansion does not duplicate items");

    sendCommandSync(frame, cmdId("CmdExpandAll"));
    assert.equal(await rows(), 39, "expand all shows the entire outline");
    sendCommandSync(frame, cmdId("CmdCollapseAll"));
    switchBack();
    assert.equal(await rows(), 3, "collapse to level one survives a rebuild");
    sendCommandSync(frame, cmdId("CmdGoToLastPage"));
    sendCommandSync(frame, cmdId("CmdExpandToCurrentPage"));
    assert.ok((await rows()) > 3, "navigation expands an unloaded destination");
    console.log("issue-6167: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6167.pdf");
  const other = tmpPath("issue-6167-other.pdf");
  writeFileSync(pdf, makeTocPdf());
  writeFileSync(other, makeTocPdf());
  const appdata = writeAppdata("issue-6167-settings", "NoHomeTab = true\nShowToc = true\n");
  if (USE_NG) {
    await testNg(pdf, other, appdata);
    return;
  }
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf, other]);
  try {
    await client.waitForRenderIdle();
    const tree = findChildWindow(frame, "SysTreeView32");
    assert.ok(tree, "TOC tree exists");
    const count = () => Number(sendMessage(tree, TVM_GETCOUNT, 0, 0));
    const switchBack = () => {
      sendCommandSync(frame, cmdId("CmdNextTab"));
      sendCommandSync(frame, cmdId("CmdNextTab"));
    };
    assert.equal(count(), 3, "initially insert only collapsed roots");
    let root = treeGetNextItem(tree, TVGN_NEXT, treeGetRoot(tree));
    sendMessage(tree, TVM_SELECTITEM, TVGN_CARET, root);
    sendMessage(tree, WM_KEYDOWN, VK_RETURN, 0);
    assert.equal(count(), 6, "expand inserts one level");
    let child = treeGetNextItem(tree, TVGN_CHILD, root);
    treeExpand(tree, TVE_EXPAND, child);
    assert.equal(count(), 9, "nested expansion inserts its children");
    treeExpand(tree, TVE_COLLAPSE, root);

    // Two rebuilds must preserve the expanded child beneath an unloaded parent.
    switchBack();
    assert.equal(count(), 3, "tab switch discards hidden native items");
    switchBack();
    assert.equal(count(), 3, "saving expansion state does not populate branches");
    root = treeGetNextItem(tree, TVGN_NEXT, treeGetRoot(tree));
    treeExpand(tree, TVE_EXPAND, root);
    assert.equal(countVisibleTreeRows(tree), 9, "hidden child keeps its saved expansion");
    child = treeGetNextItem(tree, TVGN_CHILD, root);
    treeExpand(tree, TVE_COLLAPSE, child);
    treeExpand(tree, TVE_EXPAND, child);
    assert.equal(count(), 9, "repeated expansion does not duplicate items");

    treeExpandRecursively(tree, TVE_EXPAND);
    assert.equal(count(), 39, "expand all materializes the entire model");
    assert.equal(countVisibleTreeRows(tree), 39, "all descendants are visible");
    treeExpandRecursively(tree, TVE_COLLAPSE);
    switchBack();
    assert.equal(count(), 3, "collapse to level one survives a rebuild");
    sendCommandSync(frame, cmdId("CmdGoToLastPage"));
    sendCommandSync(frame, cmdId("CmdExpandToCurrentPage"));
    assert.ok(countVisibleTreeRows(tree) > 3, "navigation expands an unloaded destination");
    console.log("issue-6167: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
