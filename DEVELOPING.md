# Developing the personal Story Architect fork

Use `dev` for this screenwriting version. `master` stays frozen as an upstream
reference. [AGENTS.md](AGENTS.md) describes the working rules and
[docs/CODEBASE.md](docs/CODEBASE.md) maps the code.

## Arch Linux setup

This checkout uses C++17, Qt 6, qmake, GNU Make, and public PDF libraries.
The GitHub credential adapter also requires QtKeychain for Qt 6 (`qtkeychain-qt6`
on Arch), linked by `src/corelib/corelib.pro`. A native Secret Service or KWallet
provider must be available and unlocked at runtime to store new credentials.
The adapter disables plaintext fallback; local editing remains available when
credential storage fails. The fork has no startup telemetry or crash-report uploader. Qt's private
GUI/Widgets headers must match the installed Qt version.

After your normal full Arch system upgrade, run from this directory:

```bash
./build/setup_arch.sh
./tools/install-git-guards.sh
./build/dev.sh doctor
./build/dev.sh compdb debug
./build/dev.sh run debug
```

The installer requests sudo to install missing packages and initializes only
`pdfhummus` and `pdftextextraction`, at the commits recorded by this repository.
It does not build the application. It uses existing package databases without
refreshing them; complete a full Arch upgrade first if they need refreshing.
Avoid partial upgrades. The script includes compiler/debugging tools, Qt
development utilities. Compilation database generation uses Python and Make,
which are already required; no editor-specific package is needed.

For an already prepared machine, start with `./build/dev.sh doctor`.
Private cloud/commercial submodules are not needed for this public build; do not
initialize all submodules indiscriminately.

## Build and run

```bash
./build/dev.sh build debug
./build/dev.sh run debug
# When a release configuration is actually needed:
./build/dev.sh build release
./build/dev.sh run release
```

The helpers work from any current directory. Generated files and output live
under `.build/debug/` or `.build/release/`; each executable is
`_build/starcapp` with its libraries and plugins alongside it. Use `JOBS=2`
for a machine with less memory; the default is four jobs. `QMAKE` is overridable.
The current helper targets Linux and Qt 6; upstream README/CI cover other hosts.
Read the `.pro` files before adapting the workflow to another platform.
The Linux helper enables the bundled TIFF library's `unistd.h` declarations via
`HAVE_UNISTD_H`; its upstream qmake project currently enables them only on macOS.
After deleting source or header files, run `make qmake_all` in `.build/<mode>`
before the next build: the generated child Makefiles keep the old header lists
and stop with "No rule to make target" until they are regenerated. A plugin that
is no longer built also leaves its library in `_build/plugins/`; remove it there.
This supplies the declarations required by modern C compilers without modifying
the pinned PDF submodule checkout.

An unprivileged QtKeychain installation can be selected with
`QTKEYCHAIN_PREFIX=/absolute/prefix` for `doctor`, `build`, and `run`. The prefix
must contain `include/qt6keychain/keychain.h` and `lib/libqt6keychain.so`. qmake
adds its include/library paths and a development runtime search path. Prefer the
system package for normal setup; a development prefix is not a system install.
If the system package is absent and `.build/keyring-deps/usr` contains a prepared
development library, the helper selects that prefix automatically and reports it
in `doctor`. Direct qmake/Qt Creator builds still need the system installation
or an explicit prefix.

The run helper supplies the local library path and preserves the desktop
environment and caller's working directory, so relative project filenames keep
their meaning. Launch from a terminal inside your desktop session providing
`DISPLAY` or `WAYLAND_DISPLAY`. Do not guess a display number. Qt's XCB backend
also needs `xcb-util-cursor`; `doctor` reports session availability separately
from build dependencies.

By default a development run uses Story Architect's normal application profile.
For experiments, use disposable files and an isolated profile:

```bash
STARC_PROFILE_DIR="$PWD/.build/profile" ./build/dev.sh run debug
```

This sets separate XDG config, data, cache, and default Documents paths,
including the application's logs. It does not turn real
projects into disposable files. An offscreen launch using `QT_QPA_PLATFORM`
proves startup only; GUI behavior and ordinary close need separate checks.

## Editor and debugger

The existing `src/.clang-format` is the formatting authority. Avoid formatting
the whole repository. `.clangd` selects `.build/debug/compile_commands.json`.
Use `./build/dev.sh compdb debug` to build and extract compilation commands
from Make's serial dry run. It works after an incremental build, preserves an
existing database on capture failure, and does not request another compilation
or clean. Recursive Make recipes still execute during a dry run, so this helper
first completes the real build and uses its generated child Makefiles.

Qt Creator is optional: open `src/starc.pro`, select a matching Qt 6 kit, and use
a separate build directory. Do not commit `.pro.user` files. Command-line
debugging of the helper's output:

