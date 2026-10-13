#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/github_credential_store.h>
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
#include <ui/projects/projects_navigator.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/tree/tree.h>
#include <utils/tools/github_files_client.h>
#include <utils/tools/project_sync.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileOpenEvent>
#include <QPointer>
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
    require(_ready(), "observable operation completes before deadline");
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

Dialog* dialogWithText(const QString& _text)
{
    for (auto candidate : QApplication::allWidgets()) {
        auto dialog = qobject_cast<Dialog*>(candidate);
        if (!dialog || !dialog->isVisible()) {
            continue;
        }
        for (auto label : dialog->findChildren<AbstractLabel*>()) {
            if (label->text().contains(_text)) {
                return dialog;
            }
        }
    }
    return nullptr;
}

void answer(const QString& _text, const Dialog::ButtonInfo& _button)
{
    waitFor([&] { return dialogWithText(_text) != nullptr; });
    QPointer<Dialog> dialog = dialogWithText(_text);
    dialog->finished(_button);
    waitFor([&] { return dialog.isNull() || !dialog->isVisible(); });
}

QByteArray script()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next(),
            "read saved screenplay XML");
    return query.value(0).toByteArray();
}

QByteArray persistedScript(const QString& _path)
{
    QByteArray bytes;
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", "persisted-script");
        database.setDatabaseName(_path);
        require(database.open(), "open independent persistence reader");
        QSqlQuery query(database);
        require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next(),
                "read committed screenplay from an independent connection");
        bytes = query.value(0).toByteArray();
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase("persisted-script");
    return bytes;
}

QByteArray fileBytes(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable file");
    return file.readAll();
}

void retain(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain exact recovery oracle");
}

Ui::ScreenplayTextEdit* openEditor()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT uuid FROM documents WHERE type = 10104") && query.next(),
            "saved story has a screenplay");
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<BusinessLayer::StructureModel*>(proxy->sourceModel());
    navigator->setCurrentIndex(proxy->mapFromSource(
        structure->indexForItem(structure->itemForUuid(QUuid(query.value(0).toString())))));
    auto edit = widget<Ui::ScreenplayTextEdit>();
    require(edit, "real screenplay editor is open");
    return edit;
}

void save()
{
    auto document
        = qobject_cast<BusinessLayer::TextDocument*>(widget<Ui::ScreenplayTextEdit>()->document());
    document->model()->saveChanges();
    widget<Ui::MenuView>()->saveProjectChangesPressed();
}

void rejectWrites(bool _reject, bool _commitFailure = false)
{
    auto query = DatabaseLayer::Database::query();
    if (qEnvironmentVariable("STARC_SAVE_FAILURE_MODE").startsWith("rollback-")) {
        require(query.exec(_reject
                               ? "CREATE TRIGGER reject_save BEFORE INSERT ON documents_changes "
                                 "BEGIN SELECT RAISE(ROLLBACK, 'test automatic rollback'); END"
                               : "DROP TRIGGER reject_save"),
                _reject ? "abort transaction after document writes, while saving history"
                        : "remove automatic rollback fault");
        return;
    }
    if (_commitFailure) {
        if (_reject) {
            require(query.exec("PRAGMA foreign_keys = ON")
                        && query.exec("CREATE TABLE save_parent (id INTEGER PRIMARY KEY)")
                        && query.exec("CREATE TABLE save_child (parent INTEGER REFERENCES "
                                      "save_parent(id) DEFERRABLE INITIALLY DEFERRED)")
                        && query.exec("CREATE TRIGGER reject_save AFTER UPDATE ON documents BEGIN "
                                      "INSERT INTO save_child VALUES (404); END"),
                    "reject COMMIT after successful document updates with a deferred constraint");
        } else {
            require(query.exec("DROP TRIGGER reject_save")
                        && query.exec("INSERT INTO save_parent VALUES (404)"),
                    "repair the deferred constraint before resubmitting pending writes");
        }
        return;
    }
    require(query.exec(_reject
                           ? "CREATE TRIGGER reject_save BEFORE UPDATE ON documents BEGIN "
                             "SELECT RAISE(ABORT, 'test persistent document write failure'); END"
                           : "DROP TRIGGER reject_save"),
            _reject ? "reject document writes in disposable story" : "restore document writes");
}

