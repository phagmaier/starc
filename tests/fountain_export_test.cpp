#include "explicit_screenplay_export.h"

#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
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
#include <ui/project/project_navigator.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/splitter/splitter.h>
#include <ui/widgets/tree/tree.h>

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QPointer>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QTextCursor>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {
using namespace BusinessLayer;

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
            "retain source or semantic oracle");
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
    auto script = findScript(structure->itemForIndex({ }));
    require(script, "imported story contains a screenplay");
    navigator->setCurrentIndex(proxy->mapFromSource(structure->indexForItem(script)));
    auto edit = widget<Ui::ScreenplayTextEdit>();
    require(edit, "real screenplay editor opens");
    waitFor([&] { return edit->isVisible(); });
    return edit;
}

void collect(TextModelItem* _item, QJsonArray& _rows)
{
    if (_item->type() == TextModelItemType::Text) {
        auto item = static_cast<TextModelTextItem*>(_item);
        if (item->isCorrection()) {
            return;
        }
        const auto text = item->text();
        QString styles(text.size(), '0');
        for (const auto& format : item->formats()) {
            require(format.from >= 0 && format.end() <= text.size(), "valid style coordinates");
            const int bits = format.isBold + 2 * format.isItalic + 4 * format.isUnderline;
            for (int position = format.from; position < format.end(); ++position) {
                styles[position] = QChar('0' + (styles[position].digitValue() | bits));
            }
        }
        QJsonArray coverage;
        for (int position = 0; position < text.size(); ++position) {
            coverage.append(QJsonArray());
        }
        for (const auto& mark : item->reviewMarks()) {
            require(mark.from >= 0 && mark.end() <= text.size(), "valid review coordinates");
            for (int position = mark.from; position < mark.end(); ++position) {
                auto messages = coverage[position].toArray();
                for (const auto& comment : mark.comments) {
                    messages.append(comment.text);
                }
                coverage[position] = messages;
            }
        }
        _rows.append(QJsonArray{ toString(item->paragraphType()), text, styles, coverage });
    }
    for (int child = 0; child < _item->childCount(); ++child) {
        collect(_item->childAt(child), _rows);
    }
}

QJsonArray snapshot(TextModel* _model)
{
    QJsonArray rows;
    collect(_model->itemForIndex({ }), rows);
    return rows;
}

void checkInlineClipboard(Ui::ScreenplayTextEdit* _edit, const QString& _root)
{
    const auto before = _edit->document()->find("Before.");
    const auto after = _edit->document()->find("After.");
    const auto note = _edit->document()->find("private");
    const auto unicode = _edit->document()->find(QString::fromUtf8("Ω🙂"));
    const auto lastNote = _edit->document()->find("last ");
    const auto finalAction = _edit->document()->find("Final action. SAVEMARKER");
    require(!before.isNull() && !after.isNull() && !note.isNull() && !unicode.isNull()
                && !lastNote.isNull() && !finalAction.isNull(),
            "find clipboard ranges before, inside and after excluded inline notes");

    const auto copyRange = [&](const char* name, int from, int to, const QByteArray& expected) {
        for (const bool reverse : { false, true }) {
            QTextCursor cursor(_edit->document());
            cursor.setPosition(reverse ? to : from);
            cursor.setPosition(reverse ? from : to, QTextCursor::KeepAnchor);
            _edit->setTextCursorForced(cursor);
            _edit->copy();
            const auto copied = QApplication::clipboard()->mimeData()->data("text/markdown");
            write(_root + "/clipboard-" + name + (reverse ? "-reverse" : "-forward") + ".fountain",
                  copied);
            require(copied == expected,
                    "both selection directions retain exact selected text and review anchors");
        }
    };

    _edit->selectAll();
    copyRange("all", _edit->textCursor().selectionStart(), _edit->textCursor().selectionEnd(),
              "INT. STUDIO - DAY\n\nBefore.[[before]]\n\nAfter.[[after]]\n\n"
              "Final action. SAVEMARKER\n");
    copyRange("before", before.selectionStart(), before.selectionEnd(), "Before.[[before]]\n");
    copyRange("after", after.selectionStart(), after.selectionEnd(), "After.[[after]]\n");
    copyRange("partial-after", after.selectionStart() + 2, after.selectionEnd(), "ter.[[after]]\n");
    copyRange("across-notes", before.selectionStart() + 2, after.selectionStart() + 3,
              "fore.[[before]]\n\nAft[[after]]\n");
    copyRange("end-in-note", before.selectionStart(), unicode.selectionEnd(),
              "Before.[[before]]\n");
    copyRange("start-in-note", unicode.selectionStart(), after.selectionStart() + 3,
              "Aft[[after]]\n");
    copyRange("only-note", note.selectionStart(), note.selectionEnd(), "\n");
    copyRange("only-notes", note.selectionStart(), unicode.selectionEnd(), "\n");
    copyRange("last-note-to-action", lastNote.selectionStart() + 2, finalAction.selectionEnd(),
              "Final action. SAVEMARKER\n");
    copyRange("final-action", finalAction.selectionStart() + 6, finalAction.selectionEnd(),
              "action. SAVEMARKER\n");
}

