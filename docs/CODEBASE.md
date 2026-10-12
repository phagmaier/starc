# Screenwriting code map

This fork focuses on screenplay workflows. `master` is the preserved upstream reference; `dev` is the working version. Read [AGENTS.md](../AGENTS.md) before changing code and [DEVELOPING.md](../DEVELOPING.md) for setup and build commands.

This is a starting map, not an exhaustive dependency inventory. The routes below were checked against source on 2026-10-11. The installed code graph helps find symbols, but Qt macros and large implementation files have parsing gaps; confirm relevant source before drawing conclusions from missing graph results.

## Application and build boundaries

| Area | Start here | Purpose |
| --- | --- | --- |
| Build order | [src/starc.pro](../src/starc.pro) | qmake subprojects: third party libraries, application, core library, document plugins, core plugin. |
| Process entry | [src/app/main.cpp](../src/app/main.cpp) | Creates `Application`, loads `coreplugin` through `QPluginLoader`, and starts the event loop. Missing runtime plugins can prevent startup even when the executable exists. |
| Application lifecycle | [ApplicationManager](../src/core/management_layer/application_manager.h) | Core plugin interface for starting the application and opening projects; implementation is in the adjacent `.cpp`. |
| Open project coordination | [ProjectManager](../src/core/management_layer/content/project/project_manager.h) | Project load/close/save, document views, navigator selection, and editor reconfiguration. |
| Available document plugins | [plugins.pro](../src/core/management_layer/plugins/plugins.pro) | Lists built document modules and conditionally includes optional modules when their project files exist. |

`src/core` contains managers and application views. `src/corelib` contains reusable business models, text documents, storage, and UI components. `src/interfaces` defines the interfaces used across plugin boundaries. `src/3rd_party` contains dependencies with their own licensing and build constraints.

## Screenplay editing

| Change you want | Start here |
| --- | --- |
| Bind screenplay models, primary/secondary views, or apply changed settings | [ManagementLayer::ScreenplayTextManager](../src/core/management_layer/plugins/screenplay_text/screenplay_text_manager.h) and its adjacent implementation. |
| Change editor interaction, paragraph types, scene/dialogue numbers, or screenplay Undo/Redo entry points | [Ui::ScreenplayTextEdit](../src/core/management_layer/plugins/screenplay_text/text/screenplay_text_edit.h). This derives from `Ui::ScriptTextEdit`. |
| Change Enter/Tab and paragraph-specific key behavior | [KeyProcessingLayer::KeyPressHandlerFacade](../src/core/management_layer/plugins/screenplay_text/text/handlers/key_press_handler_facade.h) and the neighboring handlers (`action_handler`, `character_handler`, `dialog_handler`, `scene_heading_handler`, etc.). Inspect the selected handler as well as editor event handling. |
| Change screenplay structure, scene/beat/text items, counts, or links to dictionaries/information | [BusinessLayer::ScreenplayTextModel](../src/corelib/business_layer/model/screenplay/text/screenplay_text_model.h) and neighboring model item classes. |
| Change screenplay statistics tables, plots, or report selection | [ScreenplayStatisticsManager](../src/core/management_layer/plugins/screenplay_statistics/screenplay_statistics_manager.h) and its view bind the public [ScreenplayStatisticsModel](../src/corelib/business_layer/model/screenplay/screenplay_statistics_model.h) to read-only widgets. The existing `screenplay_statistics_structure` navigator sends report/plot selection through `IDocumentManager::bind`. |
| Change how screenplay text is exposed as a rich text document | [ScreenplayTextDocument](../src/corelib/business_layer/document/screenplay/text/screenplay_text_document.cpp). It derives from `TextDocument`, installs `ScreenplayTextCorrector`, and updates screenplay/treatment page counts. |
| Change screenplay page breaks or character-name corrections | [ScreenplayTextCorrector](../src/corelib/business_layer/document/screenplay/text/screenplay_text_corrector.cpp), especially `correctPageBreaks` and correction options. Page-break corrections can change document contents; inspect Undo/Redo and export implications. |

The model and the editable rich text document are distinct layers. [TextDocument](../src/corelib/business_layer/document/text/text_document.h) binds a model, maps blocks to model items, and exposes edit transaction state. Trace both sides when diagnosing a change that appears correct on screen but saves or reopens incorrectly.

Shared editor infrastructure lives in [ScriptTextEdit](../src/corelib/ui/modules/script_text_edit/script_text_edit.h), [BaseTextEdit](../src/corelib/ui/widgets/text_edit/base/base_text_edit.h), and [PageTextEdit](../src/corelib/ui/widgets/text_edit/page/page_text_edit.h). These cover reusable script editing, text correction/formatting, and paged text widget behavior. Screenplay-specific changes should begin in the screenplay plugin when practical. Changes to shared classes may also affect novels and simple text documents.

