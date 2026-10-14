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
shot_fdx_fountain = r"""INT. STUDY - DAY #16A#

!CLOSE ON THE LETTER

The seal breaks.

!MARA, JO\_ANN

!HOLD ON **CAFÉ** Ω

A plain action stays plain.
"""
assert (checks / "shot-fdx-export.fountain").read_text() == shot_fdx_fountain, "FDX Shot/Cast List text becomes forced Action"
shot_model_fountain = r"""INT. STUDY - DAY #16B#

!CLOSE ON THE LETTER

The seal breaks.

!MARA, JO\_ANN

!HOLD ON **CAFÉ** Ω[[Check angle]]

!# INSERT
!INT. THE LETTER - DAY
!> TO:
!~ QUIET

!Title: A literal heading

!The file is C:\\temp \*literal\* \_name\_.

MARA
Look here.

!CUT TO:

A plain action stays plain.
"""
assert (checks / "model-shot-export.fountain").read_text() == shot_model_fountain, "model Shot/Cast List/Unformatted Text forcing, escapes, styles and review"
assert (checks / "manager-shot.fountain").read_text() == shot_model_fountain.replace(
    " #16B#", "", 1), "explicit manager export retains all Shot/other content text"
reserved_dialogue = (fixtures / "reserved-dialogue.fountain").read_text()
for name in ("reserved-dialogue-export.fountain", "model-reserved-dialogue-export.fountain"):
    assert (checks / name).read_text() == reserved_dialogue, (name, "reserved dialogue escapes, styles and review positions")
assert (checks / "manager-reserved-dialogue.fountain").read_text() == reserved_dialogue.replace(
    " #17A#", "", 1), "explicit Export preserves reserved-start dialogue"
dialogue_identities = json.loads((checks / "reserved-dialogue-identities.json").read_text())
assert len(dialogue_identities) == 5 and len({record["uuid"] for record in dialogue_identities}) == 5
for record in dialogue_identities:
    label = record["label"]
    original = json.loads((checks / f"{label}-reopened.json").read_text())
    assert json.loads((checks / f"{label}-process-reopened.json").read_text()) == original
    paragraphs = [element for element in ET.parse(checks / f"{label}-serialized.xml").iter()
                  if element.find("v") is not None]
    assert [(p.tag, line) for p in paragraphs
            for line in html.unescape(p.findtext("v")).replace("\u2028", "\n").split("\n")] == [
        (row["type"], row["text"]) for row in original["rows"]
    ], (label, "independent XML types and complete physical-line text")
    assert len([p for p in paragraphs if p.tag == "scene_heading"]) == 1
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
def check_review_xml(name, expected, child_order):
    tree = ET.parse(checks / name)
    paragraphs = [element for element in tree.iter() if element.find("v") is not None]
    assert len(paragraphs) == len(expected), name
    for paragraph, (text, spans, reviews) in zip(paragraphs, expected):
        assert paragraph.findtext("v") == text, (name, text)
        size = len(text.encode("utf-16-le")) // 2
        actual_styles = [0] * size
        for span in paragraph.findall("fms/fm"):
            start, length = int(span.attrib["from"]), int(span.attrib["length"])
            assert 0 <= start <= start + length <= size, (name, span.attrib)
            bits = ("bold" in span.attrib) + 2 * ("italic" in span.attrib) + 4 * ("underline" in span.attrib)
            for position in range(start, start + length):
                actual_styles[position] |= bits
        expected_styles = [0] * size
        for start, length, bits in spans:
            expected_styles[start:start + length] = [bits] * length
        assert actual_styles == expected_styles, (name, text, actual_styles)
        # Internal CDATA holds HTML-escaped comment content, unlike wire-format
        # Fountain. Decode that documented layer after parsing the XML.
        actual_reviews = [(int(mark.attrib["from"]), int(mark.attrib["length"]), [html.unescape(comment.text or "") for comment in mark.findall("c")])
                          for mark in paragraph.findall("rms/rm")]
        expected_reviews = [(start, length, [messages] if isinstance(messages, str) else messages)
                            for start, length, messages in reviews]
        assert actual_reviews == expected_reviews, (name, text, actual_reviews)
        for mark in paragraph.findall("rms/rm"):
            assert mark.attrib["bgcolor"].lower() == "#ffd302" and "done" not in mark.attrib
            for comment in mark.findall("c"):
                assert comment.attrib["author"] == "fountain author" and comment.attrib["date"]
        if spans and reviews:
            assert [child.tag for child in paragraph] == child_order, (name, list(paragraph))