void checkLiteralPaste(Ui::ScreenplayTextEdit* _edit, TextModel* _model, const QString& _root)
{
    const auto original = snapshot(_model);
    _model->saveChanges();
    const auto originalXml = _model->document()->content();
    const auto pastePlain = [&](const QString& text, bool replace) {
        auto cursor = _edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        if (replace) {
            cursor.setPosition(cursor.position() - 5, QTextCursor::KeepAnchor);
        }
        _edit->setTextCursorForced(cursor);
        QApplication::clipboard()->setText(text);
        _edit->paste();
        QApplication::processEvents();
        const auto pasted = snapshot(_model);
        write(_root + (replace ? "/paste-replacement.json" : "/paste-plain.json"),
              QJsonDocument(pasted).toJson());
        auto expected = original;
        auto row = expected.last().toArray();
        const auto prefix = row[1].toString().left(row[1].toString().size() - (replace ? 5 : 0));
        expected.removeLast();
        const auto lines = (prefix + text).split('\n');
        for (const auto& line : lines) {
            QJsonArray coverage;
            for (int i = 0; i < line.size(); ++i) {
                coverage.append(QJsonArray());
            }
            expected.append(QJsonArray{ "action", line, QString(line.size(), '0'), coverage });
        }
        require(pasted == expected, "plain clipboard preserves exact characters, lines, types and styles");
        _model->saveChanges();
        const auto pastedXml = _model->document()->content();
        _edit->undo();
        QApplication::processEvents();
        require(snapshot(_model) == original && _model->document()->content() == originalXml,
                "one Undo restores exact XML after plain paste/selection replacement");
        _edit->redo();
        QApplication::processEvents();
        require(snapshot(_model) == pasted && _model->document()->content() == pastedXml,
                "Redo restores exact XML after plain paste/selection replacement");
        _edit->undo();
        QApplication::processEvents();
        require(snapshot(_model) == original, "paste check restores original screenplay");
    };
    pastePlain(" PASTE 2 * 3; *literal* _name_ [[note]] /* text */ C:\\temp\\\n"
               "#1 is literal.\nINT. stays literal.", false);
    pastePlain("*replacement* _literal_ C:\\temp", true);
    auto cursor = _edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    _edit->setTextCursorForced(cursor);
    auto markdown = new QMimeData;
    markdown->setText("WRONG plain fallback");
    markdown->setData("text/markdown", "Markdown **bold** _word_.[[file_name 2 * 3 * 4]]\n");
    QApplication::clipboard()->setMimeData(markdown);
    _edit->paste();
    QApplication::processEvents();
    const auto pasted = snapshot(_model);
    write(_root + "/paste-markdown.json", QJsonDocument(pasted).toJson());
    const auto row = pasted.last().toArray();
    require(row[1].toString().endsWith("Markdown bold word.")
                && !row[1].toString().contains("WRONG"), "Markdown MIME retains Fountain decoding priority");
    const auto text = row[1].toString();
    const auto bold = text.indexOf("bold");
    const auto word = text.indexOf("word");
    require(row[2].toString().mid(bold, 4) == "1111"
                && row[2].toString().mid(word, 4) == "4444"
                && row[3].toArray().last().toArray() == QJsonArray{ "file_name 2 * 3 * 4" },
            "Markdown paste preserves styles and literal review comment markers");
    _edit->undo();
    QApplication::processEvents();
    require(snapshot(_model) == original, "Markdown paste Undo restores original screenplay");
    cursor = _edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    _edit->setTextCursorForced(cursor);
    const QString retained = " PASTE 2 * 3; *literal* _name_ [[note]] C:\\temp";
    QApplication::clipboard()->setText(retained);
    _edit->paste();
    QApplication::processEvents();
    const auto retainedRows = snapshot(_model);
    require(retainedRows.last().toArray()[1].toString()
                == original.last().toArray()[1].toString() + retained,
            "retain independently checked literal paste for Save/export/reopen");
    _model->saveChanges();
    write(_root + "/paste-retained.xml", _model->document()->content());
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
    require(argc == 3, "private case directory and mode");
    const QString root = QString::fromLocal8Bit(argv[1]);
    require(QDir().mkpath(root + "/tmp"), "private clipboard temporary directory");
    qputenv("TMPDIR", (root + "/tmp").toUtf8());
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    const QString mode = QString::fromLocal8Bit(argv[2]);
    const auto source = root + "/source.fountain";
    const auto input = mode == "native-reopen" ? root + "/save.starc"
        : mode == "reopen" ? root + "/export.fountain" : source;
    const auto originalSource = read(source);
    const auto project = mode == "native-reopen" ? input : root + '/' + mode + ".starc";
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
    manager->exec(input);
    QTimer::singleShot(0, &app, [&] {
        waitFor([&] {
            return DatabaseLayer::Database::currentFile() == project
                && settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString()
                == project;
        });
        picker.stop();
        require((mode == "native-reopen" && !pickerSeen && QFile::exists(input + ".lock"))
                    || (pickerSeen && !QFile::exists(input + ".lock")),
                "open route uses the expected chooser and lock state");
        auto window = widget<Ui::ApplicationView>();
        window->resize(1320, 900);
        auto splitter = window->findChild<Splitter*>(QString(), Qt::FindDirectChildrenOnly);
        require(splitter, "application window owns its split view");
        splitter->setSizes({ 250, 750 });
        auto edit = openEditor();
        auto document = qobject_cast<TextDocument*>(edit->document());
        require(document, "editor binds the imported text model");
        auto model = document->model();
        if (mode == "native-reopen") {
            require(model->document()->content() == read(root + "/saved-native.xml")
                        && model->document()->uuid().toString().toUtf8()
                            == read(root + "/saved-native-uuid"),
                    "fresh native process retains exact saved XML, text, annotations and UUID");
        }
        const auto before = read(input);
        if (mode == "save" || mode == "cold-undo") {
            // Serialize the editor's initial format projection before recording a
            // separate authored edit, as in the other editor/persistence fixtures.
            const auto imported = snapshot(model);
            model->saveChanges();
            widget<Ui::MenuView>()->saveProjectChangesPressed();
            require(!window->isWindowModified() && snapshot(model) == imported,
                    "initial native Save preserves imported text, styles and review coverage");
            if (root.endsWith("/handwritten-markers")) {
                checkLiteralPaste(edit, model, root);
            }
            const auto original = snapshot(model);
            const auto originalXml = model->document()->content();
            auto cursor = edit->textCursor();
            cursor.movePosition(QTextCursor::End);
            edit->setTextCursorForced(cursor);
            // The literal fixture also has text typed that Fountain would read as syntax.
            edit->insertPlainText(root.endsWith("/literal-syntax") ? " SAVEMARKER [[typed]] D:\\x"
                                                                   : " SAVEMARKER");
            QApplication::processEvents();
            const auto edited = snapshot(model);
            require(edited != original && edit->toPlainText().contains("SAVEMARKER"),
                    "editor applies a real pending editor edit");
            if (mode == "cold-undo") {
                // The first edit after import is one history step (archive/audit-2 issue 15):
                // Undo and Redo restore the exact stored XML before and after it.
                model->saveChanges();
                const auto editedXml = model->document()->content();
                require(editedXml != originalXml, "the first edit is recorded");
                edit->undo();
                QApplication::processEvents();
                require(snapshot(model) == original && model->document()->content() == originalXml,
                        "Undo restores the exact pre-edit XML, text, styles and reviews");
                edit->redo();
                QApplication::processEvents();
                require(snapshot(model) == edited && model->document()->content() == editedXml,
                        "Redo restores the exact edited XML, text, styles and reviews");
            }
            write(root + "/expected.json", QJsonDocument(edited).toJson());
        } else {
            require(mode == "reopen" || mode == "native-reopen", "fresh reopen mode");
            require(snapshot(model)
                        == QJsonDocument::fromJson(read(root + "/expected.json")).array(),
                    "fresh application import retains every character, style and review coverage");
            require(mode == "native-reopen" || before == read(root + "/expected-export"),
                    "fresh import leaves source bytes intact");
        }
        model->saveChanges();
        widget<Ui::MenuView>()->saveProjectChangesPressed();
        require(!window->isWindowModified(), "coordinator native Save completes");
        if (mode == "save"
            && (root.endsWith("/handwritten-markers") || root.endsWith("/inline-note-review"))) {
            write(root + "/saved-native.xml", model->document()->content());
            write(root + "/saved-native-uuid", model->document()->uuid().toString().toUtf8());
        }
        require(read(source) == originalSource && !QFile::exists(source + ".lock"),
                "native Save leaves source bytes and source lock state untouched");
        ManagementLayer::ExportManager exporter(nullptr, window);
        require(
            explicitScreenplayExport(exporter, model, root + "/export.fountain", "Fountain", false),
            "explicit Fountain export through the real manager");
        const auto saved = read(root + "/export.fountain");
        require(saved.contains("SAVEMARKER"), "typed text reaches explicit Fountain output");
        if (mode == "save" || mode == "cold-undo") {
            require(saved != before, "explicit export contains the edited screenplay");
            write(root + "/expected-export", saved);
        } else {
            require(saved == read(root + "/expected-export"),
                    "fresh application explicit export is byte-idempotent");
        }
        require(
            explicitScreenplayExport(exporter, model, root + "/export.fountain", "Fountain", false)
                && read(root + "/export.fountain") == saved,
            "repeated explicit export is byte-idempotent");
        const auto savedSnapshot = snapshot(model);
        const auto savedXml = model->document()->content();
        const bool inlineFixture = root.endsWith("/inline-note-review");
        const bool markerFixture = root.endsWith("/handwritten-markers");
        if (markerFixture) {
            const auto selected = edit->document()->find("It costs 2 * 4.");
            require(!selected.isNull(), "find reviewed Action before inline notes");
            edit->setTextCursorForced(selected);
        } else {
            edit->selectAll();
        }
        edit->copy();
        const auto copied = QApplication::clipboard()->mimeData()->data("text/markdown");
        write(root + "/clipboard.fountain", copied);
        // Clipboard export uses the ordinary default that excludes inline notes. The
        // explicit Export above enables them; compare each route with its own contract.
        const auto expectedCopy = markerFixture ? QByteArray("It costs 2 \\* 4.\n")
            : !inlineFixture
            ? saved
            : QByteArray("INT. STUDIO - DAY\n\nBefore.[[before]]\n\nAfter.[[after]]\n\n"
                         "Final action. SAVEMARKER\n");
        require(copied == expectedCopy,
                "clipboard selection retains exact included Fountain syntax");
        if (inlineFixture) {
            checkInlineClipboard(edit, root);
        }
        if (root.endsWith("/review-offsets")) {
            const auto selected = edit->document()->find("A *literal* clue.");
            require(!selected.isNull(), "find the exact reviewed literal-star paragraph");
            edit->setTextCursorForced(selected);
            edit->copy();
            require(QApplication::clipboard()->mimeData()->data("text/markdown")
                        == QByteArray("A \\*literal\\* clue.[[Check clue]]\n"),
                    "partial clipboard export retains exact literal text and review anchor");
        }
        model->saveChanges();
        require(snapshot(model) == savedSnapshot && model->document()->content() == savedXml
                    && read(root + "/export.fountain") == saved && read(source) == originalSource,
                "clipboard export preserves the model, source and explicit output");
        auto cursor = edit->textCursor();
        cursor.clearSelection();
        edit->setTextCursorForced(cursor);
        if (qEnvironmentVariableIsSet("STARC_FOUNTAIN_EXPORT_SCREENSHOTS")) {
            window->repaint();
            QGuiApplication::sync();
            require(window->screen()->grabWindow(window->winId()).save(root + '/' + mode + ".png"),
                    "retain native editor screenshot");
        }
        // Native painting can queue model normalization before the close guard.
        // Observe the actual asynchronous Save question and answer it once.
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
    QTimer::singleShot(30000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto status = app.exec();
    require(status == 0 && !QFile::exists(source + ".lock") && !QFile::exists(project + ".lock"),
            "ordinary close releases the native lock and never locks the source");
    require(read(source) == originalSource
                && read(root + "/export.fountain") == read(root + "/expected-export"),
            "ordinary close retains original source and exact explicit export");
    return status;
}
