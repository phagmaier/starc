# Active worklist

This is the only mutable issue queue. Work the first unchecked item top to bottom.
One agent at a time, directly on `dev`. See `AGENTS.md` (issue work protocol)
and `docs/WORKFLOW.md` (full checklist) before starting.

Frozen history (do not edit, reference only):

- `docs/archive/audit-1-2026-10-12.md` — first audit, 8/8 fixed.
- `docs/archive/audit-2-2026-10-13.md` — second audit, issues 1–15 fixed, 16–21 open below.
- `docs/archive/TODO-2026-10-12.md` — retired fork TODO, all done.

Keep an independent copy of important scripts while P1 items remain open.

## Open issues

### 16. P1 — Fountain export leaves out Shot paragraphs

- [x] Fix and verify.

Resolution: Shot, Cast List and Unformatted Text now export each nonempty line as forced Action; native types/XML remain intact.
Files: `screenplay_fountain_exporter.cpp`, interchange fixture/tests, and `docs/INTERCHANGE.md` document the accepted type conversion.
Oracle: `./build/dev.sh doctor`, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-screenplay-interchange.sh`, `git diff --check` passed.
Evidence: `.build/evidence/shot-paragraph-fix-ctsnZR/`, passing `.build/evidence/screenplay-interchange-fh04Jp/`, retained failing `.build/evidence/screenplay-interchange-TbJ0js/`.
Limits: debug/offscreen model, explicit Export dialog, filesystem and fresh-process native reopen; physical desktop/GPU and release unexercised.

**Location:** `src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp`, `processBlock` (no `TextParagraphType::Shot` case, hits skip-by-default branch).

**Trigger:** Shot `CLOSE ON THE LETTER` between heading and Action exports to nothing; fresh import loses the paragraph.

**Direction:** Fountain has no Shot element. Keep the words (e.g. uppercase forced Action), record accepted type change in `docs/INTERCHANGE.md`. Check other paragraph types hitting the default branch.

**Oracle:** `./tools/check-screenplay-interchange.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#16-fountain-export-leaves-out-shot-paragraphs`

### 17. P1 — Fountain export/reimport turns reserved-start dialogue into headings/cues

- [x] Fix and verify.

Resolution: Dialogue now escapes ambiguous starts on each physical line before emphasis/review positions are finalized, preserving text, types and annotation offsets.
Files: `screenplay_fountain_exporter.cpp`, reserved-dialogue fixture, interchange/editor-export checks and `docs/INTERCHANGE.md`.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-screenplay-interchange.sh`, `./tools/check-fountain-export.sh`, Bash/ShellCheck, Python syntax, changed-exporter formatting and `git diff --check` passed.
Evidence: `.build/evidence/reserved-dialogue-fix/`, passing `screenplay-interchange-MDdpwI/` and `fountain-export-96StIf/`; baseline `screenplay-interchange-LNSq5Z/` and oracle-development `screenplay-interchange-K6n8pq/` retained under `.build/evidence/`.
Limits: Linux debug/offscreen, real editor/clipboard/ordinary close, explicit Export and separate-process native reopen; physical desktop/GPU, release and third-party Fountain clients unexercised. Hand-written ambiguous dialogue needs a leading backslash.

**Locations:** `src/corelib/business_layer/import/abstract_fountain_importer.cpp` (`blockType`, scene-heading test before dialogue rule); `src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp` (Dialogue written unchanged).

**Trigger:** Speech `#1 is the best.`, `@home, he says.`, `...and then nothing.`, `INT. is short for interior.` comes back as sequence heading / cue / scene headings with lost characters. Hand-written sources import the same way. `!` forcing covers Action only.

**Direction:** Export the leading-backslash escape the importer already honours (`\#`, `\@`, `\.`, `\INT.` etc. import as dialogue today). Check type, full text, no new headings via export/fresh-import and source Save/reopen.

**Oracle:** `./tools/check-screenplay-interchange.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#17-fountain-exportreimport-turns-dialogue-with-a-reserved-line-start-into-headings-and-cues`

### 18. P2 — Review comment on inline note comes back as note text

- [x] Fix and verify.

