# Working in this fork

This is the user's personal Story Architect fork, focused on screenwriting.
Keep the existing application's useful behavior while making requested changes.
Other writing formats are outside the current product focus, but shared code must
continue to build and preserve project data.

## Branches and scope

- `master` is a frozen upstream reference. Never commit, merge, rebase, reset,
  delete, or push updates to it. Read it with `git show master:path` and compare
  with `git diff master -- path`.
- `dev` is the user's working version. Use it for normal development; optional
  task branches start from `dev` and return to `dev`, never to `master`.
- Start with `git status --short --branch`. Preserve unrelated changes and avoid
  broad staging, generated files, destructive cleanup, or unsolicited rewrites.
- Changes to the frozen reference or upstream synchronization require a direct
  user instruction. Upstream behavior is reference evidence, not proof of being
  bug free.
- Do not run upstream publication, release, upload, or notification workflows
  as part of ordinary local development.

## Start here

- [DEVELOPING.md](DEVELOPING.md): setup, build, run, diagnostics, and Git guards.
- [docs/CODEBASE.md](docs/CODEBASE.md): screenplay code routes and shared layers.
- [docs/PROJECT.md](docs/PROJECT.md): fork intent, baseline, and setup evidence.
- The `.pro` files are the build authority. This is C++17/Qt/qmake, not CMake.
  Match existing code and `src/.clang-format`; format only changed code.

## Documentation and discovery

Use Context7 MCP for current library, framework, SDK, API, CLI, or cloud-service
documentation, including setup, configuration, migrations, and library-specific
debugging. Start with `resolve-library-id` (unless given an exact `/org/project`
ID), select the best official/relevant match, then use `query-docs` with a
specific concept. Prefer it to web search. It is unnecessary for business logic,
general programming, code review, refactoring, or scripts written from scratch.

For structural code discovery, use the installed `codebase-memory` skill:
check project/index status, search symbols, trace relationships, and read exact
snippets. Check index coverage for every path relied on; verify missing, stale,
or partial source directly. Graph coverage is advisory, particularly around Qt
macros. Use `rg` for literals, configs, docs, and source fallback. If the tools
are unavailable, say so and inspect source directly.

## Development and evidence

- Run `./build/dev.sh doctor` before the first build on a machine. Initialize
  only the two public PDF submodules; private modules are optional and must not
  be fetched indiscriminately.
- Use `./build/dev.sh build debug` and `./build/dev.sh run debug`. Keep debug
  and release artifacts separate. Use `JOBS` to control memory pressure.
- For behavior changes, trace ownership before editing. Keyboard editing,
  selection, Undo/Redo, pagination, saving, reopening, and interchange formats
  are material screenwriting behavior; retain source data and recovery paths.
- Validate in proportion to the change: syntax checks for tooling, a relevant
  build for C++, and a focused behavior check where needed. Add meaningful tests
  when justified; `testapp` is not an automated regression suite. Record exact
  commands, outcomes, and any checks you could not perform.
- Use disposable projects and the isolated development profile described in
  DEVELOPING.md for runtime experiments. Do not experiment on the user's work.
- A build or offscreen startup does not prove visible GUI behavior, ordinary
  shutdown, or data round trips. Retain failed logs and report their limits.
- Finish with a concise description of changes, evidence, remaining blockers,
  and the Git state. Do not claim setup is complete while required dependencies
  or the application build remain unverified.