## Formatting, preferences, and file operations

| Area | Start here | Follow through to |
| --- | --- | --- |
| Screenplay templates | [ScreenplayTemplate](../src/corelib/business_layer/templates/screenplay_template.h) and [TemplatesFacade](../src/corelib/business_layer/templates/templates_facade.h) | Base `TextTemplate`, template loading/saving in the facade implementation, and settings views under `src/core/ui/settings/screenplay_template/`. |
| Settings screen | [SettingsManager](../src/core/management_layer/content/settings/settings_manager.h) | Signals for changed settings and the relevant editor manager's `reconfigure` implementation. |
| Setting keys and persistence | [SettingsStorage](../src/corelib/data_layer/storage/settings_storage.h) | Keys in this header, defaults in its implementation, and helpers such as `settingsValue`/`setSettingsValue` in [storage_facade.cpp](../src/corelib/data_layer/storage/storage_facade.cpp). Application and session settings are separate storage types. |
| Import UI and dispatch | [ImportManager](../src/core/management_layer/content/import/import_manager.h) | Format implementations under [business_layer/import/screenplay](../src/corelib/business_layer/import/screenplay/), including Fountain, FDX, DOCX, PDF, Celtx, Trelby, and KIT Scenarist. |
| Export UI and dispatch | [ExportManager](../src/core/management_layer/content/export/export_manager.h) | [business_layer/export/screenplay](../src/corelib/business_layer/export/screenplay/) for Fountain, FDX, DOCX, and PDF. For PDF follow [ScreenplayPdfExporter](../src/corelib/business_layer/export/screenplay/screenplay_pdf_exporter.h) into its shared exporter bases. |
| Project document persistence | [DocumentStorage](../src/corelib/data_layer/storage/document_storage.h) and [Database](../src/corelib/data_layer/database.h) | Document objects, storage implementation, SQL transactions, schema creation, and migration methods. Start project save/open investigations at `ProjectManager` and follow the actual call path. |

For layout changes, inspect the template, document correction, paged widget, and exporter paths together. Screen appearance alone does not establish PDF output or saved-document behavior.

Fountain/FDX import dispatch is `ImportManager::Implementation::importScreenplay`
-> format `importScreenplays` -> `screenplayImported` ->
`ProjectManager::storeScreenplay`. The latter constructs the screenplay bundle
and stores the internal text XML. `ProjectModelsFacade::modelFor` creates and
binds `ScreenplayTextModel` with its information/title/synopsis/dictionary models.
Fountain builds that XML through `AbstractFountainImporter::documentText`; FDX
reads Content/Paragraph/Text and validates the XML before constructing it.

For persistence, `AbstractModel::saveChanges` serializes the live model to its
`DocumentObject`; `ProjectManager::saveChanges` writes loaded documents through
`DocumentStorage` and the document mapper into the project database. Reopening
uses `loadCurrentProject` and `modelFor` again. Immediate-save fixtures must
explicitly serialize models first, then clear storage and close the database
before claiming reopen evidence.

`ExportManager::Implementation::exportScreenplay` copies production information
and export options and selects the format exporter. Both exporters use
`AbstractExporter::prepareDocument` / `ScreenplayExporter` to build a detached
`ScreenplayTextDocument`, filter content and apply the selected template.
Fountain then uses `AbstractMarkdownExporter::exportTo` and `processBlock`; FDX
writes typed XML paragraphs. Local shadow-project saves use the manager's
convenience route: FDX/FDXT destinations select FDX; Fountain/plain-text
destinations retain Fountain. The focused [interchange check](INTERCHANGE.md)
exercises these format boundaries and actual project persistence; it records
FDX's sequential dual-dialogue export and the bounded support policy explicitly.

## Local application UI and retired upstream interfaces

`ApplicationManager` constructs the local project, editor, settings and GitHub
managers; it no longer constructs `AccountManager` or switches to account content.
Its startup and language-change routes no longer download missing language or
emoji fonts. The downloader and `DesignSystem::Font::missedFonts` detector are
removed. Bundled font registration and loading valid fonts from the existing
application-data `fonts/` directory remain; local font selection/fallback and
screenplay templates are unchanged.
`MenuView` has no upstream sign-in/account/purchase actions. Writing statistics
and the sprint timer are ordinary drawer actions without an account header.
`LoglineGeneratorDialog` keeps its help toggle on the question pages and hides
the title action on the result page; Back restores help and Done applies the
logline. `WritingSprintPanel` keeps duration/start, word results and Restart.
Tweet-sharing controls and their browser handlers are removed from both tools.
The Notifications action/page, developer-release filter, release/credits widgets
and unread badges on the menu and project toolbars are removed. Generic shared
badge widgets and document comments/review controls remain.

