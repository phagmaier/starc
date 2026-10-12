#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_fdx_exporter.h>
#include <business_layer/export/screenplay/screenplay_fountain_exporter.h>
#include <business_layer/export/screenplay/screenplay_pdf_exporter.h>
#include <business_layer/import/import_options.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <interfaces/management_layer/i_document_manager.h>
#include <interfaces/ui/i_document_view.h>
#include <management_layer/content/import/import_manager.h>
#include <management_layer/content/project/project_manager.h>
#include <management_layer/content/project/project_models_facade.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <management_layer/content/settings/settings_manager.h>
#include <management_layer/content/shortcuts/shortcuts_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <management_layer/plugins_builder.h>
#include <ui/account/account_navigator.h>
#include <ui/import/import_dialog.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/project/create_document_dialog.h>
#include <ui/project/create_document_dialog_option.h>
#include <ui/project/project_navigator.h>
#include <ui/project/project_view.h>
#include <ui/projects/create_project_dialog.h>
#include <ui/settings/settings_navigator.h>
#include <ui/settings/settings_view.h>
#include <ui/widgets/app_bar/app_bar.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/tree/tree.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QTextDocument>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {
void require(bool _ok, const char* _label)
{
    if (!_ok) {
        std::fprintf(stderr, "FAIL: %s\n", _label);
        std::exit(1);
    }
    std::printf("ok: %s\n", _label);
}
void noAi(QWidget* _widget)
{
    require(_widget, "construct retained view");
    for (auto object : _widget->findChildren<QObject*>()) {
        require(!QString(object->metaObject()->className()).contains("AiAssistant"),
                "no AI widget");
        if (auto action = qobject_cast<QAction*>(object)) {
            require(!action->toolTip().contains("AI assistant"), "no AI toolbar control");
        }
    }
}
// Names of the retired formats and of the removed assistant must not be offered anywhere.
void requireRetained(const QString& _text)
{
    for (const auto retired : { "comic", "audioplay", "audio drama", "audio play", "stageplay",
                                "stage play", "ai assistant" }) {
        if (_text.contains(QLatin1String(retired), Qt::CaseInsensitive)) {
            std::fprintf(stderr, "FAIL: retired choice offered: %s\n", qPrintable(_text));
            std::exit(1);
        }
    }
}
void requireRetained(const QAbstractItemModel* _model, const QModelIndex& _parent)
{
    if (_model == nullptr) {
        return;
    }
    for (int row = 0; row < _model->rowCount(_parent); ++row) {
        const auto index = _model->index(row, 0, _parent);
        requireRetained(index.data().toString());
        requireRetained(_model, index);
    }
}
void key(QWidget* _editor, int _key, const QString& _text = {},
         Qt::KeyboardModifiers _modifiers = Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress, _key, _modifiers, _text);
    QKeyEvent release(QEvent::KeyRelease, _key, _modifiers, _text);
    QApplication::sendEvent(_editor, &press);
    QApplication::sendEvent(_editor, &release);
    QApplication::processEvents();
}
// Models write edits to their documents after a short idle period, as when a writer pauses.
void pause(int _milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(_milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
void type(QWidget* _editor, const QString& _text)
{
    for (const auto character : _text) {
        key(_editor, Qt::Key_unknown, QString(character));
    }
}
// The editor lives in a scalable graphics proxy, outside the ordinary child widget tree.
Ui::ScreenplayTextEdit* screenplayEdit()
{
    const auto widgets = QApplication::allWidgets();
    for (auto widget : widgets) {
        if (auto edit = qobject_cast<Ui::ScreenplayTextEdit*>(widget)) {
            return edit;
        }
    }
    return nullptr;
}
bool noticeShown(QWidget* _view, const char* _title)
{
    for (auto label : _view->findChildren<AbstractLabel*>()) {
        if (label->text() == _title && label->isVisibleTo(_view)) {
            return true;
        }
    }
    return false;
}
bool unsupportedNoticeShown(QWidget* _view)
{
    return noticeShown(_view, "Unsupported document format");
}
bool noEditorNoticeShown(QWidget* _view)
{
    return noticeShown(_view, "No editor for this document");
}
// Whether this build has an editor plugin to open documents of the type with. The checks
// follow what the build contains, so they hold for a build with more plugins as well.
bool hasEditor(const ManagementLayer::PluginsBuilder& _plugins, Domain::DocumentObjectType _type)
{
    return !_plugins.editorsInfoFor(Domain::mimeTypeFor(_type), false).isEmpty();
}
// The option of an information page that shows the statistics of the document in the project.
CheckBox* statisticsOption(QWidget* _page)
{
    if (_page == nullptr) {
        return nullptr;
    }
    for (auto option : _page->findChildren<CheckBox*>()) {
        if (option->text() == "Statistics") {
            return option;
        }
    }
    return nullptr;
}
QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read export");
    return file.readAll();
}
} // namespace

