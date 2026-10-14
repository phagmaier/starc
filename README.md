<div align="center">

<img src="./img/starc.png" width="220" alt="Story Architect logo">

# Story Architect — personal screenplay fork

### Your stories. Your way.

[![License: GPL v3](https://img.shields.io/badge/License-GPL_v3-blue.svg)](./LICENSE)

> [!NOTE]
> This is a personal fork for screenwriting. `dev` is the working version;
> `master` is a frozen upstream reference. Upstream links below are reference
> only — for everyday fork use, build and run `dev` per [DEVELOPING.md](DEVELOPING.md).

</div>

---

## About this fork

This repository is a screenplay-only variant of [Story Architect](https://starc.app/)
(upstream `story-apps/starc`), kept as a working writing base with focused
adjustments. See [docs/PROJECT.md](docs/PROJECT.md) for intent and setup
evidence, [docs/CODEBASE.md](docs/CODEBASE.md) for the screenplay code routes,
[DEVELOPING.md](DEVELOPING.md) for build/run, and [docs/WORKFLOW.md](docs/WORKFLOW.md)
plus [AGENTS.md](AGENTS.md) for the working rules.

## Current state

- **Screenplay editor** — typing, Enter/Tab transitions, Undo/Redo, pagination,
  drafts, comments/review marks, and information pages. Simple text documents
  remain; character/location pages keep name, description, and notes.
- **Local statistics** — fork-owned replacement plugin with summary, scene,
  location, cast, dialogue, and gender reports plus structure/activity plots.
  Read-only; no report export or filters.
- **Native `.starc` workflow** — every story is a `.starc` project. Fountain,
  FDX/FDXT, TXT, Markdown, DOCX, PDF, RTF, ODT, Celtx, Trelby, and KIT Scenarist
  sources are import inputs only; export outputs Fountain, FDX, DOCX, and PDF.
  Save writes the native project, never the source. See
  [docs/INTERCHANGE.md](docs/INTERCHANGE.md) for the exact conversion/loss policy.
- **Backups** — per-story families in the configured backup folder: seven newest
  snapshots plus daily history over the last 30 local days. Recovery is copying
  a snapshot to a working file and opening it.
- **GitHub story sync** — each local story syncs as one file in a private
  repository root, with tokens in the native keyring (no plaintext fallback).
- **English interface only**; scripts can be written in any language, left to
  right or right to left. No startup telemetry or crash-report uploader.
- **Retired in this fork** — audioplay, stageplay, comic book, novel, series,
  Worlds, AI assistant, upstream account/subscription/team, notifications feed,
  tweet sharing, and the private cloud module. Existing documents of retired
  types are preserved byte-exact and shown with a notice, never loaded or
  rewritten.

## Build and run (this fork)

```bash
./build/dev.sh doctor
JOBS=4 ./build/dev.sh build debug
./build/dev.sh run debug
```

Details, Arch Linux setup, isolated profiles (`STARC_PROFILE_DIR`), Git guards,
and the focused-checks index are in [DEVELOPING.md](DEVELOPING.md). Do not
initialize all submodules; only the two public PDF dependencies are needed:

```bash
git submodule update --init --recursive \
  src/3rd_party/pdfhummus \
  src/3rd_party/pdftextextraction
```

Debug and release artifacts live under `.build/debug/` and `.build/release/`
respectively; the executable is `_build/starcapp`.

## Upstream reference

- Website/download/docs: [starc.app](https://starc.app/) ·
  [download](https://starc.app/download) · [help](https://starc.app/help) ·
  [blog](https://starc.app/blog/).
- Upstream builds: Windows installer/portable, macOS DMG, Linux AppImage.
- Upstream issues/translations/support/pricing: [issue
  tracker](https://github.com/story-apps/starc/issues) ·
  [Transifex](https://www.transifex.com/story-apps/starc/) ·
  [support@starc.app](mailto:support@starc.app) ·
  [premium plans](https://starc.app/pricing).
- Upstream contribution and packaging workflows do not apply to this fork.
  Fork work happens directly on `dev` (single agent, no task branches); the
  issue queue is [WORKLIST.md](WORKLIST.md).

## Project structure

```text
starc/
├── build/                  Linux development helpers (dev.sh, setup_arch.sh)
├── img/                    README artwork (logo only; no current UI screenshots)
├── tools/                  Focused regression checks (see DEVELOPING.md index)
└── src/
    ├── 3rd_party/          Bundled and submodule dependencies
    ├── app/                Application entry point
    ├── core/               Main application and feature UI
    ├── corelib/            Domain, data, business, and shared UI layers
    ├── interfaces/         Cross-module interfaces
    ├── testapp/            Manual widget demo (not a regression suite)
    └── starc.pro           Top-level qmake project (C++17/Qt/qmake authority)
```

## License

Story Architect is distributed under the **GNU General Public License v3.0**. See [LICENSE](./LICENSE) for the complete terms.

Third-party components and optional modules may have their own license terms.

---

<div align="center">

**Thank you for using Story Architect — have fun creating!**

</div>
