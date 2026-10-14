// The first edit of a screenplay imported with styled review notes (ISSUES2.md issue 15):
// one-step Undo/Redo through the real coordinator and screenplay editor must restore the exact
// model XML, comments panel and editor text, before and after an initial Save and after a fresh
// native reopening, and Save must keep the undone or redone screenplay for a fresh process.
#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <data_layer/database.h>
#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/application_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <ui/application_view.h>
#include <ui/menu_view.h>
#include <ui/modules/comments/comments_model.h>
#include <ui/project/project_navigator.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/splitter/splitter.h>
#include <ui/widgets/tree/tree.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QTextCursor>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {
using namespace BusinessLayer;

const QString kTyped = QStringLiteral(" SAVEMARKER");

void require(bool _ok, const char* _label)
{
    std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
    if (!_ok) {
        std::exit(1);
    }
}

void waitFor(const std::function<bool()>& _ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!_ready() && timer.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    require(_ready(), "observable operation completes");
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

QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable source/oracle");
    return file.readAll();
}

void write(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain exact oracle");
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

Ui::ScreenplayTextEdit* openEditor()
{
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<StructureModel*>(proxy->sourceModel());
    require(structure, "project navigator shows the story structure");
    std::function<StructureModelItem*(StructureModelItem*)> findScript;
    findScript = [&](StructureModelItem* _item) -> StructureModelItem* {
        if (_item->type() == Domain::DocumentObjectType::ScreenplayText) {
            return _item;
        }
        for (int child = 0; child < _item->childCount(); ++child) {
            if (auto result = findScript(_item->childAt(child))) {
                return result;
            }
        }
        return nullptr;
    };
    // A reopened story loads its structure after the coordinator opens its file.
    waitFor([&] { return findScript(structure->itemForIndex({ })) != nullptr; });
    navigator->setCurrentIndex(
        proxy->mapFromSource(structure->indexForItem(findScript(structure->itemForIndex({ })))));
    auto edit = widget<Ui::ScreenplayTextEdit>();
    require(edit, "real screenplay editor opens");
    waitFor([&] { return edit->isVisible(); });
    return edit;
}

// The live model XML, without flushing pending changes into the change history first.
struct ModelXml : TextModel {
    static QByteArray of(const TextModel* _model)
    {
        return (_model->*(&ModelXml::toXml))();
    }
};

QByteArray xmlOf(const TextModel* _model)
{
    return ModelXml::of(_model);
}

// The committed screenplay XML and every document identity of the open native story.
QByteArray storedScript(const QUuid& _uuid)
{
    auto query = DatabaseLayer::Database::query();
    query.prepare("SELECT content FROM documents WHERE uuid = ?");
    query.addBindValue(_uuid.toString());
    require(query.exec() && query.next(), "read committed screenplay XML");
    return query.value(0).toByteArray();
}

QByteArray identities()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT uuid, type FROM documents ORDER BY uuid"),
            "read committed document identities");
    QByteArray result;
    while (query.next()) {
        result += (query.value(0).toString() + ' ' + query.value(1).toString() + '\n').toUtf8();
    }
    return result;
}

// Every comments panel thread: its text, messages with their metadata, color and state.
QJsonArray threads(const CommentsModel& _comments)
{
    QJsonArray result;
    for (int row = 0; row < _comments.rowCount(); ++row) {
        const auto index = _comments.index(row, 0);
        QJsonArray messages;
        for (const auto& message : index.data(CommentsModel::ReviewMarkRepliesRole)
                                       .value<QVector<TextModelTextItem::ReviewComment>>()) {
            messages.append(QJsonArray{ message.text, message.author, message.authorEmail,
                                        message.date, message.isEdited });
        }
        result.append(QJsonObject{
            { "text", index.data(CommentsModel::ReviewMarkSourceTextRole).toString() },
            { "messages", messages },
            { "color", index.data(CommentsModel::ReviewMarkColorRole).value<QColor>().name() },
            { "done", index.data(CommentsModel::ReviewMarkIsDoneRole).toBool() },
        });
    }
    return result;
}

// Show both thread lists when they differ, so a failed comparison explains itself.
bool sameThreads(const QJsonArray& _actual, const QJsonArray& _expected)
{
    if (_actual != _expected) {
        std::printf("threads: %s\nexpected threads: %s\n",
                    QJsonDocument(_actual).toJson(QJsonDocument::Compact).constData(),
                    QJsonDocument(_expected).toJson(QJsonDocument::Compact).constData());
    }
    return _actual == _expected;
}

QJsonArray freshThreads(TextModel* _model)
{
    CommentsModel comments;
    comments.setTextModel(_model);
    return threads(comments);
}

