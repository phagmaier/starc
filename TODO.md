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
- [x] **Tweet-sharing buttons** in the logline generator and the writing sprint
  panel.
  - Done 2026-10-12: removed both tweet handlers and the sprint share widget.
    The logline result has no title action; Back retains the help toggle. Local
    logline generation/Done and sprint results/Restart remain available.
  - Debug build, screenplay regression and private Xvfb logline/sprint checks
    passed, including a real one-minute countdown and exact document content
    across save/reopen. Both ordinary Ctrl+Q exits were 0. See `docs/PROJECT.md`.
- [x] **"User guide" link** on the story list. It opens upstream's help site,
  which documents features this fork removed.
  - Done 2026-10-12: removed the button, its signal and the localized upstream
    help-site handler. Create story, Open story and Sync with GitHub remain.
  - Debug build, screenplay scope and writing-session regressions passed.
    Private Xvfb confirmed the story list, Create and GitHub setup dialogs;
    ordinary Ctrl+Q exited 0. Evidence and limits: `docs/PROJECT.md`.
- [x] **Cover image search through upstream.** Queries go to `starc.app` and to
  Google Translate (`unsplash_images_view.cpp`, `cover_generator_sidebar.cpp`).
  cut the search and cover images.
  - Done 2026-10-12: removed project cover controls/generator, upstream search,
    its unused Google Translate request helper and story-card cover thumbnails.
    Existing cover references and image bytes remain stored unchanged.
  - Debug build, screenplay/storage regressions and private Xvfb information
    editing/save/reopen/story-card checks passed; both process exits were 0.
    Missing-image references also survive open/save. See `docs/PROJECT.md`.
- [x] **Font downloads from `starc.app`** for missing fonts
  (`application_manager.cpp`). I only need english.
  - Done 2026-10-12: removed automatic language/emoji font downloads, their
    startup/language-change hooks and progress task. Bundled interface/screenplay
    fonts, installed fonts and loading of existing local cached fonts remain.
  - Debug build, screenplay/storage regressions and isolated startup font checks
    passed. Missing emoji/language fonts trigger no download; cached font bytes
    survive. All three normal closes exited 0. Evidence/limits: `docs/PROJECT.md`.
- [x] **Novel and screenplay series.** Decided 2026-10-11: this fork is for
  screenplays only. Both are retired like the earlier formats: nothing creates,
  imports, exports or configures them, their source is deleted, and documents
  an existing project already has are kept untouched and shown as unsupported.
  The episodes of an old series are ordinary screenplays and still open.
- [x] **Remove Worlds.** Only the navigator plugin is in
  this checkout, so a world can be listed but has no editor. Existing project
  data has to be kept either way, as with the retired formats.
  - Done 2026-10-12: retired World/Worlds creation, models, exports and plugins;
    new projects have no Worlds group. Existing items show the preserved-content
    notice. IDs, MIME mappings, raw content, drafts, attachments and history stay.
    Retired Worlds records are excluded from destructive duplicate cleanup.
  - Debug build, screenplay/interchange/storage regressions and private Xvfb
    open/reopen/notice/add-dialog checks passed. Both normal exits were 0; all 67
    document hashes and 16 change-history entries stayed intact. See `docs/PROJECT.md`.
- [x] **Translation catalogs.** 33 files in `src/core/translations/`, including
  the English source. We only need the english language.
  - Done 2026-10-12: removed the 31 other languages (`.ts` and `.qm`), the
    untranslated `_en_source.ts`, the Language row and dialog of the settings and
    the language-id helper. The interface is always English and left to right;
    the system language is not consulted. `translation_en.ts`/`.qm` stay, byte
    for byte as upstream, because they hold the English plural forms ("1 scene").
    A language stored by an earlier version stays in the settings, unread.
  - Debug build, all six regressions and private Xvfb checks passed: a profile
    with Hebrew stored opened in English with correct plurals and no Language
    row; a fresh profile stored no language. Both Ctrl+Q exits were 0.
  - Limit: the English catalog still lists strings of removed features. Pruning
    it needs `lupdate`/`lrelease` (`qt6-tools`, not installed). See
    `docs/PROJECT.md`.

## Add

- [ ] **Characters and Locations.** Either a minimal page (name, description,
  notes) or hide the two groups. Characters are collected from the script, so
  the data stays in both cases.
- [x] **Scene and character report** to stand in for the missing Statistics
  view. Done 2026-10-11: our own plugin shows the six public reports as tables
  and the two public plots. Reports are recomputed when Statistics is selected;
  Refresh updates a view kept open beside the script. No new dependencies.
- [ ] **Open the last story on startup**, as an option.
