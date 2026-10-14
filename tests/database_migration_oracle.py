"""Independent byte and schema oracle for WORKLIST issue 21; disposable files only."""

import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path


ROOT = Path(sys.argv[1])
BINARY = Path(sys.argv[2])
BOM = b"\xef\xbb\xbf"
STRUCTURE = b'<structure color="#000000"><v>folder folders</v></structure>\r\n'
UNCHANGED = b'<document>\r\n<action a="folder"><v><![CDATA[folder <folder/> folders]]></v></action></document>'
UNKNOWN = b'unsupported folder\x00\xff\r\n'

# Before and after are authored separately: only the XML element names differ.
LEGACY_SCRIPT = BOM + r"""<?xml version="1.0" encoding="UTF-8"?>
<?folder folder_header?>
<!DOCTYPE document [<!ENTITY literal "folder folders">]>
<document xmlns:x="urn:future"><!-- <folder> folder_header folder_footer -->
<folder uuid="folder-id" label="folder > folders">
<folder_header uuid="heading"><v><![CDATA[🙂 The folder is C:\temp. <folder/> folders]]></v>
<rms><rm from="0" length="2" folder="folder"><c><![CDATA[folder folders folder_header]]></c></rm></rms></folder_header>
<folder_footer uuid="footer"><v>&literal; &lt;folder&gt;</v></folder_footer>
<folder_header/><folder_footer />
<folderish folder="folder"/><x:folder>folder</x:folder></folder></document>
""".replace("\n", "\r\n").encode()
MIGRATED_SCRIPT = BOM + r"""<?xml version="1.0" encoding="UTF-8"?>
<?folder folder_header?>
<!DOCTYPE document [<!ENTITY literal "folder folders">]>
<document xmlns:x="urn:future"><!-- <folder> folder_header folder_footer -->
<sequence uuid="folder-id" label="folder > folders">
<sequence_heading uuid="heading"><v><![CDATA[🙂 The folder is C:\temp. <folder/> folders]]></v>
<rms><rm from="0" length="2" folder="folder"><c><![CDATA[folder folders folder_header]]></c></rm></rms></sequence_heading>
<sequence_footer uuid="footer"><v>&literal; &lt;folder&gt;</v></sequence_footer>
<sequence_heading/><sequence_footer />
<folderish folder="folder"/><x:folder>folder</x:folder></sequence></document>
""".replace("\n", "\r\n").encode()
LEGACY_COMIC = b'''<document><!-- <page_name/> panel_name -->\r\n<page_name a="page_name panel_name"><v><![CDATA[page_name panel_name]]></v></page_name><panel_name/><panel_names/></document>'''
MIGRATED_COMIC = b'''<document><!-- <page_name/> panel_name -->\r\n<page_heading a="page_name panel_name"><v><![CDATA[page_name panel_name]]></v></page_heading><panel_heading/><panel_names/></document>'''
MODERN_SCRIPT = MIGRATED_SCRIPT
MODERN_COMIC = MIGRATED_COMIC


def connect(root):
    return sqlite3.connect(root / "story.starc")


def snapshot(root):
    with connect(root) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        documents = db.execute(
            "SELECT id, uuid, type, CAST(content AS BLOB) FROM documents ORDER BY id"
        ).fetchall()
        history_columns = [row[1] for row in db.execute("PRAGMA table_info(documents_changes)")]
        history = db.execute(
            "SELECT id, fk_document_uuid, uuid, undo_patch, redo_patch, date_time, user_name, user_email"
            " FROM documents_changes ORDER BY id"
        ).fetchall()
        variables = dict(db.execute("SELECT variable, value FROM system_variables"))
        schema = db.execute("SELECT type, name, sql FROM sqlite_master ORDER BY name").fetchall()
        synced = (db.execute("SELECT uuid, synced_at FROM documents ORDER BY uuid").fetchall()
                  if "synced_at" in [row[1] for row in db.execute("PRAGMA table_info(documents)")]
                  else None)
        history_synced = (db.execute("SELECT uuid, is_synced FROM documents_changes ORDER BY id").fetchall()
                          if "is_synced" in history_columns else None)
    return documents, history, variables, schema, synced, history_synced


def run(root, phase, label):
    with (root / f"{label}.log").open("wb") as log:
        result = subprocess.run([BINARY, root, phase], stdout=log, stderr=subprocess.STDOUT)
    assert result.returncode == 0, (root, label, (root / f"{label}.log").read_text())


def copy_case(name, version=None, old=False, mid=False):
    root = ROOT / name
    root.mkdir()
    shutil.copyfile(ROOT / "new" / "story.starc", root / "story.starc")
    with connect(root) as db:
        db.execute("DELETE FROM system_variables WHERE variable = 'application-version'")
        if version is not None:
            db.execute("INSERT INTO system_variables VALUES ('application-version', ?)", (version,))
        if old or mid:
            db.execute("ALTER TABLE documents DROP COLUMN synced_at")
        if old:
            db.execute("ALTER TABLE documents_changes DROP COLUMN is_synced")
            db.execute("UPDATE documents SET content = ? WHERE uuid = 'script'", (LEGACY_SCRIPT,))
            db.execute("UPDATE documents SET content = ? WHERE uuid = 'comic'", (LEGACY_COMIC,))
    return root