for prefix, expected in [("emphasis-review", combined_expected), ("review-offsets", [
    ("INT. OFFICE - DAY", [], []),
    ("A *literal* clue.", [], [(0, 17, "Check clue")]),
    ("The _plain_ symbol and *star* stay.", [], [(0, 35, "Check symbols")]),
    ("A red box and blue key.", [(2, 3, 1), (14, 4, 2)], [(0, 9, "First note"), (9, 13, "Second note")]),
    ("A *literal* and bold clue.", [(16, 4, 1)], [(0, 26, "Mixed note")]),
    ("MARA", [], []),
    ("A bold answer then quiet.", [(2, 4, 1), (19, 5, 4)],
     [(0, 13, "First dialogue note"), (13, 12, "Second dialogue note")]),
    ("A bold italic clue.", [(2, 11, 3), (14, 4, 4)], [(0, 19, "Combined note")]),
    ("A bold clue.", [(2, 4, 1)], [(0, 6, ["First reply", "Second reply"])]),
    ("A bold *star* still tail.", [(2, 17, 1)], [(0, 13, "Across style"), (13, 12, "Final style")]),
    ("A lead *literal*.", [], [(0, 7, "Before escape")]),
    ("A leadbold tail.", [(6, 4, 1)], [(0, 6, "Before bold")]),
    ("A 🙂 bold *literal* Ω clue.", [(5, 4, 1)], [(0, 27, "Unicode note")]),
])]:
    check_review_xml(prefix + "-raw.xml", expected, ["v", "fms", "rms"])
    check_review_xml(prefix + "-serialized.xml", expected, ["v", "rms", "fms"])

# Reviewed inline notes keep comments separate from their own text, with ranges counted
# in UTF-16 after removing the wrapper/padding, escapes and review syntax.
inline_expected = [
    ("INT. STUDIO - DAY", [], []),
    ("Before.", [], [(0, 7, "before")]),
    ("private ", [], [(0, 7, "about it")]),
    ("first then Ω🙂 [[literal]] at C:\\temp ", [(0, 5, 1), (11, 3, 4)],
     [(0, 5, ["one", "reply"]), (5, 32, "two [[x]] * and _ and \\")]),
    ("After.", [], [(0, 6, "after")]),
    ("line one\u2028line two ", [], [(0, 8, "line comment\nsecond */ inside review"), (8, 9, "tail")]),
    ("last ", [], [(0, 4, "end")]),
]
check_review_xml("inline-note-review-raw.xml", inline_expected, ["v", "fms", "rms"])
check_review_xml("inline-note-review-serialized.xml", inline_expected, ["v", "rms", "fms"])
canonical_inline = (fixtures / "inline-note-review.fountain").read_text()
assert (checks / "inline-note-review-export.fountain").read_text() == canonical_inline
model_inline = canonical_inline.replace("[[reply]]", "").replace(
    "/* line one[[line comment\nsecond \\*/ inside review]]\nline two[[tail]] */\n\n", "")
assert (checks / "model-inline-note-review-export.fountain").read_text() == model_inline
assert (checks / "manager-inline-note-review.fountain").read_text() == model_inline.replace(" #18A#", "", 1)
filtered_inline = (checks / "inline-note-review-filtered.fountain").read_text()
assert "/* private */" in filtered_inline and "/* last */" in filtered_inline
assert "[[about it]]" not in filtered_inline and "[[one]]" not in filtered_inline

# Exact output checks require each review or reply exactly once at its source
# anchor, after closing emphasis and before any next opening/escape prefix.
combined_annotated = """INT. ROOM - DAY #1#

A **bold** clue.

A clue.[[Review only]]

A **bold** clue.[[Check clue]]

The **red** box and *blue* key[[First note]] waits[[Second café & <tag>]].

MARA
A _quiet_ **bold** answer.[[Check answer]]

Final ***urgent*** _clue_.[[Final note]]
"""
assert (checks / "emphasis-review-export.fountain").read_text() == combined_annotated
expected_offsets = (fixtures / "review-offsets.fountain").read_text().replace(
    "INT. OFFICE - DAY", "INT. OFFICE - DAY #1#", 1)
assert (checks / "review-offsets-export.fountain").read_text() == expected_offsets, "review offsets, escapes, emphasis and reply syntax"

# Forced Action: the fixture is authored in the exporter's canonical spelling. Every
# Action that another paragraph rule would claim keeps "!", and no other line gains one.
forced_source = (fixtures / "forced-action.fountain").read_text()
forced_lines = [line for line in forced_source.split("\n") if line]
assert [line for line in forced_lines if not line.startswith("!")] == [
    "INT. STUDIO - DAY", "MARA", "A spoken line.", "A plain action stays plain.",
    "Into the room, a # sign and an @ sign.",
], "only the heading, the speech and the two ordinary actions are unforced"
assert [line[1] for line in forced_lines if line.startswith("!")] == list(
    "#@>=~I..!EEI(>=*@T#"), "one forced Action per reserved start or heuristic"
assert (checks / "forced-action-export.fountain").read_text() == forced_source.replace(
    "INT. STUDIO - DAY", "INT. STUDIO - DAY #1#", 1), "forced Action syntax"
# A first line beginning with a title page key would otherwise be read as a title page.
first_source = (fixtures / "forced-action-first.fountain").read_text()
assert first_source.startswith("!Source: ") and "\nTitles roll" in first_source
assert (checks / "forced-action-first-export.fountain").read_text() == first_source.replace(
    "INT. STUDIO - DAY", "INT. STUDIO - DAY #1#", 1), "forced first Action line"
