# Personal screenplay fork

The user values Story Architect's existing screenwriting experience and wants
focused adjustments for their own workflow. Preserve the application as a
working base; choose features from user requests, rather than inventing a
replacement product or a roadmap for other artistic mediums.

`dev` is the working version. `master` is the frozen reference at
`4b9677dbfb5c1faaa91c55c415fdf12715f8521d`, the initial
common ancestor of both branches. Nothing in this setup requires updating
`master`. Feature branches may be based on `dev`.

The public checkout includes the core screenplay editor, public import/export,
project management, and shared widgets. Several upstream commercial/cloud
features are separate private submodules. See [CODEBASE.md](CODEBASE.md) for
routes and [../DEVELOPING.md](../DEVELOPING.md) for the local workflow.

## Setup evidence

Setup was performed on Arch Linux on 2026-10-11 with Qt 6.12.0, GCC 16.2.1,
and Clang/clangd 23.1.1. The compiler, required Qt modules and matching private
headers, OpenSSL, zlib, debugger, and formatting tools were already installed.
Both public PDF submodules were initialized at the recorded repository commits.
Crashpad was downloaded and built in actual GN debug mode (`is_debug=true`).

Completed checks:

- `./build/dev.sh doctor`: passed for the debug development prerequisites.
- `./build/dev.sh crashpad debug`: passed, including a subsequent run with the
  tracked compatibility patch already applied.
- `./build/dev.sh compdb debug`: full public application build passed and
  generated 1,502 compiler commands for clangd.
- `clangd --check="$PWD/src/app/main.cpp"`: loaded the configured database and
  completed with zero errors.
- Shared libraries for the executable and all 39 application/document plugins
  resolved; the Qt XCB and Wayland platform plugins had no missing dependencies.
- Bash syntax, ShellCheck, documentation file links, and `git diff --check`
  passed. Git guard checks in a disposable repository allowed `dev` commits and
  rejected `master` commits, local changes/deletion, and push inputs.
- Offscreen startup outside the sandbox with `.build/profile-smoke-native`
  initialized logging and Crashpad successfully and reached the application
  window and onboarding screen. A 15-second timeout terminated the process
  (exit 124). The earlier sandbox run's blocked socket/log warnings are retained.

The setup fixes are confined to development tooling: explicit Crashpad mode and
job control, a tracked missing-header patch, and a Linux TIFF header definition
with configuration refresh. The tracked application/editor source is unchanged.

Local logs, a toolchain/dependency manifest, and retained failed attempts are in
`.build/evidence/setup-2026-10-11/`. These artifacts are intentionally ignored.

Remaining boundaries:

- Visible GUI behavior and ordinary close were not verified in this shell,
  which inherits neither `DISPLAY` nor `WAYLAND_DISPLAY`. Run
  `./build/dev.sh run debug` in a desktop terminal; it should show the normal
  onboarding or main window.
- Release compilation and private modules were not part of this setup.
- The optional Qt translation utilities (`qt6-tools`) need the user's sudo
  password to install. Existing translations and normal builds work without
  them. Install with `sudo pacman -S --needed qt6-tools` when needed.

## Fork customization: telemetry removal

Startup telemetry and BugSplat crash reporting were removed on 2026-10-11.
The fork no longer sends the startup device/app/OS report, collects Crashpad
reports, attaches logs to crash uploads, or prompts to send reports upstream.
Crashpad is no longer a build or packaging dependency, and CI no longer builds
it or uploads symbols to BugSplat. The setup evidence above describes the
original baseline, before this customization.

Local diagnostic logs and writing-session statistics remain available. Device
identity is created when saving a writing session, preserving existing IDs and
statistics. Existing account/cloud features, update checks, and asset downloads
retain their behavior; this change does not make the application offline.

Validation for this customization:

- `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`: passed
  without a Crashpad prerequisite or linkage.
- Bash/sh syntax checks, ShellCheck for the local development/setup helpers,
  libyaml parsing of the three modified CI workflows, and `git diff --check`:
  passed. Packaging and hosted workflows were not executed.
- An isolated offscreen harness loaded the actual application plugin, reached
  window creation, and exited normally (exit 0). It verified no startup device
  ID, Crashpad database, or crash-upload dialog; a seeded legacy report remained.
  This does not prove visible desktop behavior or interactive window closing.
- Writing-session checks with a stored account label passed for a new device ID,
  reopening with the same ID and retained rows, and an existing device ID. Typed
  character counts and both session rows were preserved in each run.
- A separate unset-account session check failed to persist rows. The same check
  using the pre-change writing-session implementation also failed. This was
  subsequently fixed by the anonymous-session storage change below.
- `nm -C` and `strings` on the rebuilt core plugin found no Crashpad/BugSplat
  symbols, crash-upload dialog, or removed telemetry endpoints.

Harness sources, logs, and failed controls are retained locally in
`.build/evidence/telemetry-removal-2026-10-11/` (ignored by Git).

## Anonymous writing-session statistics

An unset account email produced a null QString, which SQLite rejected with
`NOT NULL constraint failed: sessions.account_email`. The save path now stores
an empty string for anonymous sessions, matching the existing schema and read
filter. This requires no database migration and preserves account filtering.

The focused regression is `./tools/check-writing-session-storage.sh`. It checks
unset and explicitly empty email, exact session fields, account filtering,
retention of existing rows, and reopening. The original failing controls remain
in `.build/evidence/session-statistics-diagnosis-2026-10-11/`.

`JOBS=4 ./build/dev.sh build debug` and the regression check passed. The same
regression failed on the pre-fix storage implementation at the anonymous-save
assertion. Bash syntax, ShellCheck, changed-code formatting, and
`git diff --check` passed. Build/control logs are retained in
`.build/evidence/session-statistics-fix-2026-10-11/`; each regression run prints
its separate retained evidence directory.

## AI removal

AI assistant panels, toolbar controls, preferences, onboarding, credit purchases,
and text/image generation and translation request/result connections have been
removed. Ordinary application translations, spelling, comments, bookmarks,
manual images, and the non-AI avatar, cover, and logline tools remain. Existing
sidebar preferences are mapped around the removed AI tab. The optional cloud
fragment is absent; its AI connections were removed from the public manager.
GitHub sync and unrelated networking remain.

`JOBS=4 ./build/dev.sh build debug` passed. An isolated offscreen construction
check exercised all five retained text editors plus settings, onboarding, and
account widgets and found no AI panel or toolbar action. The writing-session
storage and local mock GitHub sync regressions passed. Generated dependencies
required `make qmake_all` after deleting the AI headers. Failed build and harness
attempts remain under `.build/evidence/feature-pruning/`; the construction check
is not visible GUI or document round-trip evidence.

## Audioplay, stageplay, and comic book removal

These three formats can no longer be created, imported, exported, or configured.
Their 13 public document plugins, corelib models, documents, templates,
importers, exporters, reports, export dialogs, and bundled template resources
were deleted (325 tracked paths), with the three private statistics submodule
links. Novel and simple text support is unchanged (novel was removed later, see
"Novel and series removal").

Existing project data is kept. The persisted type identifiers, MIME mappings, and
upstream database upgrade steps remain. A document of a retired format is shown
in the project tree, is never loaded into a model or written back, and shows a
notice that its content is preserved when selected. To edit it, open a copy of
the project in an application that supports the format.

Validation on 2026-10-11, all with the debug build:

- `JOBS=4 ./build/dev.sh build debug` passed after `make qmake_all` in
  `.build/debug`. The rebuilt core library and core plugin export no audioplay,
  comic book, or stageplay symbols, and 26 plugins are built (the core plugin and
  25 document plugins).
- `./tools/check-screenplay-scope.sh` passed. It found two defects that were
  fixed: adding a retired document to the structure dereferenced a missing
  model, and several view paths showed an endless loading page.
- `./tools/check-writing-session-storage.sh` and `./tools/check-project-sync.sh`
  (local stand-in server only) passed.
- The application started offscreen with an isolated profile and logged no
  warnings or errors before a 20 second timeout ended it.

Limits:

- Every check ran offscreen and in process. Visible desktop behavior, the context
  menu on a retired document, multi-selection, split view, and ordinary close
  were not exercised in a window.
- The scope check saves the model explicitly to define an Undo step and waits
  for the editor's idle timer before saving; the application does both on timers.
- The retired documents in the check are synthetic byte strings in a new
  project, not a project written by an upstream release.
- A project file is upgraded once on its second open. The retained upstream step
  rewrites comic book and screenplay text as text, which is lossless for the XML
  the application writes but not for arbitrary bytes.
- No performance measurements were made, and no release build was compiled.
- Translation catalogs still contain strings of the removed features.

Build logs, including the failed attempts, the earlier scripted-pruning scripts,
and the moved-aside plugin binaries of the retired formats are under
`.build/evidence/feature-pruning/`.

## Novel and series removal

The owner decided on 2026-10-11 that this fork is for screenplays only. Novels
and screenplay series can no longer be created, imported, exported, or
configured, and "Split to episodes" is gone from the add-document dialog. Their
9 public document plugins, corelib models, documents, template, importer,
exporters, reports, plot, the novel export dialog, and the bundled novel
templates were deleted (192 tracked paths), with five private submodule links
(novel cards, statistics and timeline; series plan and statistics).

Existing project data is kept the same way as for the other retired formats, now
32 type identifiers. A novel or a series is listed in the project tree, is never
loaded into a model or written back, and shows the preserved-content notice. The
episodes of a series are ordinary screenplays: they still open in the screenplay
editor and can be dragged out of the series. Markdown and plain text files are
offered as simple text (plain text also as a screenplay), no longer as a novel.
Novel settings already stored in a profile are left in the file and ignored.

Validation on 2026-10-11, all with the debug build:

- `JOBS=4 ./build/dev.sh build debug` passed after `make qmake_all` in
  `.build/debug`. The core library and core plugin export no novel or series
  symbols, and 17 plugins are built (the core plugin and 16 document plugins).
- `./tools/check-screenplay-scope.sh` passed with novel and series documents
  added to the retired ones (bytes kept across save and reopen, no model, no
  creation) and a screenplay nested in a retired series that still opens.
- `./tools/check-writing-session-storage.sh` and `./tools/check-project-sync.sh`
  (local stand-in server only) passed.
- In a window on a private Xvfb display, with a project the previous build had
  written a novel and an eight-episode series into: both are listed and show the
  notice, an episode opened in the screenplay editor and took typing, the
  add-document dialog offers Screenplay, Folder and Text, and the settings list
  two components. After an ordinary quit, 87 of the 88 stored documents had the
  same SHA-1 as before; the one that changed is the episode that was typed into.

Limits:

- Not exercised in a window: the create-story dialog, the import dialog,
  dragging an episode out of a series, and the context menu of a novel or series
  document. The first three are covered offscreen by the scope check.
- The module promotion widget and the subscription comparison still name series
  features; they belong to the account UI that was listed for removal in
  `archive/TODO-2026-10-12.md`.
- Translation catalogs still contain strings of the removed features, and no
  release build was compiled.
- The libraries of the removed plugins were deleted from
  `.build/debug/_build/plugins/`, not moved aside.

## Screenplay Statistics replacement

On 2026-10-11 the uninitialized private `screenplay_statistics` submodule link
and its `.gitmodules` entry were removed. Its directory now contains our own
`screenplaystatisticsplugin`, built by the existing conditional qmake entry.
It has no new dependencies and uses the public `ScreenplayStatisticsModel`,
reports, plots and bundled plotting widget. The view shows summary, scene,
location, cast, dialogue and gender reports as read-only tables, with expandable
scene/dialogue rows, plus story structure and character activity plots. The
existing navigator selects them through `IDocumentManager::bind`. Selecting
Statistics recomputes the reports; Refresh recomputes a view kept open beside
the script. This initial view has no report export or filter controls.

Availability still follows plugin libraries: the tree item and the screenplay
information page's Statistics checkbox return without a special-case override.
New screenplays receive the item. On local project load, screenplays with text
but no statistics child receive one, including nested screenplays. Existing
statistics identities, bytes and visibility choices are retained. Recycled
screenplays and upstream remote/team projects are not repaired. The repair
changes the structure document and creates the missing statistics document;
other document contents are preserved.

Validation with the debug build:

- `./build/dev.sh doctor`, `make qmake_all` in `.build/debug`, and
  `JOBS=4 ./build/dev.sh build debug`: passed.
- `./tools/check-screenplay-scope.sh`: passed, including the previously unused
  availability branch. The added checks type two scenes with ALICE and BOB in
  the real screenplay editor, then inspect the actual plugin's tables: two
  scenes, two characters, three dialogue lines, ALICE's five words/two dialogues/
  two speaking scenes and BOB's two words/one dialogue/one speaking scene.
  Navigator selection covers all six reports and both plots; returning after
  another scene is typed recomputes the report.
