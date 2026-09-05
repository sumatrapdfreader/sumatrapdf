// Builds tests/xmp-properties.pdf (Catalog /Metadata, no trailer /Info) and
// tests/xmp-properties-info.pdf (Info Title/Producer plus different XMP).
// Regenerate: bun tests/xmp-properties-make.ts

import { writeFileSync } from "node:fs";
import { join } from "node:path";

const DIR = import.meta.dir;

const XMP_ONLY = `<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description rdf:about=""
 xmlns:dc="http://purl.org/dc/elements/1.1/"
 xmlns:xmp="http://ns.adobe.com/xap/1.0/"
 xmlns:pdf="http://ns.adobe.com/pdf/1.3/"
 pdf:Producer="XmpOnlyProducer">
<dc:title><rdf:Alt>
 <rdf:li xml:lang="x-default">XmpOnlyTitle</rdf:li>
 <rdf:li xml:lang="fr">FrenchTitle</rdf:li>
</rdf:Alt></dc:title>
<dc:creator><rdf:Seq>
 <rdf:li>XmpAuthorOne</rdf:li>
 <rdf:li>XmpAuthorTwo</rdf:li>
</rdf:Seq></dc:creator>
<dc:description><rdf:Alt>
 <rdf:li xml:lang="x-default">XmpOnlySubject</rdf:li>
</rdf:Alt></dc:description>
<dc:subject><rdf:Bag>
 <rdf:li>alpha</rdf:li>
 <rdf:li>beta</rdf:li>
</rdf:Bag></dc:subject>
<dc:rights><rdf:Alt>
 <rdf:li xml:lang="x-default">XmpOnlyRights</rdf:li>
</rdf:Alt></dc:rights>
<xmp:CreatorTool>XmpOnlyTool</xmp:CreatorTool>
<xmp:CreateDate>2024-03-15T12:30:00Z</xmp:CreateDate>
<xmp:ModifyDate>2024-03-16T08:00:00+02:00</xmp:ModifyDate>
</rdf:Description>
</rdf:RDF>
</x:xmpmeta>
<?xpacket end="w"?>`;

const XMP_WITH_INFO = `<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description rdf:about=""
 xmlns:dc="http://purl.org/dc/elements/1.1/"
 xmlns:xap="http://ns.adobe.com/xap/1.0/"
 xmlns:pdf="http://ns.adobe.com/pdf/1.3/">
<dc:title>XmpTitleMustNotWin</dc:title>
<dc:creator>XmpAuthor</dc:creator>
<pdf:Producer>XmpProducerMustNotWin</pdf:Producer>
<xap:CreatorTool>XmpTool</xap:CreatorTool>
</rdf:Description>
</rdf:RDF>
</x:xmpmeta>
<?xpacket end="w"?>`;

function buildPdf(xmp: string, info?: string): Buffer {
  const content = "q Q";
  const objs: string[] = [];
  // 1 Catalog
  objs.push("<< /Type /Catalog /Pages 2 0 R /Metadata 4 0 R >>");
  // 2 Pages
  objs.push("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  // 3 Page
  objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 72 72] /Contents 5 0 R >>");
  // 4 XMP
  objs.push(`<< /Type /Metadata /Subtype /XML /Length ${xmp.length} >>\nstream\n${xmp}\nendstream`);
  // 5 content
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  if (info) {
    objs.push(info);
  }

  let pdf = "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }

  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  let trailer = `<< /Size ${objs.length + 1} /Root 1 0 R`;
  if (info) {
    trailer += " /Info 6 0 R";
  }
  trailer += " >>";
  pdf += `trailer\n${trailer}\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

const onlyPath = join(DIR, "xmp-properties.pdf");
const infoPath = join(DIR, "xmp-properties-info.pdf");
writeFileSync(onlyPath, buildPdf(XMP_ONLY));
writeFileSync(infoPath, buildPdf(XMP_WITH_INFO, "<< /Title (InfoTitle) /Producer (InfoProducer) >>"));
console.log(`wrote ${onlyPath}`);
console.log(`wrote ${infoPath}`);
