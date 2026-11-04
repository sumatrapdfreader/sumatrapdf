// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6480
//
// Convert EPUB to PDF must keep the book's links and table of contents: a note
// reference in chapter 1 still jumps to the note in chapter 2, an external link
// still opens its URL, and the chapters become PDF bookmarks.
//
// Run: bun tests/issue-6480.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kUrl = "https://www.sumatrapdfreader.org/";
const kA4 = 1;

function para(i: number): string {
  return `<p>Paragraph ${i} of chapter one, long enough to wrap over a few lines of an A4 page at twelve points.</p>`;
}

function xhtml(title: string, body: string): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>${title}</title></head><body>${body}</body></html>`
  );
}

// chapter 1: a note reference and an external link on its first page, then
// enough text to fill more pages; chapter 2 starts with the note
function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const paras = Array.from({ length: 40 }, (_, i) => para(i + 1)).join("");
  const c1 = xhtml(
    "One",
    `<h1>Chapter 1</h1><p>See the note <a href="c2.xhtml#note1">[1]</a> and <a href="${kUrl}">the site</a>.</p>${paras}`,
  );
  const c2 = xhtml("Two", `<h1>Chapter 2</h1><p id="note1">[1] The note.</p>`);
  const nav = xhtml(
    "toc",
    `<nav xmlns:epub="http://www.idpf.org/2007/ops" epub:type="toc"><ol>` +
      `<li><a href="c1.xhtml">Chapter 1</a></li><li><a href="c2.xhtml">Chapter 2</a></li></ol></nav>`,
  );
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">issue-6480</dc:identifier>` +
    `<dc:title>Links</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="c2" href="c2.xhtml" media-type="application/xhtml+xml"/></manifest>` +
    `<spine><itemref idref="c1"/><itemref idref="c2"/></spine></package>`;
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">` +
    `<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/nav.xhtml", data: enc.encode(nav) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(c1) },
    { name: "OEBPS/c2.xhtml", data: enc.encode(c2) },
  ];
  return makeZip(entries);
}

async function text(client: ControlClient, cmd: ControlCommand, args: (string | number)[]): Promise<string> {
  const res = await client.request(cmd, args);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6480");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "book.epub");
  writeFileSync(epub, makeEpub());
  const pdf = join(dir, "book.pdf");

  const { proc, client } = await launchControlled([epub]);
  try {
    await client.waitForRenderIdle();
    const conv = await text(client, ControlCommand.TestConvertEpubToPdf, [pdf, kA4, 12]);
    if (!conv.startsWith("OK")) {
      throw new Error(`#6480: conversion failed: ${conv}`);
    }

    // chapters become bookmarks; chapter 2 starts after chapter 1's pages
    const toc = await text(client, ControlCommand.TestGetToc, [pdf]);
    const ch2 = /^Chapter 2\|page=(\d+)$/m.exec(toc);
    if (!/^Chapter 1\|page=1$/m.test(toc) || !ch2 || +ch2[1]! < 2) {
      throw new Error(`#6480: the PDF should have the book's chapters as bookmarks, got:\n${toc}`);
    }
    const ch2Page = +ch2[1]!;

    // the note reference jumps to chapter 2, the external link keeps its URL
    const links = await text(client, ControlCommand.TestPageLinks, [pdf, 1]);
    const rows = [...links.matchAll(/^kind=(\S+) page=(-?\d+).* value=(.*)$/gm)].map((m) => ({
      kind: m[1]!,
      page: +m[2]!,
      value: m[3]!.trim(),
    }));
    if (!rows.some((r) => r.page === ch2Page && r.kind !== "launchURL")) {
      throw new Error(`#6480: page 1 should link to the note on page ${ch2Page}, got:\n${links}`);
    }
    if (!rows.some((r) => r.value === kUrl)) {
      throw new Error(`#6480: page 1 should link to ${kUrl}, got:\n${links}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6480: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
