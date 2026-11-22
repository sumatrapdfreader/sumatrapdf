// PdfFilter.dll and PdfPreview.dll DllGetClassObject: a known CLSID gives an
// IClassFactory, an unknown one CLASS_E_CLASSNOTAVAILABLE (COM contract).
//
// Run: bun tests/com-class-object.ts [--no-build]

import { CFunction, dlopen, FFIType, ptr, read } from "bun:ffi";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { IS_WIN } from "./host.ts";
import { ROOT, USE_NG, runStandalone } from "./util.ts";

const S_OK = 0;
const CLASS_E_CLASSNOTAVAILABLE = 0x80040111 | 0;
const IID_ICLASSFACTORY = "{00000001-0000-0000-C000-000000000046}";
const BOGUS_CLSID = "{12345678-1234-1234-1234-123456789ABC}";
const PDF_FILTER_CLSID = "{55808EA8-81FE-43c6-AAE8-1D8149F941D3}";
const PDF_PREVIEW_CLSID = "{3D3B1846-CC43-42AE-BFF9-D914083C2BA3}";

// GUID string to its 16-byte in-memory layout
function guid(s: string): Buffer {
  const h = s.replace(/[{}-]/g, "");
  const b = Buffer.alloc(16);
  b.writeUInt32LE(parseInt(h.slice(0, 8), 16), 0);
  b.writeUInt16LE(parseInt(h.slice(8, 12), 16), 4);
  b.writeUInt16LE(parseInt(h.slice(12, 16), 16), 6);
  Buffer.from(h.slice(16), "hex").copy(b, 8);
  return b;
}

function dllDir(): string {
  const exe = process.env.SUMATRA_TEST_EXE || join(ROOT, "out", "dbg64", "SumatraPDF.exe");
  return dirname(exe);
}

function checkDll(dllPath: string, clsid: string): void {
  const lib = dlopen(dllPath, {
    DllGetClassObject: { args: [FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.i32 },
  });
  const getClassObject = lib.symbols.DllGetClassObject;
  const iid = guid(IID_ICLASSFACTORY);
  const out = new BigUint64Array(1);

  out[0] = 1n;
  let hr = getClassObject(ptr(guid(BOGUS_CLSID)), ptr(iid), ptr(out));
  if (hr !== CLASS_E_CLASSNOTAVAILABLE || out[0] !== 0n) {
    throw new Error(`${dllPath}: bogus CLSID gave hr=0x${(hr >>> 0).toString(16)} ppv=${out[0]}`);
  }

  hr = getClassObject(ptr(guid(clsid)), ptr(iid), ptr(out));
  if (hr !== S_OK || out[0] === 0n) {
    throw new Error(`${dllPath}: ${clsid} gave hr=0x${(hr >>> 0).toString(16)}`);
  }

  // IUnknown::Release is vtable slot 2
  const obj = Number(out[0]);
  const vtbl = read.ptr(obj as any, 0);
  const release = CFunction({ ptr: read.ptr(vtbl as any, 2 * 8) as any, args: [FFIType.ptr], returns: FFIType.u32 });
  release(obj as any);
  lib.close();
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("SKIP com-class-object: Windows orig only");
    return;
  }
  const dir = dllDir();
  const filter = join(dir, "PdfFilter.dll");
  const preview = join(dir, "PdfPreview.dll");
  if (!existsSync(filter) || !existsSync(preview)) {
    console.log(`SKIP com-class-object: no PdfFilter.dll / PdfPreview.dll in ${dir}`);
    return;
  }
  // the DLLs load libsumatrapdf.dll from their own directory
  process.env.PATH = `${dir};${process.env.PATH}`;
  checkDll(filter, PDF_FILTER_CLSID);
  checkDll(preview, PDF_PREVIEW_CLSID);
}

if (import.meta.main) {
  await runStandalone(testit);
}
