#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/application_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <ui/application_view.h>
#include <ui/menu_view.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/project/project_navigator.h>
#include <ui/projects/create_project_dialog.h>
#include <ui/projects/projects_view.h>
#include <ui/settings/settings_view.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/text_field/text_field.h>
#include <ui/widgets/tree/tree.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLockFile>
#include <QScreen>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QTextCursor>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <functional>

namespace {
void require(bool _ok, const char* _label)
{
    std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
    if (!_ok) {
        std::exit(1);
    }
}

void waitFor(const std::function<bool()>& _ready)
{
    QElapsedTimer deadline;
    deadline.start();
    while (!_ready() && deadline.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!_ready()) {
        for (auto candidate : QApplication::allWidgets()) {
            if (QString(candidate->metaObject()->className()).contains("ProjectsView")
                || QString(candidate->metaObject()->className()).contains("ApplicationView")) {
                std::fprintf(stderr, "state: %s visible=%d hidden=%d parent=%s\n",
                             candidate->metaObject()->className(), candidate->isVisible(),
                             candidate->isHidden(),
                             candidate->parentWidget()
                                 ? candidate->parentWidget()->metaObject()->className()
                                 : "none");
            }
        }
    }
    require(_ready(), "operation settles before the deadline");
}

template<typename T>
T* widget()
{
    for (auto candidate : QApplication::allWidgets()) {
        if (auto result = qobject_cast<T*>(candidate)) {
            return result;
        }
    }
    return nullptr;
}

QString remembered()
{
    return settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString();
}

void verifyScript(const QString& _root)
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT content, uuid FROM documents WHERE type = 10104") && query.next(),
            "opened project contains a screenplay");
    const auto bytes = query.value(0).toByteArray();
    QFile expected(_root + "/script.xml");
    require(expected.open(QIODevice::ReadOnly) && expected.readAll() == bytes,
            "reopened script, emphasis and review annotations retain exact serialized bytes");
    require(bytes.contains("Zoë keeps the last story.") && bytes.contains("Keep this annotation"),
            "saved script includes the typed text and review comment");
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<BusinessLayer::StructureModel*>(proxy->sourceModel());
    const QUuid uuid(query.value(1).toString());
    navigator->setCurrentIndex(
        proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(uuid))));
    auto document
        = qobject_cast<BusinessLayer::TextDocument*>(widget<Ui::ScreenplayTextEdit>()->document());
    bool annotatedText = false;
    std::function<void(BusinessLayer::TextModelItem*)> inspect;
    inspect = [&](BusinessLayer::TextModelItem* item) {
        if (auto text = dynamic_cast<BusinessLayer::TextModelTextItem*>(item);
            text && text->text() == "Zoë keeps the last story.") {
            annotatedText = !text->formats().isEmpty() && text->formats().first().isBold
                && !text->reviewMarks().isEmpty() && !text->reviewMarks().first().comments.isEmpty()
                && text->reviewMarks().first().comments.first().text == "Keep this annotation";
        }
        for (int row = 0; row < item->childCount(); ++row) {
            inspect(item->childAt(row));
        }
    };
    inspect(document->model()->itemForIndex({ }));
    require(annotatedText, "reopened editor model decodes the text, emphasis and review comment");
}

