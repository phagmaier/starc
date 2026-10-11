# Developing the personal Story Architect fork

Use `dev` for this screenwriting version. `master` stays frozen as an upstream
reference. [AGENTS.md](AGENTS.md) describes the working rules and
[docs/CODEBASE.md](docs/CODEBASE.md) maps the code.

## Arch Linux setup

This checkout uses C++17, Qt 6, qmake, GNU Make, and public PDF libraries. The
application also links Crashpad; it must be built before the application. Qt's
private GUI/Widgets headers must match the installed Qt version.

After your normal full Arch system upgrade, run from this directory:

```bash
./build/setup_arch.sh
./tools/install-git-guards.sh
./build/dev.sh crashpad debug
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

Crashpad's upstream builder downloads Chromium `depot_tools` and dependencies
and tracks their current sources. This is the largest first-time download and
requires network access. Retain its output when diagnosing failures:

```bash
./build/dev.sh crashpad debug > .build-crashpad-debug.log 2>&1
```

The local builder selects GN debug/release explicitly, caps Ninja jobs, and
builds only required Linux targets. A tracked compatibility patch adds the
missing integer-type header to Crashpad when needed on the current toolchain.
It is checked before applying and skipped if the include is already present.

For an already prepared machine, start with `./build/dev.sh doctor`. Missing
release Crashpad is informational when the debug configuration is ready.
Private cloud/commercial submodules are not needed for this public build; do not
initialize all submodules indiscriminately.

## Build and run

```bash
./build/dev.sh build debug
./build/dev.sh run debug
# When a release configuration is actually needed:
./build/dev.sh crashpad release
./build/dev.sh build release
./build/dev.sh run release
```

The helpers work from any current directory. Generated files and output live
under `.build/debug/` or `.build/release/`; each executable is
`_build/starcapp` with its libraries and plugins alongside it. Keep the matching
Crashpad configuration in `bin/crashpad/`. Use `JOBS=2` for a machine with less
memory; the default is four jobs. `QMAKE` and `CRASHPAD_ARCH` are overridable.
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

Current setup results and any remaining machine-specific work are recorded in
[docs/PROJECT.md](docs/PROJECT.md).
