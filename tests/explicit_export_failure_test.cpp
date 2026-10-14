#include "explicit_screenplay_export.h"
#include "isolated_github_credentials.h"

#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_fdx_exporter.h>
#include <business_layer/export/screenplay/screenplay_fountain_exporter.h>
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
#include <QFileDialog>
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
    const auto root = qEnvironmentVariable("STARC_EXPLICIT_EXPORT_ROOT");
    require(!root.isEmpty() && QDir().mkpath(root + "/tmp"), "private temporary directory");
    qputenv("TMPDIR", (root + "/tmp").toUtf8());
    Application app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    app.setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);
    const auto mode = qEnvironmentVariable("STARC_EXPLICIT_EXPORT_MODE");
    const auto source = root + "/source." + qEnvironmentVariable("STARC_EXPLICIT_EXPORT_SUFFIX");
    const auto originalSource = fileBytes(source);
    const auto project = root + "/import.starc";
    setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, true);
    setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, false);
    setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
    setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    bool picked = false;
    QTimer picker;
    picker.setInterval(10);
    QObject::connect(&picker, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            if (auto chooser = qobject_cast<QFileDialog*>(candidate);
                chooser && chooser->isVisible() && !picked) {
                picked = true;
                chooser->selectFile(project);
                QMetaObject::invokeMethod(chooser, "accept", Qt::QueuedConnection);
            }
        }
    });
    picker.start();
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    manager->exec(source);
    QTimer::singleShot(0, &app, [&] {
        waitFor([&] {
            return DatabaseLayer::Database::currentFile() == project
                && settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString()
                == project;
        });
        picker.stop();
        auto window = widget<Ui::ApplicationView>();
        require(picked && !QFile::exists(source + ".lock"), "import opens only a native project");
        auto edit = openEditor();
        waitFor([&] { return edit->isVisible(); });
        save();
        edit->moveCursor(QTextCursor::End);
        edit->insertPlainText(" EXPORTMARKER");
        auto model = qobject_cast<BusinessLayer::ScreenplayTextModel*>(
            qobject_cast<BusinessLayer::TextDocument*>(edit->document())->model());
        model->saveChanges();
        const auto edited = model->document()->content();
        require(window->isWindowModified(), "real editor has unsaved authored text");
        if (mode == "clipboard") {
            const auto temporaryOutput = QDir::temp().absoluteFilePath("clipboard.fountain");
            edit->selectAll();
            edit->copy();
            require(QApplication::clipboard()->mimeData()->hasFormat("text/markdown"),
                    "successful clipboard export supplies Fountain data");
            const QByteArray stale("A previous, unrelated clipboard selection.\n");
            retain(temporaryOutput, stale);
            require(QFile::setPermissions(temporaryOutput, QFileDevice::ReadOwner),
                    "refuse the next clipboard export");
            edit->copy();
            const auto copied = QApplication::clipboard()->mimeData();
            require(!copied->hasFormat("text/markdown") && copied->text().contains("EXPORTMARKER"),
                    "failed clipboard export keeps current plain text and excludes stale output");
            require(fileBytes(temporaryOutput) == stale, "failed clipboard export is atomic");
            require(QFile::setPermissions(temporaryOutput,
                                          QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                    "restore disposable clipboard permissions");
            edit->copy();
            require(QApplication::clipboard()->mimeData()->hasFormat("text/markdown")
                        && !QApplication::clipboard()
                                ->mimeData()
                                ->data("text/markdown")
                                .contains(stale),
                    "clipboard export recovers with the current selection");
            auto cursor = edit->textCursor();
            cursor.clearSelection();
            cursor.movePosition(QTextCursor::End);
            edit->setTextCursorForced(cursor);
            edit->paste();
            require(edit->toPlainText().count("EXPORTMARKER") == 2,
                    "real editor pastes the copied screenplay text");
        } else {
            require(mode.startsWith("export-"), "explicit export failure case");
            ManagementLayer::ExportManager exporter(nullptr, window);
            const auto extension = mode.section('-', 1).toUtf8();
            {
                const auto destination = root + "/output." + QString::fromUtf8(extension);
                const QByteArray original("Previous complete output.\r\n");
                const QString format = QString(extension).startsWith("fd") ? "FDX"
                    : QString(extension) == "docx"                         ? "DOCX"
                    : QString(extension) == "pdf"                          ? "PDF"
                                                                           : "Fountain";
                // Independent low-level controls exercise actual kernel write failures.
                const auto exportFile = [&] {
                    BusinessLayer::ScreenplayExportOptions options;
                    options.filePath = destination;
                    options.includeTitlePage = false;
                    if (format == "DOCX")
                        return BusinessLayer::ScreenplayDocxExporter().exportTo(model, options);
                    if (format == "PDF")
                        return BusinessLayer::ScreenplayPdfExporter().exportTo(model, options);
                    if (format == "FDX")
                        return BusinessLayer::ScreenplayFdxExporter().exportTo(model, options);
                    return BusinessLayer::ScreenplayFountainExporter().exportTo(model, options);
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
                    require(!exported && fileBytes(destination) == original,
                            "failed explicit exporter keeps existing output byte-exact");
                }
                for (const auto fault : { "permission", "partial", "publish" }) {
                    retain(destination, original);
                    if (QString(fault) == "permission") {
                        require(QFile::setPermissions(destination, QFileDevice::ReadOwner),
                                "deny output writes");
                    } else {
                        qputenv("STARC_EXPLICIT_EXPORT_TARGET", destination.toUtf8());
                        qputenv("STARC_EXPLICIT_EXPORT_FAULT", fault);
                    }
                    require(!explicitScreenplayExport(exporter, model, destination, format, false),
                            "explicit manager export reports failure");
                    qunsetenv("STARC_EXPLICIT_EXPORT_FAULT");
                    require(fileBytes(destination) == original && window->isWindowModified()
                                && model->document()->content() == edited,
                            "failed explicit export keeps output, live text and unsaved state");
                    require(QFile::setPermissions(destination,
                                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner),
                            "restore output permissions");
                }
                require(QFile::remove(destination), "remove only disposable output");
                qputenv("STARC_EXPLICIT_EXPORT_TARGET", destination.toUtf8());
                qputenv("STARC_EXPLICIT_EXPORT_FAULT", "partial");
                require(!explicitScreenplayExport(exporter, model, destination, format, false)
                            && !QFile::exists(destination),
                        "failed explicit export publishes no new output");
                qunsetenv("STARC_EXPLICIT_EXPORT_FAULT");
                require(explicitScreenplayExport(exporter, model, destination, format, false)
                            && fileBytes(destination) != original,
                        "successful explicit retry publishes complete output");
                require(fileBytes(source) == originalSource && !QFile::exists(source + ".lock"),
                        "explicit exports leave imported source untouched and unlocked");
            }
        }
        save();
        require(script().contains("EXPORTMARKER") && !window->isWindowModified(),
                "Save commits edited text only in the native project");
        window->close();
    });
    QTimer::singleShot(45000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto result = app.exec();
    require(result == 0 && !QFile::exists(project + ".lock") && !QFile::exists(source + ".lock")
                && fileBytes(source) == originalSource,
            "ordinary shutdown releases native lock and retains complete source bytes");
    std::puts("PASS: explicit export and clipboard failures");
    return result;
}
