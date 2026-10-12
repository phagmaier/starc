# Developing the personal Story Architect fork

Use `dev` for this screenwriting version. `master` stays frozen as an upstream
reference. [AGENTS.md](AGENTS.md) describes the working rules and
[docs/CODEBASE.md](docs/CODEBASE.md) maps the code.

## Arch Linux setup

This checkout uses C++17, Qt 6, qmake, GNU Make, and public PDF libraries. The
fork has no startup telemetry or crash-report uploader. Qt's private
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
This supplies the declarations required by modern C compilers without modifying
the pinned PDF submodule checkout.

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

`./tools/check-screenplay-scope.sh` checks the screenplay-only scope: creation
choices and settings offer no retired format, the real screenplay editor handles
typing, Enter/Tab transitions, Undo/Redo and pagination, the Fountain, FDX, DOCX
and PDF exports contain the typed text, and documents of retired formats keep
their exact bytes across save and reopen. It also checks that only editors whose
plugin loads are offered (view switcher, add-document dialog, the view remembered
for a document, its drafts, the split view), and that statistics, mind map and
character documents of an existing project survive save and reopen. It builds
`tests/screenplay_scope_test.cpp`
against the debug build and runs it offscreen with a disposable project and
profile. It needs `pdftotext` (poppler) and does not show a window.

`./tools/check-project-sync.sh` checks the GitHub story sync logic and client
the same way. It starts `tests/github_mock_server.py`, a local stand-in for the
GitHub API, so it needs no account and sends nothing to GitHub. To try the sync
in the running application without touching a real repository, start that server
and launch with `STARC_GITHUB_API_URL=http://127.0.0.1:<port>`; its default
access token is `test-token`.

Current setup results and any remaining machine-specific work are recorded in
[docs/PROJECT.md](docs/PROJECT.md).