`NotificationsManager` is retained solely as stable-release metadata support:
the guarded cloud `notificationsReceived`/`askNotifications` feed still supplies
device/Qt-compatible stable releases to
`ApplicationManager::Implementation::askUpdateToLatestVersion` via
`lastVersionDownloadLink()`. This direct source call can be missing from the
graph's caller edges. The manager has no page/unread signals or preference
access; old show-dev/last-read settings remain stored and ignored. Its qmake
entries remain, while the page-only views' entries are gone. No private cloud
module or updater/download workflow was removed or exercised.

The interface is English only. `ApplicationManager::Implementation::setTranslation()`
runs once at startup: it sets the default locale to English, installs
`:/translations/translation_en.qm` and sets a left-to-right layout. That catalog
is the only entry of `src/core/translations.qrc`; it is kept because it holds
the English plural forms ("1 scene", "2 scenes"), which `tr()` cannot produce
from the source text. `LanguageDialog`, the Language row of `SettingsView`, the
`SettingsManager` language signals, `LanguageHelper` and `PercentRadioButton`
are removed. The settings `application/language`, `language-qt-version` and
`language-file` have no key constants and are neither read nor written; values
stored by an earlier version stay in the settings file. `DesignSystem::updateLanguage()`
is still called from `setTranslation()` to rebuild the fonts once the locale is set.
Code that asks `QLocale()` now always gets English: interface font families in
`DesignSystem`, typographic quotes in `BaseTextEdit`, the language code of DOCX
export, and the right-to-left branches of delegates, widgets and exporters. The
direction of script text comes from the text itself and is unaffected.

`ProjectsManager::createProject` -> `CreateProjectDialog` ->
`createLocalProjectRequested` -> `ApplicationManager::createLocalProject` is the
new-story route. It has no cloud/team selector or sign-in/subscription prompt.
`CreateDocumentDialog` still follows installed editor availability and retains
its unavailable-document guard, without an upgrade button. `ProjectManager`
no longer forwards plugin purchase signals.

The former `ui/account` connection-status toolbar and collaborator cursor strip
serve existing project transports, so they are retained at `ui/` and `ui/project/`
respectively. Cached team/project models, historical account labels, persisted
type IDs and database migrations remain. Guarded cloud reconnection/data paths
have no account UI callbacks; the absent private module has not been built.
GitHub onboarding, settings, sync and `GitHubCredentialStore` remain separate.

`tests/screenplay_scope_test.cpp` checks the retained menu/dialogs/settings and
GitHub onboarding for retired account offers, local writing-tool actions,
unavailable-document guarding, and exact cached team/project records through
load/save/reload, alongside its existing editor and project-data assertions.
It also checks absence of notification actions, labels, widgets and navigation/
filter callbacks in both menu states, and verifies retained stable-release
selection, duplicates/empty deliveries, malformed metadata and ignored old
preferences using synthetic feed records, without networking.

## Retired formats

Audioplay, stageplay, comic book, novel, screenplay series, and World/Worlds support was removed from this fork, together with the AI assistant. Screenplay, simple text, and the shared project documents remain. A project that already contains documents of a retired type keeps them untouched:

| Area | Start here | Notes |
| --- | --- | --- |
| Which types are retired | `isUnsupportedDocumentType` in [document_object.h](../src/corelib/domain/document_object.h) | The 34 persisted type identifiers and their MIME mappings stay in the enum and in `document_object.cpp`. Do not renumber or remove them. |
| No model, no rewrite | `ProjectModelsFacade::modelFor` in [project_models_facade.cpp](../src/core/management_layer/content/project/project_models_facade.cpp) | Returns `nullptr` for a retired type before a document is created or parsed, so its content is never loaded into a model or saved back. |
| What the user sees | `ProjectManager::showView` and [ProjectView::showUnsupportedFormatPage](../src/core/ui/project/project_view.h) | Selecting such a document shows a notice instead of an editor. It cannot be created, dragged, given drafts, or changed from the context menu. |
| Structure tree | [StructureModel](../src/corelib/business_layer/model/structure/structure_model.h) | `addDocument` refuses retired types; existing items are still parsed, shown, and serialized. |
| Old project files | [Database](../src/corelib/data_layer/database.h) | The upstream upgrade steps stay, including the one that renames comic book blocks in projects from before 0.1.3. They treat script documents as text. |

Several tables in `project_manager.cpp` still list the retired type identifiers (text items, items that cannot be removed separately, sharing). They classify existing structure items and need no retired source code.

The episodes of a series are ordinary screenplay documents nested in its "Episodes" item. Only the series documents themselves are retired: an episode still opens in the screenplay editor and can be dragged out of the series, but nothing can be dropped into it.

