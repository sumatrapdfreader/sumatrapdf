// Signing without locking, and certifying (discussion #6315).
//
// A new signature field must not lock every form field: mupdf's widget comes
// with /Lock /Action /All, which made any later signature a change the first
// one forbade (Acrobat: "Document Locked by SumatraSignature1"). A certifying
// signature adds a DocMDP transform with the allowed changes (/P) and the
// catalog's /Perms. Both are checked on the saved file's objects; the
// signature itself is verified against the test certificate.
//
// Run: bun tests/sign-certify.ts [--no-build] [-ng]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { verifyPdfSignature, writeTestPfx } from "./pki.ts";
import { assemblePdf, EXE, runStandalone, tmpPath } from "./util.ts";

const PASSWORD = "sumatra-test-pw";
const CERTIFY_NO_CHANGES = 1;
const CERTIFY_FORMS_SIGNING_ANNOTS = 3;

const LOCK_FIELDS = 1;
const LOCK_DOCUMENT = 2;

async function sign(
  client: ControlClient,
  src: string,
  dest: string,
  pfx: string,
  certify: number,
  lockFlags = 0,
): Promise<void> {
  const res = await client.request(ControlCommand.TestSignDocument, [
    src,
    dest,
    "",
    pfx,
    PASSWORD,
    "",
    -1,
    certify,
    lockFlags,
  ]);
  const out = String(res[1] ?? "").trim();
  if (res[0] !== 0 || !/ok=1/.test(out)) {
    throw new Error(`sign-certify: TestSignDocument(${dest}, certify ${certify}) failed: ${out}`);
  }
}