def verify_success(root, renames=False, colors=False, new=False):
    shutil.copyfile(root / "story.starc", root / "before.starc")
    before = snapshot(root)
    expected = [(id_, uuid, type_,
                 MIGRATED_SCRIPT if renames and uuid == "script" else
                 MIGRATED_COMIC if renames and uuid == "comic" else
                 STRUCTURE.replace(b"#000000", b"") if colors and uuid == "structure" else content)
                for id_, uuid, type_, content in before[0]]
    run(root, "open", "first-open")
    after = snapshot(root)
    assert after[0] == expected, (root, "exact document bytes, IDs and types")
    assert after[1] == before[1], (root, "exact ordered history")
    assert after[2]["application-version"] == "0.8.4", (root, "current version")
    upgrades = [key for key in after[2] if key.startswith("application-updated-to-version-")]
    assert upgrades == ([] if new else ["application-updated-to-version-0.8.4"]), root
    assert after[4] == (before[4] if before[4] is not None else
                        [(uuid, None) for uuid in sorted(row[1] for row in expected)]), root
    assert after[5] == (before[5] if before[5] is not None else [("change", 0)]), root
    with connect(root) as db, connect(ROOT / "new") as current:
        for table in ("documents", "documents_changes"):
            assert db.execute(f"PRAGMA table_info({table})").fetchall() == current.execute(
                f"PRAGMA table_info({table})").fetchall(), (root, "complete current schema")
    if new:
        assert after == before, (root, "new story is unchanged on its first reopen")
    run(root, "open", "second-open")
    assert snapshot(root) == after, (root, "second reopen is entirely unchanged")
    for _, uuid, _, content in after[0]:
        (root / f"after-{uuid}.bin").write_bytes(content)
    print(f"PASS: {root.name}: exact documents/identities/history/schema across two fresh opens")


def verify_refusal(root):
    shutil.copyfile(root / "story.starc", root / "before.starc")
    before = snapshot(root)
    for label in ("first-open", "second-open"):
        run(root, "refuse", label)
        assert snapshot(root) == before, (root, "failed migration rolled back content/schema/version")
    print(f"PASS: {root.name}: reported error and exact rollback across two fresh opens")


new = ROOT / "new"
new.mkdir()
for name, content in (("structure", STRUCTURE), ("script", MODERN_SCRIPT),
                      ("comic", MODERN_COMIC), ("unchanged", UNCHANGED), ("unknown", UNKNOWN)):
    (new / f"{name}.bin").write_bytes(content)
run(new, "create", "create")
verify_success(new, new=True)
verify_success(copy_case("unversioned-modern"))
verify_success(copy_case("empty-version-modern", version=""))
verify_success(copy_case("unversioned-0.2.4", mid=True))
verify_success(copy_case("unversioned-legacy", old=True), renames=True, colors=True)
verify_success(copy_case("versioned-0.1.2", version="0.1.2", old=True), renames=True)
verify_success(copy_case("versioned-0.1.3-dev", version="0.1.3 dev fake", old=True), renames=True)

bad = copy_case("malformed-legacy", version="0.0.9", old=True)
with connect(bad) as db:
    db.execute("UPDATE documents SET content = ? WHERE uuid = 'script'", (b"<document><folder>",))
verify_refusal(bad)

locked = ROOT / "locked-new"
locked.mkdir()
for name in ("structure", "script", "comic", "unchanged", "unknown"):
    shutil.copyfile(new / f"{name}.bin", locked / f"{name}.bin")
with connect(locked) as db:
    db.execute("PRAGMA user_version=0")
    db.commit()
    before_bytes = (locked / "story.starc").read_bytes()
    db.execute("BEGIN EXCLUSIVE")
    run(locked, "refuse", "locked-first-create")
    run(locked, "refuse", "locked-second-create")
assert (locked / "story.starc").read_bytes() == before_bytes, "failed creation preserves bytes"
with connect(locked) as db:
    assert db.execute("SELECT name FROM sqlite_master WHERE type = 'table'").fetchall() == []
run(locked, "create", "recovered-create")
verify_success(locked, new=True)

bad = copy_case("invalid-utf8-legacy", version="0.0.9", old=True)
with connect(bad) as db:
    db.execute("UPDATE documents SET content = ? WHERE uuid = 'script'", (b"<document>\xff</document>",))
verify_refusal(bad)

bad = copy_case("expanded-entity-legacy", version="0.0.9", old=True)
with connect(bad) as db:
    db.execute("UPDATE documents SET content = ? WHERE uuid = 'script'",
               (b"<!DOCTYPE folder [<!ENTITY x '<folder/>'>]><folder>&x;</folder>",))
verify_refusal(bad)

bad = copy_case("write-failure-legacy", version="0.0.9", old=True)
with connect(bad) as db:
    db.execute("CREATE TRIGGER refuse_script BEFORE UPDATE ON documents WHEN OLD.uuid = 'script'"
               " BEGIN SELECT RAISE(ABORT, 'test-only migration write failure'); END")
verify_refusal(bad)

bad = copy_case("stamp-failure-legacy", version="0.0.9", old=True)
with connect(bad) as db:
    db.execute("CREATE TRIGGER refuse_stamp BEFORE INSERT ON system_variables"
               " WHEN NEW.variable = 'application-version'"
               " BEGIN SELECT RAISE(ABORT, 'test-only migration version failure'); END")
verify_refusal(bad)
