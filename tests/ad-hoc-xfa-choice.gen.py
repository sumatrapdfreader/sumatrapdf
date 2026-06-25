#!/usr/bin/env python3
"""Generate tests/ad-hoc-xfa-data/ad-hoc-xfa-choice.pdf (pure XFA + dropDownList).

Uses the same PDF shell and prototype pattern as tests/ad-hoc-xfa.gen.py. Regenerate:

  python tests/ad-hoc-xfa-choice.gen.py
"""

from pathlib import Path

PREAMBLE = """<?xml version="1.0" encoding="UTF-8"?>
<xdp xmlns="http://ns.adobe.com/xdp/">
"""

CONFIG = """<config xmlns="http://www.xfa.org/schema/xci/3.0/">
  <present>
    <pdf>
      <fontInfo>
        <embed>1</embed>
        <defaultTypeface writingScript="*">Courier</defaultTypeface>
        <map>
          <equate from="AliasCourier" to="Courier"/>
        </map>
      </fontInfo>
    </pdf>
  </present>
  <psMap>
    <font typeface="Courier" psName="Courier" weight="normal" posture="italic"/>
  </psMap>
</config>
"""

TEMPLATE = """<template xmlns="http://www.xfa.org/schema/xfa-template/3.3/">
  <subform name="form1" layout="tb">
    <pageSet>
      <pageArea name="Page1" id="Page1">
        <medium short="8.5in" long="11in"/>
        <contentArea x="0.25in" y="0.25in" w="8in" h="10.5in">
          <draw usehref="#labelProto" x="0.75in" y="0.75in">
            <value>
              <text>Filing status:</text>
            </value>
          </draw>
          <field usehref="#choiceProto" name="filingStatus" x="0.75in" y="1in"/>
        </contentArea>
      </pageArea>
    </pageSet>
    <draw id="labelProto" w="2in" h="0.2in">
      <value>
        <text>Label:</text>
      </value>
    </draw>
    <field id="choiceProto" w="3in" h="0.25in">
      <border>
        <edge stroke="lowered"/>
      </border>
      <ui>
        <dropDownList/>
      </ui>
      <items>
        <text>Single</text>
        <text>Married filing jointly</text>
        <text>Married filing separately</text>
      </items>
      <para vAlign="middle"/>
    </field>
  </subform>
</template>
"""

DATASETS = """<datasets xmlns="http://www.xfa.org/schema/xfa-data/1.0/">
  <data>
    <filingStatus>Single</filingStatus>
  </data>
</datasets>
"""

POSTAMBLE = "</xdp>\n"


def stream_obj(text: str) -> str:
    data = text.encode("utf-8")
    return f"<< /Length {len(data)} >>\nstream\n".encode("latin-1").decode("latin-1") + text + "\nendstream"


def build_pdf() -> bytes:
    packets = [
        ("xdp:xdp", PREAMBLE),
        ("config", CONFIG),
        ("template", TEMPLATE),
        ("datasets", DATASETS),
        ("xdp:xdp", POSTAMBLE),
    ]
    objects = []

    def add(obj: str) -> int:
        objects.append(obj.encode("latin-1"))
        return len(objects)

    add("<< /Type /Catalog /Pages 2 0 R /AcroForm 4 0 R >>")
    add("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
    add("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
    xfa_refs = " ".join(f"({name}) {5 + i} 0 R" for i, (name, _) in enumerate(packets))
    add(f"<< /Fields [] /DR << /Font << /F1 {5 + len(packets)} 0 R >> >> /XFA [{xfa_refs}] >>")
    for _, text in packets:
        add(stream_obj(text))
    add("<< /Type /Font /Subtype /Type1 /BaseFont /Courier /Encoding /WinAnsiEncoding >>")

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
    out = Path(__file__).parent / "ad-hoc-xfa-data" / "ad-hoc-xfa-choice.pdf"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(build_pdf())
    print(f"wrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()