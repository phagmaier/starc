#include <app/application.h>
#include <business_layer/document/text/text_document.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/text/text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <data_layer/database.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/application_manager.h>
#include <management_layer/content/export/export_manager.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <ui/application_view.h>
#include <ui/menu_view.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/project/project_navigator.h>
#include <ui/projects/create_project_dialog.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/tree/tree.h>

#include <QAbstractButton>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileOpenEvent>
#include <QLockFile>
#include <QMessageBox>
#include <QScreen>
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

QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable file");
    return file.readAll();
}

void retain(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain exact file oracle");
}

QByteArray script()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT content FROM documents WHERE type = 10104") && query.next(),
            "read persistent screenplay XML");
    return query.value(0).toByteArray();
}

QByteArray identities()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT uuid FROM documents ORDER BY uuid"), "read document identities");
    QByteArray result;
    while (query.next()) {
        result += query.value(0).toString().toUtf8() + '\n';
    }
    return result;
}

Ui::ScreenplayTextEdit* editor()
{
    auto query = DatabaseLayer::Database::query();
    require(query.exec("SELECT uuid FROM documents WHERE type = 10104") && query.next(),
            "imported story contains a screenplay");
    auto navigator = widget<Ui::ProjectNavigator>();
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    auto structure = qobject_cast<BusinessLayer::StructureModel*>(proxy->sourceModel());
    navigator->setCurrentIndex(proxy->mapFromSource(
        structure->indexForItem(structure->itemForUuid(QUuid(query.value(0).toString())))));
    auto result = widget<Ui::ScreenplayTextEdit>();
    require(result, "real screenplay editor opens");
    waitFor([&] { return result->isVisible(); });
    return result;
}

void save()
{
    auto document
        = qobject_cast<BusinessLayer::TextDocument*>(widget<Ui::ScreenplayTextEdit>()->document());
    document->model()->saveChanges();
    widget<Ui::MenuView>()->saveProjectChangesPressed();
    require(!widget<Ui::ApplicationView>()->isWindowModified()
                && !DatabaseLayer::Database::hasError(),
            "ordinary Save commits the screenplay");
}

void dismissFormatQuestion()
{
    // Baseline-only responder: let the old unsafe Keep choice reach the Save reproduction.
    for (auto candidate : QApplication::allWidgets()) {
        if (auto dialog = qobject_cast<Dialog*>(candidate)) {
            for (auto label : dialog->findChildren<AbstractLabel*>()) {
                if (label->text().contains("Some project data cannot be saved")) {
                    dialog->finished({ 1, "Keep", Dialog::RejectButton });
                    return;
                }
            }
        }
    }
}