# Each physical line of a model Action is forced on its own; a note's own line break
# neither moves the marker nor counts as a line of the paragraph.
model_forced = """INT. LAB - NIGHT #7#

The sign reads:
!# 1 in sales
!INT. means interior.

!BOB
walks in.

A marked line[[two
lines]]
!# 2 in sales

!BANG[[SEE
this]]

ADA
Yes.

!(She waits)
!~ humming
"""
assert (checks / "model-forced-export.fountain").read_text() == model_forced, "per-line forced Action"

# Literal syntax: brackets, backslashes and slashes of the text itself are escaped wherever
# they would otherwise be read as a note, an escape or an inline note, and nowhere else.
# The imported XML is read here without the application: its text holds no escapes.
literal_expected = [
    ("INT. ARCHIVE - DAY", [], []),
    ("Screen shows [[message]] then continues.", [], []),
    ("Triple [[[x]]] too.", [], []),
    ("The folder is C:\\temp.", [], []),
    ("\\\\server\\share opens.", [], []),
    ("A lone [bracket], a path/name and 2 + 2 stay plain.", [], []),
    ("He types /* remark */ and moves on.", [], []),
    ("C:\\bold follows a backslash.", [(3, 4, 1)], []),
    ("Either and/or is fine.", [(11, 2, 2)], []),
    ("The strong/ ending.", [(4, 7, 1)], []),
    ("A [ opens.", [], [(0, 3, "Bracket note")]),
    ("Path C:\\ here.", [], [(0, 8, "Drive note")]),
    ("A clue.", [], [(0, 7, "Check [[tag]] in C:\\dir, 2 * 3 and file_name]")]),
    ("MARA", [], []),
    ("reading [[aloud]]", [], []),
    ("Read [[this]] from D:\\notes.", [], []),
    ("Stressed first word.", [(0, 8, 6)], []),
    ("@home [[x]] waits.", [], []),
    ("The last line keeps a\\b.", [], []),
]
check_review_xml("literal-syntax-raw.xml", literal_expected, ["v", "fms", "rms"])
check_review_xml("literal-syntax-serialized.xml", literal_expected, ["v", "rms", "fms"])
check_review_xml("literal-syntax-fresh-serialized.xml", literal_expected, ["v", "rms", "fms"])
literal_source = (fixtures / "literal-syntax.fountain").read_text()
literal_lines = [line for line in literal_source.split("\n") if line]
assert [line for line in literal_lines if "\\" not in line] == [
    "INT. ARCHIVE - DAY", "A lone [bracket], a path/name and 2 + 2 stay plain.", "MARA",
    "_*Stressed*_ first word.",
], "single brackets, a slash between words and ordinary emphasis need no escape"
assert sum(line.count("[[") for line in literal_lines) == 3, "only the three notes open with [["
assert (checks / "literal-syntax-export.fountain").read_text() == literal_source.replace(
    "INT. ARCHIVE - DAY", "INT. ARCHIVE - DAY #1#", 1), "literal syntax spelling"
# The same kinds of text typed into the model. A bracket before a note, a slash before
# emphasis and the text of a comment are escaped too; the note delimiters are not.
# <TAB> stands for the tab of a paragraph that ends in spaces of two different styles.
model_literal = r"""INT. VAULT - NIGHT #9#

Screen shows \[\[message\]\] then continues.

The folder is C:\\temp.

C:\\**bold** after it.

Either and\/*or* works.

*Trailing***  <TAB>**

An odd \[[[a\]\] b\\c \]]] here.

Marked \_name\_[[2 \* 3 \* 4 and \[\[x\]\]]] here.

ADA
Say \[\[it\]\] at C:\\x.

/* Keep \[\[this\]\] \*/ and a\\b */
""".replace("<TAB>", "\t")
assert (checks / "model-literal-export.fountain").read_text() == model_literal, "model literal syntax"
assert (checks / "manager-literal.fountain").read_text() == model_literal.replace(
    " #9#", "", 1), "source Save route writes the same escapes"
check_review_xml("model-literal-fresh-serialized.xml", [
    ("INT. VAULT - NIGHT", [], []),
    ("Screen shows [[message]] then continues.", [], []),
    ("The folder is C:\\temp.", [], []),
    ("C:\\bold after it.", [(3, 4, 1)], []),
    ("Either and/or works.", [(11, 2, 2)], []),
    # Trailing spaces of two styles: the words stay, one bold space is what can be written.
    ("Trailing ", [(0, 8, 2), (8, 1, 1)], []),
    ("An odd [ here.", [], [(0, 8, "a]] b\\c ]")]),
    ("Marked _name_ here.", [], [(0, 13, "2 * 3 * 4 and [[x]]")]),
    ("ADA", [], []),
    ("Say [[it]] at C:\\x.", [], []),
    # The importer keeps the space written before the closing delimiter of an inline note.
    ("Keep [[this]] */ and a\\b ", [], []),
], ["v", "rms", "fms"])


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
