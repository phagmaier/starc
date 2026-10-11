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
| Change how screenplay text is exposed as a rich text document | [ScreenplayTextDocument](../src/corelib/business_layer/document/screenplay/text/screenplay_text_document.cpp). It derives from `TextDocument`, installs `ScreenplayTextCorrector`, and updates screenplay/treatment page counts. |
| Change screenplay page breaks or character-name corrections | [ScreenplayTextCorrector](../src/corelib/business_layer/document/screenplay/text/screenplay_text_corrector.cpp), especially `correctPageBreaks` and correction options. Page-break corrections can change document contents; inspect Undo/Redo and export implications. |

The model and the editable rich text document are distinct layers. [TextDocument](../src/corelib/business_layer/document/text/text_document.h) binds a model, maps blocks to model items, and exposes edit transaction state. Trace both sides when diagnosing a change that appears correct on screen but saves or reopens incorrectly.

Shared editor infrastructure lives in [ScriptTextEdit](../src/corelib/ui/modules/script_text_edit/script_text_edit.h), [BaseTextEdit](../src/corelib/ui/widgets/text_edit/base/base_text_edit.h), and [PageTextEdit](../src/corelib/ui/widgets/text_edit/page/page_text_edit.h). These cover reusable script editing, text correction/formatting, and paged text widget behavior. Screenplay-specific changes should begin in the screenplay plugin when practical. Changes to shared classes may also affect novels, stageplays, audioplays, or other document types.

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

## Scope and validation notes

- Some modules are conditional. [core.pro](../src/core/core.pro) optionally includes `src/cloud/cloud.pri`; [plugins.pro](../src/core/management_layer/plugins/plugins.pro) conditionally includes modules such as screenplay cards, timeline, breakdown, and statistics. Their absence should be checked against the checkout and build configuration before investigating it as a runtime defect.
- [src/testapp/main.cpp](../src/testapp/main.cpp) is an interactive widget demo, and `testapp` is commented out in `src/starc.pro`. It is not evidence of an automated screenplay regression suite.
- For an editor change, choose checks that exercise the changed behavior: typing and paragraph transitions, Undo/Redo, save/reopen, and affected exports. Use copies of project files. Record what actually ran and preserve failures.
- A successful build or headless startup proves only that specific boundary. Visible desktop behavior, keyboard interaction, pagination, and exported layout require corresponding evidence.
- Use the installed `codebase-memory` skill for symbol discovery and caller/callee traces. Check coverage for relied-on files and read source for reported gaps. Use `rg` for literals, settings keys, project files, and resources.
