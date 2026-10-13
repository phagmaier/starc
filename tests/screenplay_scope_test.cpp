#include <business_layer/document/text/text_document.h>
#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_fdx_exporter.h>
#include <business_layer/export/screenplay/screenplay_fountain_exporter.h>
#include <business_layer/export/screenplay/screenplay_pdf_exporter.h>
#include <business_layer/import/import_options.h>
#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_block_parser.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <interfaces/management_layer/i_document_manager.h>
#include <interfaces/ui/i_document_view.h>
#include <management_layer/content/import/import_manager.h>
#include <management_layer/content/notifications/notifications_manager.h>
#include <management_layer/content/project/project_manager.h>
#include <management_layer/content/project/project_models_facade.h>
#include <management_layer/content/projects/projects_manager.h>
#include <management_layer/content/projects/projects_model.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <management_layer/content/settings/settings_manager.h>
#include <management_layer/content/shortcuts/shortcuts_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <management_layer/plugins_builder.h>
#include <ui/import/import_dialog.h>
#include <ui/menu_view.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/project/create_document_dialog.h>
#include <ui/project/create_document_dialog_option.h>
#include <ui/project/project_navigator.h>
#include <ui/project/project_view.h>
#include <ui/projects/create_project_dialog.h>
#include <ui/projects/project_card.h>
#include <ui/settings/settings_navigator.h>
#include <ui/settings/settings_view.h>
#include <ui/widgets/app_bar/app_bar.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/drawer/drawer.h>
#include <ui/widgets/image/image_card.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/text_field/text_field.h>
#include <ui/widgets/tree/tree.h>

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScopedPointer>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QTimer>
#include <QTranslator>
#include <QTreeView>

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

