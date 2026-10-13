#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_pdf_exporter.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/application_manager.h>
#include <management_layer/content/export/export_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <sys/resource.h>
#include <ui/application_view.h>
#include <ui/menu_view.h>
#include <ui/project/project_navigator.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/tree/tree.h>

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMimeData>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QTextCursor>
#include <QTimer>

#include <csignal>
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
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<BusinessLayer::StructureModel*>(proxy->sourceModel());
    std::function<BusinessLayer::StructureModelItem*(BusinessLayer::StructureModelItem*)>
        findScript;
    findScript
        = [&](BusinessLayer::StructureModelItem* _item) -> BusinessLayer::StructureModelItem* {
        if (_item->type() == Domain::DocumentObjectType::ScreenplayText) {
            return _item;
        }
        for (int child = 0; child < _item->childCount(); ++child) {
            if (auto scriptItem = findScript(_item->childAt(child))) {
                return scriptItem;
            }
        }
        return nullptr;
    };
    const auto scriptItem = findScript(structure->itemForIndex({ }));
    require(scriptItem, "live imported story has a screenplay");
    navigator->setCurrentIndex(proxy->mapFromSource(structure->indexForItem(scriptItem)));
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

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto root = qEnvironmentVariable("STARC_SOURCE_EXPORT_ROOT");
    require(!root.isEmpty() && QDir().mkpath(root + "/tmp"), "private temporary directory");
    qputenv("TMPDIR", (root + "/tmp").toUtf8());
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    const auto mode = qEnvironmentVariable("STARC_SOURCE_EXPORT_MODE");
    const auto suffix = qEnvironmentVariable("STARC_SOURCE_EXPORT_SUFFIX");
    const auto source = root + "/source." + suffix;
    require(!root.isEmpty() && !mode.isEmpty(), "disposable root and mode");
    setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, true);
    setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, false);
    setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
    setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    manager->exec(source);
    QTimer::singleShot(0, &app, [&] {
        auto window = widget<Ui::ApplicationView>();
        waitFor([&] { return QFile::exists(source + ".lock"); });
        require(DatabaseLayer::Database::currentFile() != source,
                "source opened through shadow database");
        auto edit = openEditor();
        waitFor([&] { return edit->isVisible(); });
        if (mode == "reopen") {
            const auto markerExpected = qEnvironmentVariableIntValue("STARC_EXPECT_MARKER") == 1;
            require(edit->toPlainText().contains("UNSAVEDMARKER") == markerExpected,
                    "fresh process reopens the expected saved or discarded text");
            require(edit->toPlainText().contains("A quiet room."), "original script survives");
            require(fileBytes(source) == fileBytes(root + "/expected-source"),
                    "fresh import leaves the exact expected source bytes untouched");
            const auto needsQuestion = window->isWindowModified();
            window->close();
            if (needsQuestion) {
                answer("Save changes?", { 1, "Don't save", Dialog::NormalButton });
            }
            return;
        }

        if (mode == "clipboard") {
            const auto temporaryOutput = QDir::temp().absoluteFilePath("clipboard.fountain");
            require(temporaryOutput.startsWith(root + "/tmp/"), "clipboard output is disposable");
            edit->selectAll();
            edit->copy();
            require(QApplication::clipboard()->mimeData()->hasFormat("text/markdown"),
                    "successful selection export supplies Fountain clipboard data");
            const QByteArray stale("A previous, unrelated clipboard selection.\n");
            retain(temporaryOutput, stale);
            require(QFile::setPermissions(temporaryOutput, QFileDevice::ReadOwner),
                    "refuse the next clipboard export");
            edit->copy();
            const auto copied = QApplication::clipboard()->mimeData();
            require(!copied->hasFormat("text/markdown") && copied->text().contains("A quiet room."),
                    "failed clipboard export keeps current plain text and excludes stale output");
            require(fileBytes(temporaryOutput) == stale, "failed clipboard export is atomic");
            require(QFile::setPermissions(temporaryOutput,
                                          QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                    "restore disposable clipboard output permissions");
            edit->copy();
            require(QApplication::clipboard()->mimeData()->hasFormat("text/markdown")
                        && !QApplication::clipboard()
                                ->mimeData()
                                ->data("text/markdown")
                                .contains(stale),
                    "clipboard export recovers with the current selection");
            retain(root + "/expected-source", fileBytes(source));
            const auto needsQuestion = window->isWindowModified();
            window->close();
            if (needsQuestion) {
                answer("Save changes?", { 1, "Don't save", Dialog::NormalButton });
            }
            return;
        }

        // Export failures use the actual convenience route, independent of the
        // application Save coordinator. The kernel rejects real staged writes.
        if (mode == "export") {
            ManagementLayer::ExportManager exporter(nullptr, window);
            auto model = qobject_cast<BusinessLayer::ScreenplayTextModel*>(
                qobject_cast<BusinessLayer::TextDocument*>(edit->document())->model());
            require(model, "loaded screenplay model for export");
            for (const auto extension : { "fountain", "txt", "fdx", "fdxt", "docx", "pdf" }) {
                const auto destination = root + "/output." + extension;
                const QByteArray original("Previous complete output.\r\n");
                const auto exportFile = [&] {
                    BusinessLayer::ScreenplayExportOptions options;
                    options.filePath = destination;
                    if (QString(extension) == "docx") {
                        BusinessLayer::ScreenplayDocxExporter writer;
                        return writer.exportTo(model, options);
                    }
                    if (QString(extension) == "pdf") {
                        BusinessLayer::ScreenplayPdfExporter writer;
                        return writer.exportTo(model, options);
                    }
                    return exporter.exportDocument(model, destination);
                };
                for (const auto byteLimit : { 0, 1 }) {
                    retain(destination, original);
                    struct rlimit originalLimit{ };
                    require(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0, "read file-size limit");
                    auto limited = originalLimit;
                    limited.rlim_cur = byteLimit;
                    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
                    require(setrlimit(RLIMIT_FSIZE, &limited) == 0, "reject staged file writes");
                    const auto exported = exportFile();
                    require(setrlimit(RLIMIT_FSIZE, &originalLimit) == 0,
                            "restore file-size limit");
                    std::signal(SIGXFSZ, handler);
                    require(!exported, "failed write returns false through export layers");
                    require(fileBytes(destination) == original, "existing output stays byte-exact");
                    std::printf("ok: %s output preserved with file-size limit %d\n", extension,
                                byteLimit);
                }
                require(QFile::remove(destination), "remove only disposable test output");
                struct rlimit originalLimit{ };
                require(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0, "read file-size limit");
                auto limited = originalLimit;
                limited.rlim_cur = 0;
                const auto handler = std::signal(SIGXFSZ, SIG_IGN);
                require(setrlimit(RLIMIT_FSIZE, &limited) == 0, "reject new output writes");
                const auto exported = exportFile();
                require(setrlimit(RLIMIT_FSIZE, &originalLimit) == 0, "restore file-size limit");
                std::signal(SIGXFSZ, handler);
                require(!exported && !QFile::exists(destination),
                        "failed export publishes no new output");
                require(exportFile() && fileBytes(destination) != original,
                        "successful retry publishes a complete output");
            }
            retain(root + "/expected-source", fileBytes(source));
            const auto needsQuestion = window->isWindowModified();
            window->close();
            if (needsQuestion) {
                answer("Save changes?", { 1, "Don't save", Dialog::NormalButton });
            }
            return;
        }

        // Finish the initial import, then keep independent source and model oracles.
        save();
        require(!window->isWindowModified(), "baseline source save succeeds");
        const auto before = fileBytes(source);
        retain(root + "/original-source", before);
        auto cursor = edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        edit->setTextCursor(cursor);
        edit->setCurrentParagraphType(BusinessLayer::TextParagraphType::Action);
        edit->insertPlainText(" UNSAVEDMARKER");
        auto document = qobject_cast<BusinessLayer::TextDocument*>(edit->document());
        document->model()->saveChanges();
        require(window->isWindowModified(), "writer edit marks real window dirty");
        const auto edited = document->model()->document()->content();

        const auto permissionFault = mode.startsWith("permission");
        if (permissionFault) {
            require(QFile::setPermissions(source, QFileDevice::ReadOwner), "deny source writes");
        } else {
            qputenv("STARC_SOURCE_EXPORT_TARGET", source.toUtf8());
            qputenv("STARC_SOURCE_EXPORT_FAULT",
                    mode.startsWith("partial") ? "partial" : "publish");
        }
        const auto closeSave = mode.endsWith("close");
        if (closeSave) {
            window->close();
            answer("Save changes?", { 2, "Save", Dialog::AcceptButton });
        } else {
            save();
        }
        waitFor([&] { return dialogWithText("Changes can't be written to the source file"); });
        require(window->isVisible() && window->isWindowModified(),
                "failed source save keeps live editor open and dirty");
        require(!DatabaseLayer::Database::hasError() && script().contains("UNSAVEDMARKER"),
                "shadow commit succeeds independently of source-output failure");
        require(fileBytes(source) == before, "failed source save keeps exact original bytes");
        require(document->model()->document()->content() == edited,
                "live text and annotations survive failed source output");
        require(QFile::exists(source + ".lock"), "failed save retains source lock");

        if (mode.endsWith("discard")) {
            answer("Changes can't be written to the source file",
                   { 0, "OK", Dialog::RejectButton });
            window->close();
            answer("Save changes?", { 0, "Cancel", Dialog::RejectButton });
            require(window->isVisible() && window->isWindowModified(),
                    "Cancel after failure retains the modified story");
            require(document->model()->document()->content() == edited,
                    "Cancel retains exact live text and annotations");
            retain(root + "/expected-source", before);
            window->close();
            answer("Save changes?", { 1, "Don't save", Dialog::NormalButton });
        } else {
            // Retry while the fault is still present must remain a failed save.
            answer("Changes can't be written to the source file",
                   { 1, "Retry", Dialog::AcceptButton });
            waitFor([&] { return dialogWithText("Changes can't be written to the source file"); });
            require(window->isWindowModified() && fileBytes(source) == before,
                    "failed Retry neither clears dirty state nor changes source bytes");
            edit->insertPlainText(" LATERMARKER");
            document->model()->saveChanges();
            if (permissionFault) {
                require(
                    QFile::setPermissions(source, QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                    "restore source write permission");
            } else {
                qunsetenv("STARC_SOURCE_EXPORT_FAULT");
            }
            answer("Changes can't be written to the source file",
                   { 1, "Retry", Dialog::AcceptButton });
            require(!window->isWindowModified() && fileBytes(source).contains("UNSAVEDMARKER")
                        && fileBytes(source).contains("LATERMARKER"),
                    "successful Retry saves both edits and clears dirty state");
            retain(root + "/expected-source", fileBytes(source));
            window->close();
        }
    });
    QTimer::singleShot(30000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto result = app.exec();
    require(result == 0 && !QFile::exists(source + ".lock"),
            "ordinary shutdown exits zero and releases the source lock");
    require(fileBytes(source) == fileBytes(root + "/expected-source"),
            "ordinary close retains exact expected source bytes");
    return result;
}
