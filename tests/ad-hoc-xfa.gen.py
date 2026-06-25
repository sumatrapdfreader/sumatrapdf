#!/usr/bin/env python3
"""Generate tests/ad-hoc-xfa.pdf: pure-XFA fixture for ad-hoc-xfa.ts.

Two pageAreas with per-page content instantiated via usehref prototypes.
Regenerate:

  python tests/ad-hoc-xfa.gen.py
"""

from pathlib import Path

# Pure-XFA form: prototypes + usehref instances on two pages.
XDP = """<?xml version="1.0" encoding="UTF-8"?>
<xdp xmlns="http://ns.adobe.com/xdp/">
<template xmlns="http://www.xfa.org/schema/xfa-template/3.3/">
  <subform name="form1" layout="tb">
    <pageSet>
      <pageArea name="Page1" id="Page1">
        <medium short="8.5in" long="11in"/>
        <contentArea x="0.25in" y="0.25in" w="8in" h="10.5in">
          <draw usehref="#labelProto" x="0.75in" y="0.75in">
            <value>
              <text>First name:</text>
            </value>
          </draw>
          <field usehref="#fieldProto" name="firstName" x="0.75in" y="1in"/>
        </contentArea>
      </pageArea>
      <pageArea name="Page2" id="Page2">
        <medium short="8.5in" long="11in"/>
        <contentArea x="0.25in" y="0.25in" w="8in" h="10.5in">
          <draw usehref="#labelProto" x="0.75in" y="0.75in">
            <value>
              <text>Last name:</text>
            </value>
          </draw>
          <field usehref="#fieldProto" name="lastName" x="0.75in" y="1in"/>
        </contentArea>
      </pageArea>
    </pageSet>
    <draw id="labelProto" w="2in" h="0.2in">
      <value>
        <text>Label:</text>
      </value>
    </draw>
    <field id="fieldProto" w="3in" h="0.25in">
      <ui>
        <textEdit/>
      </ui>
      <value>
        <text>placeholder</text>
      </value>
    </field>
  </subform>
</template>
<datasets xmlns="http://www.xfa.org/schema/xfa-data/1.0/">
  <data>
    <firstName>Alice</firstName>
    <lastName>Bob</lastName>
  </data>
</datasets>
</xdp>
"""


def build_pdf(xdp: str) -> bytes:
    xdp_bytes = xdp.encode("utf-8")
    objects = []

    def add(obj: str) -> int:
        objects.append(obj.encode("latin-1"))
        return len(objects)

    add("<< /Type /Catalog /Pages 2 0 R /AcroForm 4 0 R >>")
    add("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
    add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
    add("<< /Fields [] /XFA [(xdp:xdp) 5 0 R] >>")
    add(
        f"<< /Length {len(xdp_bytes)} >>\nstream\n".encode("latin-1").decode("latin-1")
        + xdp
        + "\nendstream"
    )

    parts = [b"%PDF-1.4\n"]
    offsets = [0]
    for i, body in enumerate(objects, start=1):
        offsets.append(sum(len(p) for p in parts))
        parts.append(f"{i} 0 obj\n".encode("latin-1"))
        parts.append(body)
        parts.append(b"\nendobj\n")

    xref_pos = sum(len(p) for p in parts)
    xref = [b"xref\n", f"0 {len(objects)+1}\n".encode("latin-1"), b"0000000000 65535 f \n"]
    for off in offsets[1:]:
        xref.append(f"{off:010d} 00000 n \n".encode("latin-1"))
    parts.extend(xref)
    parts.append(b"trailer\n")
    parts.append(f"<< /Size {len(objects)+1} /Root 1 0 R >>\n".encode("latin-1"))
    parts.append(b"startxref\n")
    parts.append(f"{xref_pos}\n".encode("latin-1"))
    parts.append(b"%%EOF\n")
    return b"".join(parts)


def main() -> None:
    out = Path(__file__).with_name("ad-hoc-xfa.pdf")
    out.write_bytes(build_pdf(XDP))
    print(f"wrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()