- The same check restores a missing item beneath a nested screenplay containing
  text, adds only one document, retains every other document's exact bytes and
  an existing hidden statistics item, and verifies save/reopen and idempotence.
  Viewing the reports also leaves stored documents unchanged.
- Four isolated slices (creation, restoration, tables, navigator binding) each
  fail at their expected assertion when only the statistics view library is
  omitted from the executable's plugin directory.
- `./tools/check-project-sync.sh` and
  `./tools/check-writing-session-storage.sh`: passed. The sync check needed
  sandbox escalation to bind its local mock server; it contacted no real account.
- Real application windows on private Xvfb `:102` at 1500x950, with isolated
  profiles, copied projects and networking disabled: Title page and Synopsis
  took typing and retained it after save/reopen; no repair was needed. All six
  reports, both plots, Refresh, navigator selection and the restored checkbox
  were exercised. Viewing/saving the reports left all 30 document contents
  unchanged. Opening a copy from the disabled-plugin restoration control added
  one Statistics document; the only existing document changed was the structure.
  Its preserved script produced a one-scene report. All app runs quit normally
  with Ctrl+Q (exit 0); no file dialogs were opened.
- `git diff --check` and changed-code formatting review: passed, preserving the
  repository's empty-brace style and the test file's existing formatting.

Limits: Statistics in a secondary panel or detached window was built but not
exercised. Plot panning/zooming and non-default character genders were not
tested; the displayed gender counts used unspecified genders. No report
PDF/XLSX export, performance measurement, release build or desktop-session test
was done. Xvfb is real-window evidence, not proof on the owner's desktop.

## Backup retention across writing sessions

Implemented on 2026-10-11 in `BackupBuilder`, keeping the existing configured
backup folder and recovery workflow: copy a `.starc` snapshot to a working file
and open it. The application's existing asynchronous save caller and its
log/task-bar error reporting remain in use. No dependencies were added.

Retention runs independently for each source path and backup-name family, only
after successful publication of a complete new snapshot:

- Keep the seven newest successful snapshots. An existing quantity preference
  greater than seven retains that many recent copies; lower values now mean
  seven. The settings label explains the minimum and the checkbox tooltip
  describes daily history. The shared builder also applies this minimum and
  daily history to the separate “before GitHub download” backup family.
- Also keep the newest snapshot from each of the last 30 **local calendar
  days**, including the day of the save (today through today minus 29 days).
  Recent and daily selections form one union, so a snapshot is never duplicated.
  Recent copies can survive beyond the daily horizon when a story is saved
  infrequently. Pruning happens on successful saves, not on a background timer.
- New names contain the readable story name, a SHA-256 digest of its cleaned
  absolute source path, a `yyyy_MM_dd_hh_mm_ss` timestamp and, when necessary,
  a numeric collision ordinal. Numeric ordering resolves same-second saves
  without replacing an existing copy. Different source folders with the same
  story filename have separate histories. Moving or renaming a story starts a
  new family and leaves its old history intact.
- Old basename-only names cannot identify which same-named source they belonged
  to, so they remain untouched, including iCloud placeholders and abandoned old
  temporary copies. Malformed timestamps/ordinals, unrelated files, directories
  and symlinks are also left alone. These unclassified files are outside the
  bounded policy; there is no automatic migration or cleanup of legacy history.
- A per-source lock prevents competing builder operations from pruning each
  other's copies. A unique temporary file in the backup directory is fully
  written and flushed before an atomic, non-overwriting rename. Failed reads,
  partial writes, flushes, lock acquisition or publication cannot prune existing
  recovery copies. Temporary files are cleaned on normal failure. A removal
  error reports both the new backup path and the old file it could not remove.
  The newly published snapshot is retained even if the clock moves backwards.

Validation:

- `./build/dev.sh doctor`: passed; optional translation utilities remain absent.
- `make qmake_all` in `.build/debug`, then
  `JOBS=4 ./build/dev.sh build debug`: passed.
- `./tools/check-backup-retention.sh`: passed. The linked debug-library check
  uses `saveAt` with explicit timestamps, without changing the machine clock.
  It covers twelve same-second saves (including ordinals 9–11), minimum seven
  and higher configured counts, twelve saves per day across 36 days, the
  inclusive 30-day boundary, recent/daily overlap, sparse old recent copies,
  same-named stories in different folders, malformed/legacy/unrelated names,
  directory and broken-symlink collisions, and missing-source, lock, invalid
  date, invalid name and folder-creation failures. A process-local file-size
  limit also forces a partial write and verifies every old copy's exact bytes
  and temporary-file cleanup. The test creates and types in a disposable
  screenplay, explicitly serializes its models before saving, compares the
  complete backup bytes, restores a copy and checks editor text and serialized
  document bytes through save/reopen.
- `./tools/check-screenplay-scope.sh`, `./tools/check-project-sync.sh` and
  `./tools/check-writing-session-storage.sh`: passed. The sync check needed
  escalation for its local mock server; no real account was contacted.
- A real application window on private Xvfb `:1`, launched with
  `STARC_PROFILE_DIR` and `unshare -Urn` (network disabled), opened a copy of the
  previous disposable mixed project. Typing `BACKUP UI SURVIVAL MARKER` in the
  screenplay and pressing Ctrl+S exercised the ordinary save and asynchronous
  backup path. The backup matched the saved file byte for byte and passed
  SQLite integrity checking. Only the screenplay content changed; the other
  37 document contents and identities stayed exact. A copied backup opened in
  a second application launch displayed the marker and retained all 38 saved
  document contents and identities. Both launches quit normally with Ctrl+Q,
  exit 0; the private X server was then stopped. No file dialogs, file managers
  or external links were opened, and no unrelated application was terminated.
- Bash syntax, ShellCheck and `git diff --check`: passed. Graph coverage was
  checked for the relied-on paths; Qt macro/parser gaps and unreliable caller
  edges were verified directly in source.

Evidence is retained under `.build/evidence/backup-retention/` and the unique
`backup-retention-*` check directories. It includes failed fixture runs and
backtraces (missing embedded icon font, then undelivered queued completion
callbacks), corrected by loading all required fonts and dispatching normal
per-keystroke events, with no sleeps or retries. The initial sandbox-blocked
Xvfb, ptrace and mock-server attempts are retained too. The UI driver,
screenshots and `ui/save-result.json` record the save, restoration and exits.

Limits: this is Linux debug and Xvfb evidence, not the owner's desktop or a
release build. The three-minute autosave timer was not waited out; deterministic
checks exercise the same backup builder it uses. Forced process/power loss,
concurrent modification of the source during copying, disk-full flush/rename
failures, and an actual pruning-permission failure were not injected. Legacy
copies and histories for moved/renamed source paths can accumulate by design.

Build, UI, disabled-plugin control and debugger logs, including the failed
fixture attempts, are retained under `.build/evidence/screenplay-statistics/`.
The fixture fixes give model serialization an explicit save boundary, keep
hosts alive for existing plugins that retain raw view pointers, and select
screenplay editors by document UUID after split-view checks.

## GitHub token storage

