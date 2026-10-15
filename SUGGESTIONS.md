# Suggestions for the screenplay fork

Reviewed on 2026-10-14, on `dev`.

My opinion: the most worthwhile next changes would make the existing features easier to reach and reuse. The screenplay focus is a good fit for this project. I would keep the current editor and build on its navigation, drafts, comments, statistics, and file workflows.

These are optional ideas, ordered roughly by how useful I think they would be. They are not newly confirmed bugs or additions to the issue queue. This review used the current source and project documentation; I did not launch the application or repeat the runtime checks you have already completed. Effort labels describe relative scope, not implementation estimates.

**My first three picks:** a command/scene launcher, a backup browser, and named export profiles.

**1. A command launcher with quick scene navigation** — high interest; moderate effort.

I would enjoy being able to press one shortcut and type “export,” “compare drafts,” “bookmarks,” or part of a scene heading. A scene result could show its number, heading, and a short preview. Selecting it would jump to that scene and return focus to the editor.

The application already has scene navigation, text search, and many useful actions spread across menus, toolbars, and side panels. This would give them a common entrance. A sensible first version would search existing actions and scene headings, display the current shortcuts, and respect whether an action is available in the current document.

Grounding: [MenuView](src/core/ui/menu_view.cpp), [ScreenplayTextView](src/core/management_layer/plugins/screenplay_text/screenplay_text_view.cpp), and the [screenplay navigator](src/core/management_layer/plugins/screenplay_text_structure/ui/screenplay_text_structure_view.cpp).

**2. A backup browser with “Open recovered copy”** — high interest; moderate effort.

The existing backup retention is useful: recent copies plus daily history. Recovery currently relies on copying a snapshot to a working file and opening it. I would add a small browser for the current story that lists available backups by readable date and time, with file size and an “Open recovered copy” action.

The first version could simply create a separately named native story from the selected backup and open it. That would make recovering yesterday's work much less fiddly. It should identify the correct story's backup family using the existing path identity rather than matching only its display name.

This would also be a good place to show the last successful backup and its folder. The save path already distinguishes a successful story save from the asynchronous backup result; making those two results visible would help the writer understand what is current. Backup failures already have log/task-bar reporting, so this is a refinement of existing feedback.