void checkStoryCardsWithoutCovers(const QString& _evidence)
{
    class RenderCard : public Ui::ProjectCard
    {
    public:
        using Ui::ProjectCard::paint;
    };
    BusinessLayer::ProjectsModel projects;
    auto project = new BusinessLayer::ProjectsModelProjectItem;
    project->setProjectType(BusinessLayer::ProjectType::Local);
    project->setName("A retained story with a description");
    project->setLogline("The story card keeps its text and status.");
    projects.appendItem(project);
    RenderCard card;
    const auto index = projects.indexForItem(project);
    auto render = [&](Qt::LayoutDirection direction) {
        card.setModelItemIndex(index);
        QImage result(card.rect().size().toSize(), QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        QStyleOptionGraphicsItem option;
        option.direction = direction;
        card.paint(&painter, &option, nullptr);
        return result;
    };
    const auto path = _evidence + "/legacy-poster.png";
    QPixmap poster(200, 300);
    poster.fill(Qt::red);
    require(poster.save(path), "seed cached legacy poster");
    project->setPosterPath(path);
    for (const auto direction : { Qt::LeftToRight, Qt::RightToLeft }) {
        const auto withPoster = render(direction);
        project->setPosterPath(_evidence + "/absent-poster.png");
        require(render(direction) == withPoster,
                "story card rendering does not depend on a stored or missing cover");
        project->setPosterPath({ });
        require(render(direction) == withPoster, "story card has no default cover");
        project->setPosterPath(path);
    }
    require(QFile::exists(path) && project->posterPath() == path,
            "retiring cover rendering keeps the cached poster and its reference");
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
void noAccountUi(QWidget* _widget)
{
    const QStringList retiredClasses = { "AccountNavigator", "AccountView", "AccountToolBar",
                                        "LoginDialog", "PurchaseDialog", "TeamDialog",
                                        "SubscriptionWidget", "SubscriptionView", "SessionWidget" };
    for (auto object : _widget->findChildren<QObject*>()) {
        const QString name = object->metaObject()->className();
        for (const auto& retired : retiredClasses) {
            require(!name.contains(retired), "no retired upstream account widget");
        }
    }
    auto checkText = [](const QString& text) {
        for (const auto& retired : { "Sign in", "Subscribe to", "Upgrade to", "CREATOR",
                                    "subscription", "Choose the team", "Save story in the cloud" }) {
            require(!text.contains(retired, Qt::CaseInsensitive), "no account or purchase offer");
        }
    };
    for (auto label : _widget->findChildren<AbstractLabel*>()) {
        checkText(label->text());
    }
    for (auto drawer : _widget->findChildren<Drawer*>()) {
        for (auto action : drawer->actions()) {
            checkText(action->text());
        }
    }
}

void noNotificationsUi(QWidget* _widget)
{
    auto checkText = [](const QString& text) {
        require(!text.contains("notification", Qt::CaseInsensitive)
                    && !text.contains("developers version", Qt::CaseInsensitive),
                "no retired notifications label or filter");
    };
    for (auto object : _widget->findChildren<QObject*>()) {
        const QString name = object->metaObject()->className();
        require(!name.contains("ReleaseView") && !name.contains("CreditsView"),
                "no notification release or credits widget");
        if (auto action = qobject_cast<QAction*>(object)) {
            checkText(action->text());
            checkText(action->toolTip());
        }
    }
    for (auto label : _widget->findChildren<AbstractLabel*>()) {
        checkText(label->text());
    }
    // Drawer actions have no QObject parent; inspect its offered actions explicitly.
    for (auto drawer : _widget->findChildren<Drawer*>()) {
        for (auto action : drawer->actions()) {
            checkText(action->text());
            checkText(action->toolTip());
        }
    }
}

void checkRetainedUpdateFeed()
{
    using NotificationType = Domain::NotificationType;
#if defined(Q_OS_LINUX)
    const auto stable = NotificationType::UpdateStableQt6Linux;
    const auto otherPlatform = NotificationType::UpdateStableQt6Mac;
    const auto oldQt = NotificationType::UpdateStableQt5Linux;
#elif defined(Q_OS_MACOS)
    const auto stable = NotificationType::UpdateStableQt6Mac;
    const auto otherPlatform = NotificationType::UpdateStableQt6Linux;
    const auto oldQt = NotificationType::UpdateStableQt5Mac;
#else
    const auto stable = NotificationType::UpdateStableQt6Windows;
    const auto otherPlatform = NotificationType::UpdateStableQt6Linux;
    const auto oldQt = NotificationType::UpdateStableQt5Windows64;
#endif
    const QString devKey = "/managers/notifications/show-dev-versions";
    const QString readKey = "/managers/notifications/last-read-notification-datetime";
    const auto readTime = QDateTime::fromString("2026-10-01T12:00:00Z", Qt::ISODate);
    QSettings preferences;
    preferences.setValue(devKey, true);
    preferences.setValue(readKey, readTime);

    ManagementLayer::NotificationsManager feed;
    require(feed.metaObject()->indexOfSignal(
                "showNotificationsRequested(QVector<Domain::Notification>)") < 0
                && feed.metaObject()->indexOfSignal("hasUnreadNotificationsChanged(bool)") < 0,
            "retained update feed has no page or unread callbacks");
    require(feed.lastVersionDownloadLink().isEmpty(), "empty retained update feed");
    const QString ignored = R"({"download_link":"https://ignored.invalid/update"})";
    feed.processNotifications({ { NotificationType::CreditsAdded, ignored, readTime },
                                { NotificationType::UpdateDevLinux, ignored, readTime },
                                { NotificationType::UpdateDevMac, ignored, readTime },
                                { NotificationType::UpdateDevWindows, ignored, readTime },
                                { NotificationType::ProSubscriptionEnds, ignored, readTime },
                                { otherPlatform, ignored, readTime },
                                { oldQt, ignored, readTime } });
    require(feed.lastVersionDownloadLink().isEmpty(),
            "updater ignores retired notices, dev releases and incompatible stable releases");
    const Domain::Notification older = {
        stable, R"({"download_link":"https://update.invalid/older"})", readTime
    };
    const Domain::Notification latest = {
        stable, R"({"download_link":"https://update.invalid/latest"})", readTime.addDays(1)
    };
    feed.processNotifications({ latest, older });
    require(feed.lastVersionDownloadLink() == "https://update.invalid/latest",
            "updater retains newest applicable stable release link");
    feed.processNotifications({ older });
    feed.processNotifications({});
    require(feed.lastVersionDownloadLink() == "https://update.invalid/latest",
            "duplicate and empty feed deliveries retain the update link");
    feed.processNotifications({ { stable, R"({"download_link":"https://update.invalid/newer"})",
                                  readTime.addDays(2) } });
    require(feed.lastVersionDownloadLink() == "https://update.invalid/newer",
            "later stable release replaces the retained update link");
    require(preferences.value(devKey).toBool()
                && preferences.value(readKey).toDateTime() == readTime,
            "retired notification preferences remain stored and ignored");
    ManagementLayer::NotificationsManager malformed;
    malformed.processNotifications({ { stable, "invalid JSON", readTime } });
    require(malformed.lastVersionDownloadLink().isEmpty(), "invalid update metadata has no link");
}

// The interface is English only: one embedded catalog, which still supplies the plural forms.
void checkEnglishOnlyCatalog()
{
    require(QDir(":/translations").entryList() == QStringList{ "translation_en.qm" },
            "only the English catalog is embedded");
    QTranslator english;
    require(english.load(":/translations/translation_en.qm"), "embedded English catalog loads");
    const auto context = "Ui::ScreenplayTextStructureView";
    require(QCoreApplication::translate(context, "%n scene(s)", "", 2) == "2 scene(s)",
            "plural forms come from the catalog, not from the source text");
    require(QApplication::installTranslator(&english), "install the English catalog");
    require(QCoreApplication::translate(context, "%n scene(s)", "", 1) == "1 scene"
                && QCoreApplication::translate(context, "%n scene(s)", "", 2) == "2 scenes"
                && QCoreApplication::translate(context, "%n word(s)", "", 1) == "1 word"
                && QCoreApplication::translate(context, "%n page(s)", "", 3) == "3 pages",
            "English catalog supplies singular and plural forms");
    require(QApplication::removeTranslator(&english), "remove the English catalog");
}

// Settings offer no interface language, and a choice stored by an earlier version is kept as is.
void checkNoInterfaceLanguageChoice(const ManagementLayer::SettingsManager& _settings,
                                    const QVariantMap& _storedLanguage)
{
    const auto view = _settings.view();
    require(_settings.metaObject()->indexOfSignal("applicationLanguageChanged(QLocale::Language)")
                    < 0
                && _settings.metaObject()->indexOfSignal("applicationLanguageFileChanged(QString)")
                    < 0
                && view->metaObject()->indexOfSignal("applicationLanguagePressed()") < 0,
            "settings have no interface language callbacks");
    for (auto object : view->findChildren<QObject*>()) {
        const QString name = object->metaObject()->className();
        require(!name.contains("LanguageDialog") && !name.contains("PercentRadioButton"),
                "no interface language chooser");
    }
    for (auto label : view->findChildren<AbstractLabel*>()) {
        require(label->text() != "Language", "settings have no interface language row");
    }
    // The settings screen used to reread the stored language on every language change.
    QEvent languageChange(QEvent::LanguageChange);
    QApplication::sendEvent(view, &languageChange);
    for (auto stored = _storedLanguage.begin(); stored != _storedLanguage.end(); ++stored) {
        // A value read back from the settings file is text.
        require(settingsValue(stored.key()).toString() == stored.value().toString(),
                "stored interface language preference remains stored and ignored");
    }
}

// Names of the retired formats and of the removed assistant must not be offered anywhere.
void requireRetained(const QString& _text)
{
    for (const auto retired : { "comic", "audioplay", "audio drama", "audio play", "stageplay",
                                "stage play", "novel", "ai assistant" }) {
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
Ui::ScreenplayTextEdit* screenplayEdit(const QUuid& _uuid = {})
{
    const auto widgets = QApplication::allWidgets();
    for (auto widget : widgets) {
        if (auto edit = qobject_cast<Ui::ScreenplayTextEdit*>(widget)) {
            auto document = qobject_cast<BusinessLayer::TextDocument*>(edit->document());
            if (_uuid.isNull()
                || (document && document->model() && document->model()->document()
                    && document->model()->document()->uuid() == _uuid)) {
                return edit;
            }
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

BusinessLayer::StructureModelItem* childOfType(BusinessLayer::StructureModelItem* _parent,
                                               Domain::DocumentObjectType _type)
{
    if (_parent == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < _parent->childCount(); ++i) {
        if (_parent->childAt(i)->type() == _type) {
            return _parent->childAt(i);
        }
    }
    return nullptr;
}

// Exercise the actual dynamically loaded view, not just the public number calculator. A named
// slice can run alone against an executable whose plugins directory omits the statistics view.
void checkStatistics(const QString& _evidence, const QString& _slice)
{
    using Type = Domain::DocumentObjectType;
    const bool all = _slice.isEmpty();
    const QString path = _evidence + "/statistics-project.starc";
    DatabaseLayer::Database::setCurrentFile(path);
    ManagementLayer::PluginsBuilder plugins;
    QWidget host;
    ManagementLayer::ProjectManager manager(nullptr, &host, plugins);
    BusinessLayer::StructureModel* structure = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::structureModelChanged,
                     [&](BusinessLayer::AbstractModel* _model) {
                         structure = qobject_cast<BusinessLayer::StructureModel*>(_model);
                     });
    BusinessLayer::ProjectsModelProjectItem project;
    project.setProjectType(BusinessLayer::ProjectType::Local);
    project.setPath(path);
    project.setName("Statistics regression");
    auto storage = DataStorageLayer::StorageFacade::documentStorage();
    const auto reopen = [&] {
        manager.closeCurrentProject(path);
        storage->clear();
        DatabaseLayer::Database::closeCurrentFile();
        DatabaseLayer::Database::setCurrentFile(path);
        manager.loadCurrentProject(&project);
    };
    manager.loadCurrentProject(&project);
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    reopen(); // Finish the retained upstream version upgrade before comparing document bytes.
    auto story = structure->itemForIndex(structure->addDocument(Type::Screenplay, "Known script"));
    require(story, "statistics fixture screenplay");
    auto statistics = childOfType(story, Type::ScreenplayStatistics);
    if (all || _slice == "creation") {
        require(statistics, "statistics view restores Statistics for new screenplays");
    }
    // The disabled-view control still needs a statistics item to try to select.
    if (statistics == nullptr) {
        statistics = new BusinessLayer::StructureModelItem(
            QUuid::createUuid(), Type::ScreenplayStatistics, "Statistics", "First draft", {},
            true, false, false);
        structure->appendItem(statistics, story);
    }
    const auto statisticsUuid = statistics->uuid();
    const auto scriptUuid = childOfType(story, Type::ScreenplayText)->uuid();
    auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    const auto select = [&](const QUuid& _uuid) {
        navigator->setCurrentIndex(proxy->mapFromSource(
            structure->indexForItem(structure->itemForUuid(_uuid))));
        QApplication::processEvents();
    };
    select(scriptUuid);
    auto edit = screenplayEdit(scriptUuid);
    require(edit, "statistics fixture uses real screenplay editor");
    const auto paragraph = [&](BusinessLayer::TextParagraphType _type, const QString& _text) {
        edit->setCurrentParagraphType(_type);
        type(edit, _text);
        key(edit, Qt::Key_Return, "\r");
    };
    paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. ROOM - DAY");
    paragraph(BusinessLayer::TextParagraphType::Action, "A door opens.");
    paragraph(BusinessLayer::TextParagraphType::Character, "ALICE");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "Hello Bob.");
    paragraph(BusinessLayer::TextParagraphType::Character, "BOB");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "Hello Alice.");
    paragraph(BusinessLayer::TextParagraphType::SceneHeading, "EXT. PARK - NIGHT");
    paragraph(BusinessLayer::TextParagraphType::Character, "ALICE");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "See you tomorrow.");
    pause(1000);
    manager.saveChanges();
    DatabaseLayer::Database::commit();

    if (all || _slice == "restore") {
        // Mimic a screenplay created by a build with no statistics view, nested in a folder.
        structure->setOmittedDocumentTypes({ Type::ScreenplayStatistics });
        const auto folder = structure->addDocument(Type::Folder, "Old scripts");
        const auto missing = structure->addDocument(Type::Screenplay, "Missing statistics", folder);
        const auto missingUuid = structure->itemForIndex(missing)->uuid();
        require(!childOfType(structure->itemForUuid(missingUuid), Type::ScreenplayStatistics),
                "seed screenplay from a build without statistics");
        const auto missingScriptUuid
            = childOfType(structure->itemForUuid(missingUuid), Type::ScreenplayText)->uuid();
        select(missingScriptUuid);
        edit = screenplayEdit(missingScriptUuid);
        require(edit, "old screenplay without statistics opens in the text editor");
        paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. OLD ROOM - DAY");
        paragraph(BusinessLayer::TextParagraphType::Character, "ALICE");
        paragraph(BusinessLayer::TextParagraphType::Dialogue, "Keep this older screenplay intact.");
        pause(1000);
        structure->setOmittedDocumentTypes({});
        const auto hidden = structure->addDocument(Type::Screenplay, "Hidden statistics");
        auto hiddenStatistics
            = childOfType(structure->itemForIndex(hidden), Type::ScreenplayStatistics);
        hiddenStatistics->setVisible(false);
        structure->updateItem(hiddenStatistics);
        const auto hiddenUuid = hiddenStatistics->uuid();
        structure->saveChanges();
        manager.saveChanges();
        const QByteArray oldStatistics = "<old-statistics unknown='preserve'/>\r\n";
        storage->document(hiddenUuid)->setContent(oldStatistics);
        require(storage->saveDocument(storage->document(hiddenUuid)), "seed old statistics bytes");
        DatabaseLayer::Database::commit();
        QHash<QUuid, QByteArray> original;
        for (auto document : storage->documents()) {
            if (document->type() != Type::Structure) {
                original.insert(document->uuid(), document->content());
            }
        }
        reopen();
        auto restored
            = childOfType(structure->itemForUuid(missingUuid), Type::ScreenplayStatistics);
        require(restored, "statistics view restores the missing item on project load");
        const auto restoredUuid = restored->uuid();
        require(storage->documents().size() == original.size() + 2,
                "restoration adds only one document beside the existing structure");
        require(structure->itemForUuid(hiddenUuid)
                    && !structure->itemForUuid(hiddenUuid)->isVisible(),
                "restoration preserves existing statistics identity and hidden choice");
        structure->saveChanges();
        manager.saveChanges();
        DatabaseLayer::Database::commit();
        for (auto it = original.cbegin(); it != original.cend(); ++it) {
            require(storage->document(it.key())->content() == it.value(),
                    "restoration leaves every other document byte exact");
        }
        reopen();
        require(childOfType(structure->itemForUuid(missingUuid), Type::ScreenplayStatistics)->uuid()
                    == restoredUuid,
                "restoration is idempotent after save and reopen");
        require(storage->document(hiddenUuid)->content() == oldStatistics,
                "hidden old statistics bytes survive restoration and reopen");
        for (auto it = original.cbegin(); it != original.cend(); ++it) {
            require(storage->document(it.key())->content() == it.value(),
                    "restoration byte preservation survives reopen");
        }
    }

    if (all || _slice == "reports" || _slice == "navigator") {
        QHash<QUuid, QByteArray> original;
        for (auto document : storage->documents()) {
            original.insert(document->uuid(), document->content());
        }
        select(statisticsUuid);
        const auto table = [&](const char* _name) {
            auto result = manager.view()->findChild<QTreeView*>(QLatin1String(_name));
            require(result && result->model(),
                    "statistics selection displays computed report tables");
            return result;
        };
        if (all || _slice == "reports") {
            auto scenes = table("sceneReport")->model();
            require(scenes->rowCount() == 2, "statistics reports the two typed scenes");
            require(scenes->index(0, 0).data().toString() == "INT. ROOM - DAY"
                        && scenes->index(1, 0).data().toString() == "EXT. PARK - NIGHT"
                        && scenes->index(0, 3).data().toInt() == 2
                        && scenes->index(1, 3).data().toInt() == 1,
                    "statistics scene names and character counts match typed script");
            auto cast = table("castReport")->model();
            require(cast->rowCount() == 2, "statistics reports two distinct characters");
            QHash<QString, QVector<int>> counts;
            for (int row = 0; row < cast->rowCount(); ++row) {
                counts.insert(cast->index(row, 0).data().toString(),
                              { cast->index(row, 1).data().toInt(),
                                cast->index(row, 2).data().toInt(),
                                cast->index(row, 3).data().toInt() });
            }
            require(counts.value("ALICE") == QVector<int>({ 5, 2, 2 })
                        && counts.value("BOB") == QVector<int>({ 2, 1, 1 }),
                    "statistics cast words, dialogues and speaking scenes match typed script");
            auto dialogues = table("dialoguesReport")->model();
            require(dialogues->rowCount() == 2
                        && dialogues->rowCount(dialogues->index(0, 0)) == 2
                        && dialogues->rowCount(dialogues->index(1, 0)) == 1
                        && dialogues->index(0, 1, dialogues->index(1, 0)).data().toString()
                            == "See you tomorrow.",
                    "statistics dialogue hierarchy contains the three typed lines");
            require(table("locationReport")->model()->rowCount() == 2,
                    "statistics reports the two typed locations");
            int sceneCount = 0;
            auto summary = table("summaryScenes")->model();
            for (int row = 0; row < summary->rowCount(); ++row) {
                sceneCount += summary->index(row, 1).data().toInt();
            }
            require(sceneCount == 2, "statistics summary counts the typed scenes");
            require(table("genderCharacters")->model()->rowCount() > 0,
                    "statistics gender table is populated");
        }
        if (all || _slice == "navigator") {
            const QString viewMime = "application/x-starc/view/screenplay/statistics";
            const QString navigatorMime = "application/x-starc/navigator/screenplay/statistics";
            require(plugins.initPlugin(viewMime) && plugins.initPlugin(navigatorMime),
                    "statistics view and navigator plugins load");
            auto navigation = plugins.plugin(navigatorMime);
            navigation->view(nullptr);
            plugins.bind(viewMime, navigatorMime);
            auto selector = manager.view()->findChild<QComboBox*>("statisticsReportSelector");
            require(selector, "statistics view has a report selector");
            auto navigationTrees = navigation->view()->asQWidget()->findChildren<Tree*>();
            require(navigationTrees.size() == 2, "statistics navigator has reports and plots");
            for (auto tree : navigationTrees) {
                const bool reports = tree->model()->rowCount() == 6;
                for (int row = 0; row < tree->model()->rowCount(); ++row) {
                    emit tree->clicked(tree->model()->index(row, 0), true);
                    require(selector->currentIndex() == (reports ? row : row + 6),
                            "statistics navigator selects the requested report or plot");
                }
            }
        }
        manager.saveChanges();
        DatabaseLayer::Database::commit();
        for (auto it = original.cbegin(); it != original.cend(); ++it) {
            require(storage->document(it.key())->content() == it.value(),
                    "viewing statistics does not rewrite project documents");
        }
        select(scriptUuid);
        edit = screenplayEdit(scriptUuid);
        require(edit, "screenplay editor reopens after viewing statistics");
        paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. OFFICE - DAY");
        pause(1000);
        select(statisticsUuid);
        require(table("sceneReport")->model()->rowCount() == 3,
                "returning to statistics recomputes after a script edit");
    }
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
}
void checkCharacterRenameModel()
{
    using namespace BusinessLayer;
    ScreenplayInformationModel information;
    ScreenplayTextModel model;
    model.setInformationModel(&information);
    auto add = [&](TextParagraphType _type, const QString& _text) {
        auto item = new ScreenplayTextModelTextItem(&model);
        item->setParagraphType(_type);
        item->setText(_text);
        model.appendItem(item);
        return item;
    };
    bool passed = true;
    const auto check = [&](bool _ok, const char* _label) {
        std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
        passed &= _ok;
    };
    const QVector<QPair<QString, QString>> names = {
        { "ZOË", "Zoë ZOË zoë Zoëy" },
        { "ÉMILE", "Émile ÉMILE émile PréÉmile" },
        { "2ND COP", "2ND COP 2Nd Cop 2nd cop" },
        { "A|B", "A|B A|b a|b A B" },
        { "$AGENT", "$AGENT $Agent $agent" },
        { "WILL", "Will WILL will Willé ÉWill Will\u0301 Will_ Will2" },
    };
    for (const auto& fixture : names) {
        auto item = add(TextParagraphType::Action, fixture.second);
        check(model.characterMentionsCount(fixture.first) == 2,
              qPrintable("literal Unicode mention count for " + fixture.first));
        model.updateCharacterName(fixture.first, "BOB", true);
        check(item->text().count("Bob") + item->text().count("BOB") == 2,
              qPrintable("count agrees with Unicode replacements for " + fixture.first));
        model.removeItem(item);
    }

    auto cue = add(TextParagraphType::Character, "  WILL  (V.O.)");
    auto list = add(TextParagraphType::SceneCharacters, "ALICE,  WILL (V.O.), WILL, YOUNG WILL");
    auto action = add(TextParagraphType::Action, "Will sees Will. Tail.");
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    QTextCharFormat italic;
    italic.setFontItalic(true);
    QTextCharFormat underline;
    underline.setFontUnderline(true);
    action->setFormats({ { 0, 4, bold }, { 5, 4, italic }, { 16, 4, underline } });
    auto expectedFormats = action->formats();
    TextModelTextItem::ReviewMark nameMark;
    nameMark.from = 10;
    nameMark.length = 4;
    nameMark.backgroundColor = Qt::yellow;
    nameMark.isDone = true;
    TextModelTextItem::ReviewComment comment;
    comment.author = "Reviewer";
    comment.authorEmail = "reviewer@example.invalid";
    comment.date = "2026-10-12T00:00:00";
    comment.text = "Keep this comment on the name";
    nameMark.comments.append(comment);
    auto tailMark = nameMark;
    tailMark.from = 16;
    tailMark.isDone = false;
    action->setReviewMarks(QVector<TextModelTextItem::ReviewMark>{ nameMark, tailMark });
    auto expectedReviews = action->reviewMarks();
    TextModelTextItem::ResourceMark resource;
    resource.from = 16;
    resource.length = 4;
    resource.uuid = QUuid::createUuid();
    action->setResourceMarks(QVector<TextModelTextItem::ResourceMark>{ resource });
    auto expectedResources = action->resourceMarks();
    model.updateItem(action);

    model.updateCharacterName("WILL", "WILLIAM", true);
    check(cue->text() == "  WILLIAM  (V.O.)", "cue whitespace and extension survive renaming");
    check(list->text() == "ALICE,  WILLIAM (V.O.), WILLIAM, YOUNG WILL",
          "every exact scene-character entry is renamed with its whitespace intact");
    check(action->text() == "William sees William. Tail.", "both formatted mentions are renamed");
    expectedFormats[0].length = 7;
    expectedFormats[1].from = 8;
    expectedFormats[2].from = 22;
    expectedReviews[0].from = 13;
    expectedReviews[0].length = 7;
    expectedReviews[1].from = 22;
    expectedResources[0].from = 22;
    check(action->formats() == expectedFormats, "growing names preserve emphasis on the same text");
    check(action->reviewMarks() == expectedReviews,
          "growing names preserve review ranges, comments and status");
    check(action->resourceMarks() == expectedResources, "growing names preserve resource links");

    model.updateCharacterName("WILLIAM", "BO", true);
    expectedFormats[0].length = 2;
    expectedFormats[1].from = 3;
    expectedFormats[2].from = 12;
    expectedReviews[0].from = 8;
    expectedReviews[0].length = 2;
    expectedReviews[1].from = 12;
    expectedResources[0].from = 12;
    check(action->text() == "Bo sees Bo. Tail." && action->formats() == expectedFormats
              && action->reviewMarks() == expectedReviews
              && action->resourceMarks() == expectedResources,
          "shortening names preserves all attached content");
    require(passed, "character rename model regressions");
}

void checkLocationRenameModel()
{
    using namespace BusinessLayer;
    struct LocationFixture {
        QString heading;
        QString oldName;
        QString expected;
    };
    const QVector<LocationFixture> locationFixtures = {
        { "INT. INT - DAY", "INT", "INT. ROOM - DAY" },
        { "EXT. EXT - NIGHT", "EXT", "EXT. ROOM - NIGHT" },
        { "INT./EXT. EXT - DAY", "EXT", "INT./EXT. ROOM - DAY" },
        { "  EXT.  EXT  -- LATER  ", "EXT", "  EXT.  ROOM  -- LATER  " },
        { "INT. int - DAY", "INT", "INT. ROOM - DAY" },
        { "INT. INT. GATE - NIGHT", "INT. GATE", "INT. ROOM - NIGHT" },
        { "INT. INT  GATE - DAY", "INT GATE", "INT. ROOM - DAY" },
        { "EXT. EXT\tGATE -- DAWN", "EXT GATE", "EXT. ROOM -- DAWN" },
        { "INT. NIGHT.NIGHT", "NIGHT", "INT. ROOM.NIGHT" },
        { "EXT. DAY. DAY", "DAY", "EXT. ROOM. DAY" },
        { "INT. A|B - DAY", "A|B", "INT. ROOM - DAY" },
        { "INT. ÉTÉ - DAY", "ÉTÉ", "INT. ROOM - DAY" },
        { "ﬃ. ﬃ - DAY", "FFI", "ﬃ. ROOM - DAY" },
    };
    for (const auto& fixture : locationFixtures) {
        ScreenplayInformationModel information;
        ScreenplayTextModel model;
        model.setInformationModel(&information);
        auto heading = new ScreenplayTextModelTextItem(&model);
        heading->setParagraphType(TextParagraphType::SceneHeading);
        heading->setText(fixture.heading);
        model.appendItem(heading);
        require(ScreenplaySceneHeadingParser::location(fixture.heading) == fixture.oldName,
                "location fixture agrees with the existing heading parser");
        model.updateLocationName(fixture.oldName, "ROOM");
        require(
            heading->text() == fixture.expected
                && ScreenplaySceneHeadingParser::location(heading->text()) == "ROOM",
            qPrintable("rename preserves heading prefix, separators and time: " + fixture.heading));
    }
    // Include annotations before, on, after, across and strictly inside the replaced name.
    const QVector<QPair<int, int>> originalRanges
        = { { 0, 3 }, { 5, 5 }, { 13, 3 }, { 3, 11 }, { 7, 2 } };
    for (const QString& newName : { QString("HO"), QString("LIVING ROOM"), QString("CABIN") }) {
        ScreenplayInformationModel information;
        ScreenplayTextModel model;
        model.setInformationModel(&information);
        auto heading = new ScreenplayTextModelTextItem(&model);
        heading->setParagraphType(TextParagraphType::SceneHeading);
        heading->setText("INT. HOUSE - DAY");
        model.appendItem(heading);
        auto action = new ScreenplayTextModelTextItem(&model);
        action->setParagraphType(TextParagraphType::Action);
        action->setText("INT. HOUSE - DAY");
        model.appendItem(action);
        QVector<TextModelTextItem::TextFormat> formats;
        QVector<TextModelTextItem::ReviewMark> reviews;
        QVector<TextModelTextItem::ResourceMark> resources;
        for (const auto& range : originalRanges) {
            TextModelTextItem::TextFormat format;
            format.from = range.first;
            format.length = range.second;
            format.isBold = true;
            formats.append(format);
            TextModelTextItem::ReviewMark review;
            review.from = range.first;
            review.length = range.second;
            review.backgroundColor = Qt::yellow;
            review.textColor = Qt::black;
            review.isDone = true;
            TextModelTextItem::ReviewComment comment;
            comment.author = "Reviewer";
            comment.authorEmail = "reviewer@example.invalid";
            comment.date = "2026-10-12T00:00:00";
            comment.text = "Keep this annotation";
            review.comments.append(comment);
            reviews.append(review);
            TextModelTextItem::ResourceMark resource;
            resource.from = range.first;
            resource.length = range.second;
            resource.uuid = QUuid::createUuid();
            resources.append(resource);
        }
        QVector<QTextLayout::FormatRange> layoutFormats;
        for (const auto& format : formats) {
            layoutFormats.append({ format.from, format.length, format.charFormat() });
        }
        heading->setFormats(layoutFormats);
        // setFormats records canonical font metadata as well as the supplied emphasis.
        formats = heading->formats();
        require(formats.size() == originalRanges.size(), "model fixture stores every format range");
        for (int i = 0; i < originalRanges.size(); ++i) {
            require(formats[i].from == originalRanges[i].first
                        && formats[i].length == originalRanges[i].second && formats[i].isBold,
                    "model fixture stores the intended emphasis and ranges");
        }
        heading->setReviewMarks(reviews);
        heading->setResourceMarks(resources);
        model.updateItem(heading);

        const int length = newName.length();
        const QVector<QPair<int, int>> expectedRanges
            = { { 0, 3 }, { 5, length }, { 8 + length, 3 }, { 3, 6 + length }, { 5, length } };
        for (int i = 0; i < expectedRanges.size(); ++i) {
            formats[i].from = reviews[i].from = resources[i].from = expectedRanges[i].first;
            formats[i].length = reviews[i].length = resources[i].length = expectedRanges[i].second;
        }
        model.updateLocationName("HOUSE", newName);
        require(heading->text() == "INT. " + newName + " - DAY"
                    && action->text() == "INT. HOUSE - DAY",
                "location renaming changes scene headings only");
        require(heading->formats() == formats && heading->reviewMarks() == reviews
                    && heading->resourceMarks() == resources,
                qPrintable("location annotation ranges and payloads survive rename to " + newName));
    }
}

// The pages of characters and locations: what they show and store, renaming through the
// screenplay, the names they refuse, and the documents they leave as they are.
void checkCharacterAndLocationPages(const QString& _evidence)
{
    using Type = Domain::DocumentObjectType;
    const QString path = _evidence + "/pages-project.starc";
    DatabaseLayer::Database::setCurrentFile(path);
    ManagementLayer::PluginsBuilder plugins;
    require(hasEditor(plugins, Type::Character) && hasEditor(plugins, Type::Location),
            "this build opens characters and locations");
    for (const auto type : { Type::Character, Type::Location }) {
        const auto pages = plugins.editorsInfoFor(Domain::mimeTypeFor(type), false);
        require(plugins.navigatorMimeTypeFor(pages.first().mimeType).isEmpty(),
                "a character or location page has no navigator of its own");
    }
    // Plugins keep their views for as long as the process runs, so the widget that comes to
    // own them is never deleted.
    auto host = new QWidget;
    host->resize(1280, 800);
    ManagementLayer::ProjectManager manager(nullptr, host, plugins);
    BusinessLayer::StructureModel* structure = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::structureModelChanged,
                     [&](BusinessLayer::AbstractModel* _model) {
                         structure = qobject_cast<BusinessLayer::StructureModel*>(_model);
                     });
    BusinessLayer::AbstractModel* current = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::currentModelChanged,
                     [&](BusinessLayer::AbstractModel* _model) { current = _model; });
    BusinessLayer::ProjectsModelProjectItem project;
    project.setProjectType(BusinessLayer::ProjectType::Local);
    project.setPath(path);
    project.setName("Pages regression");
    auto storage = DataStorageLayer::StorageFacade::documentStorage();
    const auto reopen = [&] {
        manager.closeCurrentProject(path);
        storage->clear();
        DatabaseLayer::Database::closeCurrentFile();
        DatabaseLayer::Database::setCurrentFile(path);
        manager.loadCurrentProject(&project);
    };
    const auto save = [&] {
        pause(1000);
        manager.saveChanges();
        DatabaseLayer::Database::commit();
    };
    manager.loadCurrentProject(&project);
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    reopen(); // Finish the retained upstream version upgrade before comparing document bytes.
    auto story = structure->itemForIndex(structure->addDocument(Type::Screenplay, "Pages script"));
    require(story, "pages fixture screenplay");
    const auto scriptUuid = childOfType(story, Type::ScreenplayText)->uuid();
    auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    const auto select = [&](const QUuid& _uuid) {
        navigator->setCurrentIndex(proxy->mapFromSource(
            structure->indexForItem(structure->itemForUuid(_uuid))));
        QApplication::processEvents();
    };
    Ui::ProjectView* panel = nullptr;
    for (auto view : manager.view()->findChildren<Ui::ProjectView*>()) {
        if (!view->isHidden()) {
            panel = view;
        }
    }
    require(panel, "pages fixture document panel");

    // The characters and locations of the project come from the script, as a writer types it.
    select(scriptUuid);
    auto edit = screenplayEdit(scriptUuid);
    require(edit, "pages fixture uses real screenplay editor");
    const auto paragraph = [&](BusinessLayer::TextParagraphType _type, const QString& _text) {
        edit->setCurrentParagraphType(_type);
        type(edit, _text);
        key(edit, Qt::Key_Return, "\r");
    };
    paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. HOUSE - DAY");
    paragraph(BusinessLayer::TextParagraphType::Action, "Alice greets Bob.");
    paragraph(BusinessLayer::TextParagraphType::Character, "ALICE");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "Hello Bob.");
    paragraph(BusinessLayer::TextParagraphType::Character, "BOB");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "Hello Alice.");
    paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. HOUSE - KITCHEN - NIGHT");
    paragraph(BusinessLayer::TextParagraphType::Character, "ALICE");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "Still here.");
    paragraph(BusinessLayer::TextParagraphType::SceneHeading, "INT. HOUSEBOAT - DAY");
    paragraph(BusinessLayer::TextParagraphType::Character, "BOB");
    paragraph(BusinessLayer::TextParagraphType::Dialogue, "On the water.");
    save();
    const auto member = [&](Type _group, const QString& _name) {
        const auto group = structure->itemForType(_group);
        for (int i = 0; group != nullptr && i < group->childCount(); ++i) {
            if (group->childAt(i)->name() == _name) {
                return group->childAt(i)->uuid();
            }
        }
        return QUuid();
    };
    const auto members = [&](Type _group) {
        QStringList names;
        const auto group = structure->itemForType(_group);
        for (int i = 0; group != nullptr && i < group->childCount(); ++i) {
            names.append(group->childAt(i)->name());
        }
        names.sort();
        return names;
    };
    const auto script = [&] { return QString::fromUtf8(storage->document(scriptUuid)->content()); };
    const auto text = [&](const QUuid& _uuid) {
        return QString::fromUtf8(storage->document(_uuid)->content());
    };
    require(members(Type::Characters) == QStringList({ "ALICE", "BOB" }),
            "characters of the typed script are collected");
    require(members(Type::Locations) == QStringList({ "HOUSE", "HOUSE - KITCHEN", "HOUSEBOAT" }),
            "locations of the typed script are collected");
    const auto aliceUuid = member(Type::Characters, "ALICE");
    const auto bobUuid = member(Type::Characters, "BOB");
    const auto houseUuid = member(Type::Locations, "HOUSE");
    const auto kitchenUuid = member(Type::Locations, "HOUSE - KITCHEN");
    const auto houseboatUuid = member(Type::Locations, "HOUSEBOAT");

    // A character an existing project describes in more detail than the page shows.
    const QString danaXml
        = "<?xml version=\"1.0\"?>\n"
          "<document mime-type=\"application/x-starc/document/character\" version=\"1.0\">\n"
          "<name><![CDATA[DANA]]></name>\n"
          "<story_role><![CDATA[1]]></story_role>\n"
          "<age><![CDATA[52]]></age>\n"
          "<gender><![CDATA[1]]></gender>\n"
          "<one_sentence_description><![CDATA[Kept description]]></one_sentence_description>\n"
          "<long_description><![CDATA[Kept notes]]></long_description>\n"
          "<dreamcast><![CDATA[Kept dreamcast]]></dreamcast>\n"
          "<nickname><![CDATA[Dee]]></nickname>\n"
          "<secrets><![CDATA[Kept secret]]></secrets>\n"
          "</document>";
    manager.storeCharacter("DANA", danaXml);
    const auto danaUuid = member(Type::Characters, "DANA");
    require(!danaUuid.isNull(), "seed a character with upstream details");

    // The page of a document shown in the panel, with its name, description and notes fields.
    const auto fieldsOf = [&](const QUuid& _uuid, const char* _page) {
        select(_uuid);
        auto page = panel->currentEditor();
        require(page && QString(page->metaObject()->className()) == _page
                    && !noEditorNoticeShown(manager.view()) && manager.currentDocument()
                    && manager.currentDocument()->uuid() == _uuid,
                "character or location opens in its page");
        require(manager.toolBar()->actions().size()
                    == 1 + plugins.editorsInfoFor(Domain::mimeTypeFor(
                                                      structure->itemForUuid(_uuid)->type()),
                                                  false)
                              .size(),
                "page is listed in the view switcher");
        const auto fields = page->findChildren<TextField*>();
        require(fields.size() == 3, "page has name, description and notes");
        return fields;
    };
    const auto enter = [&](TextField* _field, const QString& _text) {
        _field->selectAll();
        if (_text.isEmpty()) {
            key(_field, Qt::Key_Backspace);
        } else {
            type(_field, _text);
        }
    };
    const auto leave = [&](TextField* _field, Qt::FocusReason _reason) {
        QFocusEvent focusOut(QEvent::FocusOut, _reason);
        QApplication::sendEvent(_field, &focusOut);
        QApplication::processEvents();
    };

    // What a rename asks when the text of the script mentions the character: the questions
    // that are open, what one says, and the ways to answer it. Its buttons are, in this order,
    // "Leave the text" and "Rename in the text too".
    const auto questions = [&] {
        QApplication::processEvents();
        QVector<Dialog*> open;
        for (auto dialog : host->findChildren<Dialog*>()) {
            if (!dialog->isHidden()) {
                open.append(dialog);
            }
        }
        return open;
    };
    const auto question = [&]() -> Dialog* {
        const auto open = questions();
        return open.size() == 1 ? open.constFirst() : nullptr;
    };
    const auto questionText = [](Dialog* _question) {
        QStringList texts;
        for (auto label : _question->findChildren<AbstractLabel*>()) {
            texts.append(label->text());
        }
        return texts.join("\n");
    };
    enum Answer { LeaveText, RenameInText };
    const auto answer = [](Dialog* _question, Answer _answer) {
        const auto buttons = _question->findChildren<Button*>();
        if (buttons.size() != 2) {
            return false;
        }
        buttons[_answer]->click();
        QApplication::processEvents();
        return true;
    };
    const auto clickOutside = [&](Dialog* _question) {
        // A click that only brings the application forward is ignored for a second.
        pause(1100);
        const QPoint corner(_question->width() - 5, _question->height() - 5);
        QMouseEvent press(QEvent::MouseButtonPress, corner, _question->mapToGlobal(corner),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(_question, &press);
        QApplication::processEvents();
    };

    // What an existing project holds is shown, and the details the page has no fields for
    // stay in the document when the page changes it.
    auto fields = fieldsOf(danaUuid, "Ui::CharacterInformationView");
    require(fields[0]->text() == "DANA" && fields[1]->text() == "Kept description"
                && fields[2]->text() == "Kept notes",
            "page shows the stored name, description and notes");
    fields[2]->moveCursor(QTextCursor::End);
    type(fields[2], " Added.");
    save();
    for (const auto kept :
         { "<name><![CDATA[DANA]]></name>", "<story_role><![CDATA[1]]></story_role>",
           "<age><![CDATA[52]]></age>", "<gender><![CDATA[1]]></gender>",
           "<one_sentence_description><![CDATA[Kept description]]></one_sentence_description>",
           "<long_description><![CDATA[Kept notes Added.]]></long_description>",
           "<dreamcast><![CDATA[Kept dreamcast]]></dreamcast>",
           "<nickname><![CDATA[Dee]]></nickname>", "<secrets><![CDATA[Kept secret]]></secrets>" }) {
        if (!text(danaUuid).contains(kept)) {
            std::fprintf(stderr, "missing %s in %s\n", kept, qPrintable(text(danaUuid)));
        }
        require(text(danaUuid).contains(kept), "editing notes keeps the other character details");
    }

    // Description and notes are stored as they are typed.
    fields = fieldsOf(aliceUuid, "Ui::CharacterInformationView");
    auto alice = qobject_cast<BusinessLayer::CharacterModel*>(current);
    require(alice && fields[0]->text() == "ALICE" && fields[1]->text().isEmpty()
                && fields[2]->text().isEmpty(),
            "page of a collected character starts with its name only");
    type(fields[1], "A retired pilot.");
    type(fields[2], "Afraid of heights.");
    key(fields[2], Qt::Key_Return, "\r");
    type(fields[2], "Keeps a logbook.");
    require(alice->oneSentenceDescription() == "A retired pilot."
                && alice->longDescription() == "Afraid of heights.\nKeeps a logbook.",
            "typed description and notes reach the character");

    // A name is applied when it is confirmed, not while it is typed: renaming changes the
    // screenplay.
    const auto scriptBefore = script();
    require(scriptBefore.count("ALICE") == 2 && scriptBefore.contains("Alice greets Bob.")
                && scriptBefore.contains("Hello Alice."),
            "script names the character before renaming");
    enter(fields[0], "alicia");
    save();
    require(alice->name() == "ALICE" && structure->itemForUuid(aliceUuid)->name() == "ALICE"
                && script() == scriptBefore,
            "typing a name renames nothing yet");
    key(fields[0], Qt::Key_Return, "\r");
    // The action and a dialogue line name Alice too, so the rename asks before it changes
    // anything. Told to rename in the text too, it changes the whole script, which it used to
    // do without asking.
    auto asked = question();
    require(asked && questionText(asked).contains("ALICE")
                && questionText(asked).contains("ALICIA")
                && questionText(asked).contains("2 times"),
            "Enter asks about the two mentions of the character first");
    save();
    require(alice->name() == "ALICE" && structure->itemForUuid(aliceUuid)->name() == "ALICE"
                && script() == scriptBefore,
            "nothing is renamed while the question is open");
    require(answer(asked, RenameInText), "answer: rename in the text too");
    save();
    require(question() == nullptr, "answered question closes");
    require(alice->name() == "ALICIA" && fields[0]->text() == "ALICIA"
                && fields[0]->error().isEmpty()
                && structure->itemForUuid(aliceUuid)->name() == "ALICIA",
            "Enter renames the character, in capitals");
    require(script().count("ALICIA") == 2 && !script().contains("ALICE")
                && script().contains("Alicia greets Bob.") && script().contains("Hello Alicia.")
                && !script().contains("Alice") && script().count("BOB") == 2
                && script().contains("Hello Bob."),
            "renaming a character with its mentions renames it all through the screenplay");
    require(members(Type::Characters) == QStringList({ "ALICIA", "BOB", "DANA" })
                && alice->oneSentenceDescription() == "A retired pilot.",
            "renamed character stays one document with its description");

    // The name of another character is refused: the renamed one would be removed as its
    // duplicate. So is an empty name.
    const auto scriptRenamed = script();
    for (const QString& refused : { QString("bob"), QString("  Dana "), QString() }) {
        enter(fields[0], refused);
        key(fields[0], Qt::Key_Return, "\r");
        save();
        require(!fields[0]->error().isEmpty() && alice->name() == "ALICIA"
                    && members(Type::Characters) == QStringList({ "ALICIA", "BOB", "DANA" })
                    && storage->document(aliceUuid) && storage->document(bobUuid)
                    && storage->document(danaUuid) && script() == scriptRenamed
                    && alice->longDescription() == "Afraid of heights.\nKeeps a logbook.",
                "taken or empty name is refused and removes nothing");
        leave(fields[0], Qt::TabFocusReason);
        require(!fields[0]->error().isEmpty() && alice->name() == "ALICIA",
                "refused name stays refused when the field is left");
    }
    enter(fields[0], "Alicia");
    require(fields[0]->error().isEmpty(), "typing again clears the name error");
    key(fields[0], Qt::Key_Return, "\r");
    require(fields[0]->text() == "ALICIA" && fields[0]->error().isEmpty()
                && script() == scriptRenamed,
            "confirming the current name changes nothing");

    // Moving on to another field applies the name; switching to another window or opening a
    // menu leaves it as typed.
    enter(fields[0], "Alicia Grey");
    leave(fields[0], Qt::ActiveWindowFocusReason);
    leave(fields[0], Qt::PopupFocusReason);
    require(alice->name() == "ALICIA" && fields[0]->text() == "Alicia Grey",
            "name being typed survives a window switch unapplied");
    leave(fields[0], Qt::TabFocusReason);
    // Asked in the same way. Told to leave the text, it renames the cues and keeps the two
    // mentions as they are written; it used to turn them into "Alicia Grey" unasked.
    asked = question();
    require(asked && questionText(asked).contains("2 times"),
            "leaving the name field for another one asks about the mentions too");
    require(answer(asked, LeaveText), "answer: leave the text");
    save();
    require(alice->name() == "ALICIA GREY" && fields[0]->text() == "ALICIA GREY"
                && script().count("ALICIA GREY") == 2,
            "leaving the name field for another one renames the character");
    require(script().contains("Alicia greets Bob.") && script().contains("Hello Alicia.")
                && !script().contains("Alicia Grey"),
            "mentions the writer leaves stay as they are written");

    // A page that may not be changed locks its fields and applies no name.
    auto characterPage = dynamic_cast<Ui::IDocumentView*>(panel->currentEditor());
    require(characterPage, "page is a document view");
    characterPage->setEditingMode(ManagementLayer::DocumentEditingMode::Read);
    require(fields[0]->isReadOnly() && fields[1]->isReadOnly() && fields[2]->isReadOnly(),
            "read-only page locks name, description and notes");
    leave(fields[0], Qt::TabFocusReason);
    require(alice->name() == "ALICIA GREY", "read-only page applies no name");
    characterPage->setEditingMode(ManagementLayer::DocumentEditingMode::Edit);
    require(!fields[0]->isReadOnly() && !fields[1]->isReadOnly() && !fields[2]->isReadOnly(),
            "editable page unlocks its fields");

    // A character added by hand gets the same page.
    const auto eveUuid
        = structure->itemForIndex(structure->addDocument(Type::Character, "Eve"))->uuid();
    fields = fieldsOf(eveUuid, "Ui::CharacterInformationView");
    require(fields[0]->text() == "EVE" && fields[1]->text().isEmpty(),
            "page of a character added by hand");
    type(fields[1], "The new neighbour.");

    // Locations: the page, and what renaming does to scene headings and nested locations.
    fields = fieldsOf(houseUuid, "Ui::LocationInformationView");
    auto house = qobject_cast<BusinessLayer::LocationModel*>(current);
    require(house && fields[0]->text() == "HOUSE", "page of a collected location");
    type(fields[1], "A narrow terraced house.");
    type(fields[2], "The stairs creak.");
    require(house->oneSentenceDescription() == "A narrow terraced house."
                && house->longDescription() == "The stairs creak.",
            "typed description and notes reach the location");
    require(script().contains("INT. HOUSE - DAY") && script().contains("INT. HOUSE - KITCHEN - NIGHT")
                && script().contains("INT. HOUSEBOAT - DAY"),
            "script names the locations before renaming");
    enter(fields[0], "home");
    key(fields[0], Qt::Key_Return, "\r");
    save();
    require(house->name() == "HOME" && fields[0]->text() == "HOME"
                && structure->itemForUuid(houseUuid)->name() == "HOME",
            "Enter renames the location, in capitals");
    require(script().contains("INT. HOME - DAY") && !script().contains("INT. HOUSE - DAY"),
            "renaming a location renames it in the scene headings");
    require(structure->itemForUuid(kitchenUuid)->name() == "HOME - KITCHEN"
                && script().contains("INT. HOME - KITCHEN - NIGHT")
                && !script().contains("INT. HOUSE - KITCHEN"),
            "location nested in the renamed one follows it");
    require(structure->itemForUuid(houseboatUuid)->name() == "HOUSEBOAT"
                && script().contains("INT. HOUSEBOAT - DAY"),
            "location that only begins with the same letters is left alone");
    require(members(Type::Locations) == QStringList({ "HOME", "HOME - KITCHEN", "HOUSEBOAT" }),
            "renaming keeps every location");

    // A nested location is not renamed onto a location that already exists: it would be
    // removed as a duplicate, with the description it has.
    manager.storeLocation("HOME - GARDEN", {});
    manager.storeLocation("FLAT - GARDEN", {});
    const auto homeGardenUuid = member(Type::Locations, "HOME - GARDEN");
    const auto flatGardenUuid = member(Type::Locations, "FLAT - GARDEN");
    require(!homeGardenUuid.isNull() && !flatGardenUuid.isNull(), "seed two garden locations");
    fields = fieldsOf(homeGardenUuid, "Ui::LocationInformationView");
    type(fields[1], "Overgrown.");
    fields = fieldsOf(flatGardenUuid, "Ui::LocationInformationView");
    type(fields[1], "Paved.");
    fields = fieldsOf(houseUuid, "Ui::LocationInformationView");
    enter(fields[0], "Flat");
    key(fields[0], Qt::Key_Return, "\r");
    save();
    require(members(Type::Locations)
                    == QStringList({ "FLAT", "FLAT - GARDEN", "FLAT - KITCHEN", "HOME - GARDEN",
                                     "HOUSEBOAT" })
                && structure->itemForUuid(kitchenUuid)->name() == "FLAT - KITCHEN"
                && structure->itemForUuid(homeGardenUuid)->name() == "HOME - GARDEN"
                && structure->itemForUuid(flatGardenUuid)->name() == "FLAT - GARDEN",
            "nested location keeps its name when the new one is taken");
    require(text(homeGardenUuid).contains("Overgrown.") && text(flatGardenUuid).contains("Paved.")
                && script().contains("INT. FLAT - DAY")
                && script().contains("INT. FLAT - KITCHEN - NIGHT"),
            "both garden locations keep their descriptions");
    for (const QString& refused : { QString("houseboat"), QString("flat - garden"), QString() }) {
        enter(fields[0], refused);
        key(fields[0], Qt::Key_Return, "\r");
        require(!fields[0]->error().isEmpty() && house->name() == "FLAT"
                    && members(Type::Locations).size() == 5,
                "taken or empty location name is refused and removes nothing");
    }
    enter(fields[0], "FLAT");
    key(fields[0], Qt::Key_Return, "\r");
    require(fields[0]->error().isEmpty(), "location page accepts its current name again");

    // The two groups explain themselves instead of reporting a missing editor.
    for (const auto group : { Type::Characters, Type::Locations }) {
        select(structure->itemForType(group)->uuid());
        require(noticeShown(manager.view(), group == Type::Characters ? "Characters" : "Locations")
                    && !noEditorNoticeShown(manager.view())
                    && hasEditor(plugins, group) == (panel->currentEditor() != nullptr),
                "group of characters or locations explains what it lists");
    }

    // Both can be added by hand again, now that there is a page to describe them with.
    emit navigator->addDocumentClicked();
    QApplication::processEvents();
    auto addDocument = host->findChild<Ui::CreateDocumentDialog*>();
    require(addDocument, "add-document dialog of the pages project");
    bool characterOffered = false;
    bool locationOffered = false;
    for (auto option : addDocument->findChildren<Ui::CreateDocumentDialogOption*>()) {
        characterOffered = characterOffered || option->documentType() == Type::Character;
        locationOffered = locationOffered || option->documentType() == Type::Location;
    }
    require(characterOffered && locationOffered, "character and location are offered to add");
    delete addDocument;

    // Everything written through the pages survives a reopen, and opening a page afterwards
    // changes nothing.
    save();
    QHash<QUuid, QByteArray> saved;
    for (const auto documentType : { Type::Character, Type::Location, Type::Characters,
                                     Type::Locations, Type::ScreenplayText }) {
        for (const auto document : storage->documents(documentType)) {
            saved.insert(document->uuid(), document->content());
        }
    }
    require(saved.size() == 4 + 5 + 3, "characters, locations, their groups and the script");
    require(text(aliceUuid).contains("<name><![CDATA[ALICIA GREY]]></name>")
                && text(aliceUuid).contains("A retired pilot.")
                && text(aliceUuid).contains("Afraid of heights.&#10;Keeps a logbook.")
                && text(eveUuid).contains("The new neighbour.")
                && text(houseUuid).contains("<name><![CDATA[FLAT]]></name>")
                && text(houseUuid).contains("A narrow terraced house.")
                && text(houseUuid).contains("The stairs creak."),
            "pages are stored in the character and location documents");
    reopen();
    const auto unchanged = [&](const char* _label) {
        for (auto it = saved.cbegin(); it != saved.cend(); ++it) {
            const auto document = storage->document(it.key());
            if (document == nullptr || document->content() != it.value()) {
                std::fprintf(stderr, "document %s: expected [%s] found [%s]\n",
                             qPrintable(it.key().toString()), it.value().constData(),
                             document ? document->content().constData() : "nothing");
            }
            require(document && document->content() == it.value(), _label);
        }
    };
    unchanged("characters, locations and script survive save/reopen exactly");
    fields = fieldsOf(aliceUuid, "Ui::CharacterInformationView");
    require(fields[0]->text() == "ALICIA GREY" && fields[1]->text() == "A retired pilot."
                && fields[2]->text() == "Afraid of heights.\nKeeps a logbook.",
            "reopened character page shows what was written");
    fields = fieldsOf(danaUuid, "Ui::CharacterInformationView");
    require(fields[1]->text() == "Kept description" && fields[2]->text() == "Kept notes Added.",
            "reopened page of the detailed character");
    fields = fieldsOf(houseUuid, "Ui::LocationInformationView");
    require(fields[0]->text() == "FLAT" && fields[1]->text() == "A narrow terraced house."
                && fields[2]->text() == "The stairs creak.",
            "reopened location page shows what was written");
    for (const auto& uuid : { eveUuid, bobUuid, kitchenUuid, homeGardenUuid, flatGardenUuid,
                              houseboatUuid, scriptUuid }) {
        select(uuid);
    }
    save();
    unchanged("opening pages does not rewrite project documents");

    // What a rename did to the screenplay, cues and mentions together, is one Undo step of the
    // script. The character itself keeps the new name.
    fields = fieldsOf(bobUuid, "Ui::CharacterInformationView");
    const auto scriptWithBob = script();
    enter(fields[0], "Rob");
    key(fields[0], Qt::Key_Return, "\r");
    asked = question();
    require(asked && questionText(asked).contains("2 times")
                && answer(asked, RenameInText),
            "second rename asks about its two mentions and is told to rename them");
    save();
    require(script().count("ROB") == 2 && !script().contains("BOB")
                && script().contains("greets Rob.") && script().contains("Hello Rob."),
            "second rename reaches the cues and the mentions of the screenplay");
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    require(edit, "screenplay editor after renaming");
    require(edit->toPlainText().count("ROB") == 2 && !edit->toPlainText().contains("BOB"),
            "open screenplay editor shows the renamed character");
    edit->undo();
    save();
    require(script() == scriptWithBob && edit->toPlainText().count("BOB") == 2
                && !edit->toPlainText().contains("ROB"),
            "one script Undo takes the renamed cues and mentions back out of the screenplay");
    require(structure->itemForUuid(bobUuid)->name() == "ROB",
            "character keeps its new name after the script Undo");

    // A name that is an ordinary word as well. The cues of the character and the lists of
    // scene characters are renamed without a question. The mentions in the running text are
    // the writer's choice, asked once for all screenplays of the project, because "Will you
    // come?" can't be told apart from the name. A lowercase "will" is never a mention, and
    // neither the cue of another character (YOUNG WILL) nor a scene heading (WILL'S BAR) is
    // rewritten: the characters and locations of the project are collected from those.
    using Paragraph = BusinessLayer::TextParagraphType;
    auto mentionsStory
        = structure->itemForIndex(structure->addDocument(Type::Screenplay, "Mentions script"));
    require(mentionsStory, "mentions fixture screenplay");
    const auto mentionsUuid = childOfType(mentionsStory, Type::ScreenplayText)->uuid();
    select(mentionsUuid);
    edit = screenplayEdit(mentionsUuid);
    require(edit, "mentions fixture uses real screenplay editor");
    paragraph(Paragraph::SceneHeading, "INT. WILL'S BAR - DAY");
    paragraph(Paragraph::SceneCharacters, "WILL, YOUNG WILL");
    paragraph(Paragraph::Action, "Will smiles. WILL pours a drink.");
    paragraph(Paragraph::Character, "WILL");
    paragraph(Paragraph::Parenthetical, "to Will");
    paragraph(Paragraph::Dialogue, "I will go.");
    paragraph(Paragraph::Character, "YOUNG WILL");
    paragraph(Paragraph::Dialogue, "Will you come?");
    // The first screenplay mentions the character too.
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    require(edit, "first screenplay takes another line");
    edit->moveCursor(QTextCursor::End);
    paragraph(Paragraph::Action, "Will waits outside.");
    save();
    const auto stored = [](const char* _type, const QString& _text) {
        return QString("<%1><v><![CDATA[%2]]></v></%1>").arg(_type, _text);
    };
    const auto mentions = [&] { return text(mentionsUuid); };
    const auto firstTyped = script();
    const auto mentionsTyped = mentions();
    for (const auto& paragraphXml :
         { stored("scene_heading", "INT. WILL'S BAR - DAY"),
           stored("scene_characters", "WILL, YOUNG WILL"),
           stored("action", "Will smiles. WILL pours a drink."), stored("character", "WILL"),
           stored("parenthetical", "to Will"), stored("dialogue", "I will go."),
           stored("character", "YOUNG WILL"), stored("dialogue", "Will you come?") }) {
        require(mentionsTyped.contains(paragraphXml), "mentions fixture is stored as typed");
    }
    require(firstTyped.contains(stored("action", "Will waits outside.")),
            "first screenplay stores the added mention");
    // What renaming WILL to BILL has to leave: with the cues only, and with the mentions too.
    auto mentionsWithCues = mentionsTyped;
    mentionsWithCues.replace(stored("character", "WILL"), stored("character", "BILL"));
    mentionsWithCues.replace(stored("scene_characters", "WILL, YOUNG WILL"),
                             stored("scene_characters", "BILL, YOUNG WILL"));
    auto mentionsRenamed = mentionsWithCues;
    mentionsRenamed.replace(stored("action", "Will smiles. WILL pours a drink."),
                            stored("action", "Bill smiles. BILL pours a drink."));
    mentionsRenamed.replace(stored("parenthetical", "to Will"), stored("parenthetical", "to Bill"));
    mentionsRenamed.replace(stored("dialogue", "Will you come?"),
                            stored("dialogue", "Bill you come?"));
    auto firstRenamed = firstTyped;
    firstRenamed.replace(stored("action", "Will waits outside."),
                         stored("action", "Bill waits outside."));
    // The other character and the location keep their names in the lists and in the script.
    const auto otherNamesKept = [&] {
        const auto characters = members(Type::Characters);
        const auto locations = members(Type::Locations);
        return characters.contains("YOUNG WILL") && !characters.contains("YOUNG BILL")
            && locations.contains("WILL'S BAR") && !locations.contains("BILL'S BAR")
            && mentions().contains(stored("character", "YOUNG WILL"))
            && mentions().contains(stored("scene_heading", "INT. WILL'S BAR - DAY"));
    };
    const auto willUuid = member(Type::Characters, "WILL");
    require(!willUuid.isNull() && otherNamesKept(),
            "WILL, YOUNG WILL and WILL'S BAR are collected from the script");
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    auto will = qobject_cast<BusinessLayer::CharacterModel*>(current);
    require(will && will->name() == "WILL", "page of the character named like a word");
    const auto characterNamed = [&](const QString& _name) {
        return will->name() == _name && fields[0]->text() == _name
            && structure->itemForUuid(willUuid)->name() == _name;
    };
    const auto nothingRenamed = [&] {
        return will->name() == "WILL" && structure->itemForUuid(willUuid)->name() == "WILL"
            && mentions() == mentionsTyped && script() == firstTyped;
    };
    const auto askToRename = [&](const QString& _name) {
        enter(fields[0], _name);
        key(fields[0], Qt::Key_Return, "\r");
        return question();
    };

    // Every way of closing the question without choosing renames the cues and leaves the text.
    for (const auto dismissal : { "a click outside the question", "Escape" }) {
        std::printf("-- question closed by %s\n", dismissal);
        asked = askToRename("bill");
        require(asked && questionText(asked).contains("WILL")
                    && questionText(asked).contains("BILL")
                    && questionText(asked).contains("5 times"),
                "renaming asks once, with the number of mentions in all screenplays");
        if (QLatin1String(dismissal) == QLatin1String("Escape")) {
            key(asked, Qt::Key_Escape);
        } else {
            save();
            require(nothingRenamed(), "nothing is renamed before the question is answered");
            // The field loses the focus to the question and enters its name once more.
            leave(fields[0], Qt::OtherFocusReason);
            require(questions().size() == 1 && nothingRenamed(),
                    "name entered again while the question is open asks nothing more");
            // Enter, pressed once more after confirming the name, chooses nothing.
            key(asked, Qt::Key_Return, "\r");
            require(question() == asked && nothingRenamed(), "Enter does not answer the question");
            clickOutside(asked);
        }
        save();
        require(questions().isEmpty(), "question closes");
        require(characterNamed("BILL") && !members(Type::Characters).contains("WILL"),
                "closing the question renames the character");
        require(mentions() == mentionsWithCues && script() == firstTyped,
                "closing the question renames cues and scene characters and leaves the mentions");
        require(otherNamesKept(),
                "another character's cue and the scene heading still match their list entries");
        // The text doesn't mention BILL, so the way back asks nothing.
        require(askToRename("will") == nullptr && questions().isEmpty() && characterNamed("WILL"),
                "a name the text doesn't mention is renamed without a question");
        save();
        require(mentions() == mentionsTyped && script() == firstTyped,
                "renaming back restores the typed script");
    }

    // Told to rename in the text too, the rename changes the mentions of every screenplay.
    asked = askToRename("bill");
    require(asked && questionText(asked).contains("5 times")
                && answer(asked, RenameInText),
            "the same question, answered: rename in the text too");
    save();
    require(questions().isEmpty() && characterNamed("BILL"),
            "answering renames the character");
    if (mentions() != mentionsRenamed || script() != firstRenamed) {
        std::fprintf(stderr, "expected [%s] found [%s]\n", qPrintable(mentionsRenamed),
                     qPrintable(mentions()));
    }
    require(mentions() == mentionsRenamed && script() == firstRenamed,
            "mentions are renamed in every screenplay, capitals staying capitals");
    require(mentions().contains(stored("dialogue", "I will go.")),
            "a lowercase word is never taken for the name");
    require(otherNamesKept(),
            "renaming the mentions leaves another character's cue and the scene heading alone");
    const auto named = [](const QString& _script) {
        return _script.count("will", Qt::CaseInsensitive);
    };
    require(named(mentionsWithCues) + named(firstTyped) - named(mentions()) - named(script()) == 5,
            "the number asked about is the number of mentions renamed");

    // One Undo in each screenplay takes its cues and mentions back out together.
    select(mentionsUuid);
    edit = screenplayEdit(mentionsUuid);
    require(edit && edit->toPlainText().contains("Bill you come?")
                && edit->toPlainText().contains("I will go."),
            "open screenplay editor shows the renamed mentions");
    edit->undo();
    save();
    require(mentions() == mentionsTyped && script() == firstRenamed,
            "one script Undo takes cues and mentions back out of that screenplay");
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    require(edit, "first screenplay editor after renaming the mentions");
    edit->undo();
    save();
    require(script() == firstTyped, "one Undo in the other screenplay takes its mention back");
    require(structure->itemForUuid(willUuid)->name() == "BILL",
            "renamed character keeps its name after both script Undos");

    // A name applied without the page, the way a stored change applies it, asks nothing and
    // leaves the mentions, also right after a rename that was told to change them. First the
    // character goes back to the name its script uses again.
    will->setName("WILL");
    save();
    require(questions().isEmpty() && nothingRenamed(),
            "character goes back to the name of its script");
    will->setName("BILL");
    require(questions().isEmpty(), "a name set without the page asks nothing");
    save();
    require(mentions() == mentionsWithCues && script() == firstTyped && otherNamesKept(),
            "a name set without the page renames cues only");
    will->setName("WILL");
    save();
    require(nothingRenamed(), "the name set back without the page restores the typed script");

    // Apply an actual character-document patch while a question is open. The stale answer
    // must not overwrite the patched name, rewrite mentions or clear a later question.
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    asked = askToRename("bill");
    require(asked, "question before a character patch");
    auto patchedCharacter = will->document()->content();
    patchedCharacter.replace("<![CDATA[WILL]]>", "<![CDATA[WILLIAM]]>");
    const auto patches = will->adoptDocumentChanges(patchedCharacter);
    will->applyDocumentChanges({ patches.first });
    require(will->name() == "WILLIAM" && answer(asked, RenameInText) && will->name() == "WILLIAM",
            "a stale question cannot overwrite a patched character name");
    save();
    auto patchedScript = mentionsWithCues;
    patchedScript.replace(stored("character", "BILL"), stored("character", "WILLIAM"));
    patchedScript.replace(stored("scene_characters", "BILL, YOUNG WILL"),
                          stored("scene_characters", "WILLIAM, YOUNG WILL"));
    require(mentions() == patchedScript && script() == firstTyped && questions().isEmpty(),
            "a real applied patch renames cues and leaves mentions without a question");
    will->applyDocumentChanges({ patches.second });
    save();
    require(nothingRenamed(), "the inverse character patch restores the cues only");
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    asked = askToRename("bill");
    require(asked, "another question before a patch and its inverse");
    will->applyDocumentChanges({ patches.first });
    will->applyDocumentChanges({ patches.second });
    will->requestRename("BILL");
    Dialog* laterQuestion = nullptr;
    for (auto open : questions()) {
        if (open != asked) {
            laterQuestion = open;
        }
    }
    require(laterQuestion && answer(asked, RenameInText) && !laterQuestion->isHidden()
                && will->name() == "WILL",
            "answering a fading stale question leaves a later question alone");
    require(answer(laterQuestion, LeaveText), "answer the later question");
    save();
    will->setName("WILL");
    save();

    // A question that is still open when the project closes goes away and renames nothing.
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    asked = askToRename("bill");
    require(asked, "question before the project closes");
    save();
    manager.closeCurrentProject(path);
    // Its buttons can still be reached while it fades.
    require(answer(asked, RenameInText), "answer to a question of a closed project");
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    // The project as it is before the rename, kept for trying the application by hand.
    const QString fixture = _evidence + "/rename-mentions-fixture.starc";
    QFile::remove(fixture);
    require(QFile::copy(path, fixture), "keep the project of the mentioned character");
    DatabaseLayer::Database::setCurrentFile(path);
    manager.loadCurrentProject(&project);
    pause(500);
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    will = qobject_cast<BusinessLayer::CharacterModel*>(current);
    require(will && questions().isEmpty() && nothingRenamed() && fields[0]->text() == "WILL",
            "answer to a question of a closed project renames nothing");
    asked = askToRename("bill");
    require(asked, "second question before the project closes");
    save();
    reopen();
    pause(500);
    require(questions().isEmpty(), "question goes away with its project");
    fields = fieldsOf(willUuid, "Ui::CharacterInformationView");
    will = qobject_cast<BusinessLayer::CharacterModel*>(current);
    require(will && nothingRenamed() && fields[0]->text() == "WILL",
            "closing the project with the question open renames nothing");

    // The next rename asks again, and the Leave the text button answers like the other two.
    std::printf("-- question closed by the Leave the text button\n");
    asked = askToRename("bill");
    require(asked && questionText(asked).contains("5 times")
                && answer(asked, LeaveText),
            "rename after reopening asks again and is told to leave the text");
    save();
    require(questions().isEmpty() && characterNamed("BILL") && mentions() == mentionsWithCues
                && script() == firstTyped && otherNamesKept(),
            "leaving the text renames cues and scene characters only");

    // A single mention is asked about in the singular.
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    require(edit, "first screenplay takes a line about another character");
    edit->moveCursor(QTextCursor::End);
    paragraph(Paragraph::Action, "Dana waves.");
    save();
    fields = fieldsOf(danaUuid, "Ui::CharacterInformationView");
    enter(fields[0], "dina");
    key(fields[0], Qt::Key_Return, "\r");
    asked = question();
    require(asked && questionText(asked).contains("1 time")
                && !questionText(asked).contains("1 times") && answer(asked, LeaveText),
            "a single mention is asked about in the singular");
    save();
    require(structure->itemForUuid(danaUuid)->name() == "DINA"
                && script().contains(stored("action", "Dana waves.")),
            "the single mention stays as the writer chose");

    // A real page rename with a longer accented name, emphasis and review comments. Exercise
    // serialization, the editor's Undo/Redo and a fresh project load, not just the matcher.
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    edit->moveCursor(QTextCursor::End);
    const QString unicodeText = "Zoë greets Émile, while zoë waits.";
    paragraph(Paragraph::Action, unicodeText);
    paragraph(Paragraph::Character, "ZOË (V.O.)");
    paragraph(Paragraph::Dialogue, "Hello.");
    save();
    auto unicodeModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
    require(unicodeModel, "formatted Unicode fixture uses the real screenplay model");
    const auto findText = [&](BusinessLayer::ScreenplayTextModel* _model, const QString& _text) {
        BusinessLayer::TextModelTextItem* found = nullptr;
        std::function<void(BusinessLayer::TextModelItem*)> visit;
        visit = [&](BusinessLayer::TextModelItem* _item) {
            if (auto textItem = dynamic_cast<BusinessLayer::TextModelTextItem*>(_item);
                textItem && textItem->text() == _text) {
                found = textItem;
            }
            for (int row = 0; row < _item->childCount(); ++row) {
                visit(_item->childAt(row));
            }
        };
        visit(_model->itemForIndex({ }));
        return found;
    };
    auto unicodeItem = findText(unicodeModel, unicodeText);
    require(unicodeItem, "find the typed Unicode action");
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    QTextCharFormat italic;
    italic.setFontItalic(true);
    BusinessLayer::TextModelTextItem::ReviewMark unicodeReview;
    unicodeReview.from = 11;
    unicodeReview.length = 5;
    unicodeReview.backgroundColor = Qt::yellow;
    unicodeReview.textColor = Qt::black;
    BusinessLayer::TextModelTextItem::ReviewComment unicodeComment;
    unicodeComment.author = "Reviewer";
    unicodeComment.text = "Keep this on Émile";
    unicodeReview.comments.append(unicodeComment);
    auto annotationCursor = edit->document()->find(unicodeText);
    require(!annotationCursor.isNull(), "find the action in the live editor for annotation");
    const int annotationStart = annotationCursor.selectionStart();
    annotationCursor.beginEditBlock();
    const auto annotate = [&](int _from, int _length, const QTextCharFormat& _format) {
        annotationCursor.setPosition(annotationStart + _from);
        annotationCursor.setPosition(annotationStart + _from + _length, QTextCursor::KeepAnchor);
        annotationCursor.mergeCharFormat(_format);
    };
    annotate(0, 3, bold);
    annotate(4, 6, italic);
    annotate(11, 5, unicodeReview.charFormat());
    annotationCursor.endEditBlock();
    save();
    auto unicodeFormats = unicodeItem->formats();
    require(unicodeFormats.size() == 2 && unicodeFormats[0].from == 0
                && unicodeFormats[0].length == 3 && unicodeFormats[0].isBold
                && unicodeFormats[1].from == 4 && unicodeFormats[1].length == 6
                && unicodeFormats[1].isItalic
                && unicodeItem->reviewMarks()
                    == QVector<BusinessLayer::TextModelTextItem::ReviewMark>{ unicodeReview },
            "the live editor stores the intended emphasis and review comment");
    const auto unicodeBefore = script();
    const auto zoeUuid = member(Type::Characters, "ZOË");
    require(!zoeUuid.isNull(), "accented cue is collected as a character");
    fields = fieldsOf(zoeUuid, "Ui::CharacterInformationView");
    enter(fields[0], "zoe long");
    key(fields[0], Qt::Key_Return, "\r");
    asked = question();
    require(asked && questionText(asked).contains("1 time") && answer(asked, RenameInText),
            "the real page counts and renames an accented mention, leaving lowercase words");
    save();
    const auto unicodeAfter = script();
    const QString unicodeRenamed = "Zoe Long greets Émile, while zoë waits.";
    unicodeFormats[0].length = 8;
    unicodeFormats[1].from = 9;
    unicodeReview.from = 16;
    select(scriptUuid);
    unicodeModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
    unicodeItem = findText(unicodeModel, unicodeRenamed);
    require(unicodeItem && unicodeItem->formats() == unicodeFormats
                && unicodeItem->reviewMarks()
                    == QVector<BusinessLayer::TextModelTextItem::ReviewMark>{ unicodeReview },
            "page rename preserves emphasis and review comments on their text");
    edit = screenplayEdit(scriptUuid);
    edit->undo();
    save();
    require(script() == unicodeBefore,
            "one Undo restores accented cues, text and annotations exactly");
    edit->redo();
    save();
    require(script() == unicodeAfter,
            "one Redo restores accented cues, text and annotations exactly");
    reopen();
    select(scriptUuid);
    unicodeModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
    unicodeItem = findText(unicodeModel, unicodeRenamed);
    require(script() == unicodeAfter && unicodeItem && unicodeItem->formats() == unicodeFormats
                && unicodeItem->reviewMarks()
                    == QVector<BusinessLayer::TextModelTextItem::ReviewMark>{ unicodeReview },
            "renamed Unicode text, emphasis and review comments survive save and reopen");

    // A name that was available before the question can be taken while it is open. The old
    // merge route would delete the renamed character and its notes in this case.
    fields = fieldsOf(zoeUuid, "Ui::CharacterInformationView");
    const QPointer<BusinessLayer::CharacterModel> pendingCharacter
        = qobject_cast<BusinessLayer::CharacterModel*>(current);
    enter(fields[0], "zoe next");
    key(fields[0], Qt::Key_Return, "\r");
    asked = question();
    require(asked, "question before another character takes the proposed name");
    manager.storeCharacter("ZOE NEXT",
                           "<?xml version=\"1.0\"?>"
                           "<document mime-type=\"application/x-starc/document/character\" "
                           "version=\"1.0\"><name><![CDATA[ZOE NEXT]]></name>"
                           "<long_description><![CDATA[Keep these notes]]>"
                           "</long_description></document>");
    const auto takenUuid = member(Type::Characters, "ZOE NEXT");
    require(!takenUuid.isNull() && answer(asked, RenameInText) && !pendingCharacter.isNull()
                && pendingCharacter->name() == "ZOE LONG" && storage->document(zoeUuid)
                && storage->document(takenUuid),
            "a pending rename cannot merge or delete characters when its target is taken");
    save();
    require(script() == unicodeAfter && fields[0]->error().contains("already exists")
                && text(takenUuid).contains("Keep these notes"),
            "the stale duplicate name reports an error and preserves scripts and notes");

    // Reproduce the location-page annotation loss. The prefix, name and time-of-day have
    // independent annotations so a misplaced range cannot pass.
    select(scriptUuid);
    edit = screenplayEdit(scriptUuid);
    edit->moveCursor(QTextCursor::End);
    paragraph(Paragraph::SceneHeading, "INT. HOUSE - DAY");
    paragraph(Paragraph::Action, "The house is quiet.");
    save();
    const auto annotatedHouseUuid = member(Type::Locations, "HOUSE");
    require(!annotatedHouseUuid.isNull(), "annotated location fixture collects its location");
    auto locationModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
    auto locationItem = findText(locationModel, "INT. HOUSE - DAY");
    require(locationItem, "find the location heading to annotate");
    BusinessLayer::TextModelTextItem::ReviewMark locationReview = unicodeReview;
    locationReview.from = 13;
    locationReview.length = 3;
    locationReview.isDone = true;
    locationReview.comments[0].text = "Keep this on DAY";
    BusinessLayer::TextModelTextItem::ResourceMark nameResource;
    nameResource.from = 5;
    nameResource.length = 5;
    nameResource.uuid = QUuid::createUuid();
    auto dayResource = nameResource;
    dayResource.from = 13;
    dayResource.length = 3;
    dayResource.uuid = QUuid::createUuid();
    annotationCursor = edit->document()->find("INT. HOUSE - DAY");
    require(!annotationCursor.isNull(), "find the location heading in the live editor");
    const int locationStart = annotationCursor.selectionStart();
    const auto annotateLocation = [&](int _from, int _length, const QTextCharFormat& _format) {
        annotationCursor.setPosition(locationStart + _from);
        annotationCursor.setPosition(locationStart + _from + _length, QTextCursor::KeepAnchor);
        annotationCursor.mergeCharFormat(_format);
    };
    annotationCursor.beginEditBlock();
    annotateLocation(0, 3, bold);
    annotateLocation(5, 5, italic);
    annotateLocation(13, 3, bold);
    annotateLocation(13, 3, locationReview.charFormat());
    annotateLocation(5, 5, nameResource.charFormat());
    annotateLocation(13, 3, dayResource.charFormat());
    annotationCursor.endEditBlock();
    save();
    auto locationFormats = locationItem->formats();
    QVector<BusinessLayer::TextModelTextItem::ResourceMark> locationResources
        = { nameResource, dayResource };
    require(locationFormats.size() == 3 && locationFormats[0].from == 0
                && locationFormats[0].length == 3 && locationFormats[0].isBold
                && locationFormats[1].from == 5 && locationFormats[1].length == 5
                && locationFormats[1].isItalic && locationFormats[2].from == 13
                && locationFormats[2].length == 3 && locationFormats[2].isBold
                && locationItem->reviewMarks()
                    == QVector<BusinessLayer::TextModelTextItem::ReviewMark>{ locationReview }
                && locationItem->resourceMarks() == locationResources,
            "live editor stores location emphasis, completed comment and resource links");
    for (const QString& newName : { QString("INT"), QString("HO"), QString("LIVING ROOM") }) {
        const auto before = script();
        fields = fieldsOf(annotatedHouseUuid, "Ui::LocationInformationView");
        enter(fields[0], newName);
        key(fields[0], Qt::Key_Return, "\r");
        save();
        const auto after = script();
        const QString renamedHeading = "INT. " + newName + " - DAY";
        locationFormats[1].length = newName.length();
        locationFormats[2].from = newName.length() + 8;
        locationReview.from = newName.length() + 8;
        locationResources[0].length = newName.length();
        locationResources[1].from = newName.length() + 8;
        select(scriptUuid);
        locationModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
        locationItem = findText(locationModel, renamedHeading);
        const auto annotationsKept = [&] {
            return locationItem && locationItem->formats() == locationFormats
                && locationItem->reviewMarks()
                == QVector<BusinessLayer::TextModelTextItem::ReviewMark>{ locationReview }
            && locationItem->resourceMarks() == locationResources;
        };
        require(annotationsKept() && structure->itemForUuid(annotatedHouseUuid)->name() == newName,
                qPrintable("page rename preserves location annotations and identity for " + newName));
        edit = screenplayEdit(scriptUuid);
        edit->undo();
        save();
        require(script() == before, "one Undo restores location headings and annotations exactly");
        edit->redo();
        save();
        require(script() == after, "one Redo restores renamed locations and annotations exactly");
        reopen();
        select(scriptUuid);
        locationModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(current);
        locationItem = findText(locationModel, renamedHeading);
        require(script() == after && annotationsKept()
                    && structure->itemForUuid(annotatedHouseUuid)->name() == newName,
                "location emphasis, comments, links and identities survive save and reopen");
    }
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
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
    checkCharacterRenameModel();
    checkLocationRenameModel();
    if (qEnvironmentVariable("STARC_RENAME_CHECK") == "model") {
        return 0;
    }
    if (qEnvironmentVariable("STARC_RENAME_CHECK") == "pages") {
        checkCharacterAndLocationPages(evidence);
        std::puts("PASS: character and location rename models and pages");
        return 0;
    }
    const auto statisticsSlice = qEnvironmentVariable("STARC_STATISTICS_CHECK");
    if (!statisticsSlice.isEmpty()) {
        checkStatistics(evidence, statisticsSlice);
        std::puts("PASS: statistics slice");
        return 0;
    }
    using Type = Domain::DocumentObjectType;
    static_assert(static_cast<int>(Type::ComicBook) == 10200);
    static_assert(static_cast<int>(Type::Audioplay) == 10300);
    static_assert(static_cast<int>(Type::Stageplay) == 10400);
    static_assert(static_cast<int>(Type::Novel) == 10500);
    static_assert(static_cast<int>(Type::ScreenplaySeries) == 10110);
    static_assert(static_cast<int>(Type::ScreenplayText) == 10104);
    static_assert(static_cast<int>(Type::Worlds) == 50000);
    static_assert(static_cast<int>(Type::World) == 50001);
    require(Domain::mimeTypeFor(Type::Worlds) == "application/x-starc/document/worlds"
                && Domain::typeFor("application/x-starc/document/worlds") == Type::Worlds
                && Domain::mimeTypeFor(Type::World) == "application/x-starc/document/world"
                && Domain::typeFor("application/x-starc/document/world") == Type::World,
            "persisted World identifiers and MIME mappings retained");
    checkStoryCardsWithoutCovers(evidence);
    checkEnglishOnlyCatalog();

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
    QSettings().setValue("widgets/create-document-dialog/last-selected-type",
                         static_cast<int>(Type::World));
    require(createDocument->lastSelectedType() == Type::Screenplay,
            "remembered World creation choice falls back to screenplay");
    QSettings().remove("widgets/create-document-dialog/last-selected-type");
    for (auto option : createDocument->findChildren<Ui::CreateDocumentDialogOption*>()) {
        require(option->documentType() != Type::World, "World absent from add-document dialog");
        require(!Domain::isUnsupportedDocumentType(option->documentType()),
                "no retired document choice");
    }
    // An interface language, its Qt version and a translation file stored by an earlier version.
    const QVariantMap storedLanguage = {
        { "application/language", static_cast<int>(QLocale::Hebrew) },
        { "application/language-qt-version", 6 },
        { "application/language-file", evidence + "/custom-translation.qm" },
    };
    for (auto stored = storedLanguage.begin(); stored != storedLanguage.end(); ++stored) {
        setSettingsValue(stored.key(), stored.value());
    }
    // The settings screen as the application builds it, with its shortcut tables.
    ManagementLayer::PluginsBuilder plugins;
    ManagementLayer::ShortcutsManager shortcuts;
    ManagementLayer::SettingsManager settings(nullptr, &host, plugins, &shortcuts);
    checkNoInterfaceLanguageChoice(settings, storedLanguage);
    auto onboarding = new Ui::OnboardingNavigator(&host);
    checkRetainedUpdateFeed();
    auto menu = new Ui::MenuView(&host);
    QApplication::processEvents();
    require(menu->metaObject()->indexOfSignal("notificationsPressed()") < 0
                && menu->metaObject()->indexOfSignal("showDevVersionsChanged(bool)") < 0,
            "menu has no notifications navigation or filter callbacks");
    noNotificationsUi(menu);
    require(menu->metaObject()->indexOfSignal("signInPressed()") < 0
                && menu->metaObject()->indexOfSignal("accountPressed()") < 0
                && menu->metaObject()->indexOfSignal("renewProPressed()") < 0,
            "menu has no account/sign-in/purchase callbacks");
    require(createProject->metaObject()->indexOfSignal("loginPressed()") < 0
                && createProject->metaObject()->indexOfSignal("renewSubscriptionPressed()") < 0,
            "new story has no sign-in/subscription callbacks");
    require(createDocument->metaObject()->indexOfSignal("upgradeToProPressed()") < 0,
            "new document has no purchase callback");
    bool statisticsRequested = false;
    QObject::connect(menu, &Ui::MenuView::writingStatisticsPressed,
                     [&] { statisticsRequested = true; });
    bool sprintRequested = false;
    QObject::connect(menu, &Ui::MenuView::writingSprintPressed,
                     [&] { sprintRequested = true; });
    auto drawer = menu->findChild<Drawer*>();
    require(drawer, "retained local menu");
    menu->setProjectActionsVisible(true);
    QApplication::processEvents();
    noNotificationsUi(menu);
    for (auto action : drawer->actions()) {
        if (action->text() == "Writing statistics") {
            require(action->isVisible() && action->isEnabled(), "local statistics action available");
            action->trigger();
        } else if (action->text() == "Writing sprint") {
            require(action->isVisible() && action->isEnabled(), "local sprint action available");
            action->trigger();
        }
    }
    require(statisticsRequested && sprintRequested, "local writing tools work without account");
    menu->setProjectActionsVisible(false);
    QApplication::processEvents();
    for (QWidget* widget :
         QVector<QWidget*>{ settings.view(), settings.navigator(), onboarding, menu,
                            createProject, createDocument }) {
        noAi(widget);
        noAccountUi(widget);
        for (auto label : widget->findChildren<AbstractLabel*>()) {
            requireRetained(label->text());
        }
        for (auto tree : widget->findChildren<Tree*>()) {
            requireRetained(tree->model(), {});
        }
    }
    {
        // The components are the second group of the settings list.
        const auto navigation = settings.navigator()->findChild<Tree*>()->model();
        require(navigation->rowCount(navigation->index(1, 0)) == 2,
                "settings list simple text and screenplay components");
    }

    createDocument->setBlockedDocumentTypes({ Type::Screenplay });
    createDocument->setDocumentType(Type::Screenplay);
    noAccountUi(createDocument);
    bool disabledCreation = false;
    for (auto button : createDocument->findChildren<Button*>()) {
        disabledCreation = disabledCreation || !button->isEnabled();
    }
    require(disabledCreation, "unavailable document remains guarded without purchase prompt");
    createDocument->setBlockedDocumentTypes({});

    // Keep the persisted team/project records and historical account label through the UI cut.
    const QJsonObject cachedProject = {
        { "is_team", false }, { "id", 73 }, { "team_id", 42 },
        { "type", static_cast<int>(BusinessLayer::ProjectType::Cloud) },
        { "uuid", "{12cf11aa-e03d-4d50-a09a-429e9b57c427}" },
        { "name", "Historical shared story" }, { "logline", "Retained logline" },
        { "path", evidence + "/cached-cloud.starc" }, { "poster_path", evidence + "/legacy-poster.png" },
        { "last_edit_time", "2026-10-01T12:00:00.000" }, { "can_ask", true },
        { "is_review_enabled", true }, { "is_owner", false }, { "role", 0 },
        { "permissions", QJsonArray{} }
    };
    const QJsonArray cachedProjects = { QJsonObject{
        { "is_team", true }, { "id", 42 }, { "name", "Historical team label" },
        { "description", "Retained description" }, { "is_opened", true },
        { "projects", QJsonArray{ cachedProject } }
    } };
    setSettingsValue(DataStorageLayer::kApplicationProjectsKey,
                     QJsonDocument(cachedProjects).toJson(QJsonDocument::Compact).toHex());
    setSettingsValue(DataStorageLayer::kAccountEmailKey, "historical@example.invalid");
    {
        ManagementLayer::ProjectsManager projects(nullptr, &host);
        projects.loadProjects();
        require(projects.project(73) && projects.project(73)->teamId() == 42,
                "cached team project still loads");
        projects.saveProjects();
        const auto saved = QJsonDocument::fromJson(QByteArray::fromHex(
            settingsValue(DataStorageLayer::kApplicationProjectsKey).toByteArray())).array();
        require(saved == cachedProjects, "cached team and project records survive exactly");
        require(settingsValue(DataStorageLayer::kAccountEmailKey).toString()
                    == "historical@example.invalid", "historical account label retained");
        projects.createProject();
        auto localDialog = host.findChildren<Ui::CreateProjectDialog*>().constLast();
        noAccountUi(localDialog);
        localDialog->hideDialog();
    }
    {
        ManagementLayer::ProjectsManager reopenedProjects(nullptr, &host);
        reopenedProjects.loadProjects();
        require(reopenedProjects.project(73) && reopenedProjects.project(73)->teamId() == 42,
                "cached team project reopens after UI construction");
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
    {
        ManagementLayer::ProjectModelsFacade facade(structure, nullptr, nullptr);
        require(!facade.modelFor(Type::Worlds) && !facade.modelFor(Type::World),
                "World model lookups in new projects are refused");
    }
    require(!structure->itemForType(Type::Worlds) && storage->documents(Type::Worlds).isEmpty()
                && storage->documents(Type::World).isEmpty(),
            "new projects create no Worlds group or World documents");
    // A project file gets its version on the second open, after the upstream upgrade steps
    // ran once. Those steps rewrite script documents as text, so reach the state of an
    // existing project before adding the documents whose bytes are compared.
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    manager.closeCurrentProject(path);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    DatabaseLayer::Database::setCurrentFile(path);
    // Seed an old project's cover independently of the retired UI. A trailing
    // payload makes re-encoding the PNG detectable even when its decoded pixels
    // would remain identical.
    const auto legacyCoverUuid = QUuid::createUuid();
    QPixmap legacyCover(20, 30);
    legacyCover.fill(Qt::green);
    QByteArray legacyCoverBytes;
    QBuffer coverBuffer(&legacyCoverBytes);
    require(coverBuffer.open(QIODevice::WriteOnly) && legacyCover.save(&coverBuffer, "PNG"),
            "seed legacy cover PNG");
    legacyCoverBytes += QByteArray::fromHex("0000ff") + "retained cover payload";
    auto coverDocument = storage->createDocument(legacyCoverUuid, Type::ImageData);
    coverDocument->setContent(legacyCoverBytes);
    require(storage->saveDocument(coverDocument), "store legacy cover bytes");
    auto projectInfo = storage->document(Type::Project);
    require(projectInfo, "stored project information");
    const auto projectInfoUuid = projectInfo->uuid();
    auto projectInfoBytes = projectInfo->content();
    const QByteArray emptyCover
        = "<cover><![CDATA[{00000000-0000-0000-0000-000000000000}]]></cover>";
    const auto legacyCoverTag
        = "<cover><![CDATA[" + legacyCoverUuid.toString().toUtf8() + "]]></cover>";
    require(projectInfoBytes.contains(emptyCover), "seed project has an empty cover reference");
    projectInfoBytes.replace(emptyCover, legacyCoverTag);
    projectInfo->setContent(projectInfoBytes);
    require(storage->saveDocument(projectInfo), "store legacy cover reference");
    DatabaseLayer::Database::commit();
    manager.loadCurrentProject(&project);
    require(structure, "reopen the new project");
    require(manager.projectCover().toImage() == legacyCover.toImage(),
            "retired cover remains decodable from the project");
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
    retired.insert(legacyCoverUuid, legacyCoverBytes);
    QVector<QUuid> retiredText;
    const auto worldImageUuid = QUuid::createUuid();
    auto worldImage = storage->createDocument(worldImageUuid, Type::ImageData);
    const auto worldImageBytes = legacyCoverBytes + "world attachment";
    worldImage->setContent(worldImageBytes);
    require(storage->saveDocument(worldImage), "seed retained World image attachment");
    retired.insert(worldImageUuid, worldImageBytes);
    const QByteArray worldBytes = QByteArray::fromHex("efbbbf")
        + "<world unknown='keep'>\r\n<photo>" + worldImageUuid.toString().toUtf8() + "</photo>"
        + QByteArray::fromHex("00ff") + "</world>\r\n";
    auto worlds = new BusinessLayer::StructureModelItem(QUuid::createUuid(), Type::Worlds,
                                                        "Retained Worlds", "First draft", { }, true,
                                                        false, false);
    const QByteArray worldsBytes = "<worlds future='keep'>\r\nmap coordinates and routes</worlds>";
    structure->appendItem(worlds, nullptr, worldsBytes);
    auto world = new BusinessLayer::StructureModelItem(
        QUuid::createUuid(), Type::World, "Retained World", "First draft", { }, true, false, false);
    structure->appendItem(world, worlds, worldBytes);
    retired.insert(worlds->uuid(), worldsBytes);
    retired.insert(world->uuid(), worldBytes);
    auto emptyWorlds = new BusinessLayer::StructureModelItem(QUuid::createUuid(), Type::Worlds,
                                                             "Retained empty Worlds", "First draft",
                                                             { }, true, false, false);
    structure->appendItem(emptyWorlds, nullptr, worldsBytes + "empty");
    retired.insert(emptyWorlds->uuid(), worldsBytes + "empty");
    const auto worldChangeUuid = QUuid::createUuid();
    const QByteArray worldUndo = QByteArray::fromHex("00ff") + "retained World undo";
    const QByteArray worldRedo = "retained World redo\r\n";
    {
        auto worldChangeQuery = DatabaseLayer::Database::query();
        worldChangeQuery.prepare(
            "INSERT INTO documents_changes "
            "(fk_document_uuid, uuid, undo_patch, redo_patch, date_time, user_name, is_synced) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        worldChangeQuery.addBindValue(worlds->uuid().toString());
        worldChangeQuery.addBindValue(worldChangeUuid.toString());
        worldChangeQuery.addBindValue(worldUndo);
        worldChangeQuery.addBindValue(worldRedo);
        worldChangeQuery.addBindValue("2020-01-01 00:00:00:000");
        worldChangeQuery.addBindValue("Retained World author");
        worldChangeQuery.addBindValue(true);
        require(worldChangeQuery.exec(), "seed retained Worlds change history");
        worldChangeQuery.finish();
    }
    retiredText.append(worlds->uuid());
    retiredText.append(world->uuid());
    retiredText.append(emptyWorlds->uuid());
    const auto worldsUuid = worlds->uuid();
    const auto worldUuid = world->uuid();
    structure->addItemDraft(world, "Retained World draft", { }, false, worldBytes + "draft", false);
    retired.insert(world->drafts().first()->uuid(), worldBytes + "draft");
    require(!structure->addDocument(Type::Worlds, "Refused Worlds").isValid()
                && !structure->addDocument(Type::World, "Refused World").isValid(),
            "World and Worlds creation refused");
    {
        QScopedPointer<QMimeData> mime(structure->mimeData({ structure->indexForItem(world) }));
        require(!structure->canDropMimeData(mime.data(), Qt::MoveAction, -1, 0, { }),
                "World cannot be moved even through direct drag MIME data");
        mime.reset(structure->mimeData({ structure->indexForItem(story) }));
        require(!structure->canDropMimeData(mime.data(), Qt::MoveAction, -1, 0,
                                            structure->indexForItem(worlds))
                    && !structure->canDropMimeData(mime.data(), Qt::MoveAction, -1, 0,
                                                   structure->indexForItem(world)),
                "retired Worlds and World refuse dropped documents");
        mime.reset(structure->mimeData(
            { structure->indexForItem(world), structure->indexForItem(story) }));
        require(!structure->canDropMimeData(mime.data(), Qt::MoveAction, -1, 0, { }),
                "mixed selection cannot move a retired World");
    }
    // All 32 persisted format identifiers, plus drafts and binary/image data. A novel and a
    // series have one document more than the other retired formats.
    BusinessLayer::StructureModelItem* episodes = nullptr;
    for (const int base : { 10200, 10300, 10400, 10500, 10110 }) {
        BusinessLayer::StructureModelItem* bundle = nullptr;
        const int lastOffset = base == 10500 || base == 10110 ? 6 : 5;
        for (int offset = 0; offset <= lastOffset; ++offset) {
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
            if (documentType == Type::ScreenplaySeriesEpisodes)
                episodes = item;
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
    // The episodes of a series are ordinary screenplays. They stay in a retired series and can
    // be opened and moved out of it like any other screenplay.
    require(episodes, "seed the episodes document of a series");
    const auto episode
        = structure->addDocument(Type::Screenplay, "Kept episode", structure->indexForItem(episodes));
    require(episode.isValid() && structure->itemForIndex(episode)->parent() == episodes,
            "screenplay inside a retired series");
    const auto episodeUuid = structure->itemForIndex(episode)->uuid();
    QUuid episodeScriptUuid;
    for (int i = 0; i < structure->itemForIndex(episode)->childCount(); ++i) {
        if (structure->itemForIndex(episode)->childAt(i)->type() == Type::ScreenplayText)
            episodeScriptUuid = structure->itemForIndex(episode)->childAt(i)->uuid();
    }
    require(!episodeScriptUuid.isNull(), "episode has its screenplay text");
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
    navigator->setCurrentIndex(proxyIndex(projectInfoUuid));
    QWidget* informationView = nullptr;
    for (auto widget : manager.view()->findChildren<QWidget*>()) {
        if (QString(widget->metaObject()->className()) == "Ui::ProjectInformationView")
            informationView = widget;
    }
    require(informationView && informationView->findChildren<ImageCard*>().isEmpty(),
            "project information has no cover image card");
    require(informationView->metaObject()->indexOfSignal("coverChanged(QPixmap)") < 0,
            "project information has no cover mutation signal");
    require(QMetaObject::invokeMethod(informationView, "nameChanged",
                                      Q_ARG(QString, "Retained cover project renamed"))
                && QMetaObject::invokeMethod(informationView, "loglineChanged",
                                             Q_ARG(QString, "Retained description edited")),
            "retained project information edit connections");
    require(manager.projectName() == "Retained cover project renamed"
                && manager.projectLogline() == "Retained description edited"
                && storage->document(projectInfoUuid)->content().contains(legacyCoverTag),
            "editing name and description retains the cover reference");
    navigator->setCurrentIndex(proxyIndex(episodeScriptUuid));
    require(manager.currentDocument() && manager.currentDocument()->uuid() == episodeScriptUuid
                && !unsupportedNoticeShown(manager.view()),
            "episode of a retired series opens as a screenplay");
    require(structure->flags(structure->indexForItem(structure->itemForUuid(episodeUuid)))
                & Qt::ItemIsDragEnabled,
            "episode can be moved out of a retired series");
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
    const auto savedProjectInfoBytes = storage->document(projectInfoUuid)->content();
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
        for (const auto retiredType :
             { Type::ComicBookDictionaries, Type::AudioplayDictionaries,
               Type::StageplayDictionaries, Type::NovelDictionaries, Type::Worlds, Type::World }) {
            require(!facade.modelFor(retiredType),
                    "retired document types are not created on request");
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
    require(storage->document(projectInfoUuid)->content() == savedProjectInfoBytes
                && savedProjectInfoBytes.contains(legacyCoverTag)
                && manager.projectName() == "Retained cover project renamed"
                && manager.projectLogline() == "Retained description edited",
            "retired cover reference and information edits survive save/reopen "
            "exactly");
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
    {
        auto worldChangeQuery = DatabaseLayer::Database::query();
        worldChangeQuery.prepare(
            "SELECT fk_document_uuid, undo_patch, redo_patch, user_name FROM documents_changes "
            "WHERE uuid = ?");
        worldChangeQuery.addBindValue(worldChangeUuid.toString());
        require(worldChangeQuery.exec() && worldChangeQuery.next()
                    && worldChangeQuery.value(0).toString() == worldsUuid.toString()
                    && worldChangeQuery.value(1).toByteArray() == worldUndo
                    && worldChangeQuery.value(2).toByteArray() == worldRedo
                    && worldChangeQuery.value(3).toString() == "Retained World author",
                "retired Worlds change identity, patches and author survive reopen");
        worldChangeQuery.finish();
    }
    require(structure->itemForUuid(worldsUuid) && structure->itemForUuid(worldUuid)
                && structure->itemForUuid(worldsUuid)->type() == Type::Worlds
                && structure->itemForUuid(worldUuid)->type() == Type::World
                && structure->itemForUuid(worldUuid)->parent()->uuid() == worldsUuid
                && structure->itemForUuid(worldUuid)->drafts().size() == 1,
            "legacy Worlds hierarchy, identities and draft metadata survive "
            "reopen");
    for (const auto uuid : { worldsUuid, worldUuid }) {
        navigator->setCurrentIndex(proxyIndex(uuid));
        require(!manager.currentDocument() && unsupportedNoticeShown(manager.view()),
                "reopened Worlds remain preserved and unsupported");
    }
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

    // The information page of a screenplay has an option that shows its statistics in the
    // project. It is there only with a view for them, in either panel, and hiding it leaves the
    // choice it holds and the item as they are.
    {
        struct {
            Type document = Type::Screenplay;
            Type statistics = Type::ScreenplayStatistics;
            QUuid uuid;
        } set;
        set.uuid = screenplayUuid;
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

        // A Markdown file was offered as a novel before that format was retired.
        ManagementLayer::ImportManager markdownImport(nullptr, &parent, plugins);
        markdownImport.import({ evidence + "/notes.md" });
        QApplication::processEvents();
        importDialog = parent.findChild<Ui::ImportDialog*>();
        require(importDialog, "import dialog for a Markdown file");
        importTo = importDialog->findChild<ComboBox*>();
        require(importTo && importTo->model() && importTo->model()->rowCount() > 0,
                "import dialog lists the document types for Markdown");
        bool simpleTextOffered = false;
        for (int row = 0; row < importTo->model()->rowCount(); ++row) {
            const auto type = importTo->model()->index(row, 0).data().toString();
            requireRetained(type);
            simpleTextOffered = simpleTextOffered || type == "Simple text";
        }
        require(simpleTextOffered, "Markdown file can be imported as simple text");
        delete importDialog;
    }
    manager.closeCurrentProject(path);
    // A cover whose image is absent must retain its reference, rather than
    // being reset when the information page is opened or its model is saved.
    auto missingCoverInfo = storage->document(projectInfoUuid);
    const auto missingCoverTag
        = "<cover><![CDATA[" + QUuid::createUuid().toString().toUtf8() + "]]></cover>";
    auto missingCoverInfoBytes = missingCoverInfo->content();
    require(missingCoverInfoBytes.contains(legacyCoverTag),
            "cover reference before missing-image check");
    missingCoverInfoBytes.replace(legacyCoverTag, missingCoverTag);
    missingCoverInfo->setContent(missingCoverInfoBytes);
    require(storage->saveDocument(missingCoverInfo), "seed missing cover image reference");
    DatabaseLayer::Database::commit();
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    DatabaseLayer::Database::setCurrentFile(path);
    manager.loadCurrentProject(&project);
    navigator->setCurrentIndex(proxyIndex(projectInfoUuid));
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    require(storage->document(projectInfoUuid)->content() == missingCoverInfoBytes
                && manager.projectCover().isNull()
                && storage->document(legacyCoverUuid)->content() == legacyCoverBytes,
            "missing cover image reference and stored image bytes survive "
            "open/save exactly");
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
    // Keep the original hosts alive: several existing plugins retain their primary view as a
    // raw pointer, and QPluginLoader shares a plugin instance between builders in this process.
    checkCharacterAndLocationPages(evidence);
    checkStatistics(evidence, {});
    std::puts("PASS: screenplay scope, keyboard editing, Undo/Redo, "
              "pagination, exports, "
              "mixed-project preservation, editors limited to what this build "
              "contains, "
              "notifications UI absent with retained update metadata, "
              "cover UI retired with exact legacy image preservation, "
              "character and location pages with guarded renaming, "
              "character mentions renamed only when the writer chooses to");
}