// the objects an incremental save appended after the original file
function appended(path: string, originalLen: number): string {
  return readFileSync(path).subarray(originalLen).toString("latin1");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("sign-certify");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pfx = join(dir, "signer.pfx");
  const cert = writeTestPfx(pfx, "SumatraPDF CertifyTest", PASSWORD);
  const pdf = join(dir, "blank.pdf");
  const blank = assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
  writeFileSync(pdf, blank, "latin1");
  const signed1 = join(dir, "signed1.pdf");
  const signed2 = join(dir, "signed2.pdf");
  const certified = join(dir, "certified.pdf");
  const certified3 = join(dir, "certified3.pdf");

  await withControlledSumatra(
    EXE,
    async (client) => {
      // an approval signature locks nothing: no /Lock on the field, the
      // FieldMDP transform includes no fields
      await sign(client, pdf, signed1, pfx, 0);
      verifyPdfSignature(signed1, cert);
      let added = appended(signed1, blank.length);
      if (/\/Lock\b/.test(added)) {
        throw new Error("sign-certify: the new signature field has a /Lock");
      }
      if (/\/Action\s*\/All\b/.test(added)) {
        throw new Error("sign-certify: the signature's FieldMDP transform locks all fields");
      }
      if (!/\/TransformMethod\s*\/FieldMDP/.test(added) || !/\/Action\s*\/Include/.test(added)) {
        throw new Error(`sign-certify: unexpected FieldMDP transform:\n${added.slice(0, 2000)}`);
      }
      if (/\/DocMDP/.test(added)) {
        throw new Error("sign-certify: an approval signature certified the document");
      }

      // a second approval signature goes on top of the first, and the first
      // signature's bytes are untouched (an incremental update)
      await sign(client, signed1, signed2, pfx, 0);
      verifyPdfSignature(signed2, cert);
      const first = readFileSync(signed1);
      if (!readFileSync(signed2).subarray(0, first.length).equals(first)) {
        throw new Error("sign-certify: the second signature rewrote the first signature's bytes");
      }

      // certifying: DocMDP with the chosen permission, first in /Reference,
      // and the catalog's /Perms pointing at the signature
      await sign(client, pdf, certified, pfx, CERTIFY_NO_CHANGES);
      verifyPdfSignature(certified, cert);
      added = appended(certified, blank.length);
      const docMdp = /\/TransformMethod\s*\/DocMDP[^>]*\/TransformParams\s*<<[^>]*\/P\s+1\b/.exec(added);
      const refs = /\/Reference\s*\[\s*<<[^>]*\/TransformMethod\s*\/(\w+)/.exec(added);
      if (!docMdp || !refs || refs[1] !== "DocMDP") {
        throw new Error(`sign-certify: no DocMDP /P 1 transform first in /Reference:\n${added.slice(0, 2000)}`);
      }
      if (!/\/Perms\s*<<\s*\/DocMDP\s+\d+ 0 R/.test(added)) {
        throw new Error(`sign-certify: the catalog has no /Perms /DocMDP reference:\n${added.slice(0, 2000)}`);
      }
      if (/\/Lock\b/.test(added)) {
        throw new Error("sign-certify: the certifying signature field has a /Lock");
      }

      await sign(client, pdf, certified3, pfx, CERTIFY_FORMS_SIGNING_ANNOTS);
      verifyPdfSignature(certified3, cert);
      if (
        !/\/TransformMethod\s*\/DocMDP[^>]*\/TransformParams\s*<<[^>]*\/P\s+3\b/.test(
          appended(certified3, blank.length),
        )
      ) {
        throw new Error("sign-certify: certifying with annotations allowed did not write /P 3");
      }
      // "lock form fields": the field keeps /Lock /Action /All and the
      // FieldMDP transform locks all fields, as every signature used to
      const locked = join(dir, "locked-fields.pdf");
      await sign(client, pdf, locked, pfx, 0, LOCK_FIELDS);
      verifyPdfSignature(locked, cert);
      added = appended(locked, blank.length);
      if (
        !/\/Lock\s*<<[^>]*\/Action\s*\/All/.test(added) ||
        !/\/TransformMethod\s*\/FieldMDP[^>]*\/TransformParams\s*<<[^>]*\/Action\s*\/All/.test(added)
      ) {
        throw new Error(`sign-certify: lock fields did not lock all fields:\n${added.slice(0, 2000)}`);
      }
      if (/\/P\s+1\b/.test(added)) {
        throw new Error("sign-certify: lock fields must not lock the document (/P 1)");
      }

      // "lock document after signing" (PDF 2.0): /P 1 in the field's /Lock
      // and in the FieldMDP transform; allowed on a signed document
      const lockedDoc = join(dir, "locked-doc.pdf");
      await sign(client, signed1, lockedDoc, pfx, 0, LOCK_DOCUMENT);
      verifyPdfSignature(lockedDoc, cert);
      added = appended(lockedDoc, readFileSync(signed1).length);
      if (!/\/Lock\s*<<[^>]*\/P\s+1\b/.test(added)) {
        throw new Error(`sign-certify: lock document did not put /P 1 in the field's /Lock:\n${added.slice(0, 2000)}`);
      }
      if (!/\/TransformMethod\s*\/FieldMDP[^>]*\/TransformParams\s*<<[^>]*\/P\s+1\b/.test(added)) {
        throw new Error(
          `sign-certify: lock document did not put /P 1 in the FieldMDP transform:\n${added.slice(0, 2000)}`,
        );
      }
      if (/\/DocMDP/.test(added)) {
        throw new Error("sign-certify: lock document is not a certification");
      }

      // [CmdSignDocument certify] parses (opens the dialog set to certify)
      const inv = await client.request(ControlCommand.TestInvokeCommand, ["CmdSignDocument certify"]);
      if (inv[0] !== 0) {
        throw new Error(`sign-certify: [CmdSignDocument certify] was rejected: ${String(inv[1] ?? "").trim()}`);
      }
    },
    [pdf],
  );

  // Properties on a chain: the first signature is intact, later ones were
  // added, which is not "the document was changed"
  await withControlledSumatra(
    EXE,
    async (client) => {
      const res = await client.request(ControlCommand.TestDocumentProperties, [""]);
      const text = String(res[1] ?? "");
      if (!/1 later signature\(s\) were added/.test(text)) {
        throw new Error(`sign-certify: Properties of a 2-signature chain:\n${text.slice(0, 1500)}`);
      }
      if (!/wasn't changed since the signature was applied/.test(text)) {
        throw new Error(
          `sign-certify: the last signature of the chain should count as unchanged:\n${text.slice(0, 1500)}`,
        );
      }
      console.log("sign-certify: OK");
    },
    [signed2],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
