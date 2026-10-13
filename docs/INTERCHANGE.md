# Screenplay interchange checks

Run `./tools/check-screenplay-interchange.sh` after the debug build. This focused
check complements the export smoke checks in `check-screenplay-scope.sh`; it does
not replace them. No account, remote repository or desktop keyring is needed.
The runner compiles against the actual debug libraries/plugins and uses a new
evidence directory, disposable profile/project and absent private session bus.

`tests/fixtures/interchange/` contains independently authored UTF-8 Fountain and
FDX inputs. The C++ test contains explicit expected paragraph types, ordered
text, per-character bold/italic/underline, scene numbers, note text and dialogue
column ownership. Each fixture is imported through `ImportManager`, stored by
`ProjectManager`, selected by document UUID, compared with those expectations,
serialized, saved, closed and reopened from SQLite, exported to a separate path,
and imported into a fresh model. Both the original and fresh imports cross the
save/reopen boundary. An independently constructed application model exercises
both exporters without an imported seed, including a multiline dialogue item.
The ordinary shadow-project save route (`ExportManager::exportDocument` with a
destination path) is checked too: an FDX destination must contain FDX XML and
freshly import with the production information settings, rather than being
replaced with Fountain text. The default information setting hides scene numbers;
the direct format checks separately enable number export.