void screenshot(const QString& _root, const QString& _name)
{
    if (qEnvironmentVariableIsSet("STARC_IMPORT_ONLY_SCREENSHOTS")) {
        auto window = widget<Ui::ApplicationView>();
        window->repaint();
        QGuiApplication::sync();
        require(window->screen()->grabWindow(window->winId()).save(_root + "/" + _name + ".png"),
                "retain native window screenshot");
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
    const auto root = qEnvironmentVariable("STARC_IMPORT_ONLY_ROOT");
    const auto mode = qEnvironmentVariable("STARC_IMPORT_ONLY_MODE");
    require(!root.isEmpty() && !mode.isEmpty(), "disposable root and test mode provided");
    const auto suffix = mode.startsWith("pdf-") ? "pdf"
        : mode.startsWith("md-")                ? "md"
        : mode.startsWith("DOCX-")              ? "DOCX"
                                                : "docx";
    const auto source = root + "/source." + suffix;
    const auto original = read(source);
    const auto base = root + "/base.starc";
    const auto target = root + "/copies/copy.starc";
    const bool reopen = mode == "reopen";
    const bool startup = mode == "docx-startup" || mode == "docx-onboarding";
    const bool cancel = mode == "docx-cancel";
    const bool existing = mode == "docx-existing";
    const bool locked = mode == "docx-locked";
    const bool refusal = cancel || existing || locked;
    const auto selected = existing    ? base
        : mode == "docx-no-extension" ? root + "/copies/copy"
                                      : target;
    bool pickerSeen = false;
    QTimer picker;
    picker.setInterval(10);
    QObject::connect(&picker, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            if (auto overwrite = qobject_cast<QMessageBox*>(candidate);
                overwrite && overwrite->isVisible()) {
                auto yes = overwrite->button(QMessageBox::Yes);
                require(yes, "chooser offers an explicit overwrite confirmation");
                yes->click();
                return;
            }
        }
        if (auto dialog = widget<QFileDialog>(); dialog && dialog->isVisible() && !pickerSeen) {
            pickerSeen = true;
            if (mode == "docx-repeated") {
                const auto current = DatabaseLayer::Database::currentFile();
                QFileOpenEvent competing(root + "/source.md");
                require(QCoreApplication::sendEvent(&app, &competing),
                        "deliver competing file event during import chooser");
                require(DatabaseLayer::Database::currentFile() == current && !QFile::exists(target),
                        "competing file event cannot switch stories or replace the pending import");
            }
            screenshot(root, "import-destination");
            if (cancel) {
                dialog->reject();
            } else {
                if (dialog->focusWidget()) {
                    dialog->focusWidget()->clearFocus();
                }
                dialog->selectFile(selected);
                require(dialog->selectedFiles() == QStringList{ selected },
                        "import chooser selects the intended native destination");
                QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
            }
        }
    });
    setSettingsValue(DataStorageLayer::kApplicationConfiguredKey,
                     mode != "prepare" && mode != "docx-onboarding");
    setSettingsValue(DataStorageLayer::kApplicationOpenLastStoryKey, false);
    setSettingsValue(DataStorageLayer::kApplicationUseAutoSaveKey, false);
    setSettingsValue(DataStorageLayer::kApplicationSaveBackupsKey, false);
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    QLockFile held(target + ".lock");
    if (locked) {
        require(held.tryLock(), "hold new destination's lock without an existing file");
    }
    auto manager = new ManagementLayer::ApplicationManager;
    app.setApplicationManager(manager);
    const auto request = reopen ? QString::fromUtf8(read(root + "/opened-path.txt"))
        : startup               ? source
                                : QString();
    picker.start();
    manager->exec(request);
    QTimer closeQuestion;
    closeQuestion.setInterval(10);
    QObject::connect(&closeQuestion, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            if (auto dialog = qobject_cast<Dialog*>(candidate); dialog && dialog->isVisible()) {
                for (auto label : dialog->findChildren<AbstractLabel*>()) {
                    if (label->text().contains("Save changes?")) {
                        closeQuestion.stop();
                        dialog->finished({ 2, "Save", Dialog::AcceptButton });
                        return;
                    }
                }
            }
        }
    });
    bool onboardingDone = false;
    QTimer steps;
    steps.setInterval(10);
    QObject::connect(&steps, &QTimer::timeout, [&] {
        auto window = widget<Ui::ApplicationView>();
        if (mode == "docx-onboarding" && !onboardingDone) {
            if (!widget<Ui::OnboardingNavigator>()->isVisible()) {
                return;
            }
            require(!pickerSeen && !QFile::exists(target),
                    "onboarding precedes the pending import");
            onboardingDone = true;
            widget<Ui::OnboardingNavigator>()->skipGitHubSyncRequested();
            return;
        }
        // Let the startup chooser unwind and creation complete before editing in a new event.
        if (startup
            && (DatabaseLayer::Database::currentFile() != target
                || settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString()
                    != target)) {
            return;
        }
        steps.stop();
        if (mode == "prepare") {
            waitFor([] { return widget<Ui::OnboardingNavigator>()->isVisible(); });
            widget<Ui::OnboardingNavigator>()->skipGitHubSyncRequested();
            waitFor([&] { return window->view()->isVisible(); });
            window->resize(1200, 740);
            window->close();
            return;
        }
        if (reopen) {
            waitFor([&] { return DatabaseLayer::Database::currentFile() == request; });
            require(script() == read(root + "/expected.xml")
                        && identities() == read(root + "/identities.txt"),
                    "fresh process reopens exact script, emphasis, review and document identities");
            window->close();
            return;
        }
        if (!startup) {
            waitFor([&] { return window->view()->isVisible(); });
            window->resize(1200, 740);
            widget<Ui::MenuView>()->createProjectPressed();
            auto creation = widget<Ui::CreateProjectDialog>();
            require(creation, "create the current disposable story through the real dialog");
            creation->setProjectName("base");
            creation->setProjectFolder(root);
            creation->setProjectType(static_cast<int>(Domain::DocumentObjectType::Screenplay));
            creation->createProjectPressed();
            widget<Ui::MenuView>()->saveProjectChangesPressed();
            // Finish the queued startup decision before testing an event during editing.
            QCoreApplication::processEvents();
            const auto baseBytes = read(base);
            QFileOpenEvent event(source);
            require(QCoreApplication::sendEvent(&app, &event), "deliver actual QFileOpenEvent");
            if (refusal) {
                require(
                    pickerSeen && DatabaseLayer::Database::currentFile() == base
                        && read(base) == baseBytes && read(source) == original
                        && QFile::exists(base + ".lock") && !QFile::exists(target),
                    "canceled or refused import keeps current story, lock and exact source bytes");
                retain(root + "/opened-path.txt", base.toUtf8());
                retain(root + "/expected.xml", script());
                retain(root + "/identities.txt", identities());
                window->close();
                return;
            }
        }
        if (startup) {
            waitFor([&] { return QFile::exists(target + ".lock"); });
        }
        dismissFormatQuestion();
        widget<Ui::MenuView>()->saveProjectChangesPressed();
        retain(root + "/source-after-first-save", read(source));
        require(read(source) == original, "opening and Save leave the complete source byte-exact");
        require(pickerSeen && DatabaseLayer::Database::currentFile() == target
                    && settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString()
                        == target
                    && QFile::exists(target + ".lock") && !QFile::exists(source + ".lock"),
                "import-only source opens as a native copy with native path and lock");
        auto edit = editor();
        require(edit->toPlainText().contains("Import-only source survives."),
                "native story contains the imported screenplay text");
        edit->moveCursor(QTextCursor::End);
        edit->insertPlainText(" Zoë SOURCE_EDIT_MARKER");
        edit->undo();
        require(!edit->toPlainText().contains("SOURCE_EDIT_MARKER"), "imported copy supports Undo");
        edit->redo();
        require(edit->toPlainText().contains("SOURCE_EDIT_MARKER"),
                "Redo restores the copy's edits");
        auto cursor = edit->document()->find("Zoë");
        require(!cursor.isNull(), "imported copy accepts Unicode editing");
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        cursor.mergeCharFormat(bold);
        BusinessLayer::TextModelTextItem::ReviewMark mark;
        mark.from = 0;
        mark.length = 3;
        mark.backgroundColor = Qt::yellow;
        BusinessLayer::TextModelTextItem::ReviewComment comment;
        comment.author = "Reviewer";
        comment.text = "Review annotation on imported copy";
        mark.comments.append(comment);
        cursor.mergeCharFormat(mark.charFormat());
        save();
        const auto expected = script();
        require(expected.contains("SOURCE_EDIT_MARKER") && expected.contains("bold=")
                    && expected.contains("Review annotation on imported copy"),
                "native Save preserves the edits, emphasis and review annotation");
        retain(root + "/opened-path.txt", target.toUtf8());
        retain(root + "/expected.xml", expected);
        retain(root + "/identities.txt", identities());
        ManagementLayer::ExportManager exporter(nullptr, window);
        auto model = qobject_cast<BusinessLayer::TextDocument*>(edit->document())->model();
        exporter.exportDocument(model, source);
        require(read(source) == original,
                "source-save export route also refuses rewriting the source");
        for (const auto& extension : { "docx", "DOCX", "pdf", "md", "odt", "rtf", "celtx", "trelby",
                                       "kitsp", "unknown", "" }) {
            const auto destination = root + "/guarded"
                + (QString(extension).isEmpty() ? "" : "." + QString(extension));
            retain(destination, original);
            exporter.exportDocument(model, destination);
            require(read(destination) == original,
                    "unsupported source-save destinations retain exact bytes");
        }
        for (const auto& extension : { "fountain", "TXT", "fdx", "FDXT" }) {
            const auto destination = root + "/supported." + extension;
            exporter.exportDocument(model, destination);
            const auto bytes = read(destination);
            const bool isFdx = QString(extension).startsWith("fd", Qt::CaseInsensitive);
            require(bytes.contains(isFdx ? "SOURCE_EDIT_MARKER" : "SOURCE\\_EDIT\\_MARKER")
                        && bytes.startsWith("<?xml") == isFdx,
                    "supported source formats still select Fountain or FDX correctly");
        }
        screenshot(root, "saved-native-copy");
        // The first supported export initializes the existing empty title-page document.
        // Answer its normal save question; reopen still requires unchanged screenplay XML.
        closeQuestion.start();
        window->close();
    });
    steps.start();
    QTimer::singleShot(45000, &app, [] { require(false, "ordinary shutdown watchdog"); });
    const auto result = app.exec();
    require(result == 0 && read(source) == original && !QFile::exists(base + ".lock")
                && (locked || !QFile::exists(target + ".lock")),
            "ordinary exit releases application locks and leaves the source byte-exact");
    std::puts("PASS: import-only source regression");
    return result;
}
