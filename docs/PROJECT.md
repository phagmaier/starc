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