Completed 2026-10-11. GitHub story-sync credentials now pass through
`GitHubCredentialStore` in corelib. The small public [QtKeychain library](https://github.com/frankosterfeld/qtkeychain)
provides asynchronous native read/write/delete jobs; the adapter explicitly sets
`setInsecureFallback(false)`. qmake links the Qt 6 library, and the Arch setup
includes `qtkeychain-qt6`. Repository names and sync history remain ordinary
settings; `kGitHubSyncTokenKey` is reserved for migration and ordinary settings
setters refuse credential writes.

Credential policy:

- An existing keyring entry takes precedence over legacy settings, including an
  explicitly empty entry. Stale migration data never replaces that entry.
- When the entry is absent, the legacy token is stored and read back through the
  keyring before removal from settings. Removal uses an atomic settings writer
  and checks persistence before clearing the live cache. A fresh writer on each
  attempt permits retry after a settings access error; Qt's long-lived settings
  object retains its [first error status](https://doc.qt.io/qt-6/qsettings.html#status).
- Failed migration retains the legacy credential for authentication and reports
  the incomplete step. Failed replacement/deletion retains the previous confirmed
  credential and the unsaved edit. Clearing requires any legacy value to be
  durably retired first, so a subsequent startup cannot resurrect it.
- Jobs are serialized. Operation/revision guards reject stale completions and
  preserve newer drafts, including clearing during the first write or reverting
  to the previous token during replacement. Successful writes/deletes are read
  back before the application reports success.
- Programmatic field population blocks change signals. Typing stages only an
  in-memory draft; Save token, Clear token, Retry keyring, leaving settings, and
  normal close use the shared boundary. Onboarding finishes only after confirmed
  storage. Close/restart waits for jobs or reports unsaved changes, with retry,
  keep-open, and explicit close-without-saving choices after a failure.
- Startup/manual/per-story sync waits for readiness. Credential changes cancel
  requests using the previous token; a missing or unavailable credential does
  not prevent local editing. Backend diagnostics are mapped to safe text, and
  server-reflected bearer credentials are removed from error messages.
- Settings resets retain the keyring credential and any legacy value still
  awaiting migration. Clear the token explicitly in GitHub settings. Isolated
  development profiles use a path-derived service distinct from the ordinary
  `app.starc.screenplay.github` service; tests additionally isolate D-Bus/provider
  storage. No real keyring, GitHub account, or remote story repository was used.

Validation against the debug build:

- `./build/dev.sh doctor`, `make qmake_all` in `.build/debug`, then
  `JOBS=4 ./build/dev.sh build debug`: passed. On this host these commands used
  `QTKEYCHAIN_PREFIX="$PWD/.build/keyring-deps/usr"`: QtKeychain 0.17.0 and the
  native-test provider were unpacked from public Arch packages into an ignored
  development prefix because sudo installation requires a password. This is
  not a system package installation; the normal setup script installs the dependency.
  The helper also passed without an explicit prefix: when the system package is
  absent it selects this prepared development prefix and reports the path in doctor.
- `./tools/check-github-credential-storage.sh`: passed. Injected backend checks
  cover first storage/fresh-instance retrieval, migration success/failure,
  precedence conflicts, empty entries, failed write/delete/confirmation,
  ordering and destruction, field population, onboarding failure/retry, pending
  close and canceled-close ordering, diagnostic redaction, and sync waiting for
  readiness/authenticating with the expected fake token to the local mock server.
  Separate real filesystem checks denied atomic settings publication with a
  read-only disposable directory, retained the legacy bytes, restored permissions,
  and successfully retried. Persisted settings contain no token after success.
- `QT_QPA_PLATFORM=offscreen python3 tests/isolated_keyring.py <evidence>/native --
  <evidence>/credential-test --native app.starc.test.credentials`: passed against
  an actual disposable GNOME Secret Service on a private bus with desktop
  activation disabled. First storage, fresh-instance retrieval, replacement,
  deletion, native migration and persisted plaintext removal passed. Its separate
  `--unavailable` / `--native-unavailable` run passed with no provider and no
  plaintext fallback. The native files contain no fake credential plaintext.
- Real application on private Xvfb: onboarding stored a fake token and authenticated
  to `tests/github_mock_server.py` via `STARC_GITHUB_API_URL`. A second disposable
  profile migrated a seeded legacy token, populated settings without writes,
  replaced and cleared it, then settled a staged token on Ctrl+Q. Reopening
  authenticated with that saved token. Both settings-run Ctrl+Q exits were `0`;
  settings bytes after close/reopen contain neither the legacy key nor token.
  The whole application/server pair ran inside a private network namespace with
  only loopback enabled; no file dialogs, file managers or external links were used.
- `./tools/check-project-sync.sh`, `./tools/check-screenplay-scope.sh`,
  `./tools/check-writing-session-storage.sh`, `./tools/check-backup-retention.sh`:
  passed. Bash syntax, ShellCheck, Python syntax, linkage and `git diff --check`
  also passed.

Evidence is retained in `.build/evidence/github-credentials/` (doctor, qmake,
all build attempts and regression summaries), unique `github-credentials-*`
directories (injected/native logs and disposable storage), and
`github-credentials-ui-1` through `github-credentials-ui-4` (driver transcripts,
screenshots, profiles and actual exits). The successful focused/native run is
`github-credentials-DtmeIC`; successful settings/reopen UI evidence is in
`github-credentials-ui-4`. Existing backup/statistics evidence was not overwritten.

Retained failures include initial constructor build errors, a sandbox-blocked
mock-server socket, a private network namespace with loopback initially down,
a test-driver namespace-inspection permission error, and a regression-host
TaskBar assertion. The latter has its core/backtrace retained; the host now
registers TaskBar like the application. Early driver input before credential
readiness and title-bar/minimize coordinate captures are retained rather than
used as passing behavior evidence. The input method remains zero-delay xdotool
injection; corrections addressed readiness, loopback and event routing.

Limits: native locked-keyring prompts and native write/delete fault conditions
were not exercised (denial/failure paths use the injected backend). KWallet,
macOS/Windows, the ordinary desktop/profile/keyring, release packaging, update
installation, and interactive settings-reset/restart dialogs were not exercised.
The shared settlement boundary for restart/update exits is source-verified and
covered by the close callback tests. The native evidence proves the adapter on
the isolated GNOME provider; injected tests do not stand in for native evidence.

## Fountain and FDX round trips

Completed 2026-10-11. `tools/check-screenplay-interchange.sh` now checks
independently authored Fountain/FDX fixtures against explicit ordered screenplay
semantics, both import/export/fresh-import paths, an independently constructed
application model, and actual serialized project save/close/reopen. Python
checks canonical Fountain spelling and FDX XML independently of the importers,
including fixture hashes and SQLite integrity. [INTERCHANGE.md](INTERCHANGE.md)
records the exact normalization/loss policy, options, input contract and limits;
[CODEBASE.md](CODEBASE.md) records the production routes.

The checks exposed and fixed bounded interchange defects:

- Standard Fountain headings skipped number export; both exporters used display
  numbers with punctuation. Both now export the underlying scene-number value.
  Removing a Fountain number also removes its separator whitespace, avoiding
  accumulated trailing heading spaces.
- FDX ignored `keepSceneNumbers`, accepted parse/open failures as empty or
  partial scripts, synthesized whitespace/empty paragraphs, and left emphasis
  offsets pointing at removed parenthetical delimiters. Import validates XML/root/
  Content first, iterates paragraph elements, retains spacing-only Text runs on
  current Qt, normalizes multiline text, and corrects the offsets.
- FDX exported dual-dialogue table boundary blocks as blank General paragraphs.
  Those layout blocks are now skipped. Speech text/order survives; FDX still loses
  simultaneous column layout and review comments, explicitly checked and documented.
- Ordinary shadow-project saves always selected Fountain, including for an FDX
  destination. The convenience export route now selects FDX for FDX/FDXT paths
  (case insensitive), preserving the existing Fountain route for other text paths.
  The pre-fix manager-route control retained an FDX destination containing Fountain
  text; the fixed route passes XML, semantic and save/reopen checks.

Validation:

- `./build/dev.sh doctor`: passed with Qt 6.12.0 and the prepared development
  QtKeychain prefix; optional translation utilities remain absent.
- `JOBS=4 ./build/dev.sh build debug`: passed, including the final formatting
  build. No application source/header was added or removed, so no qmake list
  regeneration was required.
- `./tools/check-screenplay-interchange.sh`: passed. It covers headings, action,
  cues, dialogue, parentheticals, transitions, exact order/text, multiline lines,
  Unicode, bold/italic/underline, forced Fountain syntax, literal asterisks, XML
  entities, custom numbers/renumbering/hidden-number export, notes, dual dialogue,
  inclusion options, empty/invalid/unreadable input and actual permission failures.
  Both original and fresh imports retain their complete serialized screenplay
  bytes across cache clear/database close/reopen. FDX multiline paragraph shape
  and exported syntax/XML are checked independently of model normalization.
- `./tools/check-github-credential-storage.sh`, `./tools/check-project-sync.sh`,
  `./tools/check-screenplay-scope.sh`, `./tools/check-writing-session-storage.sh`
  and `./tools/check-backup-retention.sh`: passed after the routing change. Sync
  uses only the local mock server; credentials use the injected backend and an
  absent private bus. Remaining offscreen runs also use disposable profiles and
  an absent bus. Retired documents remain byte exact in the scope regression.
- Real application on private Xvfb with an isolated native Secret Service and
  `unshare -Urn`: disposable Fountain and FDX working copies opened in the actual
  editor. Ordinary Ctrl+S saved them in the appropriate format; reopening retained
  the body text and emphasis. An appended `FDX UI SAVE MARKER` survived FDX XML
  save and fresh application reopen. The independent XML comparison retained all
  eleven FDX body paragraphs and their text/order, including two spaces, embedded
  lines and escaped text; only the intended marker was added. Default information
  settings hide scene numbers, as separately checked in the manager regression.
  All four successful launches quit normally with Ctrl+Q, actual exit `0`.
  The driver was copied/adapted from the retained backup driver into a new directory;
  only disposable geometry/splitter settings were copied. No file dialogs, file
  managers or external links were opened, and no unrelated application was stopped.
- Bash syntax, ShellCheck, Python syntax, changed-code formatting and
  `git diff --check`: passed. Graph coverage was checked for relied-on paths;
  Qt/parser gaps and inaccurate caller edges were verified in source.

Evidence: `.build/evidence/interchange-work-skCl1n/` contains doctor, all build
attempts, the manager-route failing control and regression summaries. Unique
`screenplay-interchange-*` directories contain complete inputs/outputs, models,
serialized XML and projects, including failed assertions and compile attempts.
Successful GUI evidence is in `.build/evidence/interchange-ui-vb9tocvz/`, with
the adapted driver, isolated provider logs, screenshots, command transcript and
`ui-result.json`. Earlier UI setup-only/absent-provider attempts remain separate;
the absent-provider launch displayed a credential-readiness warning and is not
credited as passing import/save evidence. That launch also quit normally, exit `0`.
Original repository fixtures and all earlier backup/credential/statistics evidence
remain untouched.

Limits: Linux debug/offscreen and private Xvfb only; no owner desktop, other
platform, release execution, third-party application compatibility, performance
or broad format-conformance run. Native FDX DualDialogue/ScriptNotes import,
title-page/synopsis/treatment round trips, revision/style metadata, attachments,
every Fountain escape or damaged-markup case, and large scripts remain outside
the tested subset. FDX sequential dialogue and comment loss, Fountain physical-line
splitting/whitespace normalization, and number visibility are documented explicitly;
this is not a claim of byte-identical or universally lossless interchange.

## Upstream account interface removal

Completed 2026-10-12. `AccountManager` and the upstream account, sign-in,
subscription, purchase/gift, login-session and team-management pages/dialogs were
removed, including the embedded upstream API token in `session_widget.cpp`.
Application construction/content switching, menu and macOS actions, project
creation and move-to-cloud offers, document-upgrade forwarding, subscription
notifications and guarded cloud callbacks no longer refer to those controls.
New stories are local; unavailable documents still have a creation guard without
an upgrade offer. Local writing statistics and the sprint timer are ordinary
menu actions. The Notifications page remains for the next separate TODO item.

The connection-status toolbar and collaborator cursor strip had project
consumers, so their exact source was moved out of `ui/account` and retained.
Existing cloud/team project records and historical account labels remain; no
persisted type IDs, database schema/migration, writing-session storage, installed
plugin discovery, GitHub credentials, onboarding, settings or sync implementation
was changed. Guarded cloud reconnection/data paths retain their transport logic
without account UI references; the unavailable private module was not fetched or
built. [CODEBASE.md](CODEBASE.md) describes the revised routes.

Validation:

- `./build/dev.sh doctor`: passed with Qt 6.12.0 and the automatically selected
  ignored development QtKeychain prefix. Optional translation utilities remain
  absent. `make qmake_all` in `.build/debug`, then
  `JOBS=4 ./build/dev.sh build debug`: passed, including the final style build.
- `./tools/check-github-credential-storage.sh`, `./tools/check-project-sync.sh`,
  `./tools/check-screenplay-scope.sh`, `./tools/check-screenplay-interchange.sh`,
  `./tools/check-writing-session-storage.sh` and
  `./tools/check-backup-retention.sh`: all passed. Credential/sync checks use
  injected storage, an absent private bus and the local mock GitHub server;
  other checks use disposable projects/profiles and an absent private bus.
  The sync, writing-session and backup wrappers inherit the absent bus from the
  evidence runner; they were repeated with that isolation explicit, retaining
  their earlier logs.
- The scope test retains its editor availability, Statistics, keyboard,
  Undo/Redo, pagination, export, retired-format byte preservation and project
  save/reopen assertions. It replaces `AccountNavigator` construction with
  retained menu/dialog checks: no account/sign-in/purchase callback or offer,
  working local writing tools, unavailable-document guarding, and exact cached
  team/project records plus a historical account label through load/save/reload.
- Source/build-entry and rebuilt-plugin symbol audits found no retired account
  classes or integration references. The former embedded token was checked in
  memory against the rebuilt core plugin and debug symbols and was absent;
  its value was never emitted. Frozen upstream and existing history were not rewritten.
- Graph coverage was checked for relied-on sources. Qt macro/parser gaps and
  inaccurate caller edges were verified directly against source; the retained
  evidence driver is ignored/unindexed and was read directly. `git diff --check`
  and Python driver syntax passed. No dependency was added.

Real application evidence on private Xvfb, using a disposable native Secret
Service and a network-isolated `unshare -Urn` application:

- Configured startup and both empty/local-project menus offered no upstream
  account or purchase controls. Application settings retained the separate
  GitHub fields and Save/Clear/Retry controls with an empty ready credential.
- The local-only new-story dialog created a screenplay in a disposable folder.
  Typing a heading and `ACCOUNT CUT UI SURVIVAL`, then ordinary Ctrl+S, persisted
  both lines in the actual SQLite document. Statistics showed one heading, one
  action and nine words. A fresh application reopen retained all 13 document
  UUIDs, type IDs and serialized UTF-8 content hashes; the marker was visible in
  the editor. SQLite integrity passed. The writing-statistics menu page opened
  without an account and retained the prior session's eleven written words.
- Fresh onboarding offered GitHub sync and its separate credential controls;
  “Set up later” reached the local story list. All three launches quit normally
  with Ctrl+Q, actual exit `0`. Only the private Xvfb/provider/bus children were
  then stopped. No file dialog, file manager, external link, real credential or
  ordinary desktop keyring was used.

Evidence is retained in `.build/evidence/account-removal-3ypzjgfg/`, including
all build logs, regression summaries, coverage/source gaps, the newly adapted
copy of the retained interchange UI driver, provider logs, command transcript,
screenshots, exits and serialized document hashes. Successful scope evidence is
`.build/evidence/screenplay-scope-c68G30/`; the other scripts print their unique
evidence directories in the retained summaries. All earlier backup, credential,
interchange and other evidence remains untouched.

Retained failures include the first build's stale account-action label, a test
compile using a getter this project's Button does not expose, and a menu fixture
checking labels before their queued initialization was dispatched. The fixture
now dispatches normal queued events; input remains zero-delay. The driver's
initial syntax error and a SQLite TEXT/BLOB hashing assumption are retained too.
Early animation captures and an unbound Ctrl+N probe are not credited as passing
workflow evidence. Qt shutdown timer warnings remain in the logs; actual exits
were zero, with no forced application teardown.

Limits: Linux debug/offscreen and private Xvfb only. The absent private cloud
build, the owner's desktop/profile/keyring, other platforms (including native
macOS menus), release packaging, and real remote GitHub operations were not
exercised. Native credential mutation was not repeated for this UI cut; the
requested injected credential and mock-sync regressions passed. Notifications
and every subsequent Cut/Add item remain outside this task.

## Upstream Notifications interface removal

Completed 2026-10-12, limited to the first unchecked Cut item. Removed the
Notifications drawer and macOS menu entry, page construction/navigation, developer
release filter, release/credits views and their qmake entries. The menu and
project/projects managers no longer offer page/unread callbacks or set the
retired feed's toolbar badges. Generic badge widgets and document comments/review
controls remain. Translation catalogs and subsequent TODO items are unchanged.

`NotificationsManager` remains because
`ApplicationManager::Implementation::askUpdateToLatestVersion` directly calls
`lastVersionDownloadLink()`, despite the graph reporting no caller. Its remaining
support caches device/Qt-compatible stable-release records and extracts that link.
The guarded cloud `notificationsReceived` connection, login-time `askNotifications`
request and version-upgrade/download flow remain for that consumer. The manager
has no page signals, read-state tracking or preference access; existing show-dev
and last-read settings are left stored and ignored. No cloud module was fetched,
and no update download, installer, release or upstream networking workflow ran.

Validation:

- `./build/dev.sh doctor`: passed with Qt 6.12.0 and the automatically selected
  ignored QtKeychain prefix. Optional translation utilities remain absent.
  `make qmake_all` in `.build/debug`, then
  `JOBS=4 ./build/dev.sh build debug`: passed.
- `./tools/check-github-credential-storage.sh`, `./tools/check-project-sync.sh`,
  `./tools/check-screenplay-scope.sh`, `./tools/check-screenplay-interchange.sh`,
  `./tools/check-writing-session-storage.sh` and
  `./tools/check-backup-retention.sh`: all passed. Every wrapper received an
  explicitly absent private session-bus address and disposable runner profile;
  wrappers that set their own absent bus retain that stricter isolation. GitHub
  checks use only injected storage and a local mock server. No actual tokens,
  projects or desktop keyring were read or changed.
- The scope test preserves account-removal, installed-editor availability,
  Statistics, editing/Undo/Redo/pagination/export, cached records and retired
  document/save/reopen assertions. New checks inspect notification labels,
  actions, widgets and callbacks with and without project actions, even with
  old preferences seeded. Synthetic feed checks cover compatible stable links,
  irrelevant/dev/retired notices, ordered arrivals, duplicate/empty deliveries,
  malformed JSON and unchanged dormant preferences without network access.
- Source/build-entry and rebuilt-plugin symbol audits found no retired page,
  release/credits view or menu/toolbar badge integration. The retained updater
  symbol is present. Graph coverage and reported macro/parser gaps were checked
  directly in source; graph edges were treated as advisory. The ignored driver
  was read directly and copied into the new evidence directory before use.
  Changed-code formatting review, Python driver syntax and `git diff --check`
  passed, preserving the existing empty-brace and test formatting.

Real application on private Xvfb, with a disposable native Secret Service and
network-isolated `unshare -Urn` application children:

- Settled story-list and local-project menus contain no Notifications action or
  filter. Writing statistics and Writing sprint remain available; the latter
  opens duration/start controls. Settings retain GitHub repository/token fields
  and Save/Clear/Retry with an empty ready credential.
- The actual Create story dialog created a local screenplay. Zero-delay typing
  of a heading and `NOTIFICATIONS CUT UI SURVIVAL`, ordinary Ctrl+S and normal
  close persisted the exact text. Statistics displayed one heading, one action
  and nine words. Typing created a derived Location record and updated its group
  and structure; all 13 original document UUIDs/type IDs remained. A full-file
  backup matched the saved project exactly, and SQLite integrity passed.
- Fresh application reopen and a second normal close preserved all 14 saved
  document UUIDs, type IDs and serialized UTF-8 content hashes exactly. SQLite
  TEXT/BLOB retrieval was normalized before hashing. The marker was visible;
  writing statistics opened with the prior session's eleven written words.
- Fresh GitHub onboarding retained its independent credential controls;
  “Set up later” reached local stories. All three Ctrl+Q exits were actually `0`.
  Only the exact private Xvfb/provider/bus children were stopped after application
  exit. No file dialogs, file managers or external links were opened.

Evidence: `.build/evidence/notifications-removal-gkm_pnu1/` contains doctor,
qmake/build logs, all six regression summaries and their unique evidence paths,
coverage/source checks, the adapted driver, provider logs, command transcript,
settled screenshots, exits and document hashes. The scope check's full evidence
is `.build/evidence/screenplay-scope-ZLLNBo/`. Earlier evidence remains untouched.

The first live database marker read failed before the normal-close snapshot;
its timing against queued save completion was not established. The failed
assertion and diagnosis remain. No input was repeated, slowed or retried; the
accepted exact-byte comparisons use the completed normal-close/save/reopen
boundary. A harmless quote-area click is recorded and excluded from creation
evidence. Existing Qt shutdown timer and initial cover-size warnings remain in
the logs; none caused a nonzero exit or forced application teardown.

Limits: Linux debug/offscreen and private Xvfb only. The absent private cloud
build, native macOS menu, other platforms, release packaging, owner's desktop/
profile/keyring, real remote GitHub, updater network/download/installation and
sprint countdown were not exercised. Native credential mutation was not repeated
for this UI cut; the required credential and mock-sync regressions passed.

## Tweet-sharing buttons removed

Completed 2026-10-12, limited to the next unchecked Cut item. The logline
generator no longer converts its help icon into a tweet action: it hides the
icon on the result page and retains the help connection/state when going Back.
The writing sprint panel's share widget, layout/styling/visibility code and tweet
handler are removed. Logline generation/Done, sprint timing, results and Restart
remain. No dependency, persisted setting or document format changed.

Validation:

- `./build/dev.sh doctor`, `JOBS=4 ./build/dev.sh build debug` and
  `./tools/check-screenplay-scope.sh`: passed. The existing scope regression
  covers editor availability, account/notifications cuts, keyboard editing,
  Undo/Redo, pagination, exports and mixed-project preservation. Source and
  rebuilt library/debug-symbol audits found no retired tweet handler/control.
- In the actual application on private Xvfb, a disposable story's logline wizard
  generated "A determined writer must finish a screenplay." Its result page had
  no share action. Back retained the enabled hints and their working toggle;
  Continue/Done applied the exact text to the screenplay information page.
- A normal one-minute sprint completed after zero-delay screenplay typing and
  ordinary Ctrl+S. The result showed nine written words without a share button;
  Restart restored duration/start/sound controls. Sound playback was not tested.
- Normal close and a fresh application reopen retained the exact logline and
  screenplay markers. All 14 document UUIDs, type IDs and serialized UTF-8
  content hashes were unchanged; SQLite integrity passed. Both actual Ctrl+Q
  exits were `0`, followed by teardown of only the private display/provider/bus.
- The UI runner used a private native Secret Service, separate profile and
  network-isolated `unshare -Urn` application. No real credentials, user projects,
  desktop keyring or external browser were used. Graph coverage was checked;
  reported parser gaps and the ignored driver were read directly. Driver syntax
  and `git diff --check` passed.

Evidence is retained in `.build/evidence/tweet-removal-tnap3yv6/`, including the
build/doctor logs, scope summary, reused existing driver, command transcript,
screenshots, exits and saved/reopened document hashes. Full scope evidence is
`.build/evidence/screenplay-scope-3Vp9b9/`. The first private-bus launch was blocked
by the sandbox's socket restriction; its failure remains in `native/bus.log`.
The permitted isolated run passed. Initial navigation probes and animation
captures remain in the transcript and are not credited as settled UI checks.

Limits: Linux debug/offscreen and private Xvfb only; the owner's desktop,
other platforms, private cloud builds and release packaging were not exercised.
Later TODO items and existing translation catalogs remain outside this task.

## Story-list user guide removed

Completed 2026-10-12, limited to the next unchecked Cut item. Removed the
story-list User guide button, its `ProjectsNavigator::helpPressed` signal and
the manager's localized upstream-help URL handler. Create story, Open story
and Sync with GitHub remain. No dependency, setting or project format changed.

Validation:

- `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`: passed.
  `./tools/check-screenplay-scope.sh` and
  `./tools/check-writing-session-storage.sh`: passed, including existing
  editing/Undo/Redo/pagination/export and save/reopen preservation checks.
- Source and rebuilt-plugin symbol checks found no story-list guide control,
  signal or help URL; the create/open/sync signals remain. Graph coverage was
  checked and the navigator's reported Qt macro/parser gaps read directly.
  Changed-line formatting and `git diff --check` passed.
- The real application on private Xvfb showed the three retained buttons and
  no User guide. Create opened the screenplay creation dialog; Sync opened
  the GitHub setup prompt. Both were cancelled. Ordinary Ctrl+Q exited `0`
  before teardown of the private display, bus and disposable Secret Service.

Evidence: `.build/evidence/user-guide-removal-hekxi0f6/`, including doctor/build
logs, source/symbol audits, the copied disposable driver, screenshots, transcript
and exit. Full regressions: `.build/evidence/screenplay-scope-qNu2ZY/` and
`.build/evidence/writing-session-storage-0pLHVj/`. The initial startup screenshot
preceded content display; `story-list-ready.png` is the settled UI evidence.

Limits: Linux debug/offscreen and private Xvfb only. The private display used an
isolated profile/keyring and network-isolated application. The owner's desktop,
real remote sync, native Open file dialog, other platforms and release packaging
were not exercised. Existing translation catalogs and later TODO items remain.

## Cover images and upstream image search removed

Completed 2026-10-12, limited to the next unchecked Cut item. Removed the project
information page's cover image card, upload/paste/reset/cropping controls and
cover generator; the page retains its name and short description fields. Story
cards use the available width for text and no longer load or draw cover/default
poster thumbnails. The generator/search sources and qmake entries are deleted,
along with the Google Translate request helper whose only remaining consumer
was cover search. The upstream Unsplash search/download paths are gone.

Persisted cover UUIDs, project image documents, cached poster paths and their
compatibility model/storage behavior remain. Opening or editing a project does
not clear an old cover, including a reference whose image is missing. Shared
image widgets, character/location image data, ordinary UI translations and
GitHub sync remain available. No dependency or project-format migration changed.

Validation:

- `./build/dev.sh doctor`, `make qmake_all` in `.build/debug`, and
  `JOBS=4 ./build/dev.sh build debug`: passed. Source/build-entry and rebuilt
  corelib/core/project-information plugin audits found no retired cover/search
  classes, view cover mutation methods or request helper.
- `./tools/check-screenplay-scope.sh`: passed. New checks independently seed a
  legacy cover PNG with an extra binary payload, edit the bound information
  page, and verify exact reference/image preservation through save/reopen.
  A missing-image reference survives open/save unchanged. Cached team/project
  records retain a nonempty poster path. Actual story-card painting is identical
  for existing, missing and default covers in both text directions. Existing
  keyboard editing, Undo/Redo, pagination, exports and mixed-project preservation
  checks remain green.
- `./tools/check-writing-session-storage.sh`, changed-code formatting review,
  disposable driver syntax and `git diff --check`: passed. Graph coverage was
  checked; reported Qt/parser gaps and ignored runtime drivers were read directly.
- The actual application on private Xvfb opened a disposable previously created
  screenplay project with a seeded old cover. Its information page displayed
  only name and description fields. Zero-delay edits and ordinary Ctrl+S saved
  the expected text while retaining exact image bytes and the cover UUID.
  Only the project-information/structure documents changed as expected; all 15
  document UUIDs/type IDs and all other content hashes remained intact.
- Fresh startup showed a text-only story card with the saved name/description.
  Opening it restored those fields without cover controls. A second normal
  close preserved every saved document hash exactly; SQLite integrity passed.
  The first run ended with Ctrl+W (exit `0`), and the reopened run with Ctrl+Q
  (exit `0`), before teardown of the private display, bus and disposable keyring.

Evidence: `.build/evidence/cover-removal-hhhgjsp2/` retains doctor/qmake/build
logs, audits, driver, screenshots, command/exit transcripts and before/saved/
reopened document hashes. Full final scope evidence is
`.build/evidence/screenplay-scope-reuhyd/`; the storage evidence is
`.build/evidence/writing-session-storage-uam5Mn/`. The earlier scope run is
retained in `.build/evidence/screenplay-scope-WmCtW0/`. Startup/animation and
post-window-close blank captures are retained and excluded from settled UI
evidence; `reopened-story-list.png` and the information `*-ready.png` files show
the verified surfaces.

Limits: Linux debug/offscreen and private Xvfb only, using disposable projects,
an isolated profile/Secret Service and network-isolated application children.
No owner projects/keyring, external cover search, real remote GitHub, private
cloud build, other platform or release packaging was exercised. Later TODO
items and existing translation catalogs remain outside this task.

## Automatic upstream font downloads removed

Completed 2026-10-12 after owner approval of the bounded scope. Removed
`ApplicationManager::Implementation::loadMissedFonts`, its startup and
language-change calls, the language/emoji URL map, and the `fonts-loading`
progress task. Its sole missing-font detector, `DesignSystem::Font::missedFonts`,
is removed as well. The updater retains its own `NetworkRequest` include and
download route; other network consumers are unchanged.

The bundled interface, icon and screenplay font resources, their startup
registration, and the existing local font-cache loader are unchanged. Existing
valid cached fonts still load, and installed fonts remain available. English
screenplay fonts need no upstream download. Missing emoji/non-English glyphs
now depend on available local fonts. No template, document format, font file,
translation catalog, or later TODO item changed.

Validation:

- `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`: passed,
  including the final build after replacing the downloader-only include with
  the request header needed by the updater.
- `./tools/check-screenplay-scope.sh`: passed, including keyboard editing,
  Undo/Redo, save/reopen, pagination, exports, exact legacy-document/image
  preservation and independent DOCX/PDF inspection (seven PDF pages).
- `./tools/check-writing-session-storage.sh`: passed for anonymous/account
  filtering, retained writing-session rows and reopen.
- The retained `font-startup-probe.cpp` uses the real `Application` and actual
  core plugin, a private D-Bus without a keyring provider, disposable profiles,
  an empty Fontconfig font directory and network-isolated application processes.
  Fresh English, English with a cached font, and Chinese with a cached font all
  passed and exited `0` after normal window-close requests. Each verifies
  bundled interface/icons/Courier families, actual Courier Prime selection and
  English glyph coverage, no font-loading task despite missing emoji/Chinese
  fonts, and no new font-cache files. Cached cases verify that the fixture font
  becomes available and retains its exact bytes.
- Source/binary audits confirm removed downloader symbols/endpoint/task,
  byte-identical bundled/cache startup code and unchanged font resources.
  `git diff --check` passed. Graph coverage was checked; parser gaps and ignored
  probe/driver sources were verified directly.

Evidence: `.build/evidence/font-download-removal-2404kqlc/` contains build logs,
audits, probe sources/binaries, private-bus logs and per-process exit results;
the passing font cases are under `lifecycle-startup/`. Regression evidence is
`.build/evidence/screenplay-scope-IfDXN7/` and
`.build/evidence/writing-session-storage-B5paic/`. Failed attempts are retained:
the sandbox denied the first private-bus socket; an initial probe incorrectly
required synchronous close acceptance; a plain `QApplication` probe then left
the manager alive until after Qt teardown and crashed in `QStandardPaths`
(GDB trace retained). Using the application's existing manager ownership fixed
the probe lifetime without application lifecycle changes. A probe-only compile
failure and its subsequent missing-executable run are also retained.

Limits: Linux debug/offscreen startup and in-process screenplay checks only.
No visible desktop/Xvfb interaction, owner projects/keyring, real remote sync,
upstream font request, private cloud build, other platform or release packaging
was exercised. The removal prevents these font downloads; it does not change
other network behavior or guarantee glyphs for unavailable local fonts.

## Worlds retired with existing project data preserved

Completed 2026-10-12, limited to the next unchecked Cut item. Removed World and
Worlds model construction, creation controls, export dispatch/dialogs/exporters,
the public World information navigator, and the uninitialized private World
information and Worlds map plugin links. Their source/build/plugin registrations
and World preview images are gone. New project structures create no Worlds group;
an old remembered World creation choice falls back to Screenplay.

The persisted IDs `50000`/`50001`, MIME mappings, stored setting keys, and reserved
promo identifiers remain intact. Existing groups and World documents are listed
with the same preserved-content notice as other retired types, without loading a
model or rewriting content. Empty groups remain visible. Retired items reject
dragging and drops, including direct/mixed MIME data. World map/profile previews
are removed; Characters and Locations remain. The international screenplay
templates `world_cp`/`world_cn` and the logline generator's story-world questions
are unrelated to these documents and remain unchanged.

The first preservation run exposed an inherited destructive boundary:
`Database::validateDatabase` removed all but one Worlds group on open, together
with the deleted records' change history. Retired Worlds are now excluded from
that singleton cleanup. The regression keeps both populated and empty groups
and asserts exact World/draft/image bytes, identities, hierarchy and history
through save/reopen. Other singleton validation and database upgrade steps stay.

Validation:

- `./build/dev.sh doctor`, `make qmake_all` in `.build/debug`, and
  `JOBS=4 ./build/dev.sh build debug`: passed. The final incremental build after
  the preservation fix passed too. The obsolete World navigator library/debug
  artifact was moved into retained evidence before runtime checks.
- `./tools/check-screenplay-scope.sh`: passed. New checks cover no fresh Worlds
  records or model creation, absent creation choices, remembered-choice fallback,
  preserved MIME/IDs, both group records, a World with BOM/CRLF/unknown and binary
  content, its draft and image attachment, unchanged structure/hierarchy, and
  exact stored change identity/patch/author. Existing keyboard editing, Undo/Redo,
  pagination, all four exports, mixed-project preservation and Statistics pass;
  independent DOCX/PDF inspection found the expected text on seven PDF pages.
- `./tools/check-screenplay-interchange.sh` and
  `./tools/check-writing-session-storage.sh`: passed for independent Fountain/FDX
  import/export/save/reopen and writing-session storage respectively.
- The actual application on private Xvfb opened and reopened a disposable copy
  of the mixed project. Populated Worlds, its World child, and the empty group
  remained listed; selecting the populated group and child showed the
  preserved-content notice without editor/export/draft controls. Add document
  offered Screenplay, Folder and Text, with no World. Both normal Ctrl+Q exits
  were `0`. All 67 document UUIDs/types/content hashes and all 16 change-history
  identities/patches/authors stayed unchanged; both Worlds groups remained and
  SQLite integrity passed.
- Source/binary audits found no retired World model/exporter/navigator class
  routes, and confirmed unchanged enum IDs, MIME maps, screenplay templates,
  logline questions and setting keys. Changed-code formatting, driver syntax
  and `git diff --check` passed. Graph coverage was checked and parser gaps and
  ignored driver files were read directly.

Evidence: `.build/evidence/world-removal-1i27n15n/` contains qmake/build logs,
coverage/source/binary audits, removed runtime artifacts and private Xvfb driver,
screenshots, command/exit logs and before/after manifests. Passing regressions:
`.build/evidence/screenplay-scope-xj9TGC/`,
`.build/evidence/screenplay-interchange-em8CZr/`, and
`.build/evidence/writing-session-storage-iJ8ACb/`. The pre-cut creation failure
is retained in `.build/evidence/screenplay-scope-RtgZQk/`; the failed duplicate
preservation run and its project are retained in
`.build/evidence/screenplay-scope-ebLSqA/`. Git's initial refusal to remove the
private links until `.gitmodules` was staged required no source workaround.

Limits: Linux debug/offscreen and private Xvfb only; World fixtures were generated
independently of the removed models, not imported from a private upstream editor.
The UI used a private bus without a keyring provider and dismissed its expected
startup credential warning; no credentials were edited. Startup, post-close and
right-click blank captures are retained but excluded from settled GUI evidence;
`worlds-notice-ready.png`, `world-notice-ready.png` and `add-document-ready.png`
show the verified surfaces. Owner projects/keyring/desktop, real remote sync,
private cloud modules, other platforms and release packaging were not exercised.
Translation catalogs and later TODO items remain outside this task.

## Translation catalogs reduced to English

Completed 2026-10-12, limited to the next unchecked Cut item. Removed the `.ts`
and `.qm` catalogs of the 31 other interface languages and the untranslated
`_en_source.ts` template (63 files, about 46 MB). `src/starc.pro` and
`src/core/translations.qrc` list only `translation_en`. Both English files are
byte for byte the upstream ones (`git hash-object` equals the `master` blobs).
They stay because the source strings are English but plural forms are not:
without the catalog `tr("%n scene(s)")` renders "2 scene(s)".

`ApplicationManager::Implementation::setTranslation()` now takes no language. It
sets the default locale to English, installs the embedded English catalog and a
left-to-right layout, once at startup. The system language, the stored language
and the developer translation-file override are not consulted. The settings
Language row, `LanguageDialog`, the `SettingsManager`/`SettingsView` language
signals and setters, `LanguageHelper` (Qt 5/6 language-id mapping) and
`PercentRadioButton` (used only by the dialog) are removed with their qmake
entries. Onboarding no longer stores the system language.

The settings `application/language`, `application/language-qt-version` and
`application/language-file` lost their key constants and default. Values stored
by an earlier version are neither read nor rewritten. A full settings reset
clears them like any other setting. Spelling dictionaries, screenplay templates
for other regions and right-to-left script text are separate and unchanged.

Evidence:

- `./build/dev.sh doctor` and `./build/dev.sh build debug`: passed after
  `make qmake_all`, before and after the files were deleted from disk. Optional
  translation utilities remain absent.
- `./tools/check-screenplay-scope.sh`: passed. New checks require one embedded
  catalog, "2 scene(s)" without it and "1 scene"/"2 scenes"/"1 word"/"3 pages"
  with it, no language signals, chooser classes or "Language" label in the
  settings, and unchanged stored Hebrew/Qt-version/file values after the settings
  screen is built and receives a language-change event.
- `./tools/check-screenplay-interchange.sh`, `check-writing-session-storage.sh`,
  `check-backup-retention.sh`, `check-project-sync.sh` and
  `check-github-credential-storage.sh`: passed on the final tree.
- The actual application on private Xvfb, with no network and a private bus
  without a keyring provider: a configured profile seeded with Hebrew (`103`),
  Qt version `6` and a translation-file path opened a disposable copy of the
  mixed project in English, left to right. The scene navigator showed "5 pages",
  "1 scene", "1133 words"; the settings page starts at "User interface" with no
  Language row. After Ctrl+Q (exit `0`) the three stored values were unchanged.
  A fresh profile showed the English first-run page, and after "Set up later"
  and Ctrl+Q (exit `0`) held no interface-language key. Both logs record
  `Setup translation for en`.
- Source search found no remaining reference to the removed classes, keys or
  catalogs; `libcoreplugin.so` has no `LanguageDialog` string and `libcorelib.so`
  exports no `LanguageHelper`/`PercentRadioButton` symbol. Changed-code
  formatting and `git diff --check` passed. Graph coverage was checked;
  `application_manager.cpp` is only partly indexed (the private `setTranslation`
  is absent from the graph), so its callers were traced in source.

Evidence: `.build/evidence/translations-english-only-eiBpVH/` holds the
doctor/qmake/build/regression logs (`logs/`) and the Xvfb driver, command and
exit logs, application logs, profiles and screenshots (`ui/`). Regressions on the
final tree: `.build/evidence/screenplay-scope-reFZ6B/`,
`.build/evidence/screenplay-interchange-1CdUZE/`,
`.build/evidence/writing-session-storage-VeGkuq/`,
`.build/evidence/backup-retention-rpMPd1/`,
`.build/evidence/project-sync-jtxOvC/` and
`.build/evidence/github-credentials-VGruIs/`.

Limits: Linux debug/offscreen and private Xvfb only. The Xvfb run used the build
made before the retired files were deleted from disk; the rebuild afterwards
left `libcoreplugin.so` as it was. The new scope checks were not run against the
build from before the cut. The startup keyring warning was expected and
dismissed; no credentials were edited. `translation_en.ts` still lists strings
of removed features (and three English "translations" that differ from the
source by whitespace); pruning or regenerating it needs `lupdate`/`lrelease`
from `qt6-tools`, which need sudo to install, so the catalog was left as is.
Code that reads `QLocale()` was not pruned: the per-language interface fonts in
`DesignSystem`, the Russian branches of `about_application_dialog.cpp` and
`price_helper.cpp`, and right-to-left branches of delegates now always take the
English path. Typographic quote replacement and the DOCX language code follow
that locale, as they already did with English selected. Upstream CI and packaging
scripts were not run or changed. Later TODO items remain outside this task.

## Character and location pages

Completed 2026-10-12, limited to the next unchecked Add item. Of the two choices
the item left open, the pages were built rather than the groups hidden: notes
stay attached to a character while the script changes, and the data was in the
project either way.

A character or a location now opens in a page with a name, a description and
notes. The two pages are local plugins at the paths of upstream's private
`character_information` and `location_information` submodules, whose links and
`.gitmodules` entries are removed, as was done for Statistics. Description maps to
the existing `one_sentence_description` field and notes to `long_description`, so
nothing new is stored and what an upstream project already holds in those fields
is shown. All other character and location fields stay in the document untouched.
With a page to describe them, "Add character" and "Add location" are offered
again, by the checks that already follow plugin availability.

Description and notes are saved as typed. A name is applied on Enter, or when the
field is left for another one, never while it is typed: renaming goes through
upstream's existing route, which replaces the name in every screenplay of the
project. The page converts the name to capitals, like collected names, and
refuses an empty one. It also refuses the name of another character or location:
upstream's route would then permanently remove the renamed document as a
duplicate, with its description and notes. Focus lost to another window or a
menu leaves a half-typed name unapplied.

Two changes outside the plugins:

- Upstream renamed "nested" locations by bare prefix, so renaming HOUSE to HOME
  also turned HOUSEBOAT and its scene heading into HOMEBOAT (observed with the
  unchanged code), and it removed a nested location whose new name was taken. A nested location is now
  one with " - " or ". " after the renamed name; a taken name leaves it as it is.
- "Characters" and "Locations" have no editor of their own. Selecting one shows
  a short explanation in place of the "No editor for this document" notice.

The trait navigators `character_information_structure` and
`location_information_structure` are removed: they listed the categories of the
private editors and had nothing left to navigate. The add-document dialog no
longer promises relation tracking for a character.

Evidence:

- `./build/dev.sh doctor`, `make qmake_all` in `.build/debug` and
  `JOBS=4 ./build/dev.sh build debug`: passed, with no warning in the new or
  changed files. The two obsolete navigator libraries were removed from
  `_build/plugins/`.
- `./tools/check-screenplay-scope.sh`: passed, 127 new checks on a project typed
  through the real screenplay editor. They cover the page of a collected, a
  hand-added and an upstream-detailed character (age, gender, role, nickname,
  secrets and dreamcast kept after an edit), typed description and multi-line
  notes, a name unapplied while typed, renaming on Enter and on leaving the
  field, the cue, action and dialogue text of the script afterwards, refused
  taken and empty names with every document still present, read-only mode, the
  location page, scene headings, nested and look-alike locations, a nested name
  that is taken, both group explanations, the add dialog, exact bytes of all
  twelve documents through save/reopen, pages that rewrite nothing when merely
  opened, and one script Undo taking a rename back out of the script.
- The checks fail without the guards: upstream's original nested rename fails
  "location that only begins with the same letters is left alone"; without the
  taken-name skip, "nested location keeps its name when the new one is taken";
  without the page's duplicate check, "taken or empty name is refused and
  removes nothing". Each was rebuilt, run and restored.
- `check-screenplay-interchange.sh`, `check-backup-retention.sh`,
  `check-project-sync.sh`, `check-writing-session-storage.sh` and
  `check-github-credential-storage.sh`: passed on the final build.
- The actual application on private Xvfb, with no network: in a disposable copy
  of the regression project and in an imported Fountain script, the pages, the
  group explanation, the context menu's "Add character", the add dialog, a
  refused duplicate name with its error, Enter and click-away renames, the
  renamed script, the split view (an edit in one panel showed in the other) and
  "Open in new window" were exercised by mouse and keyboard. After Ctrl+S and
  Ctrl+Q the project passed SQLite's integrity check and held the typed
  description, the extended notes beside the untouched upstream details, the
  hand-added character and the renamed cues; a fresh start showed them again.
  The three Ctrl+Q exits that were recorded were `0`. The first two launches
  (onboarding and the Fountain import) closed normally by their logs, but their
  exit codes were not captured.
- Changed-code formatting (local `{ }` rewrites not applied) and
  `git diff --check` passed. Graph coverage was checked: `project_manager.cpp`
  is partly indexed and the graph lists no callers for `updateLocationName` or
  `showNotImplementedPage`, whose calls sit in lambdas, so the rename and
  no-editor routes were traced in source with `rg`.

Evidence: `.build/evidence/character-location-pages-2026-10-12/` holds build,
regression and mutation logs (`logs/`) and the screenshots, application logs,
exit codes and both disposable stories (`ui/`). Regressions on the final build:
`.build/evidence/screenplay-scope-BXqxEj/`,
`.build/evidence/screenplay-interchange-oBQoF6/`,
`.build/evidence/backup-retention-6K6n5G/`, `.build/evidence/project-sync-sFGfO5/`,
`.build/evidence/writing-session-storage-wxPg6Q/` and
`.build/evidence/github-credentials-VAHsKy/`. Failed runs kept:
`screenplay-scope-crinxQ` (a wrong expectation of the new check: newlines are
stored as `&#10;`), and the three mutation runs `screenplay-scope-sVhDuF`,
`-R7m3en` and `-21H7Gu`.

Limits:

- A character rename replaces the name wherever it stands as a whole word in
  action and dialogue, in any letter case. That is upstream's replacement,
  unchanged. Renaming a character called WILL, MAY or MARK also rewrites those
  ordinary words. One Undo in the script restores the script; the character
  keeps its new name.
- A story opened directly from a Fountain or FDX file saves only its script.
  Descriptions and notes typed there were gone after reopening (observed);
  renames stay, because they are in the script. A `.starc` project keeps all of it.
- Two characters or locations can no longer be merged by renaming one to the
  other's name, which upstream's route allowed.
- A location whose new name is the beginning of its old one (HOUSE OLD to HOUSE)
  leaves its nested locations with the old name; that upstream check is unchanged.
- Role, age, gender, photos, relations, routes and the other upstream fields
  have no controls.
- The new strings are not in the English catalog and need none (no plurals).
- Linux debug, offscreen and private Xvfb only, on a private bus without a
  keyring provider; its expected startup warning was dismissed and no
  credentials were edited. The last change, shorter helper texts under the name
  fields, was seen on the location page only. The first split of a view logs an
  upstream "Plugin isn't found for mime-type" line that predates this work
  (`ProjectManager::Implementation::switchViews`, present on `master`). Recycle-bin
  pages, right-to-left layout, other platforms, release packaging and the owner's
  projects were not exercised.

## Character renaming that asks about mentions

Completed 2026-10-12, as the "Make renaming a character safe for the script"
item under Fix. It supersedes the first limit listed in the section above:
renaming a character no longer rewrites ordinary words.
This records the initial implementation. The review follow-up below supersedes
its Unicode, formatting and applied-patch limitations.

Upstream's rename replaced the name wherever it stood as a whole word, in any
letter case and in every paragraph. Observed with the unchanged code on a typed
script, renaming WILL to BILL gave "I Bill go.", "Bill you come?", the cue
YOUNG BILL and the heading INT. BILL'S BAR - DAY, while Characters still listed
YOUNG WILL and Locations still listed WILL'S BAR.

What a rename does now:

1. A lowercase word is never a mention. "I will go" stays; "Will" and "WILL"
   are mentions and keep upstream's case mapping ("Bill", "BILL").
2. Cues and scene-character lists are renamed without a question, as before.
   Mentions are the writer's choice. If the running text of any screenplay in
   the project names the character, the page asks once, with the number of
   mentions: "Leave the text" or "Rename in the text too". With no mentions it
   asks nothing. "Will you come?" cannot be told apart from the name, which is
   why this is asked and not guessed; the question says that an ordinary word
   starting a sentence is counted.
3. Mentions are looked for in running text only: action, dialogue,
   parenthetical, lyrics, shot, transition, beat, inline note and unformatted
   text. Scene headings, cues and scene-character lists feed the Locations and
   Characters lists and are not searched, so YOUNG WILL and WILL'S BAR stay as
   the lists have them. Act and sequence headings and footers are not searched
   either.

Decisions made while building it, each within the agreed design:

- The question is asked before anything is renamed, not after the character has
  its new name. The page hands the name to `CharacterModel::requestRename`;
  `ProjectManager::Implementation::renameCharacter` counts, asks, and calls
  `setName` after the answer. Asking after `setName` would leave the character
  renamed and its cues not for as long as the question is open, and a save,
  crash or quit in that time would store the character split in two. This way
  an unanswered question means no rename at all.
- Every answer renames the character and its cues. "Leave the text" is the
  dialog's reject button, so Escape and a click outside the card choose it;
  neither cancels the rename.
- No button is bound to Enter. The writer has just pressed Enter to confirm the
  name, and a repeated key press should not choose to rewrite the text. Tab
  reaches the buttons.
- Only the page asks. `setName` is also reached from `applyPatch` (undo or redo
  of the character document, applied remote changes) and from document
  creation. Those rename cues and scene-character lists, ask nothing and leave
  mentions alone. Upstream replaced mentions there too.
- Counting and renaming use one matcher (`characterMentions`) on the same
  paragraph types and the same screenplay texts, so the number shown is the
  number of places changed. Mentions are found in the paragraph as it is and
  replaced from the last to the first; upstream searched again after each
  replacement.
- The message is built from plain sentences, one for "1 time" and one for
  "%1 times", because the English catalog cannot be regenerated for a new
  plural form.

Undo: a rename is still one Undo step in a script, cues and mentions together,
because the script is changed in one call after the answer. Each screenplay has
its own history, so a project with two screenplays takes one Undo in each. The
character keeps its new name after the script Undo, as before.

Evidence:

- Reproduction before any change: a probe in the scope check typed the second
  screenplay described below through the real editor and renamed WILL to BILL
  on the page, on the unchanged build. It stopped at "the cue of another
  character is left alone"; its dump shows the damaged lines quoted above, so
  the cue and heading problem, which came from reading the code, is real.
- `./build/dev.sh doctor` and `JOBS=4 ./build/dev.sh build debug`: passed. The
  only warnings in a changed file are two unused parameters of an upstream
  lambda in `project_manager.cpp`, present before.
- `./tools/check-screenplay-scope.sh`: passed, 12,818 check lines against
  12,737 before. The three old assertions that expected mentions to be renamed
  unasked now answer the question: ALICE to ALICIA "Rename in the text too"
  (two mentions, all renamed), ALICIA to ALICIA GREY "Leave the text" (cues
  renamed, "Alicia greets Bob." kept), BOB to ROB "Rename in the text too" and
  then one script Undo. New checks type a second screenplay with the heading
  INT. WILL'S BAR - DAY, the scene characters "WILL, YOUNG WILL", "Will smiles.
  WILL pours a drink.", a WILL cue, the parenthetical "to Will", "I will go.", a
  YOUNG WILL cue and "Will you come?", plus "Will waits outside." in the first
  screenplay. They require: the question once, with "5 times" for both
  screenplays; nothing renamed while it is open; no second question when the
  field loses focus to it; Enter answering nothing; a click outside, Escape and
  "Leave the text" each renaming the character, its cue and its scene-characters
  entry and nothing else, compared against the exact stored XML; no question
  when the text does not mention the name; "Rename in the text too" changing
  exactly the five mentions, with "I will go.", the YOUNG WILL cue, the heading
  and both list entries untouched; one Undo per screenplay restoring it exactly;
  a name set without the page asking nothing and leaving the mentions; a
  question left open when the project closes going away and renaming nothing;
  and "1 time" for a single mention.
- Each guard fails without its fix. One was removed at a time, the build and
  the scope check were run, and the source was restored:
  the lowercase test (count check fails), the paragraph-type limit of the
  replacement (YOUNG BILL and BILL'S BAR come back), the same limit of the count
  (count check), the answer passed to the replacement ("mentions the writer
  leaves stay as they are written"), the ignored repeated request ("asks nothing
  more"), "Leave the text" as reject button ("question closes"), no button on
  Enter ("Enter does not answer the question"), the check for a closed project
  (segmentation fault after "question before the project closes"), hiding the
  question on close ("question goes away with its project"), the no-mentions
  shortcut ("renamed without a question"), and resetting the choice after the
  rename ("a name set without the page renames cues only").
- `check-screenplay-interchange.sh`, `check-backup-retention.sh`,
  `check-project-sync.sh`, `check-writing-session-storage.sh` and
  `check-github-credential-storage.sh`: passed on the final build.
- The actual application on private Xvfb, with no network, on copies of the
  project the scope check leaves before the rename
  (`rename-mentions-fixture.starc`). Copy A: WILL renamed to "bill" with Enter;
  the question showed the title "Rename it in the text too?", "The cues of WILL
  become BILL either way.", "5 times" and both buttons, and the navigator still
  said WILL; "Rename in the text too" by mouse; the editor showed the renamed
  script; Ctrl+S, Ctrl+Q. Copy B: the same with "Leave the text". Both stored
  projects passed SQLite's integrity check and held exactly the expected lines,
  and a fresh start showed them again and changed nothing. Copy C: a click
  outside the card renamed the cues only; renaming back to WILL asked nothing;
  renaming again with the mentions and one Ctrl+Z in the editor put the whole
  script back, cue and mentions. Copy D: a second Enter at the question did
  nothing; Ctrl+Q with the question open closed the application and the stored
  project still had WILL everywhere. Copy E: Tab, Tab, Enter chose "Rename in
  the text too" with the same result as copy A. All eight Ctrl+Q exits,
  including the first-run profile setup, were `0`.
- Changed-code formatting (local `{ }` rewrites not applied) and
  `git diff --check` passed. The rename routes were traced in source with `rg`:
  the graph lists no caller for `updateCharacterName` and only `applyPatch` and
  `setDocumentName` for `CharacterModel::setName`; `project_manager.cpp`,
  `screenplay_text_model.cpp`, `abstract_model.cpp` and the test are partly
  indexed.

Evidence: `.build/evidence/character-rename-mentions-2026-10-12/` holds the
reproduction, build, regression and mutation logs (`logs/`) and the
screenshots, application logs, exit codes, dumps and disposable projects
(`ui/`). Regressions on the final build: `.build/evidence/screenplay-scope-Uh5u3x/`,
`.build/evidence/screenplay-interchange-lx4c5i/`,
`.build/evidence/backup-retention-hpYn5m/`, `.build/evidence/project-sync-1N0Je5/`,
`.build/evidence/writing-session-storage-ylZ1sY/` and
`.build/evidence/github-credentials-JTFzr5/`. Failed runs kept:
`screenplay-scope-xWrEzI` (the reproduction), `screenplay-scope-osSJPG` (a wrong
expectation of a new check: a closed question takes a moment to fade), and the
mutation runs named in `logs/m01` to `m11`. `logs/invalid-stale-library/` holds
a first attempt at four of them that proved nothing: the driver restored a
source file with its old timestamp, so the build kept the previous mutation.
Those four were repeated after the driver was corrected.

Limits:

- A capitalised ordinary word is counted and, if the writer chooses, renamed:
  "Will you come?" becomes "Bill you come?". The question says so, but the
  writer has to look at the script before choosing.
- A mention of another character whose name contains the renamed one ("Young
  Will enters.") counts as a mention of WILL.
- Act and sequence headings and footers are not searched.
- The whole-word test is upstream's and knows only unaccented letters, digits
  and the underscore as parts of a word, so a name that begins or ends with
  another letter (ZOË, ÉMILE) is not found in running text: its mentions are
  neither counted nor renamed. A name that begins with a digit (2ND COP) is
  counted in lowercase too.
- The three cases above were checked with the expression on its own, in a
  small Qt program (`logs/13-matcher-edge-cases.log`), not through the
  application.
- Replacing a name with one of another length does not move bold, italic or
  review marks that follow it in the paragraph. That is upstream's `setText`,
  unchanged, and applied to cues and mentions before as well. Not run.
- A name applied by a patch was exercised through a direct `setName` call, not
  through a stored change or a remote one; the cloud client is absent from
  this checkout and the character page has no Undo control.
- The question belongs to the main window. A page opened with "Open in new
  window" was not tried; by the code its question appears in the main window.
- After a script Undo the character keeps the new name while that script has
  the old cue again, as before this change.
- The new strings are not in the English catalog and need none.
- Linux debug, offscreen and private Xvfb only, on a private bus without a
  keyring provider; its expected startup warning was dismissed and no
  credentials were edited. Escape and the "1 time" wording were checked
  offscreen only. Other platforms, release packaging, right-to-left layout and
  the owner's projects were not exercised.

## Character rename review follow-up

Reviewed and improved 2026-10-12, starting from `370afe06` on a clean `dev`.
The existing choice before renaming, conservative dismissal, paragraph scope
and one Undo step per screenplay remain the intended behavior.

The new model probe failed against the unchanged build: accented names were
missed, digit-prefixed lowercase names were counted, Unicode word boundaries
were wrong, extra cue/list whitespace was mishandled, and growing or shrinking
names left emphasis, review comments and resource links at their old offsets.
The matcher also used an incomplete regex escape function. Names are now escaped
as literals with Unicode letter/mark/number boundaries; the first letter is
checked even after a digit or punctuation. Exact cue/list names use original
source positions, preserve spaces and extensions, and update every matching
entry without touching another character's name.

`TextModelTextItem::replaceText` moves emphasis, review ranges with their full
comments/status, and resource links with each replacement. Ranges overlapping
a replaced name cover the replacement. Replacements run from right to left
inside one paragraph transaction, so cached XML is rebuilt once per paragraph.
`setText` and unrelated text-editing paths are unchanged.

The pending question now belongs to its request and its project manager. A
character-name patch dismisses it; an old or fading answer cannot overwrite
that patch or clear a later question. The proposed name is checked again when
the answer arrives. If another character has taken it, the page shows the
duplicate-name error instead of entering the existing merge/delete route.

Validation:

- `./build/dev.sh doctor`: exit 0.
- `JOBS=4 ./build/dev.sh build debug`: exit 0, including the new model method
  and signal. Existing warnings remain in unchanged code; the changed manager
  file has its two pre-existing unused lambda parameters.
- `STARC_RENAME_CHECK=pages ./tools/check-screenplay-scope.sh`: exit 0. Model
  cases cover ZOË, ÉMILE, digit prefixes, regex punctuation, Unicode word
  boundaries, cue whitespace/extensions, repeated scene-character entries,
  longer/shorter names and all three kinds of attached ranges. The page cases
  include real `applyDocumentChanges` patches, stale answers, a target taken
  while the question is open, and annotated Unicode text through the real
  editor's Undo/Redo and project save/reopen, compared against exact stored XML.
- `./tools/check-screenplay-scope.sh`: exit 0 on the final code and fixture,
  including the existing creation, keyboard, pagination, export, preservation
  and statistics cases as well as the new rename cases.
- `./tools/check-screenplay-interchange.sh`: exit 0, including independent
  Fountain/FDX fixtures, syntax/XML/styles and save/reopen checks.
- Bash syntax, ShellCheck, changed-line formatting with `src/.clang-format`,
  and `git diff --check`: passed. The formatter used read-only Git diffs because
  `git clang-format` cannot create its temporary index in this sandbox.

Evidence: `.build/evidence/character-rename-review-2026-10-12-s2qbp7ss/` retains
the baseline, build, regression and failed-attempt logs. Successful focused
pages: `.build/evidence/screenplay-scope-A6SLxk/`; successful interchange:
`.build/evidence/screenplay-interchange-EMPNZQ/`.
The final full scope run is `.build/evidence/screenplay-scope-Vs4xId/`.

Failed attempts are retained separately: `screenplay-scope-6rlCRd` is the
baseline probe; `screenplay-scope-N9ziK8` failed to compile a test calling a
protected character serializer; `screenplay-scope-RvNCJl` exposed a missing
second capitalized match in a punctuation fixture; `screenplay-scope-NjdNSa`
exposed sentence auto-capitalization in the typed fixture; `screenplay-scope-wNUe22`
and `screenplay-scope-D8JYtH` exposed noncanonical font/color metadata from a
test that set model formatting directly. The final fixture applies emphasis
and review comments through the live editor, verifies the intended ranges
before renaming, and retains exact comparisons after renaming and Undo/Redo.

Limits: Linux debug and in-process offscreen evidence, not a new visible desktop
or private Xvfb run. Detached windows, other platforms, release builds and the
owner's projects were not exercised. Capitalized ordinary words and names
inside longer running-text names remain ambiguous and require the writer's
choice. Act/sequence titles remain excluded. Script Undo still leaves the
character document's new name in place, as documented above.

## Optional last-story startup

Implemented 2026-10-12, starting from clean `dev` at `e190c63f`.
Application settings, under User interface, now offer **Open the last story on
startup**, disabled by default. `application/open-last-story` and
`application/last-story-path` use the existing application settings storage,
including its encoded keys and ordinary sync/close persistence.

The remembered path is the absolute source path of the most recently
successfully opened or created local story, even while the option is disabled.
A successful Save As opens its new target and records that target. Fountain/FDX
stories record their source path, never the temporary internal shadow database.
Missing, unreadable, unsupported, malformed, corrupt, newer-version or refused
locked files do not replace a valid remembered path. The remembered path stays
stored when a file is unavailable, allowing one attempt on a later startup.

Initialization still loads the story list before choosing onboarding or normal
content. Startup opening is queued after initialization, with one consumed
decision per application manager. File requests arriving before or during that
initialization are buffered; an explicit request takes precedence over automatic
reopening and never falls back if it fails. Repeated callbacks/`exec` calls and
duplicate file events do not load another copy. First-run onboarding remains
visible and completes before a pending explicit request opens. Automatic
reopening applies only to profiles already configured when startup began.

Automatic opening shares the ordinary coordinator, including lock refusal and
the existing Ignore and open choice, read-only editing and project loading.
It verifies source availability before creating a shadow database, checks the
existing newer-version policy, and treats failed imports/database loads as
failures. A failed automatic attempt leaves the story list usable. Project,
backup and recovery storage policies are unchanged. Toggling the checkbox only
persists a preference for startup; it does not switch the current story.
The checkbox participates in the existing settings theme, and programmatic
population blocks its change signal.

Validation:

- `./build/dev.sh doctor`: exit 0; no desktop display inherited. No optional
  private modules or translation tools were installed.
- `JOBS=4 ./build/dev.sh build debug`: exit 0, including the final checkbox
  theme correction. Existing warnings in unchanged code remain.
- `./tools/check-startup-last-story.sh`: exit 0, 25 process launches using the
  real `Application`/`ApplicationManager`, isolated `STARC_PROFILE_DIR` profiles
  and disposable stories. Covers defaults and disk persistence, successful
  creation/open/Save As and failed operations, disabled/enabled/explicit/unavailable
  successive launches, early/queued/duplicate file events, repeated startup,
  onboarding and a pending explicit file, held locks, read-only editor policy,
  Fountain/FDX source paths and later automatic source imports. Every process
  completed ordinary shutdown with exit 0 and released its owned story lock.
  Reopened `.starc` script XML is byte-exact, and the live editor model retains
  Unicode text, emphasis and the review comment. Rejected corrupt, unsupported,
  malformed and newer-version sources retain their SHA-256 hashes.
- `python3 .build/evidence/startup-last-story-2026-10-12/visible_check.py`:
  exit 0, the same 25 cases on an automatically allocated private Xvfb display
  with `STARC_STARTUP_PLATFORM=xcb STARC_STARTUP_SCREENSHOTS=1`. Native window
  screenshots were inspected; the checkbox label and checked state render
  correctly. Qt signals/dialog responses drive this regression. Reimported
  sources can acquire pending editor changes after rendering; the existing
  ordinary-close Save question is answered through its dialog.
- `python3 .build/evidence/startup-last-story-2026-10-12/binary_check.py`:
  exit 0, nine launches of the production executable through
  `STARC_PROFILE_DIR=<disposable profile> ./build/dev.sh run debug [file]` on
  private Xvfb. Two successive enabled, disabled, explicit-override and
  unavailable-story launches, plus a failed explicit request, had the expected
  screen/open count. All nine Ctrl+Q exits were 0, with no fallback on failure,
  no missing-file creation and exact saved script/annotation bytes.
- `./tools/check-screenplay-interchange.sh`: exit 0 on the final file-opening
  implementation, including independent Fountain/FDX syntax, XML, styles,
  serialization/save/reopen and fixture hashes.
- `./tools/check-screenplay-scope.sh`: exit 0 after the final implementation,
  including keyboard editing, Undo/Redo, pagination, exports, retained project
  data and the existing character-rename regressions.
- Changed-code formatting with `src/.clang-format`, Bash syntax, ShellCheck
  and `git diff --check`: passed.

Evidence: `.build/evidence/startup-last-story-2026-10-12/` retains doctor/build
logs, all attempted regression logs and the native orchestration/capture tools.
Final focused offscreen: `.build/evidence/startup-last-story-X1jkwa/`;
final native matrix/screenshots: `.build/evidence/startup-last-story-1wjM4o/`;
final production launches: the `binary-launches-fi21xv9o/` directory under the
main evidence root. Final full scope: `.build/evidence/screenplay-scope-nLPjA6/`;
interchange: `.build/evidence/screenplay-interchange-ANHQIv/`.

Earlier failed probes remain separate: missing enum includes/incorrect widget
class names in compile probes; a configured fixture with no saved geometry
(`hsaJJt`, `MYoOcu`); a new-project query before the first save (`f68ecJ`);
an assertion during the story-list animation (`P2NZrE`); source XML queried
before serialization (`E0rUV1`); Save As selection while the filename field had
focus (`CnQ18U`, `uOCqTV`); a newer-version fixture whose UPDATE affected no row
(`NODnkG`); and native screenshot/close probes before accounting for the active
page and existing source Save question (`icXPUc`, `EECQZJ`). These names refer
to `startup-last-story-*` directories under `.build/evidence/`. The Save As
probe now clears filename focus and verifies the selected target before
acceptance; it does not use repeated acceptance or slower input to hide the
problem. Native screenshot inspection also caught the initial missing checkbox
theme registration; the final regression checks its text/background colors.

Limits: Linux debug, offscreen and private Xvfb evidence; no physical desktop/GPU,
macOS OS-level file events, Windows, release build or real user projects were
exercised. The native profiles deliberately have no D-Bus/keyring provider,
so expected GitHub credential warnings can appear in screenshots; no credential
or remote repository was used. Fountain/FDX retain their existing interchange
limits, including source-format annotation losses documented above. This item
changes neither those formats nor recovery/backup policy.

## Stories are native projects; source files are import/export only

Implemented 2026-10-13, starting from clean `dev` at `9d2b49e8`.
This supersedes the source-path and shadow-project behavior recorded in the
optional last-story startup section above. `master` remains the frozen reference.

A story is always a `.starc` project. Save, autosave and Save on close write only
its SQLite project. Fountain, TXT, FDX and FDXT now take the same direct-open route
as DOCX, PDF, Markdown and the other importable formats: **Import .X as a .STARC
story** chooses a new native destination before the current story is closed.
Cancel keeps the current story, its exact unsaved model, dirty flag, lock and
Undo/Redo history. Existing, locked, symlink and otherwise refused destinations
keep the current story and existing destination intact.

Startup file arguments, actual application file-open events, Open story, story
cards and their context action share the coordinator. Old source entries start
that import on an explicit click. Automatic last-story startup opens native
projects only; an old source path leaves the story list visible without a chooser
or source lock. The old setting remains stored until a successful native open
replaces it. Dropped files on the project navigator use the existing Import
dialog inside the current story. Unsupported suffixes are refused before
closing the current story or taking a source lock. The create-story dialog still
accepts an import input; native story copying continues to use its safe copy route.

The temporary shadow database, separate story/real paths, source-write branch and
its retry/error dialog, source-save convenience export API and suffix predicate,
keep-format question, Switch to .STARC conversion, shadow Save As branch and
shadow-file cleanup have been removed. Atomic temporary files used to publish a
new native story or Save As remain necessary. Explicit exporters and clipboard
Fountain output retain their separate temporary files and atomic publication.
The first-script model accessor remains used by native backup/scope fixtures;
it no longer participates in a source-save route.

Story types were serialized as JSON integers. Invalid remains 0, Local remains 1
and Cloud remains 3; value 2 is reserved for the retired LocalShadow. The loader
maps old value 2 to Local, while dispatching its source path through the import
chooser. The obsolete `can_ask` preference is ignored and omitted on list save.
The scope check compares all cached team/cloud fields exactly, apart from that
explicitly retired preference, and pins Cloud's original numeric value.

Open/create guards no longer mark a live story saved merely because Don't save
was chosen before a chooser. The Open story chooser returns its selected path
to the already-approved callback, avoiding a second save guard while the first
question disappears. Story-card and application file events still use their
single ordinary unsaved-edit guard. This is needed to keep cancellation truthful.

The two source-save checks are retired:

- `check-fountain-review-source.sh` / `fountain_review_source_test.cpp` become
  `check-fountain-export.sh` / `fountain_export_test.cpp`: real-editor typed text,
  exact styles/review spans, canonical forced Action/literal syntax, native Save,
  explicit Fountain Export, full/partial clipboard, ordinary close and a fresh
  application import. Repeated explicit exports are byte-identical.
- `check-source-export-failure.sh` / its test and fault adapter become
  `check-explicit-export-failure.sh` / `explicit_export_failure_test.cpp` and
  `explicit_export_failure_fault.cpp`: explicit Export UI and actual exporters,
  existing-output preservation, refused/short writes, failed publication,
  recovery, clipboard failure/recovery and actual editor paste. Each output
  format has its own bounded process, rather than sharing one shutdown watchdog.

The interchange manager controls now use the explicit Export dialog, including
FDX format dispatch and literal Fountain output. Startup destination-safety
cases now use import creation, including a renamed `import_publish_collision`
interposer; native story-copy controls remain. `check-import-only-sources.sh`
covers all four newly import-only suffixes, uppercase forms, read-only inputs,
menu/card/event/startup routes, onboarding, competing events, cancellation,
Import/drop, source bytes/no source lock at open/edit/Save/close and native
fresh-process content/UUID checks. Source fixtures deliberately retain BOM,
CRLF, DOCX private parts and FDX metadata that an export would replace.
Application source/export fixtures inject asynchronous in-memory credentials;
no native keyring is used. Private Xvfb checks are additional injected/native
window evidence, separate from physical desktop and production-binary input.

Validation commands and retained failures are recorded below. Issue 15 remained
open at that point: its current runnable command imports into a chosen native project and
still reproduced the same first-edit Undo XML assertion. No resolved issue entry
was rewritten, and no other open issue was undertaken. Issue 15 has since been
fixed; its cause, fix and evidence are in `archive/audit-2-2026-10-13.md`.

Validation (all files/profiles disposable under `.build/evidence/`):

```bash
./build/dev.sh doctor
(cd .build/debug && make qmake_all)
JOBS=4 ./build/dev.sh build debug
./tools/check-import-only-sources.sh
./tools/check-startup-last-story.sh
./tools/check-fountain-export.sh
./tools/check-explicit-export-failure.sh
./tools/check-screenplay-interchange.sh
./tools/check-screenplay-scope.sh
./tools/check-save-as-safety.sh
./tools/check-save-failure.sh
bash -n tools/check-import-only-sources.sh tools/check-startup-last-story.sh tools/check-fountain-export.sh tools/check-explicit-export-failure.sh
shellcheck tools/check-import-only-sources.sh tools/check-startup-last-story.sh tools/check-fountain-export.sh tools/check-explicit-export-failure.sh
git diff --check
```

- Doctor, qmake regeneration and debug builds: exit 0. Qt 6.12.0 and the already
  prepared local QtKeychain dependency were used; optional `lrelease` remains
  absent. Final build log: `source-mode-build-verified.log`.
- Import-only matrix: exit 0, 53 cases / 157 ordinary process exits, then the
  independent Python source-hash, archive, exact screenplay/UUID and lock oracle.
  Final `source-mode-import-final-stable.log` points to
  `import-only-sources-C9gsy1/`. The separately selected six dirty cancellation
  cases also passed in `import-only-sources-NwXoZ3/`.
- Startup: exit 0, 37 ordinary process exits; `startup-last-story-cqFj6o/` and
  `source-mode-startup-final.log` include old source settings, native reopen,
  import publication/collision safety and retained native story-copy controls.
- Fountain editor/export/fresh import: exit 0, ten ordinary process exits plus
  independent canonical syntax/style/review checks, `fountain-export-HWJbfc/`.
- Explicit export and clipboard failures: exit 0, 28 ordinary process exits,
  `explicit-export-failure-CI032o/`. Every selected failure must report refusal
  and preserve an existing output; successful retry and paste are separate controls.
- Interchange: exit 0, including independent Python output/fixture hashes and
  separate-process XML/UUID/annotation reopening, `screenplay-interchange-7gOKmf/`.
- Full screenplay scope: exit 0, `screenplay-scope-OW3t4B/`; native Save As:
  exit 0, `save-as-safety-GhTgVV/`; Save/close/file-event/lock failures:
  exit 0, `save-failure-O5hY7q/`. The latter requires its authorized disposable
  localhost server outside the socket-restricting sandbox. Their final logs are
  `source-mode-scope-final.log`, `source-mode-save-as-final.log` and
  `source-mode-save-failure-final.log`.
- Changed-code formatting uses `src/.clang-format`; Bash syntax, ShellCheck and
  whitespace checks pass. Dynamic symbol inspection of the final core plugin
  finds only the explicit vector/index `ExportManager::exportDocument` overload;
  the source-path overload, predicate, switch preference and real-path APIs are absent.

Additional native checks:

```bash
python3 .build/evidence/source-mode-native/check.py
python3 .build/evidence/source-mode-cli-complete/check.py
```

Both exited 0 on private, automatically allocated Xvfb displays with TCP
listening disabled. The first runs twelve source routes / 36 ordinary process
exits with `STARC_IMPORT_ONLY_PLATFORM=xcb STARC_IMPORT_ONLY_SCREENSHOTS=1`;
`import-only-sources-EsVr7R/` retains the images and complete byte/XML/lock
oracles. Chooser and saved-story screenshots were inspected; the fixture's saved window
shows an expanded navigator, so those images do not establish editor layout or
physical keyboard behavior. Real editor actions and persisted content are
checked separately. The second runs
the actual production command `STARC_PROFILE_DIR=<disposable profile>
./build/dev.sh run debug <source>` for each of Fountain, TXT, FDX and FDXT,
accepting and canceling the destination chooser in separate processes.
All eight exits were ordinary status 0. Accepted cases verify native SQLite
screenplay content after Ctrl+S; their ordinary Save-on-close question is answered
through the visible Save button. Source bytes and source/native lock release
are checked again after close. `source-mode-cli-complete/manifest.json` retains
the exact argument arrays, hashes, exit results and Save-on-close outcomes.
These production launches inject only an in-memory credential backend via a
test preload library; native keyring integration is not credited.

Negative controls and retained failures:

```bash
STARC_IMPORT_ONLY_BASELINE=1 STARC_IMPORT_ONLY_CASES=fountain-event ./tools/check-import-only-sources.sh
STARC_IMPORT_ONLY_BASELINE=1 STARC_IMPORT_ONLY_CASES=fdx-event ./tools/check-import-only-sources.sh
STARC_IMPORT_ONLY_BASELINE=1 STARC_IMPORT_ONLY_CASES=fountain-cancel ./tools/check-import-only-sources.sh
STARC_IMPORT_ONLY_BASELINE=1 STARC_IMPORT_ONLY_CASES=fountain-automatic ./tools/check-import-only-sources.sh
STARC_IMPORT_ONLY_BASELINE=1 STARC_IMPORT_ONLY_OUTPUT="$PWD/.build/evidence/source-mode-baseline-build/output" STARC_IMPORT_ONLY_CASES='fountain-event txt-event fdx-event fdxt-event fountain-cancel txt-cancel fdx-cancel fdxt-cancel fountain-startup fdx-startup fountain-menu fdx-menu fountain-list fdx-list fountain-automatic txt-automatic fdx-automatic fdxt-automatic' ./tools/check-import-only-sources.sh
STARC_IMPORT_ONLY_CASES='fountain-dirty-cancel fdx-dirty-cancel fountain-menu-dirty-cancel fdx-menu-dirty-cancel fountain-list-dirty-cancel fdx-list-dirty-cancel' ./tools/check-import-only-sources.sh
STARC_FOUNTAIN_EXPORT_MODES=cold-undo ./tools/check-fountain-export.sh
```

The unchanged-coordinator controls deliberately exit 1. Its four original
controller/model objects from `9d2b49e8` are rebuilt in a separate core plugin,
using the current unchanged dependencies; the exact archive/compiler/linker
argument arrays are retained in `source-mode-baseline-build/commands.json`.
The final 18-case control is `source-mode-baseline-final-stable.log` /
`import-only-sources-x4S0Th/`. For all four suffixes, the new source-byte/no-lock
assertions fail at open, edit, Save and close; native-path/native-only Save,
cancellation and automatic-startup assertions fail too. An independent extraction
checks every named failure in `source-mode-baseline-assertions.json`. Startup,
menu and legacy card routes also fail their native-copy contracts.

The pre-guard-fix dirty cancellation case fails its exact dirty-state assertion
in `source-mode-dirty-cancel.log`; the subsequent Open-menu probe exposed its
second guard (`source-mode-menu-cancel-diagnostic.log`). The six final cases pass
without clearing the model, dirty flag or Undo/Redo history. The separate cold
Undo command still exited 1 then because its child aborts at `TextModel::applyPatch`,
line 2887, with `newContent == toXml()`. `source-mode-cold-undo-final.log` and
`fountain-export` case evidence retain the failure; this was the then-open issue
15, not a passing export check or a new source-mode regression. After the issue 15
fix the same command exits 0 (see `archive/audit-2-2026-10-13.md` issue 15).

Development failures remain distinct from those semantic controls. Early test
compilations rejected a mis-scoped baseline return expression and a missing
`QAbstractItemModel` include. Early export probes sampled queued dialogs before
their controls were visible; a standalone host also needed to be shown. The
first baseline relink included stale unused objects; the corrected link uses
qmake's actual object list. A fixed 64-byte fault did not reject a shorter
FDX-derived Fountain output; the final injector rejects the second half of the
actual staged output. An aggregate multi-format test exhausted its one watchdog;
format cases now have separate processes with the original watchdog unchanged.
An early immediate-close probe ran while the previous save question was still
being destroyed; final tests observe that lifecycle before requesting close.
The first scope oracle expected the deliberately removed `can_ask` field.

Two running shell suites were edited during development and failed parsing, and
an overlapping relink caused one launch to report a temporarily short library.
Those attempts are not credited as passes; their individual case directories
and the corresponding `source-mode-*.log` records remain. The first native
wrapper log was subsequently replaced, so its shell-tail parse error is recorded
here; its disposable case artifacts remain. Final verification used stable
scripts and a completed build. Initial Xvfb/localhost attempts were refused by
the sandbox's socket policy and were rerun with authorized local sockets.
The optional CLI driver first lacked its local library path, then failed to
answer the observed normal close question; its forced teardown is not ordinary
shutdown evidence. A capture helper lacked Pillow. The final driver uses the
installed ffmpeg capture tool and answers that visible question once. All CLI
attempt directories and their logs remain separate.

Physical desktop/GPU, release builds/packaging, other operating systems,
third-party Fountain/Final Draft clients, every importable format's content
fidelity and crash/power-loss durability remain unexercised. Import/export's
existing normalization and unsupported semantics are unchanged. The source
preservation claim is backed by real coordinator, editor, filesystem and native
window/production-launch checks on the bounded disposable fixtures above.