void seedScript(const QString& _root)
{
    widget<Ui::MenuView>()->createProjectPressed();
    auto dialog = widget<Ui::CreateProjectDialog>();
    require(dialog, "new-story dialog is connected to the application");
    dialog->setProjectName("remembered");
    dialog->setProjectFolder(_root);
    dialog->setProjectType(static_cast<int>(Domain::DocumentObjectType::Screenplay));
    dialog->createProjectPressed();
    require(remembered() == _root + "/remembered.starc", "successful creation is remembered");
    widget<Ui::MenuView>()->saveProjectChangesPressed();

    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT uuid FROM documents WHERE type = 10104") && query.next(),
            "new story creates the screenplay document");
    const QUuid uuid(query.value(0).toString());
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<BusinessLayer::StructureModel*>(proxy->sourceModel());
    navigator->setCurrentIndex(
        proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(uuid))));
    auto edit = widget<Ui::ScreenplayTextEdit>();
    require(edit, "new story opens the real screenplay editor");
    edit->setCurrentParagraphType(BusinessLayer::TextParagraphType::Action);
    edit->insertPlainText("Zoë keeps the last story.");
    auto cursor = edit->document()->find("Zoë");
    require(!cursor.isNull(), "typed Unicode text is present");
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    cursor.mergeCharFormat(bold);
    BusinessLayer::TextModelTextItem::ReviewMark mark;
    mark.from = 0;
    mark.length = 3;
    mark.backgroundColor = Qt::yellow;
    mark.textColor = Qt::black;
    BusinessLayer::TextModelTextItem::ReviewComment comment;
    comment.author = "Reviewer";
    comment.text = "Keep this annotation";
    mark.comments.append(comment);
    cursor.mergeCharFormat(mark.charFormat());
    auto document = qobject_cast<BusinessLayer::TextDocument*>(edit->document());
    require(document && document->model(), "editor binds the persistent text model");
    document->model()->saveChanges();
    widget<Ui::MenuView>()->saveProjectChangesPressed();
    require(!DatabaseLayer::Database::hasError(), "save annotated script successfully");
    require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next(),
            "read saved screenplay bytes");
    QFile expected(_root + "/script.xml");
    require(expected.open(QIODevice::WriteOnly), "retain exact script oracle");
    const auto bytes = query.value(0).toByteArray();
    require(expected.write(bytes) == bytes.size(), "write exact script oracle");
    expected.close();
    verifyScript(_root);
}

QByteArray readFile(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable file bytes");
    return file.readAll();
}