Grounding: [backup retention and recovery](docs/PROJECT.md#backup-retention-across-writing-sessions), [BackupBuilder](src/corelib/utils/tools/backup_builder.cpp), and [ApplicationManager](src/core/management_layer/application_manager.cpp).

**3. Named export profiles** — high interest; moderate effort.

Export already remembers options and supports draft selection, scene ranges, title pages, notes, and character highlighting. I would add named combinations such as “Clean reading PDF,” “PDF with revision notes,” “Fountain exchange,” and “Selected scenes.”

The benefit would be switching between repeatable outputs without checking the same controls each time. Start by saving the existing options under a name. Show the chosen draft, scene scope, note inclusion, and destination before exporting. A reusable profile should keep its options separate from the last output filename and from a particular project's draft identity.

Grounding: [ScreenplayExportDialog](src/core/ui/export/screenplay_export_dialog.cpp) and [ScriptExportDialog](src/core/ui/export/script_export_dialog.cpp).

**4. Make the local statistics more useful during revision** — high interest; moderate effort.

The replacement statistics view already has a substantial set of reports and plots. I would improve interaction with that data before adding more kinds of analysis: filter by character or location, sort useful numeric columns, copy selected rows, and export the current table as CSV.

My favorite extension would be clicking a scene or dialogue result to open its source in the screenplay. That makes a report useful for acting on a question such as “Where does this character disappear for too long?” The initial implementation could concentrate on one scene table and one dialogue table. Source navigation would need stable scene/document identity where the report currently exposes only display text.

Grounding: the [statistics view](src/core/management_layer/plugins/screenplay_statistics/screenplay_statistics_view.cpp) currently displays read-only tables and plots with report selection and Refresh; the [replacement notes](docs/PROJECT.md#screenplay-statistics-replacement) describe the existing scope.

**5. Named writing and revision workspaces** — medium interest; moderate effort.

The editor already remembers panel visibility, sidebar selection, zoom, and splitter state. Focused paragraphs, typewriter scrolling, scene isolation, and full screen are also available. I would let the writer save a couple of combinations and switch between them quickly.

For example, “Write” could keep the page prominent with minimal panels, while “Revise” could show comments and the scene navigator. These should be presentation preferences, with a predictable way back to the previous layout. I would avoid having a workspace silently change editing permissions or start recording revisions.

Grounding: `loadViewSettings` and `saveViewSettings` in [ScreenplayTextView](src/core/management_layer/plugins/screenplay_text/screenplay_text_view.cpp), plus the existing [editor preferences](src/core/ui/settings/settings_view.cpp).

**6. A more manageable comments list** — medium interest; small to moderate effort.

Comments already support replies, colors, done/undone state, and selection in the script. I would add visible filters for unresolved comments and comments in the current scene, followed by text search if it proves useful.

A “Next unresolved comment” action and a small remaining-count indicator would make a revision pass feel more organized. This can use the existing review marks and resolution state. I would keep it within the comments panel so it remains part of revising the screenplay.

Grounding: [CommentsView](src/corelib/ui/modules/comments/comments_view.cpp) and its connections in [ScreenplayTextView](src/core/management_layer/plugins/screenplay_text/screenplay_text_view.cpp).

**7. Searchable settings and a compact keyboard reference** — medium interest; small to moderate effort.

The settings page offers a lot of useful control, including configurable paragraph shortcuts and Enter/Tab transitions. A search field that takes you to “smart quotes,” “backup,” “scene numbers,” or “typewriter” would make occasional adjustments easier.

I would also like a small keyboard-reference popover that shows the actual configured keys and paragraph transitions. It could be opened from the editor when you forget how a particular paragraph behaves. Both features should derive their labels and bindings from the current settings so customizations stay accurately represented.

Grounding: the existing preferences and shortcut tables in [SettingsView](src/core/ui/settings/settings_view.cpp).

**8. A short, document-specific interchange summary** — medium interest; moderate effort.

The project has careful interchange documentation, including accepted conversions and limits. I would surface the relevant few facts in the import/export flow. For example, exporting a script with Shot paragraphs to Fountain could explain that their text becomes Action; FDX export could identify omitted review comments and sequential output of dual dialogue.

This should be a compact summary of what applies to the chosen document and format. It should appear only when there is something useful to explain. The existing FDX dialog already hides the unsupported review-mark option; a summary could explain the resulting output more clearly. On import, detected unsupported structures could be listed before proceeding, while the source file remains untouched under the current native-project workflow.

Grounding: [INTERCHANGE.md](docs/INTERCHANGE.md), [ScriptExportDialog](src/core/ui/export/script_export_dialog.cpp), and the [native-project/import-only contract](docs/PROJECT.md#stories-are-native-projects-source-files-are-importexport-only).

**9. Extend FDX dual-dialogue support if you exchange those scripts** — conditional interest; significant effort.

One substantive format improvement would be importing native FDX `DualDialogue` containers and exporting simultaneous dialogue with its column ownership intact. The current importer walks direct `Content/Paragraph` elements, and the documented export contract preserves those speeches sequentially.

This is worth considering if exchanging dual-dialogue scripts is part of your workflow. I would treat it as one bounded feature with independent fixtures and native save/reopen coverage. FDX ScriptNotes could be considered separately if you need them; the current review-comment limitation is already documented. I would keep the priority tied to the formats you actually use.

Grounding: [ScreenplayFdxImporter](src/corelib/business_layer/import/screenplay/screenplay_fdx_importer.cpp) and the supported contract and limits in [INTERCHANGE.md](docs/INTERCHANGE.md).

**10. Optional offline scene read-aloud** — a fun addition; significant effort.

If I were choosing one more playful feature, it would be a local read-aloud mode for the current scene or selection. Hearing dialogue can help reveal awkward rhythm or repetition. Basic play, pause, and stop would be enough for a first version, with optional character voice assignments later.

This would be a new capability rather than a refinement I found in the reviewed editor flows. Its value depends on whether you like listening to drafts and on the quality of available local voices. I would keep it optional and let playback follow the script without changing it. It belongs below the smaller workflow improvements in priority.

**11. One convenient entry point for the existing focused checks** — useful for future maintenance; moderate effort.

The repository already has substantial focused checks and a useful command index. I would add a thin runner that selects related checks by area, such as editor/history, persistence/backups, or interchange, and produces one index of their outcomes and retained evidence directories.

It should run the existing oracles, retain failed results, and let the caller choose the relevant group. The smallest version can remain local. If you later want automated verification on `dev`, use a dedicated workflow for that purpose: the inherited Linux workflow currently targets `master` and includes packaging/publication paths. A future `dev` workflow should invoke the fork's documented dependencies and checks, with publication kept under a separate explicit workflow.

This is convenience for the next change, not a request to repeat your completed verification now.

Grounding: [the focused checks index](DEVELOPING.md#focused-checks-index), [tools](tools/), and the inherited [Linux workflow](.github/workflows/build_starc_linux.yml).

I would preserve the existing screenplay focus, native-project ownership, source-file protection, local statistics, draft creation/comparison, and recovery behavior as the foundation for any of these ideas. My preference would be to pick one small improvement you would use often, live with it, and decide whether another is worth adding.
