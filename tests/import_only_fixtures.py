"""Disposable import sources with data that a screenplay export must not rewrite."""

from pathlib import Path
import sys
import zipfile

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
document = b'''<?xml version="1.0" encoding="UTF-8"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:body><w:p><w:r><w:t>INT. LAB - DAY</w:t></w:r></w:p>
<w:p><w:r><w:t>Import-only source survives.</w:t></w:r></w:p>
<w:sectPr/></w:body></w:document>'''
with zipfile.ZipFile(root / "source.docx", "w") as archive:
    archive.writestr("[Content_Types].xml", '''<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>''')
    archive.writestr("_rels/.rels", '''<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>''')
    archive.writestr("word/document.xml", document)
    archive.writestr("customXml/retained.xml", "<retained>Preserve this source-only content.</retained>")
(root / "source.DOCX").write_bytes((root / "source.docx").read_bytes())
(root / "source.md").write_bytes(b"INT. LAB - DAY\n\nImport-only source survives.\n")
for suffix in ("fountain", "txt", "FOUNTAIN", "TXT"):
    (root / ("source." + suffix)).write_bytes(
        b"\xef\xbb\xbfINT. LAB - DAY\r\n\r\n!Import-only source survives.\r\n\r\n"
        b"/* Source-only spelling and CRLF must remain untouched. */\r\n")
for suffix in ("fdx", "fdxt", "FDX", "FDXT"):
    (root / ("source." + suffix)).write_bytes(b'''<?xml version="1.0" encoding="UTF-8"?>\r
<FinalDraft DocumentType="Script" Version="1"><Content>\r
<Paragraph Type="Scene Heading"><Text>INT. LAB - DAY</Text></Paragraph>\r
<Paragraph Type="Action"><Text>Import-only source survives.</Text></Paragraph>\r
</Content><RetainedMetadata>Never rewrite this source-only data.</RetainedMetadata></FinalDraft>\r
''')

stream = b"BT /F1 12 Tf 72 720 Td (INT. LAB - DAY) Tj 0 -36 Td (Import-only source survives.) Tj ET\n"
objects = [
    b"<< /Type /Catalog /Pages 2 0 R >>",
    b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>",
    b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"endstream",
]
pdf = bytearray(b"%PDF-1.4\n")
offsets = [0]
for number, obj in enumerate(objects, 1):
    offsets.append(len(pdf))
    pdf.extend(f"{number} 0 obj\n".encode() + obj + b"\nendobj\n")
xref = len(pdf)
pdf.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
for offset in offsets[1:]:
    pdf.extend(f"{offset:010d} 00000 n \n".encode())
pdf.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
(root / "source.pdf").write_bytes(pdf)