Resolution: Reviews inside inline notes now retain separate comments/replies and ranges in the note's own text; export normalizes wrapper padding without changing native content.
Files: `abstract_fountain_importer.cpp`, `screenplay_fountain_exporter.cpp`, inline-note fixture, interchange/editor-export checks and `docs/INTERCHANGE.md`.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-screenplay-interchange.sh`, `./tools/check-fountain-export.sh`, focused first-edit Undo/Redo, syntax/format checks and `git diff --check` passed.
Evidence: `.build/evidence/inline-note-review-fix/`, passing `screenplay-interchange-bjQWAh/`, `fountain-export-YN0aa6/`, `inline-note-review-undo-KEHby0/`; baseline `screenplay-interchange-R6FZ0G/`, padding `screenplay-interchange-bi34Lv/` and clipboard failures retained under `.build/evidence/`.
Limits: Linux debug/offscreen, editor Save/explicit Export/ordinary close and fresh-process native reopen; physical desktop/GPU, release and third-party clients unexercised. Separate full-selection clipboard defect queued as issue 22.

**Locations:** `src/corelib/business_layer/export/screenplay/screenplay_fountain_exporter.cpp` (`processBlock` inserts review before InlineNote `/* */` wrap); `src/corelib/business_layer/import/abstract_fountain_importer.cpp` (`processBlock` `[` case opens no note inside inline note).

**Trigger:** Inline note `private` + comment `about it` writes `/* private[[about it]] */`, reimports as note text with no comment.

**Direction:** Either read notes inside inline notes (range in note's own text) or don't write them there and record loss in `docs/INTERCHANGE.md`. Check comment, range, note text via export/fresh-import and source Save/reopen.

**Oracle:** `./tools/check-screenplay-interchange.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#18-a-review-comment-on-an-inline-note-comes-back-as-part-of-the-notes-text`

### 19. P1 — Fountain import removes asterisk/underscore text

- [x] Fix and verify.

Resolution: Fountain preserves unpaired/whitespace-padded markers and ordinary path backslashes; reviews stay literal, and plain-text paste uses normal editor insertion while native/Markdown MIME retain priority.
Files: `abstract_fountain_importer.cpp`, `abstract_markdown_importer.cpp`, `screenplay_text_edit.cpp`, handwritten-marker fixtures/checks and `docs/INTERCHANGE.md`.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-screenplay-interchange.sh`, `./tools/check-fountain-export.sh`, focused paste/native reopen, Bash/ShellCheck/Python syntax, changed-code formatting and `git diff --check` passed.
Evidence: `.build/evidence/handwritten-marker-fix/`, passing `screenplay-interchange-HZkRUa/` and `fountain-export-BmBYr6/`; baseline `screenplay-interchange-7BK3tu/`, compatibility `screenplay-interchange-KwiUAr/` and known issue-22 clipboard failure `fountain-export-tz2DgN/` retained under `.build/evidence/`.
Limits: Linux debug/offscreen, clipboard/one-step Undo/Redo, Save/explicit Export/ordinary close and separate-process exact native XML/UUID reopen; physical desktop/GPU, release and third-party clients unexercised. Issue 22 remains separate.

**Locations:** `src/corelib/business_layer/import/abstract_fountain_importer.cpp` (`processBlock` `*` case); `src/corelib/business_layer/import/abstract_markdown_importer.cpp` (`collectSelectionTypes`); `src/core/management_layer/plugins/screenplay_text/text/screenplay_text_edit.cpp` (`insertFromMimeData` paste path).

**Trigger:** Hand-written `It costs 2 * 4.`, `5 * 3 = 15`, `[[see file_name ... 2 * 3 * 4]]` lose lone markers or mis-pair them as emphasis (also `C:\temp` → `C:temp`). App-typed text unaffected (exporter escapes); paste goes through same reader.

**Direction:** Keep unpaired markers as text; decide if emphasis applies inside review notes; decide if plain-text paste should use Fountain reader. Preserve issue-12 escape round-trips. Compare full text + styles on hand-written sources.

**Oracle:** `./tools/check-screenplay-interchange.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#19-fountain-import-removes-asterisks-and-underscores-that-are-text`

### 20. P2 — Imported character/location names keep Fountain markup

- [x] Fix and verify.

Resolution: Character/location names now come from the screenplay's typed, cleaned XML, with matching HTML decoding/name parsers and fresh syntax/emphasis state for each import call.
Files: `abstract_fountain_importer.{cpp,h}`, `abstract_markdown_importer.{cpp,h}`, Fountain names fixture/interchange checks and `docs/INTERCHANGE.md`.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-screenplay-interchange.sh`, `./tools/check-screenplay-scope.sh`, independent XML/name checks, Python syntax, changed-code formatting and `git diff --check` passed.
Evidence: `.build/evidence/fountain-names-fix/`, passing `screenplay-interchange-Y1BuGH/` and `screenplay-scope-V2t3R2/`; original failure `screenplay-interchange-m4HP0h/` and model-oracle capitalization correction `screenplay-interchange-CegVNB/` retained under `.build/evidence/`.
Limits: Linux debug/offscreen, actual project lists, option flags, repeated parses, Save/export/fresh import and separate-process exact XML/name/document UUID reopen; physical desktop/GPU, release and third-party readers unexercised.

**Location:** `src/corelib/business_layer/import/abstract_fountain_importer.cpp`, `importDocuments` (reads lines without script-import emphasis/escape/note handling).

**Trigger:** `**MARA**`, `JO\_ANN`, `INT. LAB \[\[A\]\] - DAY` give correct script text but list names `**MARA**`, `JO\_ANN`, `LAB \[\[A\]\]`.

**Direction:** Take names from same cleaned text as script paragraphs. Compare names vs cues/headings incl. review-note cue.

**Oracle:** `./tools/check-screenplay-interchange.sh`, `./tools/check-screenplay-scope.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#20-imported-character-and-location-names-keep-fountain-markup`

### 21. P1 — New story first reopen rewrites "folder" as "sequence"

- [x] Fix and verify.

Resolution: Creation stamps the app version; unversioned modern schemas skip old content migrations. Legacy renames change only XML element names, with content/schema/version rollback on failure.
Files: `database.cpp`, database migration harness/oracle, `check-first-edit-undo.sh`, `DEVELOPING.md` and `docs/CODEBASE.md`.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, `./tools/check-database-migrations.sh` (13 cases), `./tools/check-first-edit-undo.sh` (24 cases + focused literal repeats), `./tools/check-transaction-recovery.sh`, syntax/format checks and `git diff --check` passed.
Evidence: `.build/evidence/database-version-fix-1qCUrj/` indexes passing migration/editor/transaction evidence and retained baseline failures.
Limits: Linux debug/offscreen, injected editor Save/Undo/Redo/ordinary close and separate-process SQLite reopen; synthetic legacy schemas. Physical desktop/GPU, release, other OS and power-loss behavior unexercised.

**Locations:** `src/corelib/data_layer/database.cpp` (`open`/`checkState`/`createTables` record no `application-version` for new files; `updateDatabase` treats missing as 0.0.1 and runs all migrations; `updateDatabaseTo_0_1_3` `.replace("folder","sequence")` over whole screenplay XML).

**Trigger:** New story with `The folder is C:\temp.`, Save, close, reopen → `The sequence is C:\temp.` plus `application-updated-to-version-0.8.4` row. Rewrites text, `folders`, comment text, no history entry.

**Direction:** Record app version at creation; distinguish unversioned-new vs genuinely-old before migrating; element renames must not touch text. Check exact XML across first + second reopen.

**Repro:** `STARC_FIRST_EDIT_UNDO_FIXTURES=literal-syntax STARC_FIRST_EDIT_UNDO_CASES=saved-redone ./tools/check-first-edit-undo.sh`

**Details:** `docs/archive/audit-2-2026-10-13.md#21-a-new-storys-first-reopen-rewrites-folder-in-script-text-as-sequence`

### 22. P1 — Copy All with inline notes empties the Fountain clipboard output

- [x] Confirm and fix.

Resolution: Clipboard selections track source UTF-16 endpoints through detached-document filtering, preserving included text/reviews even when notes before or within the selection are excluded.
Files: `abstract_exporter.{cpp,h}`, `abstract_markdown_exporter.cpp`, Fountain editor test/check and interchange/development docs.
Oracle: doctor, `make -C .build/debug qmake_all`, `JOBS=4 ./build/dev.sh build debug`, Fountain export/interchange suites, focused first-edit Undo/Redo, Bash/ShellCheck/Python syntax, changed-code formatting and `git diff --check` passed.
Evidence: `.build/evidence/clipboard-selection-fix-ps1o6uft/manifest.json` indexes passing runs and retained baseline `fountain-export-yerC2h/` (clipboard `0a`); default editor checks include 22 forward/reverse selection cases.
Limits: Linux debug/offscreen, real editor Copy/Save/Export/ordinary close and separate-process exact native XML/UUID reopen; physical desktop/GPU, release, other OS and third-party clients unexercised.

**Trigger:** Import `tests/fixtures/interchange/inline-note-review.fountain` into a disposable native story, then select all and copy; clipboard defaults exclude inline notes.
**Observed:** `text/markdown` is exactly one LF byte (`0a`), losing all included script text/reviews; explicit Export retains them. Evidence: `.build/evidence/fountain-export-tzFE28/inline-note-review/{save.log,clipboard.fountain,export.fountain}`.
**Location:** Shared `AbstractExporter::prepareDocument` and `AbstractMarkdownExporter::exportTo`; the original path filtered notes before applying unadjusted selection offsets (`QTextCursor::setPosition: Position '130' out of range`).
**Oracle:** `./tools/check-fountain-export.sh` includes Copy All and partial/notes-only selections in both directions by default, with exact source/model/annotation checks and native reopen.

## Untriaged (new defects found during fixes)

Copy this template, keep it short. Promote the audit link once confirmed.

```markdown
### N. P1/P2 — one-line symptom

- [ ] Confirm and fix.

**Trigger:** file/steps, disposable profile only.
**Observed:** what changed/lost, exact bytes if data safety.
**Location:** file:line if known, else unknown.
**Oracle:** check script + cases added.
```
