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