QJsonArray withoutTyped(QJsonArray _threads)
{
    for (int row = 0; row < _threads.size(); ++row) {
        auto thread = _threads.at(row).toObject();
        thread["text"] = thread["text"].toString().remove(kTyped);
        _threads[row] = thread;
    }
    return _threads;
}

QList<QByteArray> lines(const QByteArray& _xml)
{
    return _xml.split('\n');
}

class MemoryCredentials final : public GitHubCredentialBackend
{
public:
    void read(Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Missing, { }); });
    }
    void write(const QString&, Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }
    void remove(Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }
};
} // namespace

GitHubCredentialStore* GitHubCredentialStore::instance()
{
    static auto store
        = new GitHubCredentialStore(std::make_unique<MemoryCredentials>(),
                                    { [] { return QString(); }, [] { return true; } }, qApp);
    return store;
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    require(argc == 3, "private case directory and phase");
    const QString root = QString::fromLocal8Bit(argv[1]);
    require(QDir().mkpath(root + "/tmp"), "private temporary directory");
    qputenv("TMPDIR", (root + "/tmp").toUtf8());
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    //
    // prepare: import, Save and close without editing
    // import-*: edit first after import, before any Save
    // saved-*: edit first after an initial Save of the import
    // native-*: edit first after a fresh process reopens the prepared native story
    // verify: a fresh process reopens the saved story
    //
    const QString phase = QString::fromLocal8Bit(argv[2]);
    const auto start = phase.section('-', 0, 0);
    const auto finalState = phase.section('-', 1);
    require(phase == "prepare" || phase == "verify"
                || ((start == "import" || start == "saved" || start == "native")
                    && (finalState == "undone" || finalState == "redone")),
            "known phase");
    const bool isNative = start == "native" || start == "verify";
    const auto source = root + "/source.fountain";
    const auto project = root + "/story.starc";
    const auto originalSource = read(source);
    bool pickerSeen = false;
    QTimer picker;
    picker.setInterval(10);
    QObject::connect(&picker, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            if (auto chooser = qobject_cast<QFileDialog*>(candidate);
                chooser && chooser->isVisible() && !pickerSeen) {
                pickerSeen = true;
                chooser->selectFile(project);
                QMetaObject::invokeMethod(chooser, "accept", Qt::QueuedConnection);
            }
        }
    });
    picker.start();
    setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, true);
    setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, false);
    setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
    setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    manager->exec(isNative ? project : source);
    QTimer::singleShot(0, &app, [&] {
        waitFor([&] {
            return DatabaseLayer::Database::currentFile() == project
                && settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString()
                == project;
        });
        picker.stop();
        require(pickerSeen != isNative && !QFile::exists(source + ".lock"),
                isNative ? "native story reopens without an import chooser"
                         : "source imports into a chosen native story without a source lock");
        auto window = widget<Ui::ApplicationView>();
        window->resize(1320, 900);
        auto splitter = window->findChild<Splitter*>(QString(), Qt::FindDirectChildrenOnly);
        require(splitter, "application window owns its split view");
        splitter->setSizes({ 250, 750 });
        auto edit = openEditor();
        auto document = qobject_cast<TextDocument*>(edit->document());
        require(document, "editor binds the screenplay model");
        auto model = document->model();
        const auto uuid = model->document()->uuid();
        auto structure = qobject_cast<StructureModel*>(
            qobject_cast<QSortFilterProxyModel*>(
                widget<Ui::ProjectNavigator>()->findChild<Tree*>()->model())
                ->sourceModel());
        // Save commits the changes each model has recorded; record any still pending first, as
        // the other coordinator fixtures do, so that a Save right after import keeps its structure.
        auto save = [&] {
            structure->saveChanges();
            model->saveChanges();
            widget<Ui::MenuView>()->saveProjectChangesPressed();
            require(!window->isWindowModified() && !DatabaseLayer::Database::hasError()
                        && storedScript(uuid) == xmlOf(model),
                    "coordinator Save commits the exact model XML");
        };

        if (phase == "verify") {
            const auto expected = read(root + "/expected.xml");
            require(storedScript(uuid) == expected
                        && identities() == read(root + "/identities.txt"),
                    "fresh process reopens the exact saved screenplay and document identities");
            require(xmlOf(model) == expected && model->document()->content() == expected,
                    "fresh editor model is the exact saved screenplay");
            require(edit->toPlainText() == QString::fromUtf8(read(root + "/expected.txt")),
                    "fresh editor shows the exact saved text");
            require(sameThreads(freshThreads(model),
                                QJsonDocument::fromJson(read(root + "/expected-threads.json"))
                                    .array()),
                    "fresh comments panel shows the exact saved threads");
        } else {
            if (phase == "prepare" || start == "saved") {
                save();
            }
            if (phase == "prepare") {
                write(root + "/prepared.xml", xmlOf(model));
                write(root + "/prepared-identities.txt", identities());
            } else {
                if (start == "native") {
                    require(xmlOf(model) == read(root + "/prepared.xml")
                                && identities() == read(root + "/prepared-identities.txt"),
                            "fresh process reopens the exact prepared screenplay");
                }
                const auto original = xmlOf(model);
                const auto originalText = edit->toPlainText();
                const auto originalThreads = freshThreads(model);
                require(model->document()->content() == original,
                        "opening the editor leaves the stored screenplay unchanged");
                require(!originalThreads.isEmpty(),
                        "comments panel shows the imported review threads");

                auto cursor = edit->textCursor();
                cursor.movePosition(QTextCursor::End);
                edit->setTextCursorForced(cursor);
                edit->insertPlainText(kTyped);
                QApplication::processEvents();
                // The model records the authored edit after its usual debounce.
                waitFor([&] {
                    return xmlOf(model) != original
                        && model->document()->content() == xmlOf(model);
                });
                const auto edited = xmlOf(model);
                const auto editedText = edit->toPlainText();
                const auto editedThreads = freshThreads(model);
                require(editedText == originalText + kTyped, "editor applies the typed text");
                {
                    // Each paragraph is one line of the model XML.
                    const auto before = lines(original);
                    const auto after = lines(edited);
                    int lastParagraph = before.size() - 1;
                    while (lastParagraph > 0
                           && !before.at(lastParagraph).contains("<v><![CDATA[")) {
                        --lastParagraph;
                    }
                    QVector<int> changed;
                    for (int line = 0; line < before.size() && before.size() == after.size();
                         ++line) {
                        if (before.at(line) != after.at(line)) {
                            changed.append(line);
                            std::printf("changed line %d: %s\n", line,
                                        after.at(line).left(160).constData());
                        }
                    }
                    require(before.size() == after.size()
                                && changed == QVector<int>{ lastParagraph }
                                && after.at(lastParagraph).contains(kTyped.toUtf8()),
                            "the first edit changes only the edited paragraph's stored XML");
                }
                require(sameThreads(withoutTyped(editedThreads), originalThreads),
                        "the first edit keeps every review thread, extending only the typed one");

                edit->undo();
                QApplication::processEvents();
                require(xmlOf(model) == original && model->document()->content() == original,
                        "Undo restores the exact pre-edit model XML");
                require(edit->toPlainText() == originalText, "Undo restores the editor text");
                require(sameThreads(freshThreads(model), originalThreads),
                        "Undo restores every review thread");

                edit->redo();
                QApplication::processEvents();
                require(xmlOf(model) == edited && model->document()->content() == edited,
                        "Redo restores the exact edited model XML");
                require(edit->toPlainText() == editedText, "Redo restores the editor text");
                require(sameThreads(freshThreads(model), editedThreads),
                        "Redo restores every review thread");

                if (finalState == "undone") {
                    edit->undo();
                    QApplication::processEvents();
                    require(xmlOf(model) == original && edit->toPlainText() == originalText
                                && sameThreads(freshThreads(model), originalThreads),
                            "Undo after Redo restores the exact pre-edit screenplay");
                }
                // Asynchronous editor corrections must not change the chosen state either.
                QElapsedTimer settle;
                settle.start();
                while (settle.elapsed() < 500) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                }
                const auto& expected = finalState == "undone" ? original : edited;
                require(xmlOf(model) == expected,
                        "editor corrections keep the exact undone or redone model XML");
                save();
                write(root + "/original.xml", original);
                write(root + "/edited.xml", edited);
                write(root + "/expected.xml", expected);
                write(root + "/expected.txt",
                      (finalState == "undone" ? originalText : editedText).toUtf8());
                write(root + "/expected-threads.json",
                      QJsonDocument(finalState == "undone" ? originalThreads : editedThreads)
                          .toJson());
                write(root + "/identities.txt", identities());
            }
        }
        require(read(source) == originalSource && !QFile::exists(source + ".lock"),
                "the source keeps its exact bytes without a lock");

        // Observe an asynchronous Save question, if any, and answer it once.
        auto closeQuestionObserver = new QTimer(&app);
        closeQuestionObserver->setInterval(10);
        QObject::connect(closeQuestionObserver, &QTimer::timeout, &app, [closeQuestionObserver] {
            if (auto question = dialogWithText("Save changes?")) {
                closeQuestionObserver->stop();
                question->finished({ 2, "Save", Dialog::AcceptButton });
            }
        });
        closeQuestionObserver->start();
        window->close();
    });
    QTimer::singleShot(45000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto status = app.exec();
    require(status == 0 && !QFile::exists(source + ".lock") && !QFile::exists(project + ".lock"),
            "ordinary close releases the native lock and never locks the source");
    require(read(source) == originalSource, "ordinary close keeps the exact source bytes");
    return status;
}
