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
The explicit Export dialog in `ExportManager` is checked too: selecting FDX
must produce FDX XML that freshly imports with the production information
settings. The default information setting hides scene numbers; direct format
checks separately enable number export.

`emphasis-review.fountain` combines emphasis and review notes in the same
paragraph, including multiple spans/notes and styled dialogue. It checks both
supported internal XML child orders and native annotation persistence. A second
process checks the saved UUIDs, complete serialized bytes, text, styles and review
coverage again. Contiguous formatting fragments count as one review range only
when all review metadata matches, including the complete comment list.
Full annotated Fountain export/reimport is checked without filtering reviews.
`review-offsets.fountain` additionally covers repeated literal escapes, multiple
notes/replies, styles spanning notes, boundaries before escapes/opening emphasis,
and UTF-16 positions across an emoji. All four original/fresh scripts cross a
separate-process native reopen. Exact exported syntax independently requires each
note or reply once, after closing emphasis and before a next opening/escape prefix.

`forced-action.fountain` and `forced-action-first.fountain` are authored in the
exporter's canonical spelling. They hold Action paragraphs that another Fountain
rule would claim without the forcing `!`: each reserved first character (`.`, `!`,
`@`, `>`, `=`, `~`, `#`), each scene-heading start, a parenthesized line after a
speech, `>centered<` text, a page-break line, bold and reviewed variants, an
uppercase line, and a first line of the file that begins with a title page key.
Ordinary Action lines beside them are controls that must stay unforced. A
model-authored script adds Action paragraphs with embedded line breaks, where each
physical line is forced on its own, and review notes that carry line breaks.
Expected types and text are written out in the tests; the exported bytes must equal
the fixtures apart from the scene number.

`shot.fdx` independently supplies Shot and Cast List paragraphs between a heading
and ordinary Action controls. Fountain exports both as forced Action (`!` on each
nonempty physical line), retaining their ordered text and emphasis. A model-authored
script additionally covers Unformatted Text, a reviewed/styled Unicode Shot,
multiline Shots with reserved starts, literal escapes and a Shot after Dialogue.
Direct export and the manager's explicit Export dialog must produce the pinned
Fountain bytes. Original native types and converted Action types, complete XML,
UUIDs, styles and review coverage cross save/close/reopen and a separate process.

`literal-syntax.fountain` is authored in the same canonical spelling. It holds
script text that Fountain would read as syntax unless it is escaped: doubled square
brackets, backslashes (at the start of a line, before emphasis and before a note),
a literal `/*` and `*/`, a slash directly before and at the end of emphasis, a
single bracket directly in front of a review note, and a comment holding brackets,
a backslash, a star, an underscore and a closing bracket at its end. Single
brackets, a slash between words and ordinary emphasis beside them are controls
that stay unescaped. A model-authored script holds the same kinds of text as typed
into the application, with an inline note, a comment with paired stars and a
paragraph ending in spaces of two styles; it goes out through the exporter and
through the manager's explicit Export dialog. Expected text is written out without
escapes in the tests, and Python reads the imported XML without the application.

`reserved-dialogue.fountain` pins the leading-backslash spelling for speech that
starts with a reserved marker or scene-heading prefix, including ellipses and
parenthetical-looking spoken text. Export escapes each physical speech line before
adding emphasis, so the marker stays literal and review offsets include the escape.
The imported fixture and an independently authored multiline model must preserve
Dialogue types, every character, Unicode styles and review coverage through direct
and explicit-dialog exports, fresh import and separate-process native save/reopen.
Hand-written sources need the same leading escape for these ambiguous starts.

