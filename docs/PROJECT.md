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
  features; they belong to the account UI that is listed for removal in
  `TODO.md`.
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