void screenshot(const QString& _root, const QString& _name)
{
    if (!qEnvironmentVariableIsSet("STARC_SAVE_FAILURE_SCREENSHOTS")) {
        return;
    }
    auto window = widget<Ui::ApplicationView>();
    window->repaint();
    QGuiApplication::sync();
    require(window->screen()->grabWindow(window->winId()).save(_root + "/" + _name + ".png"),
            "retain native window screenshot");
}

void fileOpen(Application& _app, const QString& _path)
{
    QFileOpenEvent event(_path);
    require(QCoreApplication::sendEvent(&_app, &event),
            "application handles actual QFileOpenEvent");
}

int saveQuestionCount()
{
    int count = 0;
    for (auto candidate : QApplication::allWidgets()) {
        if (auto dialog = qobject_cast<Dialog*>(candidate); dialog && dialog->isVisible()) {
            for (auto label : dialog->findChildren<AbstractLabel*>()) {
                if (label->text().contains("Save changes?")) {
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    const auto root = qEnvironmentVariable("STARC_SAVE_FAILURE_ROOT");
    const auto mode = qEnvironmentVariable("STARC_SAVE_FAILURE_MODE");
    const auto path = root + "/story.starc";
    const auto target = root + "/target.starc";
    require(!root.isEmpty() && !mode.isEmpty(), "disposable root and test mode provided");
    const bool sync = mode.startsWith("sync-");
    const bool fileEvent = mode.startsWith("file-open-");
    const bool commitFailure = mode.startsWith("commit-") || mode == "file-open-commit";
    if (sync) {
        // Exercise the production credential boundary's retained-legacy route with a fake token
        // and an absent private session bus. No desktop keyring or real repository is used.
        QSettings settings;
        settings.setValue(DataStorageLayer::kGitHubSyncTokenKey.toUtf8().toHex(), "test-token");
        settings.sync();
    }
    setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, mode == "reopen");
    setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, false);
    setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
    setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    setSettingsValue(DataStorageLayer::kApplicationBackupsFolderKey, root + "/backups");
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    manager->exec(mode == "reopen" ? path : QString());
    QTimer::singleShot(0, &app, [&] {
        auto window = widget<Ui::ApplicationView>();
        auto menu = widget<Ui::MenuView>();
        if (mode == "reopen") {
            waitFor([&] { return DatabaseLayer::Database::currentFile() == path; });
            auto edit = openEditor();
            require(script() == fileBytes(root + "/expected.xml"),
                    "fresh process reopens exact script, emphasis and review bytes");
            const auto ids = fileBytes(root + "/identities.txt");
            auto query = DatabaseLayer::Database::query();
            require(query.exec("SELECT uuid FROM documents ORDER BY uuid"), "read document IDs");
            QByteArray reopenedIds;
            while (query.next()) {
                reopenedIds += query.value(0).toString().toUtf8() + '\n';
            }
            require(ids == reopenedIds, "fresh process preserves every document identity");
            require(edit->toPlainText().contains("UNSAVED_MARKER")
                        == (qEnvironmentVariable("STARC_EXPECT_MARKER") == "1"),
                    "reopened text matches the writer's Save or Don't save choice");
            if (QFile::exists(target)) {
                require(manager->openProject(target), "fresh process opens the event target");
                require(script() == fileBytes(root + "/target.xml"),
                        "event target reopens with its exact original screenplay and annotations");
                query = DatabaseLayer::Database::query();
                require(query.exec("SELECT uuid FROM documents ORDER BY uuid"),
                        "read target document identities");
                QByteArray targetIds;
                while (query.next()) {
                    targetIds += query.value(0).toString().toUtf8() + '\n';
                }
                require(targetIds == ids, "event target keeps every original document identity");
            }
            window->close();
            return;
        }

        waitFor([] { return widget<Ui::OnboardingNavigator>()->isVisible(); });
        widget<Ui::OnboardingNavigator>()->skipGitHubSyncRequested();
        waitFor([&] { return window->view()->isVisible() && window->width() >= 1200; });
        waitFor([] { return GitHubCredentialStore::instance()->isReady(); });
        while (dialogWithText("system keyring")) {
            answer("system keyring", { 0, "Close", Dialog::RejectButton });
        }
        menu->createProjectPressed();
        auto creation = widget<Ui::CreateProjectDialog>();
        require(creation, "new story uses the production creation dialog");
        creation->setProjectName("story");
        creation->setProjectFolder(root);
        creation->setProjectType(static_cast<int>(Domain::DocumentObjectType::Screenplay));
        creation->createProjectPressed();
        menu->saveProjectChangesPressed();
        auto edit = openEditor();
        waitFor([&] { return edit->isVisible(); });
        edit->setCurrentParagraphType(BusinessLayer::TextParagraphType::Action);
        edit->insertPlainText("Zoë retains her edits.");
        auto cursor = edit->document()->find("Zoë");
        require(!cursor.isNull(), "Unicode text is present");
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        cursor.mergeCharFormat(bold);
        BusinessLayer::TextModelTextItem::ReviewMark mark;
        mark.from = 0;
        mark.length = 3;
        mark.backgroundColor = Qt::yellow;
        BusinessLayer::TextModelTextItem::ReviewComment comment;
        comment.author = "Reviewer";
        comment.text = "Retain this annotation";
        mark.comments.append(comment);
        cursor.mergeCharFormat(mark.charFormat());
        save();
        const auto original = script();
        require(original.contains("Retain this annotation") && original.contains("bold="),
                "baseline contains emphasis and review annotations");
        auto query = DatabaseLayer::Database::query();
        require(query.exec("SELECT uuid FROM documents ORDER BY uuid"), "read baseline IDs");
        QByteArray ids;
        while (query.next()) {
            ids += query.value(0).toString().toUtf8() + '\n';
        }
        retain(root + "/identities.txt", ids);

        if (fileEvent) {
            require(QFile::copy(path, target), "prepare another disposable story for file events");
            retain(root + "/target.xml", original);
            const auto targetBytes = fileBytes(target);
            if (mode == "file-open-clean") {
                fileOpen(app, target);
                require(DatabaseLayer::Database::currentFile() == target && saveQuestionCount() == 0
                            && !QFile::exists(path + ".lock"),
                        "clean file-open switches immediately without a save question");
                retain(root + "/expected.xml", original);
                window->close();
                return;
            }
            edit->moveCursor(QTextCursor::End);
            edit->insertPlainText(" UNSAVED_MARKER");
            waitFor([&] { return window->isWindowModified(); });
            if (mode == "file-open-same") {
                fileOpen(app, root + "/./story.starc");
                require(DatabaseLayer::Database::currentFile() == path && window->isWindowModified()
                            && saveQuestionCount() == 0
                            && edit->toPlainText().contains("UNSAVED_MARKER"),
                        "same-story event preserves unsaved edits without a question");
                save();
                retain(root + "/expected.xml", script());
                window->close();
                return;
            }
            if (mode == "file-open-failure" || mode == "file-open-commit") {
                rejectWrites(true, commitFailure);
            }
            if (mode == "file-open-menu") {
                menu->openProjectPressed();
            } else {
                fileOpen(app, target);
            }
            require(
                DatabaseLayer::Database::currentFile() == path && window->isWindowModified()
                    && QFile::exists(path + ".lock") && !QFile::exists(target + ".lock")
                    && fileBytes(target) == targetBytes
                    && edit->toPlainText().contains("UNSAVED_MARKER"),
                "file-open asks before switching and keeps source edits, lock and target bytes");
            waitFor([] { return saveQuestionCount() == 1; });
            screenshot(root, "file-open-question");
            if (mode == "file-open-repeated" || mode == "file-open-menu") {
                const auto other = root + "/other.starc";
                require(QFile::copy(target, other), "prepare a competing external open request");
                fileOpen(app, other);
                require(!manager->openProject(other),
                        "public coordinator reports no immediate open during the save question");
                require(DatabaseLayer::Database::currentFile() == path && saveQuestionCount() == 1
                            && fileBytes(other) == targetBytes,
                        "repeated requests neither replace the first question nor open a story");
                if (mode == "file-open-repeated") {
                    save();
                    require(!window->isWindowModified(), "manual Save settles the live edits");
                    fileOpen(app, other);
                    require(
                        DatabaseLayer::Database::currentFile() == path && saveQuestionCount() == 1,
                        "external event cannot bypass an outstanding question after manual Save");
                }
            }
            if (mode == "file-open-cancel" || mode == "file-open-menu") {
                answer("Save changes?", { 0, "Cancel", Dialog::RejectButton });
                require(window->isWindowModified() && DatabaseLayer::Database::currentFile() == path
                            && QFile::exists(path + ".lock") && fileBytes(target) == targetBytes,
                        "Cancel keeps the original story, dirty state, lock and unopened target");
                edit->undo();
                require(!edit->toPlainText().contains("UNSAVED_MARKER"),
                        "Undo survives a canceled external request");
                edit->redo();
                require(edit->toPlainText().contains("UNSAVED_MARKER"),
                        "Redo restores the edits after Cancel");
                save();
                retain(root + "/expected.xml", script());
                window->close();
                return;
            }
            answer("Save changes?",
                   mode == "file-open-discard"
                       ? Dialog::ButtonInfo{ 1, "Don't save", Dialog::NormalButton }
                       : Dialog::ButtonInfo{ 2, "Save", Dialog::AcceptButton });
            if (mode != "file-open-failure" && mode != "file-open-commit") {
                require(
                    DatabaseLayer::Database::currentFile() == target
                        && !QFile::exists(path + ".lock") && QFile::exists(target + ".lock")
                        && script() == original,
                    "approved file-open switches to the requested story and transfers the lock");
                const auto expected = persistedScript(path);
                require(
                    expected.contains("UNSAVED_MARKER") == (mode != "file-open-discard")
                        && expected.contains("bold=")
                        && expected.contains("Retain this annotation"),
                    "Save preserves edits and annotations; Don't save preserves committed text");
                if (mode == "file-open-discard") {
                    require(expected == original, "explicit Don't save keeps exact committed XML");
                }
                retain(root + "/expected.xml", expected);
                screenshot(root, "file-open-switched");
                window->close();
                return;
            }
        }

        GitHubFilesClient client;
        if (sync) {
            waitFor([] { return GitHubCredentialStore::instance()->isReady(); });
            require(GitHubCredentialStore::instance()->token() == "test-token",
                    "isolated credential boundary supplies only the fake token");
            setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, "test-owner/stories");
            widget<Ui::ProjectsNavigator>()->syncProjectsPressed();
            answer("Not on GitHub yet", { 1, "Sync", Dialog::AcceptButton });
            waitFor([&] {
                return QByteArray::fromHex(
                           settingsValue(DataStorageLayer::kGitHubSyncProjectsKey).toByteArray())
                    .contains(path.toUtf8());
            });
            const auto content = fileBytes(path);
            const auto remotePath = root + "/remote.starc";
            require(QFile::copy(path, remotePath), "prepare disposable newer remote story");
            {
                auto remote = QSqlDatabase::addDatabase("QSQLITE", "remote-fixture");
                remote.setDatabaseName(remotePath);
                require(remote.open(), "open remote fixture database");
                QSqlQuery update(remote);
                update.prepare("UPDATE documents SET content = ? WHERE type = 10104");
                auto remoteXml = original;
                remoteXml.replace("retains her edits", "has a newer remote version");
                update.addBindValue(remoteXml);
                require(update.exec(), "change only the remote fixture's screenplay");
                remote.close();
            }
            QSqlDatabase::removeDatabase("remote-fixture");
            client.setRepository("test-owner/stories");
            client.setToken("test-token");
            bool uploaded = false;
            client.upload("story.starc", fileBytes(remotePath), ProjectSync::gitBlobSha(content),
                          "Remote fixture change", [&](const auto& error, const auto&) {
                              require(!error.isValid(), "publish newer version to loopback mock");
                              uploaded = true;
                          });
            waitFor([&] { return uploaded; });
            if (mode == "sync-success") {
                const auto remoteBytes = fileBytes(remotePath);
                widget<Ui::ProjectsNavigator>()->syncProjectsPressed();
                answer("Newer on GitHub", { 1, "Sync", Dialog::AcceptButton });
                waitFor([&] { return fileBytes(path) == remoteBytes; });
                require(!QFile::exists(path + ".lock"),
                        "successful replacement closes the saved story and releases its lock");
                const auto backups = QDir(root + "/backups").entryList({ "*.starc" }, QDir::Files);
                require(backups.size() == 1
                            && fileBytes(root + "/backups/" + backups.first()) == content,
                        "successful replacement retains a byte exact backup");
                require(manager->openProject(path), "open successfully downloaded story");
                retain(root + "/expected.xml", script());
                window->close();
                return;
            }
        }

        if (!fileEvent) {
            edit->moveCursor(QTextCursor::End);
            edit->insertPlainText(" UNSAVED_MARKER");
            waitFor([&] { return window->isWindowModified(); });
            rejectWrites(true, commitFailure);
        }
        if (sync) {
            widget<Ui::ProjectsNavigator>()->syncProjectsPressed();
            answer("internal database error", { 0, "OK", Dialog::RejectButton });
            answer("Newer on GitHub", { 1, "Sync", Dialog::AcceptButton });
            waitFor([&] { return dialogWithText("Not all stories were synced") != nullptr; });
        } else if (!fileEvent) {
            if (mode == "switch-failure") {
                menu->openProjectPressed();
            } else {
                window->close();
            }
            answer("Save changes?", { 2, "Save", Dialog::AcceptButton });
        }
        require(window->isWindowModified() && DatabaseLayer::Database::currentFile() == path
                    && QFile::exists(path + ".lock")
                    && edit->toPlainText().contains("UNSAVED_MARKER"),
                "failed Save retains live edits, dirty state, current story and its lock");
        require(persistedScript(path) == original,
                "failed Save does not replace the committed screenplay");
        require(!DatabaseLayer::Database::isTransactionActive(),
                "failed Save unwinds its transaction so Retry can resubmit all writes");
        if (sync) {
            require(QDir(root + "/backups").entryList(QDir::Files).isEmpty(),
                    "refused replacement publishes no backup or downloaded story");
            require(dialogWithText("was kept open and wasn't replaced") != nullptr,
                    "sync reports failed Save as the reason replacement was refused");
            answer("Not all stories were synced", { 0, "OK", Dialog::RejectButton });
        }
        screenshot(root, "save-failed");
        edit->undo();
        require(!edit->toPlainText().contains("UNSAVED_MARKER"),
                "Undo remains available after failed Save");
        edit->redo();
        require(edit->toPlainText().contains("UNSAVED_MARKER"),
                "Redo restores the unsaved edit after failed Save");

        if (mode == "close-retry" || mode == "commit-retry" || mode == "rollback-retry"
            || fileEvent) {
            answer("internal database error", { 1, "Retry", Dialog::AcceptButton });
            require(window->isWindowModified() && edit->toPlainText().contains("UNSAVED_MARKER"),
                    "another failed Retry still retains edits");
            if (commitFailure || mode == "rollback-retry") {
                edit->moveCursor(QTextCursor::End);
                edit->insertPlainText(" EDIT_AFTER_FAILED_COMMIT");
                qobject_cast<BusinessLayer::TextDocument*>(edit->document())
                    ->model()
                    ->saveChanges();
            }
            rejectWrites(false, commitFailure);
            answer("internal database error", { 1, "Retry", Dialog::AcceptButton });
            if (fileEvent) {
                require(DatabaseLayer::Database::currentFile() == path
                            && !window->isWindowModified() && QFile::exists(path + ".lock")
                            && !QFile::exists(target + ".lock"),
                        "successful Retry keeps the story open until a new file-open request");
            }
        } else {
            answer("internal database error", { 0, "OK", Dialog::RejectButton });
            require(window->isWindowModified(), "dismissing saving error retains dirty state");
            if (mode == "close-discard" || mode == "commit-discard" || mode == "rollback-discard") {
                retain(root + "/expected.xml", original);
                window->close();
                answer("Save changes?", { 1, "Don't save", Dialog::NormalButton });
                return;
            }
            window->close();
            answer("Save changes?", { 0, "Cancel", Dialog::RejectButton });
            require(window->isWindowModified() && QFile::exists(path + ".lock"),
                    "Cancel after failed Save keeps the story open");
            rejectWrites(false, commitFailure);
            save();
        }
        require(!window->isWindowModified() && !DatabaseLayer::Database::hasError()
                    && !DatabaseLayer::Database::isTransactionActive(),
                "saving succeeds after storage recovers");
        const auto recovered = script();
        require(recovered.contains("UNSAVED_MARKER") && recovered.contains("bold=")
                    && recovered.contains("Retain this annotation"),
                "recovered save includes edits, emphasis and review annotations");
        if (mode == "commit-retry" || mode == "rollback-retry") {
            require(recovered.contains("EDIT_AFTER_FAILED_COMMIT"),
                    "Retry also saves edits made after the failed commit");
        }
        retain(root + "/expected.xml", recovered);
        screenshot(root, "save-recovered");
        if (fileEvent) {
            fileOpen(app, target);
            require(DatabaseLayer::Database::currentFile() == target && script() == original,
                    "new file-open request succeeds after the failed Save has recovered");
        }
        window->close();
    });
    QTimer::singleShot(45000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto result = app.exec();
    require(result == 0 && !QFile::exists(path + ".lock"),
            "ordinary exit zero releases the story lock");
    std::puts("PASS: save failure regression");
    return result;
}
