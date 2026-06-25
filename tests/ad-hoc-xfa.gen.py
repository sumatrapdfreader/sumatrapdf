#!/usr/bin/env python3
"""Generate tests/ad-hoc-xfa.pdf: pure-XFA fixture for ad-hoc-xfa.ts.

Two pageAreas with usehref prototypes plus tb-flowed body draws (overflow to page 2).
Regenerate:

  python tests/ad-hoc-xfa.gen.py
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

# Pure-XFA form: prototypes + usehref instances on two pages.
TEMPLATE = """<template xmlns="http://www.xfa.org/schema/xfa-template/3.3/">
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
          <draw x="0.75in" y="1.5in" w="3in" h="0.3in">
            <border>
              <fill>
                <color value="220,220,255"/>
              </fill>
            </border>
            <font size="10pt"/>
            <para vAlign="bottom" hAlign="center" marginLeft="4pt"/>
            <value>
              <text>Section</text>
            </value>
          </draw>
          <draw x="0.75in" y="1.85in" w="3in" h="0pt">
            <line x1="0in" y1="0in" x2="3in" y2="0in"/>
          </draw>
          <draw x="0.75in" y="2.2in" w="2in" h="0.8in">
            <font size="10pt"/>
            <para lineHeight="14pt" vAlign="top"/>
            <value>
              <text>Line one&#xA;Line two</text>
            </value>
          </draw>
          <draw x="3in" y="2.2in" w="1.5in" h="0.6in">
            <font size="9pt"/>
            <para lineHeight="12pt" hAlign="center"/>
            <value>
              <text>This is a long text that should wrap</text>
            </value>
          </draw>
          <draw x="0.75in" y="3in" w="1.8in" h="0.7in">
            <font size="9pt"/>
            <para lineHeight="11pt" hAlign="justify"/>
            <value>
              <text>The quick brown fox jumps over the lazy dog</text>
            </value>
          </draw>
          <draw x="3in" y="3in" w="1.8in" h="0.7in">
            <font size="9pt"/>
            <para lineHeight="11pt" hAlign="justifyAll"/>
            <value>
              <text>Pack my box with five dozen liquor jugs today</text>
            </value>
          </draw>
          <subform name="lrSection" layout="lr-tb" x="3.25in" y="3.85in" w="3in" h="0.5in">
            <draw w="1.2in" h="0.2in">
              <value>
                <text>Lr A</text>
              </value>
            </draw>
            <draw w="1.2in" h="0.2in">
              <value>
                <text>Lr B</text>
              </value>
            </draw>
            <draw w="1.2in" h="0.2in">
              <value>
                <text>Lr C</text>
              </value>
            </draw>
          </subform>
          <subform name="stackedSection" layout="tb" x="0.75in" y="3.85in">
            <draw w="2in" h="0.2in">
              <value>
                <text>Stack A</text>
              </value>
            </draw>
            <draw w="2in" h="0.2in">
              <value>
                <text>Stack B</text>
              </value>
            </draw>
            <field name="stackField" w="2in" h="0.25in">
              <border>
                <edge stroke="lowered"/>
              </border>
              <value>
                <text>C</text>
              </value>
            </field>
          </subform>
          <draw x="3.25in" y="4.5in" w="2in" h="0.25in">
            <font typeface="Courier" posture="italic" size="10pt"/>
            <value>
              <text>Courier italic</text>
            </value>
          </draw>
          <draw x="0.75in" y="4.8in" w="2in" h="0.25in">
            <font typeface="AliasCourier" posture="italic" size="10pt"/>
            <value>
              <text>Alias courier</text>
            </value>
          </draw>
        </contentArea>
      </pageArea>
    </pageSet>
    <draw id="labelProto" w="2in" h="0.2in">
      <value>
        <text>Label:</text>
      </value>
    </draw>
    <field id="fieldProto" w="3in" h="0.25in">
      <border>
        <fill>
          <color value="255,255,220"/>
        </fill>
        <edge stroke="lowered"/>
      </border>
      <ui>
        <textEdit>
          <margin leftInset="2pt" rightInset="2pt" topInset="1pt" bottomInset="1pt"/>
        </textEdit>
      </ui>
      <para vAlign="middle" marginLeft="3pt"/>
      <value>
        <text>placeholder</text>
      </value>
    </field>
    <draw name="flowSpacer" w="8in" h="10.4in">
      <value>
        <text> </text>
      </value>
    </draw>
    <draw name="flowPage2" w="4in" h="0.2in">
      <value>
        <text>Flowed on page 2</text>
      </value>
    </draw>
  </subform>
</template>
"""

DATASETS = """<datasets xmlns="http://www.xfa.org/schema/xfa-data/1.0/">
  <data>
    <firstName>Alice</firstName>
    <lastName>Bob</lastName>
    <stackField>C</stackField>
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
    out = Path(__file__).with_name("ad-hoc-xfa.pdf")
    out.write_bytes(build_pdf())
    print(f"wrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()