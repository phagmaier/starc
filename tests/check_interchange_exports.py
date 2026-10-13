"""Independent wire-format assertions; never use the application's importer as the oracle."""
import hashlib
import html
import json
from pathlib import Path
import sys
import sqlite3
import xml.etree.ElementTree as ET

evidence, fixtures = map(Path, sys.argv[1:])
checks = evidence / "checks"
hashes = json.loads((evidence / "fixture-hashes.json").read_text())
assert hashes == {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in fixtures.iterdir() if p.is_file()}, "authored fixture bytes changed"

# Explicit canonical Fountain spelling, including forced headings/actions, escaping,
# scene numbers, parenthetical delimiters and exact blank-line/dialogue adjacency.
expected_fountain = """INT. CAFÉ - DAY #12A#

A **bold** *quiet* _signal_ & <lamp> greets Zoë.

A second action line.

MARA
(under her breath)
First line: café & tea.
Second line: Ω waits.

CUT TO:


.ROOFTOP AT DAWN #27#

!THE WIND RISES

MCKAY
(softly)
Keep the \\*literal\\* star.

> FADE AWAY.
"""
assert (checks / "fountain-export.fountain").read_text() == expected_fountain, "Fountain syntax/order/spacing"
model_fountain = """INT. WORKSHOP - DAY #41C#

A **copper** & <glass> bell.

ADA
(listening)
One café.
Two Ω.

FADE TO:
"""
assert (checks / "model-export.fountain").read_text() == model_fountain, "model Fountain syntax"
notes = (checks / "notes-dual-export.fountain").read_text()
assert "A clue[[Check café & <tag>]]." in notes, "review note spelling/anchor"
assert "/* Private note */" in notes, "inline note delimiter"
assert "ALICE\nLeft voice.\n\nBOB ^\nRight voice." in notes, "dual dialogue spelling/order"

# Check both supported child orders independently. Do not use a successful parse
# alone as evidence that either the notes or the emphasis survived.
combined_expected = [
    ("INT. ROOM - DAY", [], []),
    ("A bold clue.", [(2, 4, 1)], []),
    ("A clue.", [], [(0, 7, "Review only")]),
    ("A bold clue.", [(2, 4, 1)], [(0, 12, "Check clue")]),
    ("The red box and blue key waits.", [(4, 3, 1), (16, 4, 2)],
     [(0, 24, "First note"), (24, 6, "Second café & <tag>")]),
    ("MARA", [], []),
    ("A quiet bold answer.", [(2, 5, 4), (8, 4, 1)], [(0, 20, "Check answer")]),
    ("Final urgent clue.", [(6, 6, 3), (13, 4, 4)], [(0, 18, "Final note")]),
]
for name, child_order in [("emphasis-review-raw.xml", ["v", "fms", "rms"]),
                          ("emphasis-review-serialized.xml", ["v", "rms", "fms"])]:
    tree = ET.parse(checks / name)
    paragraphs = [element for element in tree.iter() if element.find("v") is not None]
    assert len(paragraphs) == len(combined_expected), name
    for paragraph, (text, spans, reviews) in zip(paragraphs, combined_expected):
        assert paragraph.findtext("v") == text, (name, text)
        actual_styles = [0] * len(text)
        for span in paragraph.findall("fms/fm"):
            start, length = int(span.attrib["from"]), int(span.attrib["length"])
            assert 0 <= start <= start + length <= len(text), (name, span.attrib)
            bits = ("bold" in span.attrib) + 2 * ("italic" in span.attrib) + 4 * ("underline" in span.attrib)
            for position in range(start, start + length):
                actual_styles[position] |= bits
        expected_styles = [0] * len(text)
        for start, length, bits in spans:
            expected_styles[start:start + length] = [bits] * length
        assert actual_styles == expected_styles, (name, text, actual_styles)
        # Internal CDATA holds HTML-escaped comment content, unlike wire-format
        # Fountain. Decode that documented layer after parsing the XML.
        actual_reviews = [(int(mark.attrib["from"]), int(mark.attrib["length"]), html.unescape(mark.findtext("c")))
                          for mark in paragraph.findall("rms/rm")]
        assert actual_reviews == reviews, (name, text, actual_reviews)
        for mark in paragraph.findall("rms/rm"):
            assert mark.attrib["bgcolor"].lower() == "#ffd302" and "done" not in mark.attrib
            assert mark.find("c").attrib["author"] == "fountain author"
            assert mark.find("c").attrib["date"] and len(mark.findall("c")) == 1
        if spans and reviews:
            assert [child.tag for child in paragraph] == child_order, (name, list(paragraph))