```bash
LD_LIBRARY_PATH="$PWD/.build/debug/_build" \
  gdb --args .build/debug/_build/starcapp
```

## Git guards and review

`tools/install-git-guards.sh` sets repository-local `core.hooksPath=.githooks`.
It refuses to replace existing active hooks. The hooks reject commits on
`master`, local changes/deletion of its ref, and pushes involving `master`.
These are local accident guards, not server branch protection; each new clone
must install them. They can be removed with
`git config --local --unset core.hooksPath` if the user changes the branch policy.

Use `git show master:path` to inspect original code without switching branches.
Keep changes on `dev` or focused branches based on it. Upstream release CI
contains publishing and notification steps; ordinary development uses these
local commands and must not trigger publication.

Validation should match the change. For tooling, use Bash syntax checks,
ShellCheck, and the relevant command. For C++, build the affected configuration
and exercise the behavior. `src/testapp` is a manual widget demo, not a general
automated test suite. For screenplay editing changes, include relevant keyboard,
Undo/Redo, save/reopen, pagination, or export evidence.

After a debug build, run `./tools/check-writing-session-storage.sh` for the
focused writing-statistics storage regression. It uses an isolated profile and
retains compile/test logs under `.build/evidence/`.

`./tools/check-startup-last-story.sh` checks the optional last-story startup setting
through the real application coordinator and application file events. It covers
default/persisted settings, onboarding, successful creation/open/Save As, failure
and lock handling, source-file paths, annotated script reopening and ordinary
shutdown across successive processes. It uses disposable `STARC_PROFILE_DIR`
profiles and retains all logs under `.build/evidence/`. The default is offscreen;
`STARC_STARTUP_PLATFORM=xcb STARC_STARTUP_SCREENSHOTS=1` on a private Xvfb display
adds native window screenshots. Test actions use Qt signals and dialog responses;
production executable and desktop keyboard checks are separate evidence.
It also checks safe new-story creation and **Switch to .STARC** conversion:
existing destinations, live locks, symlinks, unwritable folders and failed source
copies leave the source open and existing data intact. Successful conversion and
story copying retain screenplay content through save/reopen. A Linux test-only
interposer creates a conflicting destination immediately before publication;
that writer's bytes must survive and the conversion must be refused.

`./tools/check-screenplay-scope.sh` checks the screenplay-only scope: creation
choices and settings offer no retired format, the real screenplay editor handles
typing, Enter/Tab transitions, Undo/Redo and pagination, the Fountain, FDX, DOCX
and PDF exports contain the typed text, and documents of retired formats keep
their exact bytes across save and reopen. It also checks that only editors whose
plugin loads are offered (view switcher, add-document dialog, the view remembered
for a document, its drafts, the split view, the Statistics option of the
information pages, the document types of the import dialog), and that statistics,
mind map and character documents of an existing project survive save and reopen.
It also checks that upstream account, sign-in, subscription, purchase and team
creation controls are absent, local writing tools need no account, and cached
team/project records and historical account labels survive load/save/reload.
World and Worlds are retired too: new projects create neither, remembered World
creation choices fall back to Screenplay, and existing groups (including empty
and duplicate groups), World content, drafts, images and change history survive
save/reopen. Retired items refuse direct and mixed drag MIME data and show the
preserved-content notice.
Notifications navigation, filters and release/credits widgets must also be
absent from the menus. Synthetic release-feed checks retain the update-download
link's platform/Qt filtering and ordering without network access; old notification
preferences stay stored and ignored.
Project cover controls/generation/search and story-card cover thumbnails are
retired. The scope check verifies that stored cover PNG bytes and UUID references
survive name/description editing and save/reopen, absent image references remain
intact, cached poster records survive, and story cards render without covers in
both text directions.
The interface is English only: the check requires `translation_en.qm` to be the
single embedded catalog and to supply singular and plural forms, the settings to
have no Language row, chooser or language callbacks, and an interface language
stored by an earlier version to stay in the settings untouched.
It builds
`tests/screenplay_scope_test.cpp`
against the debug build and runs it offscreen with a disposable project and
profile. It needs `pdftotext` (poppler) and does not show a window.

`./tools/check-screenplay-interchange.sh` adds independent Fountain/FDX fixtures,
ordered paragraph/text/emphasis/number checks, fresh imports, and explicit model
serialization plus project save/reopen. It also checks the exported syntax/XML
with Python, input failures, options, and unchanged fixture hashes. See
[docs/INTERCHANGE.md](docs/INTERCHANGE.md) for exact normalization, intentional
loss and remaining limits. It uses a new disposable profile/project and an
absent private session-bus address so it cannot contact the desktop keyring.