int main(int argc, char** argv)
{
    // Keep the log complete when a failure stops the process abruptly.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Screenplay scope regression");
    app.setOrganizationName("Disposable tests");
    // The version the application registers. Without one every open looks like a project from
    // before 0.1.3 and the retained upstream upgrade steps rewrite script documents as text.
    app.setApplicationVersion("0.8.4");
    require(argc == 2, "evidence directory argument");
    const QString evidence = QString::fromLocal8Bit(argv[1]);
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        require(QFontDatabase::addApplicationFont(font) >= 0, "embedded font");
    }
    using Type = Domain::DocumentObjectType;
    static_assert(static_cast<int>(Type::ComicBook) == 10200);
    static_assert(static_cast<int>(Type::Audioplay) == 10300);
    static_assert(static_cast<int>(Type::Stageplay) == 10400);
    static_assert(static_cast<int>(Type::Novel) == 10500);
    static_assert(static_cast<int>(Type::ScreenplayText) == 10104);

    // Dialogs require a top-level parent to overlay.
    QWidget host;
    host.resize(1280, 800);
    auto createProject = new Ui::CreateProjectDialog(&host);
    for (auto combo : createProject->findChildren<ComboBox*>()) {
        for (int row = 0; row < combo->model()->rowCount(); ++row) {
            const auto type
                = static_cast<Type>(combo->model()->index(row, 0).data(Qt::UserRole + 1).toInt());
            require(!Domain::isUnsupportedDocumentType(type), "no retired project choice");
        }
    }
    auto createDocument = new Ui::CreateDocumentDialog(&host);
    for (auto option : createDocument->findChildren<Ui::CreateDocumentDialogOption*>()) {
        require(!Domain::isUnsupportedDocumentType(option->documentType()),
                "no retired document choice");
    }
    // The settings screen as the application builds it, with its shortcut tables.
    ManagementLayer::PluginsBuilder plugins;
    ManagementLayer::ShortcutsManager shortcuts;
    ManagementLayer::SettingsManager settings(nullptr, &host, plugins, &shortcuts);
    auto onboarding = new Ui::OnboardingNavigator(&host);
    auto account = new Ui::AccountNavigator(&host);
    QApplication::processEvents();
    for (QWidget* widget :
         QVector<QWidget*>{ settings.view(), settings.navigator(), onboarding, account }) {
        noAi(widget);
        for (auto label : widget->findChildren<AbstractLabel*>()) {
            requireRetained(label->text());
        }
        for (auto tree : widget->findChildren<Tree*>()) {
            requireRetained(tree->model(), {});
        }
    }
    {
        int components = 0;
        const auto navigation = settings.navigator()->findChild<Tree*>()->model();
        for (int row = 0; row < navigation->rowCount(); ++row) {
            components = qMax(components, navigation->rowCount(navigation->index(row, 0)));
        }
        require(components == 3, "settings list simple text, screenplay and novel components");
    }

    const QString path = evidence + "/mixed-project.starc";
    DatabaseLayer::Database::setCurrentFile(path);
    QWidget parent;
    ManagementLayer::ProjectManager manager(nullptr, &parent, plugins);
    BusinessLayer::StructureModel* structure = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::structureModelChanged,
                     [&](BusinessLayer::AbstractModel* model) {
                         structure = qobject_cast<BusinessLayer::StructureModel*>(model);
                     });
    auto storage = DataStorageLayer::StorageFacade::documentStorage();
    BusinessLayer::ProjectsModelProjectItem project;
    project.setProjectType(BusinessLayer::ProjectType::Local);
    project.setPath(path);
    project.setName("Disposable mixed screenplay");
    manager.loadCurrentProject(&project);
    require(structure, "open disposable project");
    // A project file gets its version on the second open, after the upstream upgrade steps
    // ran once. Those steps rewrite script documents as text, so reach the state of an
    // existing project before adding the documents whose bytes are compared.
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    DatabaseLayer::Database::setCurrentFile(path);
    manager.loadCurrentProject(&project);
    require(structure, "reopen the new project");
    const auto screenplay = structure->addDocument(Type::Screenplay, "Kept screenplay");
    require(screenplay.isValid(), "create screenplay");
    auto story = structure->itemForIndex(screenplay);
    BusinessLayer::StructureModelItem* script = nullptr;
    for (int i = 0; i < story->childCount(); ++i) {
        if (story->childAt(i)->type() == Type::ScreenplayText)
            script = story->childAt(i);
    }
    require(script, "screenplay text created");
    const auto scriptUuid = script->uuid();
    const auto screenplayUuid = story->uuid();
    // A document set gets statistics only when there is a view to show them with. A project
    // from a build that had one already holds them, like the item added by hand here.
    const bool statisticsView = hasEditor(plugins, Type::ScreenplayStatistics);
    BusinessLayer::StructureModelItem* statistics = nullptr;
    for (int i = 0; i < story->childCount(); ++i) {
        if (story->childAt(i)->type() == Type::ScreenplayStatistics)
            statistics = story->childAt(i);
    }
    require((statistics != nullptr) == statisticsView,
            "new screenplay gets statistics only with a view for them");
    if (statistics == nullptr) {
        statistics = new BusinessLayer::StructureModelItem(
            QUuid::createUuid(), Type::ScreenplayStatistics, "Statistics", "First draft", {}, true,
            false, false);
        structure->appendItem(statistics, story);
    }
    const auto statisticsUuid = statistics->uuid();
    // Documents an existing project can hold whether or not this build has an editor for them.
    const auto mindMapUuid
        = structure->itemForIndex(structure->addDocument(Type::MindMap, "Kept mind map"))->uuid();
    const auto characterUuid
        = structure->itemForIndex(structure->addDocument(Type::Character, "Kept character"))
              ->uuid();
    require(structure->itemForUuid(mindMapUuid)->type() == Type::MindMap
                && structure->itemForUuid(characterUuid)->type() == Type::Character,
            "seed mind map and character documents");
    QHash<QUuid, QByteArray> retired;
    QVector<QUuid> retiredText;
    // All 18 persisted format identifiers, plus drafts and binary/image data.
    for (const int base : { 10200, 10300, 10400 }) {
        BusinessLayer::StructureModelItem* bundle = nullptr;
        for (int offset = 0; offset <= 5; ++offset) {
            const auto uuid = QUuid::createUuid();
            const auto documentType = static_cast<Type>(base + offset);
            const QByteArray bytes = QByteArray::fromHex("efbbbf") + "<legacy unknown='keep'>\r\n"
                + QByteArray::number(base + offset) + QByteArray::fromHex("00ff") + "</legacy>\r\n";
            auto item = new BusinessLayer::StructureModelItem(
                uuid, documentType, "Retained legacy document", "First draft", {}, true, false,
                false);
            structure->appendItem(item, bundle, bytes);
            if (offset == 0)
                bundle = item;
            retired.insert(uuid, bytes);
            require(storage->document(uuid)->content() == bytes, "seed legacy bytes");
            if (offset == 3) {
                retiredText.append(uuid);
                structure->addItemDraft(item, "Retained draft", {}, false, bytes + "draft", false);
                const auto draft = item->drafts().first();
                retired.insert(draft->uuid(), bytes + "draft");
            }
        }
        require(!structure->addDocument(static_cast<Type>(base), "Refused").isValid(),
                "reject retired format creation");
    }
    for (const auto documentType : { Type::ImageData, Type::BinaryData }) {
        const auto uuid = QUuid::createUuid();
        auto doc = storage->createDocument(uuid, documentType);
        const QByteArray bytes = QByteArray::fromHex("00010200ffff");
        doc->setContent(bytes);
        require(storage->saveDocument(doc), "save retained raw data");
        retired.insert(uuid, bytes);
    }

    auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
    require(navigator, "project navigator");
    auto tree = navigator->findChild<Tree*>();
    require(tree, "project tree");
    auto proxy = qobject_cast<QSortFilterProxyModel*>(tree->model());
    require(proxy, "project proxy model");
    for (const auto uuid : retiredText) {
        const auto index
            = proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(uuid)));
        require(index.isValid(), "legacy document remains in navigator");
        navigator->setCurrentIndex(index);
        require(!manager.currentDocument(), "legacy document has no editor model");
        require(!(structure->flags(structure->indexForItem(structure->itemForUuid(uuid)))
                  & Qt::ItemIsDragEnabled),
                "legacy document is read only");
        require(unsupportedNoticeShown(manager.view()), "unsupported-format explanation");
    }

    const auto proxyIndex = [&](const QUuid& _uuid) {
        return proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(_uuid)));
    };
    require(proxyIndex(statisticsUuid).isValid() == statisticsView,
            "statistics are listed only with a view for them");
    for (const auto& uuid : { mindMapUuid, characterUuid }) {
        if (hasEditor(plugins, structure->itemForUuid(uuid)->type())) {
            continue;
        }
        navigator->setCurrentIndex(proxyIndex(uuid));
        require(noEditorNoticeShown(manager.view()), "document without an editor says so");
        require(manager.toolBar()->actions().size() == 1,
                "document without an editor offers no views");
        require(manager.currentDocument() && manager.currentDocument()->uuid() == uuid,
                "document without an editor stays the current one");
    }

    navigator->setCurrentIndex(proxy->mapFromSource(structure->indexForItem(script)));
    require(manager.currentDocument() && manager.currentDocument()->uuid() == scriptUuid,
            "open screenplay in mixed project");
    require(!unsupportedNoticeShown(manager.view()), "screenplay replaces the unsupported notice");
    require(!noEditorNoticeShown(manager.view()), "screenplay replaces the no-editor notice");
    // The first toolbar action is the menu, the rest switch between the views of the document.
    const auto scriptViews
        = plugins.editorsInfoFor(Domain::mimeTypeFor(Type::ScreenplayText), false);
    require(!scriptViews.isEmpty(), "screenplay text has a view");
    require(manager.toolBar()->actions().size() == 1 + scriptViews.size(),
            "view switcher lists the views this build has");
    for (const auto& view : scriptViews) {
        require(plugins.initPlugin(view.mimeType), "listed screenplay view loads");
    }
    auto edit = screenplayEdit();
    require(edit, "actual screenplay editor");
    noAi(manager.view());
    noAi(edit);
    edit->setCurrentParagraphType(BusinessLayer::TextParagraphType::SceneHeading);
    type(edit, "INT. ROOM - DAY");
    key(edit, Qt::Key_Return, "\r");
    require(edit->currentParagraphType() == BusinessLayer::TextParagraphType::Action,
            "scene heading Enter transitions to action");
    type(edit, "A writer waits.");
    key(edit, Qt::Key_Return, "\r");
    key(edit, Qt::Key_Tab, "\t");
    require(edit->currentParagraphType() == BusinessLayer::TextParagraphType::Character,
            "action Tab transitions to character");
    type(edit, "HERO");
    key(edit, Qt::Key_Return, "\r");
    require(edit->currentParagraphType() == BusinessLayer::TextParagraphType::Dialogue,
            "character Enter transitions to dialogue");
    // Names used in the script are collected into the project, with or without an editor
    // for the documents they are kept in.
    const auto collected = [&](Type _group, const QString& _name) {
        const auto group = structure->itemForType(_group);
        for (int i = 0; group != nullptr && i < group->childCount(); ++i) {
            if (group->childAt(i)->name() == _name) {
                return true;
            }
        }
        return false;
    };
    require(collected(Type::Characters, "HERO"), "character named in the script is collected");
    require(collected(Type::Locations, "ROOM"), "location named in the script is collected");
    type(edit, "We keep the screenplay.");
    // An Undo step is what the model recorded between two saves of its changes. The
    // application saves them on a timer; the check does it explicitly to get a defined step.
    auto scriptModel = manager.firstScriptModel();
    require(scriptModel && scriptModel->document()->uuid() == scriptUuid,
            "editor is bound to the screenplay model");
    scriptModel->saveChanges();
    const QString before = edit->toPlainText();
    type(edit, "X");
    require(edit->toPlainText() == before + "X", "typing exact text");
    edit->undo();
    require(edit->toPlainText() == before, "screenplay Undo");
    edit->redo();
    require(edit->toPlainText() == before + "X", "screenplay Redo");
    key(edit, Qt::Key_Return, "\r");
    edit->setCurrentParagraphType(BusinessLayer::TextParagraphType::Action);
    for (int i = 0; i < 140; ++i) {
        type(edit, "The scene continues with an ordinary action paragraph.");
        key(edit, Qt::Key_Return, "\r");
    }
    require(edit->document()->pageCount() > 1, "screenplay pagination spans pages");
    const auto expectedText = edit->toPlainText();
    // Save all loaded models through the application's project save path.
    pause(1000);
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    const auto screenplayBytes = manager.currentDocument()->content();
    const auto structureBytes = structure->document()->content();
    QHash<QUuid, QByteArray> kept;
    for (const auto& uuid : { statisticsUuid, mindMapUuid, characterUuid }) {
        require(storage->document(uuid), "document without an editor is stored");
        kept.insert(uuid, storage->document(uuid)->content());
    }
    require(screenplayBytes.contains("We keep the screenplay.X")
                && screenplayBytes.count("The scene continues with an ordinary action paragraph.")
                    == 140,
            "typed screenplay persisted");

    // The model the editor is bound to, as the application's export would use it.
    auto model = manager.firstScriptModel();
    require(model && model->document()->uuid() == scriptUuid, "screenplay model dispatcher");
    {
        ManagementLayer::ProjectModelsFacade facade(structure, nullptr, nullptr);
        for (const auto& uuid : retiredText) {
            require(!facade.modelFor(uuid), "retired model refused");
        }
        for (const int base : { 10200, 10300, 10400 }) {
            require(!facade.modelFor(static_cast<Type>(base + 4)),
                    "retired dictionaries are not created on request");
        }
    }
    BusinessLayer::ScreenplayExportOptions options;
    options.includeTitlePage = false;
    options.templateId = "world_cp";
    options.fileFormat = BusinessLayer::ExportFileFormat::Fountain;
    options.filePath = evidence + "/screenplay.fountain";
    BusinessLayer::ScreenplayFountainExporter().exportTo(model, options);
    require(read(options.filePath).contains("We keep the screenplay"), "Fountain export text");
    options.fileFormat = BusinessLayer::ExportFileFormat::Fdx;
    options.filePath = evidence + "/screenplay.fdx";
    BusinessLayer::ScreenplayFdxExporter().exportTo(model, options);
    require(read(options.filePath).contains("We keep the screenplay"), "FDX export text");
    options.fileFormat = BusinessLayer::ExportFileFormat::Docx;
    options.filePath = evidence + "/screenplay.docx";
    BusinessLayer::ScreenplayDocxExporter().exportTo(model, options);
    require(read(options.filePath).startsWith("PK"), "DOCX export container");
    options.fileFormat = BusinessLayer::ExportFileFormat::Pdf;
    options.filePath = evidence + "/screenplay.pdf";
    BusinessLayer::ScreenplayPdfExporter().exportTo(model, options);
    require(read(options.filePath).startsWith("%PDF"), "PDF export container");
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    DatabaseLayer::Database::setCurrentFile(path);
    manager.loadCurrentProject(&project);
    require(storage->document(scriptUuid)->content() == screenplayBytes,
            "screenplay bytes survive reopen");
    for (auto it = retired.cbegin(); it != retired.cend(); ++it) {
        const auto document = storage->document(it.key());
        if (document == nullptr || document->content() != it.value()) {
            std::fprintf(stderr, "retained document %s of type %d: expected %s, found %s\n",
                         qPrintable(it.key().toString()),
                         document ? static_cast<int>(document->type()) : -1,
                         it.value().toHex().constData(),
                         document ? document->content().toHex().constData() : "nothing");
        }
        require(document && document->content() == it.value(),
                "legacy document, draft, and raw data bytes survive save/reopen");
    }
    require(structure->document()->content() == structureBytes,
            "project structure survives save/reopen unchanged");
    for (auto it = kept.cbegin(); it != kept.cend(); ++it) {
        require(structure->itemForUuid(it.key()) && structure->itemForUuid(it.key())->isVisible()
                    && storage->document(it.key())
                    && storage->document(it.key())->content() == it.value(),
                "statistics, mind map and character survive save/reopen");
    }
    require(proxyIndex(statisticsUuid).isValid() == statisticsView,
            "kept statistics are listed only with a view for them");
    navigator->setCurrentIndex(
        proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(scriptUuid))));
    edit = screenplayEdit();
    QApplication::processEvents();
    if (edit != nullptr && edit->toPlainText() != expectedText) {
        const auto found = edit->toPlainText();
        int at = 0;
        while (at < found.size() && at < expectedText.size() && found[at] == expectedText[at]) {
            ++at;
        }
        std::fprintf(stderr, "reopened text differs at %d of %d/%d: expected [%s] found [%s]\n", at,
                     int(expectedText.size()), int(found.size()),
                     qPrintable(expectedText.mid(at - 20, 80)), qPrintable(found.mid(at - 20, 80)));
    }
    require(edit && edit->toPlainText() == expectedText, "screenplay editor text survives reopen");

    // The view used last is remembered for each document. One this build cannot load, like the
    // cards of a build with more plugins, gives way to the default view of the document.
    QString unloadableView = "application/x-starc/editor/screenplay/text/cards";
    if (plugins.initPlugin(unloadableView)) {
        unloadableView += "/missing";
    }
    const QString lastViewKey = DatabaseLayer::Database::currentFile() + "/"
        + scriptUuid.toString() + "/view-mime-type";
    const auto reopenScriptWithUnloadableView = [&] {
        navigator->setCurrentIndex(proxyIndex(retiredText.first()));
        setSettingsValue(lastViewKey, unloadableView);
        navigator->setCurrentIndex(proxyIndex(scriptUuid));
        QApplication::processEvents();
    };
    // The second panel stays hidden until the view is split.
    Ui::ProjectView* panel = nullptr;
    Ui::ProjectView* secondPanel = nullptr;
    for (auto view : manager.view()->findChildren<Ui::ProjectView*>()) {
        (view->isHidden() ? secondPanel : panel) = view;
    }
    require(panel && secondPanel, "one of the two document panels is in use");
    reopenScriptWithUnloadableView();
    require(panel->currentEditor() && !noEditorNoticeShown(manager.view())
                && manager.currentDocument() && manager.currentDocument()->uuid() == scriptUuid,
            "remembered view that cannot be loaded falls back to an editor");
    require(settingsValue(lastViewKey).toString() == scriptViews.first().mimeType,
            "the default view replaces the remembered one");

    // A draft of the document opens in the view of the document.
    auto scriptItem = structure->itemForUuid(scriptUuid);
    structure->addItemDraft(scriptItem, "Second draft", {}, false, screenplayBytes, false);
    const auto draftUuid = scriptItem->drafts().first()->uuid();
    reopenScriptWithUnloadableView();
    panel->setCurrentDraft(1);
    QApplication::processEvents();
    require(panel->currentEditor() && !noEditorNoticeShown(manager.view())
                && manager.currentDocument() && manager.currentDocument()->uuid() == draftUuid,
            "draft opens in an editor after the fallback");
    panel->setCurrentDraft(0);
    QApplication::processEvents();

    // The second panel of the split view resolves the remembered view the same way.
    QAction* split = nullptr;
    for (auto action :
         qobject_cast<AppBar*>(manager.toolBar())->options(AppBarOptionsLevel::View)) {
        if (action->isCheckable()) {
            split = action;
        }
    }
    require(split, "split view action");
    reopenScriptWithUnloadableView();
    setSettingsValue(lastViewKey, unloadableView);
    split->toggle();
    QApplication::processEvents();
    require(!secondPanel->isHidden() && panel->currentEditor() && secondPanel->currentEditor()
                && !noEditorNoticeShown(manager.view())
                && manager.currentDocument() && manager.currentDocument()->uuid() == scriptUuid,
            "both panels of the split view show an editor");
    split->toggle();
    QApplication::processEvents();

    // The add-document dialog, opened the way the navigator button does.
    emit navigator->addDocumentClicked();
    QApplication::processEvents();
    auto addDocument = parent.findChild<Ui::CreateDocumentDialog*>();
    require(addDocument, "add-document dialog of the project");
    bool screenplayOffered = false;
    for (auto option : addDocument->findChildren<Ui::CreateDocumentDialogOption*>()) {
        require(hasEditor(plugins, option->documentType()), "offered document has an editor");
        screenplayOffered = screenplayOffered || option->documentType() == Type::Screenplay;
    }
    require(screenplayOffered, "screenplay is offered");
    delete addDocument;

    // The information page of a screenplay, a series and a novel has an option that shows the
    // statistics of the document in the project. It is there only with a view for them, in
    // either panel, and hiding it leaves the choice it holds and the item as they are.
    struct DocumentSet {
        Type document;
        Type statistics;
        QUuid uuid;
    };
    for (auto set : {
             DocumentSet{ Type::Screenplay, Type::ScreenplayStatistics, screenplayUuid },
             DocumentSet{ Type::ScreenplaySeries, Type::ScreenplaySeriesStatistics, {} },
             DocumentSet{ Type::Novel, Type::NovelStatistics, {} },
         }) {
        if (!hasEditor(plugins, set.document)) {
            continue;
        }
        if (set.uuid.isNull()) {
            set.uuid = structure->itemForIndex(structure->addDocument(set.document, "Kept set"))
                           ->uuid();
        }
        const bool available = hasEditor(plugins, set.statistics);
        navigator->setCurrentIndex(proxyIndex(set.uuid));
        QApplication::processEvents();
        auto page = panel->currentEditor();
        auto option = statisticsOption(page);
        require(option, "information page has the statistics option");
        require(option->isVisibleTo(page) == available,
                "statistics option is there only with a view for them");

        // The option follows the documents this build can open, whichever they are.
        int changes = 0;
        const auto counter
            = QObject::connect(option, &CheckBox::checkedChanged, [&changes] { ++changes; });
        const bool checked = option->isChecked();
        const auto information = plugins.plugin(
            plugins.editorsInfoFor(Domain::mimeTypeFor(set.document), false).first().mimeType);
        require(information, "information plugin of the document");
        const QSet<QString> withStatistics = { QString(Domain::mimeTypeFor(set.statistics)) };
        information->setDocumentsWithEditors(withStatistics);
        require(option->isVisibleTo(page), "statistics option comes with a view for them");
        information->setDocumentsWithEditors({});
        require(!option->isVisibleTo(page), "statistics option goes without a view for them");
        // Opening the page puts the option back to what this build has.
        information->setDocumentsWithEditors(available ? QSet<QString>() : withStatistics);
        navigator->setCurrentIndex(proxyIndex(scriptUuid));
        navigator->setCurrentIndex(proxyIndex(set.uuid));
        QApplication::processEvents();
        require(panel->currentEditor() == page && option->isVisibleTo(page) == available,
                "opening the page sets the statistics option for this build");

        // The second panel uses another view of the same plugin.
        split->toggle();
        navigator->setCurrentIndex(proxyIndex(set.uuid));
        QApplication::processEvents();
        auto secondPage = secondPanel->currentEditor();
        auto secondOption = statisticsOption(secondPage);
        require(secondOption && secondOption != option
                    && secondOption->isVisibleTo(secondPage) == available,
                "statistics option of the second panel is there only with a view for them");
        split->toggle();
        QApplication::processEvents();

        require(changes == 0 && option->isChecked() == checked,
                "hiding the statistics option leaves its choice alone");
        QObject::disconnect(counter);
    }
    require(structure->itemForUuid(statisticsUuid)
                && structure->itemForUuid(statisticsUuid)->isVisible(),
            "statistics item stays visible in the project data");

    // A file is imported only to a document that has an editor. Nothing is read from the files
    // here, the dialog works with their names.
    {
        const QString first = evidence + "/first.pdf";
        const QString second = evidence + "/second.pdf";
        // The type chosen for a file is remembered, here by a build with more plugins.
        QSettings().setValue(QString("widgets/import-dialog/%1/document-type").arg(second),
                             static_cast<int>(Type::Presentation));
        ManagementLayer::ImportManager importManager(nullptr, &parent, plugins);
        importManager.import({ first, second });
        QApplication::processEvents();
        auto importDialog = parent.findChild<Ui::ImportDialog*>();
        require(importDialog, "import dialog for two PDF files");
        auto importTo = importDialog->findChild<ComboBox*>();
        require(importTo && importTo->model(), "import dialog lists the document types");
        QStringList offered;
        for (int row = 0; row < importTo->model()->rowCount(); ++row) {
            offered.append(importTo->model()->index(row, 0).data().toString());
        }
        require(offered.contains("Screenplay"), "PDF file can be imported as a screenplay");
        require(offered.contains("Presentation") == hasEditor(plugins, Type::Presentation),
                "presentation is offered for a PDF file only with an editor for it");
        const auto importOptions = importDialog->importOptions();
        require(importOptions.size() == 2, "import options for both files");
        for (const auto& options : importOptions) {
            require(hasEditor(plugins, options.documentType),
                    "file is imported to a document with an editor");
            if (options.filePath == second) {
                require((options.documentType == Type::Presentation)
                            == hasEditor(plugins, Type::Presentation),
                        "remembered presentation is kept only with an editor for it");
            }
        }
        delete importDialog;
    }
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();

    // Whatever the view switcher can list has to load, for every kind of document.
    {
        ManagementLayer::PluginsBuilder probe;
        for (const auto documentType :
             { Type::Project,           Type::Screenplay,          Type::ScreenplayTitlePage,
               Type::ScreenplaySynopsis, Type::ScreenplayTreatment, Type::ScreenplayText,
               Type::ScreenplayStatistics, Type::Characters,        Type::Character,
               Type::Locations,         Type::Location,            Type::Folder,
               Type::SimpleText,        Type::MindMap,             Type::ImagesGallery,
               Type::Presentation,      Type::RecycleBin }) {
            for (const auto& editor :
                 probe.editorsInfoFor(Domain::mimeTypeFor(documentType), false)) {
                require(probe.initPlugin(editor.mimeType), "listed editor loads");
            }
        }
    }
    std::puts("PASS: screenplay scope, keyboard editing, Undo/Redo, pagination, exports, "
              "mixed-project preservation, editors limited to what this build contains");
}
