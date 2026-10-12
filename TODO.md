# TODO

Follow-ups from the review of this fork on 2026-10-11. Tick an item when it
lands. If a decision or a limit is worth remembering, add a line under the item.

## Fix

- [x] **Stop offering editors that are not in this checkout.** Cards, Timeline,
  Breakdown, Statistics, character and location pages, Mind map, Image gallery
  and Presentation all end on "editor of this document not implemented yet".
  Start at `PluginsBuilder::editorsInfoFor` in
  `src/core/management_layer/plugins_builder.cpp` and at
  `src/core/ui/project/create_document_dialog.cpp`.
  - Done 2026-10-11. What is offered follows the plugin files in `plugins/`
    (`PluginsBuilder::editorsInfoFor`); a plugin added later shows up after a
    restart.
  - Resolved 2026-10-11: the replacement Statistics view is built. Opening a
    local project adds a missing Statistics item to its screenplays; existing
    items and the visibility choices they hold are kept.
  - Also done 2026-10-11: the "Statistics" checkbox on the screenplay
    information page is hidden while the statistics have no view
    (`IDocumentManager::setDocumentsWithEditors`); the choice it holds is kept.
    The import dialog offers only document types that have an editor, so
    "Presentation" is gone for PDF files, and a type remembered for a file
    falls back to the first one offered.
  - "Split to episodes" went away with the series (see Cut).
- [x] **Keep backups for longer than one writing session.** Every save writes a
  full copy and only the newest 7 are kept, while autosave runs every 3 minutes.
  Keep a few recent copies plus one per day. Start at
  `src/corelib/utils/tools/backup_builder.cpp` and its caller in
  `application_manager.cpp`.
  - Done 2026-10-11: keep seven recent successful copies (more if configured),
    plus the newest snapshot for each of the last 30 local calendar days,
    including today. Overlapping selections share one file. Prune only after
    a complete new backup is published. Source-path identities isolate stories
    with the same filename; legacy and unclassified copies remain untouched.
    Deterministic retention/failure checks and disposable-project restoration
    passed, including an ordinary save and recovery in the real Xvfb application.
- [x] **Store the GitHub token in the system keyring.**
  - Done 2026-10-11: a shared asynchronous QtKeychain store serves onboarding,
    settings and sync, with plaintext fallback disabled. Existing tokens migrate
    without re-entry; an existing keyring entry wins over legacy settings.
    Failed migration preserves the legacy credential and offers retry. Settings
    removal is confirmed on disk, and pending edits are settled or reported on close.
  - Injected failure/ordering and real settings-permission checks, isolated native
    keyring checks, local mock sync, and real Xvfb onboarding/settings/normal-close
    checks passed. See `docs/PROJECT.md` for evidence, dependency setup and limits.
- [x] **Add import and export round-trip checks** for Fountain and FDX.
  - Done 2026-10-11: independent fixtures and application-model construction
    check ordered text/types, emphasis, numbers, multiline text, notes and dual
    dialogue through export/fresh import and serialized project save/reopen.
    Independent syntax/XML and fixture-hash checks pass. Focused fixes retain
    scene numbers, FDX whitespace/parenthetical styles and valid input handling;
    ordinary FDX saves now select FDX instead of overwriting it with Fountain.
  - All requested regressions and private Xvfb save/reopen/normal-close checks
    passed. FDX sequential dual dialogue/comment loss and other format limits
    are explicit in `docs/INTERCHANGE.md`; evidence is in `docs/PROJECT.md`.

## Cut

- [x] **Account, subscription, purchase and team UI**.
  - Done 2026-10-12: removed `AccountManager`, upstream sign-in/account,
    subscription, purchase/gift, session and team-management UI, its integration
    and the embedded upstream API token. Retained project transport widgets were
    moved out of `ui/account`; cached team/project records and historical account
    labels stay intact. Local writing tools need no account; GitHub remains separate.
  - Debug build, all six requested regressions and private Xvfb startup/menu/
    settings/local-create/save/reopen/Statistics/onboarding checks passed. All
    three Ctrl+Q exits were 0. Private cloud/desktop/release limits and retained
    evidence are in `docs/PROJECT.md`.
- [x] **Notifications page** in the menu. Its only feed is the absent cloud
  client.
  - Done 2026-10-12: removed the page/action, filters, release/credits widgets,
    unread menu/toolbar badges and callbacks. `NotificationsManager` retains only
    stable-release metadata for the existing update-download consumer; old page
    preferences remain stored and ignored.
  - Debug build, all six required regressions and private Xvfb startup/menu/
    settings/local-create/edit/save/reopen/writing-tools/GitHub-onboarding checks
    passed; all three Ctrl+Q exits were 0. Evidence and limits: `docs/PROJECT.md`.
- [ ] **Tweet-sharing buttons** in the logline generator and the writing sprint
  panel.
- [ ] **"User guide" link** on the story list. It opens upstream's help site,
  which documents features this fork removed.
- [ ] **Cover image search through upstream.** Queries go to `starc.app` and to
  Google Translate (`unsplash_images_view.cpp`, `cover_generator_sidebar.cpp`).
  Decide: cut the search, or keep it knowingly.
- [ ] **Font downloads from `starc.app`** for missing fonts
  (`application_manager.cpp`). Decide: bundle what is needed, or drop it.
- [x] **Novel and screenplay series.** Decided 2026-10-11: this fork is for
  screenplays only. Both are retired like the earlier formats: nothing creates,
  imports, exports or configures them, their source is deleted, and documents
  an existing project already has are kept untouched and shown as unsupported.
  The episodes of an old series are ordinary screenplays and still open.
- [ ] **Worlds.** Decide whether they stay. Only the navigator plugin is in
  this checkout, so a world can be listed but has no editor. Existing project
  data has to be kept either way, as with the retired formats.
- [ ] **Translation catalogs.** 33 files in `src/core/translations/`, including
  the English source. Decide whether to keep languages other than English.

## Add

- [ ] **Characters and Locations.** Either a minimal page (name, description,
  notes) or hide the two groups. Characters are collected from the script, so
  the data stays in both cases.
- [x] **Scene and character report** to stand in for the missing Statistics
  view. Done 2026-10-11: our own plugin shows the six public reports as tables
  and the two public plots. Reports are recomputed when Statistics is selected;
  Refresh updates a view kept open beside the script. No new dependencies.
- [ ] **Open the last story on startup**, as an option.