`./tools/check-project-sync.sh` checks the GitHub story sync logic and client
the same way. It starts `tests/github_mock_server.py`, a local stand-in for the
GitHub API, so it needs no account and sends nothing to GitHub. To try the sync
in the running application without touching a real repository, start that server
and launch with `STARC_GITHUB_API_URL=http://127.0.0.1:<port>`; its default
access token is `test-token`.

`./tools/check-save-failure.sh` checks the real editor and application coordinator
when SQLite rejects document writes or a deferred constraint rejects the commit.
It covers Save on close, failed and recovered Retry, edits made while a commit is
pending, dismissing the saving error, Cancel, explicit Don't save, an Open-menu
request, and a refused GitHub replacement download. A successful download control
checks its exact backup. Actual `QFileOpenEvent` cases cover Save, Don't save,
Cancel, clean and same-story requests, competing requests during the save
question (including after manual Save), an external request during the Open-menu
question, and failed writes/commits with Retry recovery. External requests keep
the current editor and lock until the writer approves and any requested save
succeeds. Fresh processes reopen every result and compare script,
emphasis, review annotations and document identities. All files, profiles and
fake credentials are disposable; the GitHub API is a loopback mock server and
the session bus is absent. The default is offscreen; on a private Xvfb display,
`STARC_SAVE_FAILURE_PLATFORM=xcb STARC_SAVE_FAILURE_SCREENSHOTS=1` adds native
window evidence. Actions use Qt signals and dialog responses.

`./tools/check-import-only-sources.sh` checks direct opens of DOCX, PDF and Markdown
through the real application coordinator. Import-only inputs choose a new `.starc`
copy before editing; the original file is left byte-exact. Cases cover file
arguments, actual `QFileOpenEvent` objects, onboarding, cancellation, existing and
locked destinations, read-only inputs, omitted destination suffixes and competing
file events during the chooser. Each case prepares a disposable profile through
onboarding and ordinary close, exercises the open/edit/Save route, then reopens
in a fresh process. Tests compare exact script/emphasis/review XML and document
identities; Python also checks all source hashes, retained DOCX ZIP parts and
released locks. Source-save export controls reject unsupported suffixes while
retaining Fountain/TXT/FDX/FDXT selection. Explicit DOCX/PDF exports remain covered
by the screenplay scope check. The default is offscreen;
`STARC_IMPORT_ONLY_PLATFORM=xcb STARC_IMPORT_ONLY_SCREENSHOTS=1` on private Xvfb
adds native window screenshots. Chooser/editor actions are injected through Qt.

`./tools/check-github-credential-storage.sh` uses injected asynchronous storage,
isolated settings, actual settings/onboarding widgets, and the local mock GitHub
server. It prints a unique evidence directory and its native test command.
This injected check cannot prove native integration. For that separate check:

```bash
# Replace EVIDENCE with the directory printed by the focused check.
QT_QPA_PLATFORM=offscreen python3 tests/isolated_keyring.py EVIDENCE/native -- \
  EVIDENCE/credential-test --native app.starc.test.credentials
QT_QPA_PLATFORM=offscreen python3 tests/isolated_keyring.py EVIDENCE/unavailable \
  --unavailable -- EVIDENCE/credential-test --native-unavailable app.starc.test.unavailable
```

The runner requires `dbus-daemon`, `gdbus`, and `gnome-keyring-daemon` and creates
a private bus with no desktop-service activation, private XDG directories, and
an encrypted disposable keyring. It never contacts the normal desktop bus or
normal keyring. `STARC_TEST_KEYRING_DAEMON` selects an unprivileged daemon binary;
include its dependency directory in `LD_LIBRARY_PATH` if needed. Each destination
must be new: failed logs and profiles are retained. Never pass real credentials.

For a real application check, use `tests/github_credential_ui_driver.py` inside
that private runner and `unshare -Urn`. It allocates Xvfb with `-displayfd`, enables
only the private namespace's loopback for the local mock server, uses xdotool,
and captures PNGs with ffmpeg. Set `STARC_TEST_REPO` to the checkout. It copies
only the geometry/splitter `[application-view]` group from the retained disposable
backup-check profile. Its JSON commands distinguish window and screen coordinates;
wait for credential readiness before entering a token. Quit with its `quit`
command (Ctrl+Q) and retain the actual exit result before `stop` tears down Xvfb.

Development profiles (`STARC_PROFILE_DIR`) use a path-derived credential service
separate from the ordinary profile's `app.starc.screenplay.github` service. An
isolated profile alone does not isolate the native keyring provider: use the
private-bus runner for experiments. Settings resets retain credentials; clear
the token explicitly through the GitHub settings controls.

Current setup results and any remaining machine-specific work are recorded in
[docs/PROJECT.md](docs/PROJECT.md).