New project structures no longer add a Worlds group. Existing Worlds groups
remain visible even when empty; their World children retain their hierarchy and
drafts. `Database::validateDatabase` excludes the retired Worlds type from its
singleton cleanup, preserving duplicate records and their change history.
World models, export dialogs/exporters, the public navigator, and the two private
plugin links are removed. Historical settings/promo identifiers remain reserved;
their World previews are removed. International screenplay templates named
`world_cp`/`world_cn` and the logline generator's story-world questions are separate
screenwriting features and remain unchanged.

## Story sync with GitHub

This fork can keep each local story as one file in the root of a private GitHub repository. It replaces the upstream cloud service, whose client is a private submodule absent from this checkout.

| Area | Start here | Notes |
| --- | --- | --- |
| When and what to sync, dialogs, story menu actions | [GitHubSyncManager](../src/core/management_layer/content/github_sync/github_sync_manager.h) | Owned by `ApplicationManager`, which calls it after startup, before exit, and after every save. |
| Deciding which copy is newer | [ProjectSync](../src/corelib/utils/tools/project_sync.h) | Pure functions: content fingerprint of a project file and the state of a local/remote pair. |
| Talking to GitHub | [GitHubFilesClient](../src/corelib/utils/tools/github_files_client.h) | Repository contents API over `QNetworkAccessManager`. `STARC_GITHUB_API_URL` points it at another server for tests. |
| Credential boundary | [GitHubCredentialStore](../src/corelib/data_layer/storage/github_credential_store.h) | Shared by onboarding, settings and sync. Serializes asynchronous native QtKeychain jobs, confirms storage by reading back, migrates legacy settings, and retains newer staged edits. Injectable backend for deterministic failures. |
| Settings | `kGitHubSync*` keys in [SettingsStorage](../src/corelib/data_layer/storage/settings_storage.h) | Repository, upload on exit, and sync history. `kGitHubSyncTokenKey` is migration-only; ordinary setters refuse credential writes. |
| First-run setup | [OnboardingNavigator](../src/core/ui/onboarding/onboarding_navigator.h), [OnboardingManager](../src/core/management_layer/content/onboarding/onboarding_manager.h) | The only page of the first launch: set the repository and token now, or later in the settings. Setting them runs a sync check as soon as the main window appears; the startup check is skipped while this page is shown. |

Rules the code relies on:

- The application rewrites a project file when it is only opened, so file bytes cannot tell whether a story changed. A story counts as changed when the fingerprint of its documents differs from the one remembered at the last sync.
- A file is uploaded only over the repository version it was last synced with (GitHub refuses the upload otherwise), and downloaded over a local file only if that file has not changed since the last sync. When both changed, nothing is replaced until the user picks a version in the story's context menu.
- A local version replaced by a download is first copied to the backups folder as `<name> (before GitHub download)_<time>.starc`.
- A public repository is refused.
- Sync waits for credential loading and writes to settle. A keyring credential takes precedence over any legacy settings token; only a confirmed keyring copy permits durable plaintext removal. Failed migration retains the legacy token for authentication and reports a retryable warning.
- Settings stage token edits in memory; Save token, Clear token, Retry keyring, leaving settings, and normal close use the shared store. Programmatic population blocks change signals. Shutdown and settings-driven restarts report unsaved changes and allow retry or keeping the application open.
- The native adapter explicitly disables QtKeychain's insecure fallback. Keyring/backend messages are mapped to safe application text; bearer tokens echoed by a server are removed from errors before logs or UI receive them. See [PROJECT.md](PROJECT.md#github-token-storage) for policy and evidence.

## Scope and validation notes

- Some modules are conditional. [core.pro](../src/core/core.pro) optionally includes `src/cloud/cloud.pri`; [plugins.pro](../src/core/management_layer/plugins/plugins.pro) conditionally includes modules such as screenplay cards, timeline and breakdown. Screenplay statistics now has a local plugin in the ordinary build, replacing its private submodule. Plugin availability still controls its tree item and information-page checkbox; `ProjectManager::loadCurrentProject` restores missing statistics children in local projects. The private audioplay, stageplay, and comic book statistics modules are no longer referenced.
- [src/testapp/main.cpp](../src/testapp/main.cpp) is an interactive widget demo, and `testapp` is commented out in `src/starc.pro`. It is not evidence of an automated screenplay regression suite.
- For an editor change, choose checks that exercise the changed behavior: typing and paragraph transitions, Undo/Redo, save/reopen, and affected exports. Use copies of project files. Record what actually ran and preserve failures.
- A successful build or headless startup proves only that specific boundary. Visible desktop behavior, keyboard interaction, pagination, and exported layout require corresponding evidence.
- Use the installed `codebase-memory` skill for symbol discovery and caller/callee traces. Check coverage for relied-on files and read source for reported gaps. Use `rg` for literals, settings keys, project files, and resources.