void verifyConversion(ManagementLayer::ApplicationManager* _manager, const QString& _root,
                      const QString& _mode)
{
    const auto folder = _root + "/" + _mode;
    require(QDir().mkpath(folder), "create isolated conversion directory");
    const auto original = _root + "/remembered.starc";
    const auto originalBytes = readFile(original);
    const auto source = folder + "/source.fountain";
    const auto target = source + ".starc";
    if (_mode.startsWith("conversion-copy")) {
        auto menu = widget<Ui::MenuView>();
        menu->createProjectPressed();
        auto dialog = widget<Ui::CreateProjectDialog>();
        dialog->setProjectName("copy");
        dialog->setProjectFolder(folder);
        dialog->setProjectType(static_cast<int>(Domain::DocumentObjectType::Screenplay));
        const auto import
            = _mode == "conversion-copy-failure" ? folder + "/missing.starc" : original;
        for (auto field : dialog->findChildren<TextField*>()) {
            if (field->text().isEmpty()) {
                field->setText(import);
            }
        }
        require(dialog->importFilePath() == import, "create dialog imports the intended story");
        dialog->createProjectPressed();
        if (_mode == "conversion-copy-failure") {
            require(!QFile::exists(folder + "/copy.starc") && remembered() == original
                        && DatabaseLayer::Database::currentFile() == original,
                    "failed source copy publishes nothing and retains the current story");
            require(!QFile::exists(folder + "/copy.starc.lock"),
                    "failed source copy releases its destination lock");
        } else {
            require(remembered() == folder + "/copy.starc", "copied story opens successfully");
        }
        verifyScript(_root);
        require(readFile(original) == originalBytes, "story copy preserves its source bytes");
    } else {
        const QByteArray sourceBytes
            = "INT. LAB - DAY\n\nConversion retains the source screenplay.\n";
        QFile input(source);
        require(input.open(QIODevice::WriteOnly) && input.write(sourceBytes) == sourceBytes.size(),
                "create disposable Fountain source");
        input.close();
        const bool existing = _mode == "conversion-existing" || _mode == "conversion-locked";
        const bool symlink = _mode == "conversion-symlink" || _mode == "conversion-dangling";
        if (existing) {
            require(QFile::copy(original, target), "prepare existing saved destination");
        } else if (symlink) {
            require(
                QFile::link(_mode == "conversion-dangling" ? folder + "/absent.starc" : original,
                            target),
                "prepare conversion destination symlink");
        }
        QLockFile held(target + ".lock");
        if (_mode == "conversion-locked" || _mode == "conversion-lock-only") {
            require(held.tryLock(), "hold destination lock on behalf of another instance");
        }
        require(_manager->openProject(source), "open source through the real coordinator");
        const auto shadow = DatabaseLayer::Database::currentFile();
        Dialog* conversion = nullptr;
        for (auto candidate : QApplication::allWidgets()) {
            if (auto dialog = qobject_cast<Dialog*>(candidate)) {
                for (auto label : dialog->findChildren<AbstractLabel*>()) {
                    if (label->text().contains(".FOUNTAIN")) {
                        conversion = dialog;
                    }
                }
            }
        }
        require(conversion, "source offers the normal Switch to .STARC choice");
        const auto folderPermissions = QFile::permissions(folder);
        if (_mode == "conversion-unwritable") {
            require(QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner),
                    "make only the disposable conversion directory unwritable");
        }
        conversion->finished({ 2, "Switch to .STARC", Dialog::AcceptButton });
        if (_mode == "conversion-unwritable") {
            require(QFile::setPermissions(folder, folderPermissions),
                    "restore disposable directory permissions");
        }
        require(readFile(source) == sourceBytes,
                "conversion attempt leaves source bytes unchanged");
        if (_mode == "conversion-success") {
            require(remembered() == target && DatabaseLayer::Database::currentFile() == target,
                    "successful conversion opens the new destination");
            widget<Ui::MenuView>()->saveProjectChangesPressed();
            auto query = DatabaseLayer::Database::query();
            require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next(),
                    "converted story contains a screenplay");
            const auto bytes = query.value(0).toByteArray();
            require(bytes.contains("Conversion retains the source screenplay."),
                    "conversion retains the imported script");
            require(_manager->openProject(original) && _manager->openProject(target),
                    "saved conversion reopens successfully");
            query = DatabaseLayer::Database::query();
            require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next()
                        && query.value(0).toByteArray() == bytes,
                    "converted script survives save and reopen byte exactly");
        } else {
            require(remembered() == source && DatabaseLayer::Database::currentFile() == shadow
                        && QFile::exists(source + ".lock"),
                    "refused conversion retains the open source and its lock");
            if (existing) {
                require(readFile(target) == originalBytes,
                        "existing destination is preserved byte exactly");
            } else if (_mode == "conversion-race") {
                require(readFile(target) == "Another writer owns these bytes.\n",
                        "publication collision preserves the other writer's exact bytes");
            } else if (symlink) {
                require(QFileInfo(target).isSymLink() && readFile(original) == originalBytes,
                        "refused conversion preserves the symlink and its referent");
                require(!QFile::exists(folder + "/absent.starc"),
                        "dangling destination symlink creates no referent");
            } else {
                require(!QFile::exists(target), "refused conversion creates no destination");
            }
            if (held.isLocked()) {
                require(QFile::exists(target + ".lock"), "other instance retains its lock file");
            } else {
                require(!QFile::exists(target + ".lock"), "failure releases destination lock");
            }
        }
    }
    bool noStagingFiles = true;
    for (const auto& entry :
         QDir(folder).entryList({ "*.starc.*", ".starc-create-*" }, QDir::Files | QDir::Hidden)) {
        noStagingFiles = noStagingFiles && entry.endsWith(".lock");
    }
    require(noStagingFiles, "conversion leaves no staging files");
}

