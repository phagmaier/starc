#include <business_layer/document/text/text_document.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/content/project/project_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <management_layer/plugins_builder.h>
#include <ui/project/project_navigator.h>
#include <ui/widgets/tree/tree.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QTextBlock>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace BusinessLayer;
using DatabaseLayer::Database;
using DataStorageLayer::StorageFacade;

namespace {
void require(bool _ok, const char* _label)
{
    if (!_ok) {
        std::fprintf(stderr, "FAIL: %s\n", _label);
        std::exit(1);
    }
}

QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read retained oracle");
    return file.readAll();
}

void write(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain byte oracle");
}

struct Fixture {
    QWidget host;
    ManagementLayer::PluginsBuilder plugins;
    ManagementLayer::ProjectManager manager{ nullptr, &host, plugins };
    ProjectsModelProjectItem project;
    StructureModel* structure = nullptr;
    ScreenplayTextModel* model = nullptr;
    std::vector<std::unique_ptr<Ui::ScreenplayTextEdit>> editors;

    explicit Fixture(const QString& _path)
    {
        project.setProjectType(ProjectType::Local);
        project.setPath(_path);
        project.setName("Disposable Track Changes regression");
        QObject::connect(
            &manager, &ManagementLayer::ProjectManager::structureModelChanged,
            [&](AbstractModel* _model) { structure = qobject_cast<StructureModel*>(_model); });
        QObject::connect(
            &manager, &ManagementLayer::ProjectManager::currentModelChanged,
            [&](AbstractModel* _model) { model = qobject_cast<ScreenplayTextModel*>(_model); });
        Database::setCurrentFile(_path);
        manager.loadCurrentProject(&project);
    }

    void save()
    {
        if (model) {
            model->saveChanges();
        }
        structure->saveChanges();
        manager.saveChanges();
        Database::commit();
        require(!Database::hasError(), "save through the project coordinator");
    }

    void close()
    {
        editors.clear();
        manager.closeCurrentProject(project.path());
        StorageFacade::documentStorage()->clear();
        Database::closeCurrentFile();
        model = nullptr;
    }

    Ui::ScreenplayTextEdit* select(const QUuid& _uuid)
    {
        auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
        auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
        navigator->setCurrentIndex(
            proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(_uuid))));
        QApplication::processEvents();
        for (auto widget : QApplication::allWidgets()) {
            auto edit = qobject_cast<Ui::ScreenplayTextEdit*>(widget);
            auto document = edit ? qobject_cast<TextDocument*>(edit->document()) : nullptr;
            if (document && document->model() && document->model()->document()->uuid() == _uuid) {
                require(model && model->document()->uuid() == _uuid, "select matching model");
                edit->setCorrectionOptions(false, false);
                edit->resize(900, 700);
                edit->show();
                QApplication::processEvents();
                return edit;
            }
        }
        require(false, "open the real screenplay editor");
        return nullptr;
    }
};

struct Case {
    QString name;
    QStringList paragraphs;
    // Absolute source ranges, independent of selection direction.
    QVector<QPair<int, int>> additions;
    int from;
    int to;
    bool onlyAdded = false;
};

bool marked(const QTextCharFormat& _format, int _property)
{
    return _format.property(_property) == QStringList{ "true" };
}