# This deliberately excludes reviews while issue 13's exporter offsets remain
# open. Native annotation persistence above is checked without that filtering.
combined_filtered = """INT. ROOM - DAY #1#

A **bold** clue.

A clue.

A **bold** clue.

The **red** box and *blue* key waits.

MARA
A _quiet_ **bold** answer.

Final ***urgent*** _clue_.
"""
assert (checks / "emphasis-review-export.fountain").read_text() == combined_filtered


def check_fdx(name, expected, numbers, emphasis):
    data = (checks / name).read_bytes()
    root = ET.fromstring(data)
    assert root.tag == "FinalDraft" and root.attrib["DocumentType"] == "Script"
    paragraphs = root.find("Content").findall("Paragraph")
    actual = [(p.attrib["Type"], "".join(t.text or "" for t in p.findall("Text")))
              for p in paragraphs]
    assert actual == expected, (name, actual, expected)
    assert [p.attrib.get("Number") for p in paragraphs if p.attrib["Type"] == "Scene Heading"] == numbers
    for index, text, bits in emphasis:
        runs = paragraphs[index].findall("Text")
        rendered = "".join(r.text or "" for r in runs)
        styles = []
        for run in runs:
            words = run.attrib.get("Style", "").split("+")
            flags = ("Bold" in words) + 2 * ("Italic" in words) + 4 * ("Underline" in words)
            styles.extend([flags] * len(run.text or ""))
        start = rendered.index(text)
        assert styles[start:start + len(text)] == [bits] * len(text), (name, text, styles)
    assert b"&amp;" in data and b"&lt;" in data and b"&gt;" in data, "XML escaping"
    assert root.find("TitlePage") is None, "title page disabled in body checks"


check_fdx("fdx-export.fdx", [
    ("Scene Heading", "EXT. HARBOR - NIGHT"),
    ("Action", "A bright  blue beacon & <buoy> guides René.\u2028Another line."),
    ("Character", "LENA"), ("Parenthetical", "(almost silent)"),
    ("Dialogue", 'Say "go" & wait.\u2028Ω answers.'), ("Transition", "DISSOLVE TO:"),
    ("Scene Heading", "INT. CABIN - LATER"), ("Action", "RAIN HAMMERS THE ROOF"),
    ("Character", "NOAH"), ("Parenthetical", "(steady)"), ("Dialogue", "It's safe now.")
], ["8B", "19"], [(1, "bright", 1), (1, "blue", 6), (3, "silent", 2)])
check_fdx("model-export.fdx", [
    ("Scene Heading", "INT. WORKSHOP - DAY"), ("Action", "A copper & <glass> bell."),
    ("Character", "ADA"), ("Parenthetical", "(listening)"),
    ("Dialogue", "One café.\u2028Two Ω."), ("Transition", "FADE TO:")
], ["41C"], [(1, "copper", 1)])
manager_root = ET.parse(checks / "manager-export.fdx").getroot()
assert manager_root.tag == "FinalDraft" and manager_root.attrib["DocumentType"] == "Script"
expected_body = ET.parse(checks / "fdx-export.fdx").getroot().find("Content")
manager_body = manager_root.find("Content")
assert [(p.attrib["Type"], "".join(t.text or "" for t in p.findall("Text"))) for p in manager_body] == [
    (p.attrib["Type"], "".join(t.text or "" for t in p.findall("Text"))) for p in expected_body
], "ordinary-save route retains FDX body"
assert all("Number" not in p.attrib for p in manager_body), "production information settings hide numbers"
flattened = ET.parse(checks / "dual-flattened.fdx").getroot()
assert "Check café" not in (checks / "dual-flattened.fdx").read_text(), "FDX does not carry review comments"
assert flattened.find(".//DualDialogue") is None
assert [(p.attrib["Type"], "".join(t.text or "" for t in p.findall("Text")))
        for p in flattened.find("Content")] == [
    ("Scene Heading", "INT. STUDIO - DAY"), ("Action", "A clue."),
    ("Character", "ALICE"), ("Dialogue", "Left voice."),
    ("Character", "BOB"), ("Dialogue", "Right voice."), ("Action", "The voices stop.")
], "FDX flattening retains speech/order without table-boundary paragraphs"
assert "[[" not in (checks / "notes-filtered.fountain").read_text()
assert "/*" not in (checks / "notes-filtered.fountain").read_text()
assert "#" not in (checks / "unnumbered.fountain").read_text()
assert all("Number" not in p.attrib for p in ET.parse(checks / "unnumbered.fdx").findall(".//Content/Paragraph"))
with sqlite3.connect(f"file:{checks / 'round-trips.starc'}?mode=ro", uri=True) as database:
    assert database.execute("PRAGMA integrity_check").fetchone() == ("ok",)
print("PASS: independent Fountain syntax, FDX XML/text/styles/numbers, and fixture SHA-256 checks")