void selectSaveAs(const QString& _target, Ui::MenuView* _menu, bool _fail = false)
{
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, [&] {
        if (auto dialog = widget<QFileDialog>(); dialog && dialog->isVisible()) {
            poll.stop();
            if (_fail) {
                QObject::connect(dialog, &QDialog::accepted, [&] {
                    require(QDir().rename(QFileInfo(_target).absolutePath(),
                                          QFileInfo(_target).absolutePath() + ".moved"),
                            "make Save As destination unavailable after selection");
                });
            }
            // Qt deliberately keeps a focused filename edit unchanged in selectFile().
            if (dialog->focusWidget()) {
                dialog->focusWidget()->clearFocus();
            }
            dialog->selectFile(_target);
            require(dialog->selectedFiles() == QStringList{ _target },
                    "Save As dialog selects the intended target before acceptance");
            std::printf("Save As selection: %s\n", qPrintable(dialog->selectedFiles().join(',')));
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
    });
    poll.start();
    _menu->saveProjectAsPressed();
    if (_fail) {
        require(QDir().rename(QFileInfo(_target).absolutePath() + ".moved",
                              QFileInfo(_target).absolutePath()),
                "restore disposable destination");
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    const auto mode = qEnvironmentVariable("STARC_STARTUP_TEST_MODE");
    const auto root = qEnvironmentVariable("STARC_STARTUP_TEST_ROOT");
    const auto path = root + "/remembered.starc";
    const auto explicitPath = root + "/explicit.starc";
    const bool onboarding = mode == "create" || mode.startsWith("onboarding");
    require(!root.isEmpty() && !mode.isEmpty(), "test mode and disposable directory provided");

    if (mode == "create") {
        require(!settingsValue(DataStorageLayer::kApplicationOpenLastStoryKey).toBool()
                    && remembered().isEmpty(),
                "fresh settings default to disabled with no story");
    }
    if (!mode.startsWith("persisted")) {
        setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, !onboarding);
        setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
        setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
        if (mode != "create") {
            setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, mode != "disabled");
            setSettingsValue(DataStorageLayer::kApplicationLastStoryPathKey,
                             mode == "unavailable" ? root + "/absent.starc" : path);
            if (mode.startsWith("auto-")) {
                setSettingsValue(DataStorageLayer::kApplicationLastStoryPathKey,
                                 root
                                     + (mode == "auto-corrupt"         ? "/corrupt.starc"
                                            : mode == "auto-malformed" ? "/malformed.fdx"
                                                                       : "/newer.starc"));
            }
        }
    }
    QString request;
    if (mode == "explicit" || mode == "onboarding-explicit") {
        request = explicitPath;
    } else if (mode == "explicit-failure") {
        request = root + "/absent-explicit.starc";
    }

    QLockFile held(path + ".lock");
    if (mode == "locked") {
        require(held.tryLock(), "hold remembered story lock before startup");
    }
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    // QFileOpenEvent after the manager exists but before startup must be buffered.
    if (mode == "early-event") {
        QFileOpenEvent event(explicitPath);
        QCoreApplication::sendEvent(&app, &event);
    }
    manager->exec(request);
    if (mode == "queued-event" || mode == "duplicate") {
        QCoreApplication::postEvent(&app,
                                    new QFileOpenEvent(mode == "duplicate" ? path : explicitPath));
    }

    QTimer closeQuestion;
    closeQuestion.setInterval(20);
    QObject::connect(&closeQuestion, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            auto dialog = qobject_cast<Dialog*>(candidate);
            if (!dialog || !dialog->isVisible()) {
                continue;
            }
            for (auto label : dialog->findChildren<AbstractLabel*>()) {
                if (label->text() == "Project was modified. Save changes?") {
                    closeQuestion.stop();
                    std::puts(
                        "ok: ordinary source close offers Save; choose Save through its dialog");
                    dialog->finished({ 2, "Save", Dialog::AcceptButton });
                    return;
                }
            }
        }
    });

    QTimer::singleShot(0, &app, [&] {
        // Run after both startup callbacks have had their turn, using an observable result.
        if (onboarding) {
            waitFor([&] { return widget<Ui::OnboardingNavigator>()->isVisible(); });
            require(!QFile::exists(path + ".lock"),
                    "onboarding is not displaced by automatic or explicit opening");
            widget<Ui::OnboardingNavigator>()->skipGitHubSyncRequested();
        }
        const bool noOpen = mode.startsWith("auto-") || mode == "persisted-disabled"
            || mode == "persisted-unavailable" || mode == "create" || mode == "disabled"
            || mode == "unavailable" || mode == "explicit-failure" || mode == "locked"
            || mode == "onboarding";
        const bool sourceReopen = mode == "persisted-fountain" || mode == "persisted-fdx";
        const auto expectedPath = sourceReopen
            ? root + (mode == "persisted-fountain" ? "/source.fountain" : "/source.fdx")
            : mode == "persisted-explicit" || mode == "explicit" || mode == "early-event"
                || mode == "queued-event" || mode == "onboarding-explicit"
            ? explicitPath
            : path;
        waitFor([&] {
            return noOpen ? widget<Ui::ProjectsView>()->isVisible()
                          : QFile::exists(expectedPath + ".lock");
        });
        // Flush queued duplicate requests while inspecting the real coordinator's open count.
        QCoreApplication::processEvents();
        if (!noOpen) {
            require(remembered() == expectedPath,
                    "startup selects the expected story and remembers only its source path");
            if (sourceReopen) {
                require(DatabaseLayer::Database::currentFile() != expectedPath,
                        "automatic source reopening uses an internal shadow while remembering the "
                        "source");
                widget<Ui::MenuView>()->saveProjectChangesPressed();
                auto query = DatabaseLayer::Database::query();
                require(
                    query.exec("SELECT content FROM documents WHERE type = 10104") && query.next()
                        && query.value(0).toByteArray().contains(mode == "persisted-fountain"
                                                                     ? "A source story survives."
                                                                     : "It's safe now."),
                    "automatic source reopening imports the screenplay again");
            } else {
                verifyScript(root);
                if (mode == "read-only") {
                    require(widget<Ui::ScreenplayTextEdit>()->isReadOnly(),
                            "automatic reopening retains the read-only editor policy");
                }
            }
            manager->exec(root + "/absent-repeat.starc");
            require(!manager->openProject(QFileInfo(expectedPath).absoluteFilePath()),
                    "duplicate file request uses the already open story");
            QFile log(manager->logFilePath());
            require(log.open(QIODevice::ReadOnly), "read retained application lifecycle log");
            require(
                log.readAll().count("Closing current project") == 1,
                "queued callbacks, repeated exec and duplicate file requests load at most once");
        } else {
            require(widget<Ui::ProjectsView>()->isVisible(), "story list remains usable");
            if (mode != "locked") {
                require(!QFile::exists(path + ".lock"),
                        "disabled, failed and unavailable startup does not open a fallback");
            }
            require(!QFile::exists(root + "/absent.starc")
                        && !QFile::exists(root + "/absent-explicit.starc"),
                    "missing startup paths do not create empty project files");
        }
        auto settings = widget<Ui::SettingsView>();
        CheckBox* option = nullptr;
        for (auto box : settings->findChildren<CheckBox*>()) {
            if (box->text() == "Open the last story on startup") {
                option = box;
            }
        }
        require(option, "application settings expose the clearly labelled startup option");
        require(option->backgroundColor() == Ui::DesignSystem::color().background()
                    && option->textColor() == Ui::DesignSystem::color().onBackground(),
                "startup option uses the settings theme with a visible label");
        int changes = 0;
        QObject::connect(settings, &Ui::SettingsView::applicationOpenLastStoryChanged, &app,
                         [&](bool) { ++changes; });
        settings->setApplicationOpenLastStory(!option->isChecked());
        require(changes == 0, "programmatic settings population emits no change");
        settings->setApplicationOpenLastStory(
            settingsValue(DataStorageLayer::kApplicationOpenLastStoryKey).toBool());

        if (mode == "create") {
            seedScript(root);
            option->setChecked(true);
            require(changes == 1
                        && settingsValue(DataStorageLayer::kApplicationOpenLastStoryKey).toBool(),
                    "checkbox persists the enabled preference");
        } else if (mode == "save-as") {
            const auto target = root + "/saved-as.starc";
            widget<Ui::MenuView>()->saveProjectChangesPressed();
            selectSaveAs(target, widget<Ui::MenuView>());
            require(remembered() == target, "successful Save As follows the new target");
            verifyScript(root);
            const auto before = remembered();
            require(QDir().mkpath(root + "/fail-save"), "create disposable Save As directory");
            selectSaveAs(root + "/fail-save/failure.starc", widget<Ui::MenuView>(), true);
            require(remembered() == before, "failed Save As retains the last successful story");
        }
        if (mode == "failures") {
            const auto before = remembered();
            for (const auto& failure : { root + "/absent.starc", root + "/malformed.fdx",
                                         root + "/unsupported.xyz", root + "/corrupt.starc" }) {
                require(!manager->openProject(failure), "unsuccessful open reports failure");
                require(remembered() == before, "failed operation cannot replace remembered story");
            }
            waitFor([&] { return widget<Ui::ProjectsView>()->isVisible(); });
            require(widget<Ui::ProjectsView>()->isVisible(),
                    "failed imports and database opens return to the story list");
            widget<Ui::MenuView>()->createProjectPressed();
            auto dialog = widget<Ui::CreateProjectDialog>();
            require(dialog, "create dialog remains available after failed opens");
            dialog->setProjectName("cannot-create");
            dialog->setProjectFolder(root + "/unsupported.xyz");
            dialog->createProjectPressed();
            require(remembered() == before, "failed creation retains the last successful story");
            dialog->hideDialog();
        }
        if (mode == "fountain" || mode == "fdx") {
            const auto source = root + (mode == "fountain" ? "/source.fountain" : "/source.fdx");
            require(manager->openProject(source), "source screenplay opens successfully");
            require(remembered() == source && DatabaseLayer::Database::currentFile() != source,
                    "remember meaningful source instead of temporary shadow database");
            widget<Ui::MenuView>()->saveProjectChangesPressed();
        }
        if (mode.startsWith("conversion-")) {
            verifyConversion(manager, root, mode);
        }
        if (!noOpen && mode != "failures" && mode != "fountain" && mode != "fdx") {
            const auto before = DatabaseLayer::Database::currentFile();
            option->setChecked(false);
            require(DatabaseLayer::Database::currentFile() == before,
                    "changing the option does not switch the currently open story");
            option->setChecked(true);
        }
        DataStorageLayer::StorageFacade::settingsStorage()->sync();
        QSettings disk;
        require(
            disk.value(DataStorageLayer::kApplicationLastStoryPathKey.toUtf8().toHex()).toString()
                    == remembered()
                && disk.value(DataStorageLayer::kApplicationOpenLastStoryKey.toUtf8().toHex(),
                              false)
                        .toBool()
                    == settingsValue(DataStorageLayer::kApplicationOpenLastStoryKey).toBool(),
            "settings on disk match the preference and successful source path");
        if (qEnvironmentVariableIsSet("STARC_STARTUP_SCREENSHOTS")) {
            auto window = widget<Ui::ApplicationView>();
            waitFor([&] {
                const auto content = qobject_cast<StackWidget*>(window->view());
                return content->currentWidget()->isVisible()
                    && ((noOpen && mode != "create") || mode == "failures"
                            ? widget<Ui::ProjectsView>()->isVisible()
                            : widget<Ui::ProjectNavigator>()->isVisible());
            });
            const auto screenshots = root + "/../screenshots";
            require(QDir().mkpath(screenshots), "create native screenshot directory");
            window->repaint();
            QGuiApplication::sync();
            require(window->screen()
                        ->grabWindow(window->winId())
                        .save(screenshots + "/" + mode + ".png"),
                    "retain native XCB window screenshot");
            if (mode == "create") {
                widget<Ui::MenuView>()->settingsPressed();
                waitFor([&] { return settings->isVisible(); });
                window->repaint();
                QGuiApplication::sync();
                require(window->screen()
                            ->grabWindow(window->winId())
                            .save(screenshots + "/settings.png"),
                        "retain native settings screenshot with startup checkbox");
            }
        }
        if (mode == "fountain" || mode == "fdx" || sourceReopen
            || mode.startsWith("conversion-")) {
            closeQuestion.start();
        }
        widget<Ui::ApplicationView>()->close();
    });
    QTimer::singleShot(30000, &app,
                       [] { require(false, "ordinary close completes before watchdog"); });
    const auto result = app.exec();
    require(result == 0, "application exits ordinarily with status zero");
    if (mode != "locked") {
        require(!QFile::exists(remembered() + ".lock"),
                "ordinary shutdown releases the story lock");
    }
    std::puts("PASS: startup last story regression");
    return result;
}