QJsonObject checkCase(Fixture& _fixture, const QString& _root, const Case& _case, bool _reverse,
                      int _key)
{
    const auto name = _case.name + (_reverse ? "-reverse-" : "-forward-")
        + (_key == Qt::Key_Delete          ? "delete"
               : _key == Qt::Key_Backspace ? "backspace"
                                           : "typing");
    std::printf("case: %s\n", qPrintable(name));
    const auto story = _fixture.structure->itemForIndex(
        _fixture.structure->addDocument(Domain::DocumentObjectType::Screenplay, name));
    QUuid uuid;
    for (int i = 0; i < story->childCount(); ++i) {
        if (story->childAt(i)->type() == Domain::DocumentObjectType::ScreenplayText) {
            uuid = story->childAt(i)->uuid();
        }
    }
    _fixture.select(uuid);
    auto model = _fixture.model;
    while (model->rowCount() > 0) {
        model->removeItem(model->itemForIndex(model->index(0, 0)));
    }
    for (const auto& text : _case.paragraphs) {
        auto item = new ScreenplayTextModelTextItem(model);
        item->setParagraphType(TextParagraphType::Action);
        item->setText(text);
        model->appendItem(item);
    }
    // Bind an editable instance of the production editor to the coordinator's loaded model.
    // The project view's toolbar otherwise controls its own editor's review/edit mode.
    // Handler facades cache each editor's address. Keep fixture editors alive until
    // project close so a later case cannot reuse a destroyed editor's cached handlers.
    _fixture.editors.push_back(std::make_unique<Ui::ScreenplayTextEdit>());
    auto edit = _fixture.editors.back().get();
    edit->setCorrectionOptions(false, false);
    edit->resize(900, 700);
    edit->initWithModel(model);
    edit->show();
    QApplication::processEvents();
    const auto original = _case.paragraphs.join('\n');
    require(edit->toPlainText() == original, "authored paragraph fixture is exact");
    for (const auto& range : _case.additions) {
        auto cursor = edit->textCursor();
        cursor.setPosition(range.first);
        cursor.setPosition(range.second, QTextCursor::KeepAnchor);
        edit->setTextCursorForced(cursor);
        edit->addReviewMark(Qt::black, Qt::green, "Authored addition", true, true, false);
    }
    // An unrelated emphasis span must survive deletion marks, Undo/Redo and reopening.
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    auto cursor = edit->textCursor();
    cursor.setPosition(0);
    cursor.setPosition(1, QTextCursor::KeepAnchor);
    cursor.mergeCharFormat(bold);
    QApplication::processEvents();
    _fixture.save();
    const auto before = model->document()->content();
    write(_root + '/' + name + "-before.xml", before);

    edit->setAutoReviewMode(Qt::black, Qt::green, false, true);
    edit->setAutoReviewModeEnabled(true);
    cursor.setPosition(_reverse ? _case.to : _case.from);
    cursor.setPosition(_reverse ? _case.from : _case.to, QTextCursor::KeepAnchor);
    edit->setTextCursorForced(cursor);
    require(!edit->isReadOnly() && edit->isEnabled() && edit->textCursor().hasSelection(),
            "fixture editor is editable and selection is active");
    const bool typing = _key == Qt::Key_X;
    QKeyEvent press(QEvent::KeyPress, _key, Qt::NoModifier, typing ? "X" : "");
    QKeyEvent release(QEvent::KeyRelease, _key, Qt::NoModifier, typing ? "X" : "");
    QApplication::sendEvent(edit, &press);
    QApplication::sendEvent(edit, &release);
    QApplication::processEvents();
    QString expected = original;
    if (_case.onlyAdded) {
        expected.remove(_case.from, _case.to - _case.from);
    }
    if (typing) {
        expected.insert(_case.onlyAdded ? _case.from : _case.to, "X");
    }
    model->saveChanges();
    const auto after = model->document()->content();
    write(_root + '/' + name + "-after.xml", after);
    require(edit->toPlainText() == expected,
            "original text survives and added-only deletion works");
    if (!_case.onlyAdded) {
        for (int pos = 0; pos < original.size(); ++pos) {
            if (original[pos] == '\n') {
                continue;
            }
            auto probe = edit->textCursor();
            const int actualPos = pos + (typing && pos >= _case.to ? 1 : 0);
            probe.setPosition(actualPos);
            probe.setPosition(actualPos + 1, QTextCursor::KeepAnchor);
            require(marked(probe.charFormat(), TextBlockStyle::PropertyCommentsIsRemoval)
                        == (pos >= _case.from && pos < _case.to),
                    "removal marks cover exactly the selected source characters");
        }
        auto first = edit->textCursor();
        first.setPosition(0);
        first.setPosition(1, QTextCursor::KeepAnchor);
        require(first.charFormat().fontWeight() == QFont::Bold, "retain original emphasis");
    }
    if (typing) {
        auto inserted = edit->textCursor();
        const int pos = _case.onlyAdded ? _case.from : _case.to;
        inserted.setPosition(pos);
        inserted.setPosition(pos + 1, QTextCursor::KeepAnchor);
        require(marked(inserted.charFormat(), TextBlockStyle::PropertyCommentsIsAddition)
                    && !marked(inserted.charFormat(), TextBlockStyle::PropertyCommentsIsRemoval),
                "replacement typing is an addition rather than a removal");
    }
    edit->undo();
    QApplication::processEvents();
    model->saveChanges();
    require(model->document()->content() == before && edit->toPlainText() == original,
            "one Undo restores exact original text and revision metadata");
    edit->redo();
    QApplication::processEvents();
    model->saveChanges();
    require(model->document()->content() == after && edit->toPlainText() == expected,
            "one Redo restores exact revised text and metadata");
    _fixture.save();
    require(model->document()->content() == after, "coordinator saves exact revised XML");
    edit->hide();
    std::printf("ok: %s, exact marks, Undo/Redo and save\n", qPrintable(name));
    return { { "name", name }, { "uuid", uuid.toString() }, { "text", expected } };
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Track Changes regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    require(argc == 3, "mode and evidence directory supplied");
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        require(QFontDatabase::addApplicationFont(font) >= 0, "load embedded fonts");
    }
    const auto root = QString::fromLocal8Bit(argv[2]);
    Fixture fixture(root + "/story.starc");
    if (QString::fromLocal8Bit(argv[1]) == "--reopen") {
        const auto manifest = QJsonDocument::fromJson(read(root + "/manifest.json")).array();
        require(!manifest.isEmpty(), "read saved case identities");
        for (const auto& value : manifest) {
            const auto entry = value.toObject();
            const auto uuid = QUuid(entry["uuid"].toString());
            const auto bytes = read(root + '/' + entry["name"].toString() + "-after.xml");
            require(StorageFacade::documentStorage()->document(uuid)->content() == bytes,
                    "fresh database load preserves document identity and exact revised XML");
            auto edit = fixture.select(uuid);
            require(edit->toPlainText() == entry["text"].toString(),
                    "fresh-process editor displays exact revised text");
            fixture.model->saveChanges();
            require(fixture.model->document()->content() == bytes,
                    "fresh-process model preserves revision metadata");
            std::printf("ok: fresh-process reopen %s\n", qPrintable(entry["name"].toString()));
        }
        fixture.close();
        std::puts("PASS: fresh-process project/editor reopening");
        return 0;
    }
    // Finish the existing project's first-open schema upgrade before recording byte oracles.
    fixture.save();
    fixture.close();
    Database::setCurrentFile(fixture.project.path());
    fixture.manager.loadCurrentProject(&fixture.project);
    const QVector<Case> cases{
        { "original-added", { "Original line.", "Added line." }, { { 15, 26 } }, 0, 26 },
        { "partial-paragraphs", { "Original line.", "Added line." }, { { 15, 26 } }, 5, 21 },
        { "middle-original",
          { "Added first.", "Original middle.", "Added last." },
          { { 0, 12 }, { 30, 41 } },
          4,
          37 },
        { "single-block-mixed", { "Original.Added." }, { { 9, 15 } }, 3, 13 },
        { "original-fragment", { "AddedOriginalAdded" }, { { 0, 5 }, { 13, 18 } }, 2, 16 },
        { "original-only", { "Original line." }, { }, 2, 10 },
        { "end-at-block-start", { "Original line.", "Added line." }, { { 15, 26 } }, 0, 15 },
        { "all-added", { "Added first.", "Added last." }, { { 0, 12 }, { 13, 24 } }, 0, 24, true },
        { "only-added-fragment", { "Original added tail" }, { { 9, 14 } }, 10, 13, true }
    };
    QJsonArray manifest;
    for (const auto& test : cases) {
        const auto filter = qEnvironmentVariable("STARC_TRACK_CHANGES_CASE");
        if (!filter.isEmpty() && filter != test.name) {
            continue;
        }
        for (bool reverse : { false, true }) {
            for (int key : { Qt::Key_Delete, Qt::Key_Backspace, Qt::Key_X }) {
                manifest.append(checkCase(fixture, root, test, reverse, key));
            }
        }
    }
    require(!manifest.isEmpty(), "selected fixture cases exist");
    write(root + "/manifest.json", QJsonDocument(manifest).toJson());
    fixture.close();
    std::printf("PASS: %lld Track Changes selection cases\n",
                static_cast<long long>(manifest.size()));
    return 0;
}