`handwritten-markers.fountain` covers unescaped arithmetic stars, unpaired markers,
whitespace-padded markers, ordinary Windows paths and a final backslash. Valid emphasis
must touch its content at both ends, following [Fountain's whitespace examples](https://fountain.io/syntax/#section-emphasis).
The reader also retains the existing export spelling for an adjacent style change to
a whitespace-only tail. Review comments are literal text: stars and underscores inside
`[[...]]` never apply emphasis or pair with markers in the screenplay. Inline-note text
still supports emphasis. Explicit escapes of syntax and leading reserved-start dialogue
remain supported; a backslash before an ordinary path character remains literal.
The separate canonical export fixture pins every resulting escape and review anchor.
Raw/imported XML, types, per-character styles, comments/ranges, direct export, fresh import,
UUIDs and separate-process native save/reopen are checked.

In the screenplay editor, plain-text clipboard data now inserts literal text using the
current paragraph type and normal editor formatting. It does not interpret Fountain
headings, emphasis or notes. Clipboard data carrying `text/markdown` still uses Fountain
parsing, and the native screenplay MIME type retains priority. Editor checks cover exact
plain-text paste, multiple lines, selection replacement, one-step Undo/Redo, Markdown
formatting/reviews, native Save, explicit Export, fresh import and exact native XML in a
separate process. Copy checks precede inline notes because full-selection/filter-offset
clipboard export remains the separately queued issue 22.

`fountain-names.fountain` checks character/location import against the screenplay's
cleaned paragraph text. Dictionary extraction reads that same typed XML, including
emphasis removal, literal escapes, review-note removal and HTML decoding. Title pages,
forced Action and inline notes add no phantom names; duplicate cues add one character.
Character extensions and scene times/numbers use the existing name parsers. Each parse
starts with fresh state, so dictionary-only import, screenplay import and subsequent
sources can share an importer without inheriting syntax or emphasis from a prior source.
The fixture covers styled names, literal underscores/brackets/stars, Unicode, ampersands
and angle brackets, forced cues/headings and reviewed cues. Independent option flags,
raw XML, actual project lists, styles/reviews, export/fresh import and separate-process
native XML/name/document UUID reopening are checked. The editor's existing capitalization
of character cues is retained.

`inline-note-review.fountain` covers review comments inside `/* */` inline notes:
the original `private[[about it]]` case, separate ranges, adjacent replies,
emphasis, Unicode/UTF-16 offsets, escaped brackets/backslashes/markers, and
multiline notes/comments. Review ranges belong to the inline note's own text;
comments remain separate from that text and from surrounding Action reviews.
Raw XML, exact exported syntax, an independently authored native model, the
explicit Export dialog, review filtering and separate-process native save/reopen
are checked. Inline-note wrapper padding is normalized on export without changing
the native model. These notes remain excluded by the ordinary export defaults.
The editor check also saves/exports them and closes normally; clipboard coverage
for this fixture uses a reviewed Action selection before the notes. Full-selection
clipboard export after filtering inline notes is the separate open issue 22 in
`WORKLIST.md`, with its failing check retained behind
`STARC_FOUNTAIN_EXPORT_INLINE_CLIPBOARD=1`.

`./tools/check-fountain-export.sh` separately exercises the real application
coordinator: import each fixture into a chosen `.starc`, type in the real editor,
Save that native project, explicitly export Fountain and freshly import that
output in another application process. Full/partial clipboard exports, ordinary
close, exact per-character styles and review coverage remain covered. The
literal fixture also has a note-like word and a path typed before export.
Repeated explicit exports must be byte-idempotent. The separately selected
`STARC_FOUNTAIN_EXPORT_MODES=cold-undo` mode checks that Undo and Redo of the first
edit after import restore the exact stored XML; it was the reproduction of
[issue 15](archive/audit-2-2026-10-13.md#15-undo-of-the-first-edit-after-importing-styled-review-notes-aborts).
`./tools/check-first-edit-undo.sh` covers that edit before and after an initial Save
and after a fresh native reopening, with Save and fresh-process reopening of both the
undone and redone screenplays.

Imported review notes keep their stored form while their paragraphs are unchanged:
a note with only a background color and one range per note, as Fountain import writes
it, is not rewritten into the editor's form (an explicit contrasting text color and a
range per text format) just because the editor reads the paragraph again. A paragraph
the writer edits is stored in the editor's form, which is what a note created in the
application uses. Both forms show the same text, colors and comments threads.

Every story is a `.starc` project. Fountain, TXT, FDX, FDXT and all other source
formats are import/export inputs and outputs only. Direct opens ask for a new
`.starc` destination before editing; cancellation keeps the current story. Save
writes only the native project, never the source. Source files acquire no story
lock and are not retained as the live or remembered story path. Old list entries
start the same import when clicked; remembered source paths leave automatic
startup on the story list without opening a chooser.

`check-import-only-sources.sh` exercises the coordinator, exact source bytes and
lock state at open/edit/Save/close, chooser cancellation, legacy list/startup,
and native save/fresh-process reopen. Original BOM, CRLF, archive parts and FDX
metadata stay available separately; this policy does not promise lossless import
of every source feature. `check-explicit-export-failure.sh` checks the explicit
Export dialog and actual Fountain/FDX/DOCX/PDF exporters: read-only output,
rejected/short staged writes and failed publication leave existing output intact
and preserve the editor's unsaved text. It also checks failed clipboard export,
recovery and editor paste. The old source-save suites and APIs are retired.

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
  underscores, brackets, backslashes and slashes, ordinary and custom headings, `[[review notes]]`, `/*inline notes*/` and `^`
  dual dialogue. Mixed-case cues/transitions are not case-preservation evidence:
  the screenplay template/editor can uppercase them; the fixtures use uppercase
  cues/transitions. The forced scene heading is independent of heading heuristics.
- An Action line is written with `!` whenever this application's importer would
  otherwise read it as another paragraph, drop its first characters or skip it: a
  reserved first character, a scene-heading start, text wrapped in parentheses, a
  title page key followed by a colon, or an uppercase line after an empty line.
  The decision is made for each physical line from its own text, so a
  parenthesized or key-like Action gains `!` even in a place where the importer
  would have read it as Action. A forced Action whose text needs no protection is
  still written without `!`. The rules follow this importer (case-sensitive heading
  starts, eight title keys), not every third-party Fountain reader. Other paragraph
  types have no forcing syntax; see the open items in `WORKLIST.md` (detail in `archive/audit-2-2026-10-13.md`).
- Fountain has no Shot, Cast List (Scene Characters), or Unformatted Text element.
  These screenplay paragraphs export as forced Action on each nonempty physical
  line, so fresh imports retain their words, order, supported emphasis and review
  notes while changing their type to Action. Export uses the selected template's
  case rules (normally uppercase for Shot and Cast List) and leaves the native
  screenplay's types and serialized content intact. FDX imports retain Shot and
  Cast List types in the native project. The other default-branch types are
  undefined, shadow headings, page/table separators and paragraph types belonging
  to other writing formats; this change does not export those as screenplay text.
  Act/Sequence footers retain their existing role as structural closing markers.
- A backslash makes the character after it text, in script text, review notes and
  inline notes, at the start of a line as well. The exporter writes one before
  every backslash, `*` and `_` of the text; before a square bracket that has an
  equal bracket beside it (`[[` and `]]`, also when emphasis markup stands between
  the two, because the importer removes that first); before a single `[` directly
  in front of a review note; and before a `/` directly in front of emphasis markup
  that begins with `*`, opening or closing, because `/*` opens an inline note. A
  comment is escaped the same way inside its note, and a `]` at its end as well.
  Single brackets and a slash anywhere else are written as they are. The importer's
  emphasis search skips an escaped character together with its backslash, so
  emphasis directly after an escaped backslash or slash is still found. These rules
  follow this application's importer. Fountain itself describes the backslash for
  emphasis characters only, so another program may show the other backslashes.
  A source that holds an unescaped literal `[[text]]` or backslash, written by hand
  or by a version before this rule, is still read as a note or an escape.
- Emphasis that covers nothing but spaces cannot be written: the markers go around
  words. Trailing spaces with a style of their own come back as one styled space,
  and a styled space between two unstyled words comes back unstyled.
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
  The represented review spans start at the paragraph start or previous note's
  end, following the application's Fountain note convention. Arbitrary native
  highlight starts, colors, resolved status and author/date identities are not
  promised by these plain-text note delimiters. Adjacent notes are replies on the
  same range; distinct notes with intervening text retain distinct ranges.
  The same convention applies to reviews inside an inline note. Its `/* */`
  delimiters and leading padding do not count toward the review's UTF-16 offsets.
  A review comment containing `*/` cannot close its enclosing inline note.
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