`emphasis-review.fountain` combines emphasis and review notes in the same
paragraph, including multiple spans/notes and styled dialogue. It checks both
supported internal XML child orders and native annotation persistence. A second
process checks the saved UUIDs, complete serialized bytes, text, styles and review
coverage again. Contiguous formatting fragments count as one review range only
when all review metadata matches, including the complete comment list.
Full annotated Fountain export/reimport is still affected by
[issue 13](../ISSUES2.md#13-fountain-escaping-shifts-a-review-note-onto-the-wrong-text),
including duplicate comments across emphasis fragments. This fixture's export
explicitly disables reviews and verifies that omission; the original annotated
script crosses native save/reopen without filtering.

Ordinary source saves accept only `.fountain`, `.txt`, `.fdx` and `.fdxt` (case
insensitive). Other destinations are refused before opening a file for writing.
Directly opened import-only formats, including DOCX, PDF and Markdown, ask for a
new `.starc` story destination before editing. Their source files are never saved
back through the Fountain fallback. `check-import-only-sources.sh` exercises the
coordinator, complete source-byte preservation and native copy save/reopen. This
policy does not claim lossless import of every source feature; those original
bytes stay available separately. Explicit DOCX/PDF exports remain supported.

The serialization boundary is explicit: `AbstractModel::saveChanges()` updates
the `DocumentObject` before `ProjectManager::saveChanges()` writes it through
`DocumentStorage`. The test clears the storage cache and closes the database
before reopening. It checks the complete serialized screenplay bytes and the
independent semantic expectations again. Live-model equality alone is insufficient.
The host outlives managers, all required icon/text fonts are loaded, and TaskBar
is registered as in the working application fixtures.

`tests/check_interchange_exports.py` independently checks exact canonical
Fountain syntax/blank lines/escaping, FDX root and ordered typed paragraphs,
decoded XML text, emphasis runs and number attributes. It also compares fixture
SHA-256 hashes from before and after the run. Model snapshots, serialized XML,
the SQLite project, exports and complete compile/test logs remain in the unique
evidence directory, including failed runs.

## Representable semantics and normalization

- Both formats cover scene headings, action, character cues, dialogue,
  parentheticals, transitions, ordering, multiline text, Unicode, and
  bold/italic/underline. FDX additionally pins a whitespace-only two-space Text
  run and XML entities for ampersands, angle brackets and quotes. Formatting
  offsets exclude the parenthetical delimiters in the internal model.
- Fountain covers forced `.`, `!`, `@` and `>` syntax, escaped literal asterisks,
  ordinary and custom headings, `[[review notes]]`, `/*inline notes*/` and `^`
  dual dialogue. Mixed-case cues/transitions are not case-preservation evidence:
  the screenplay template/editor can uppercase them; the fixtures use uppercase
  cues/transitions. The forced scene heading is independent of heading heuristics.
- Interchange output is not byte-identical source preservation. Fountain removes
  CR, simplifies ordinary line whitespace, normalizes blank-line separators and
  inserts/removes forced syntax and emphasis delimiters. Each physical input line
  becomes a model text item. An embedded model line separator exports as LF and
  reimports as separate same-kind items. Assertions compare those ordered physical
  lines; FDX XML assertions additionally require embedded lines to remain in the
  original paragraph. No other text or paragraph-type differences are ignored.
- FDX concatenates Text runs, decodes XML entities and normalizes LF to the model's
  U+2028 line separator. Export emits U+2028 inside Text. It preserves spacing-only
  runs using Qt 6.5+ [`PreserveSpacingOnlyNodes`](https://doc.qt.io/qt-6/qdomdocument.html#ParseOption-enum);
  the older Qt fallback remains but
  is not covered on this host. Attribute order, element whitespace, run splitting,
  generated IDs and application template/page settings need not match the source.
- Scene-number retention follows `ImportOptions::keepSceneNumbers` (default
  false; `ImportManager::importScreenplay` sets true). Both choices are checked.
  Export follows `showScenesNumbers`, including disabled-number controls. Number
  attributes/delimiters use the underlying number, excluding display punctuation.
  Fountain's existing importer removes a terminal period from a supplied number;
  literal period-ended custom labels are outside this check's preservation claim.
- Body checks disable title-page inclusion and use the production `world_cp`
  template. Inline notes are omitted by default; review marks are enabled by
  default. Explicit include/exclude controls are checked. Whitespace immediately
  surrounding `/* */` content is normalized only for the note comparison. Review
  text and its exported anchor are checked; generated review author/date metadata
  is not an interchange identity guarantee.
- Fountain preserves the tested dual-dialogue left/right ownership. FDX exports
  the same speech paragraphs sequentially, preserving every speech and its order
  while losing simultaneity/column ownership; this loss is checked explicitly
  through save/reopen. Table boundary blocks are not script paragraphs. FDX does
  not export review comments, even with the ordinary enabled review-mark option;
  that loss is checked. Inline-note inclusion in FDX is outside the supported
  typed-body claim (its General paragraphs reimport as Action); the FDX dual control
  deliberately uses the ordinary setting that excludes inline notes.

## Input contract and limits

Unreadable/missing inputs produce no screenplay. The check denies permissions
on disposable files to exercise a real open failure. `importText=false` produces
no screenplay; disabled character/location import produces no dictionary documents.
Malformed/truncated FDX, a wrong root and missing Content produce no partial
screenplay or dictionaries. A valid empty FDX Content produces an empty internal
document; empty/whitespace-only Fountain produces one named screenplay with empty
text. Fountain is permissive plain text and has no general parse-error channel;
these checks are not a syntax linter or arbitrary damaged-markup recovery suite.

The fixture corpus is bounded. Native FDX `DualDialogue` containers are not
supported by the current importer (only direct Content/Paragraph Text runs are
read), and are not credited by the sequential-export control. FDX ScriptNotes,
scene-properties metadata, title-page/synopsis/treatment round trips, strikeout,
font identities, revision colors, attachments, deeply nested Fountain sections,
every escape combination and pathological/large files are not covered. These
checks do not add those format capabilities or promise lossless interchange of
all project data. Retired document byte preservation remains checked separately
by `check-screenplay-scope.sh`.

Linux debug/offscreen checks establish model and filesystem behavior. Application
window checks on private Xvfb are separate evidence; neither proves the owner's
desktop, another platform, release packaging or compatibility with a running
third-party Fountain/Final Draft application. See [PROJECT.md](PROJECT.md#fountain-and-fdx-round-trips)
for the commands, outcomes and retained evidence of this change.
