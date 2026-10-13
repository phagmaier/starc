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
        project.setName("Disposable hidden beat regression");
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
    QStringList texts;
    QVector<int> headings;
    QString selection;
};

QVector<QTextBlock> blocks(Ui::ScreenplayTextEdit* _edit)
{
    QVector<QTextBlock> result;
    for (auto block = _edit->document()->begin(); block.isValid(); block = block.next()) {
        result.append(block);
    }
    return result;
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
    auto first = new ScreenplayTextModelTextItem(model);
    first->setParagraphType(TextParagraphType::Action);
    first->setText(_case.texts.first());
    model->appendItem(first);
    // Keep editors alive: the production handler facade caches editor addresses.
    _fixture.editors.push_back(std::make_unique<Ui::ScreenplayTextEdit>());
    auto edit = _fixture.editors.back().get();
    edit->setCorrectionOptions(false, false);
    edit->resize(900, 700);
    edit->initWithModel(model);
    edit->setBeatsVisible(true);
    edit->show();
    for (int i = 1; i < _case.texts.size(); ++i) {
        auto cursor = edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        edit->setTextCursorForced(cursor);
        edit->addParagraph(_case.headings.contains(i) ? TextParagraphType::BeatHeading
                                                      : TextParagraphType::Action);
        cursor = edit->textCursor();
        cursor.insertText(_case.texts[i]);
        edit->setTextCursorForced(cursor);
    }
    QApplication::processEvents();
    auto sourceBlocks = blocks(edit);
    require(sourceBlocks.size() == _case.texts.size(), "authored block count");
    for (int i : _case.headings) {
        auto cursor = edit->textCursor();
        cursor.setPosition(sourceBlocks[i].position());
        cursor.setPosition(sourceBlocks[i].position() + sourceBlocks[i].text().size(),
                           QTextCursor::KeepAnchor);
        QTextCharFormat emphasis;
        emphasis.setFontWeight(QFont::Bold);
        cursor.mergeCharFormat(emphasis);
        edit->setTextCursorForced(cursor);
        edit->addReviewMark(Qt::black, Qt::yellow, "Retained beat note", false, false, false);
    }
    const bool visible = _case.selection == "visible";
    edit->setBeatsVisible(visible);
    QApplication::processEvents();
    for (int i : _case.headings) {
        require(sourceBlocks[i].isVisible() == visible, "heading visibility before editing");
    }
    const auto original = _case.texts.join('\n');
    require(edit->toPlainText() == original, "exact authored text");
    _fixture.save();
    const auto before = model->document()->content();
    write(_root + '/' + name + "-before.xml", before);
    int from = sourceBlocks.first().position() + sourceBlocks.first().text().size();
    int to = sourceBlocks.last().position();
    QStringList expectedTexts = _case.texts;
    if (_case.selection == "partial" || visible || _case.selection == "same-beat") {
        from = 2;
        const int endBlock = _case.selection == "same-beat" ? 1 : sourceBlocks.size() - 1;
        to = sourceBlocks[endBlock].position() + 2;
        if (visible || _case.selection == "same-beat") {
            expectedTexts[0] = _case.texts[0].left(2) + _case.texts[endBlock].mid(2);
            for (int i = endBlock; i > 0; --i)
                expectedTexts.removeAt(i);
        } else {
            expectedTexts[0] = expectedTexts[0].left(2);
            expectedTexts.last() = expectedTexts.last().mid(2);
            for (int i = 1; i < expectedTexts.size() - 1; ++i) {
                if (!_case.headings.contains(i))
                    expectedTexts[i].clear();
            }
        }
    } else if (_case.selection == "all") {
        from = 0;
        to = edit->document()->characterCount() - 1;
        for (int i = 0; i < expectedTexts.size(); ++i) {
            if (!_case.headings.contains(i))
                expectedTexts[i].clear();
        }
    }
    const bool typing = _key == Qt::Key_X;
    if (typing) {
        const int offset = _case.selection == "all" ? 0
            : _case.selection == "partial" || visible || _case.selection == "same-beat"
            ? 2
            : expectedTexts[0].size();
        expectedTexts[0].insert(offset, "X");
    }
    auto cursor = edit->textCursor();
    if (_case.selection == "single-key") {
        cursor.setPosition(_key == Qt::Key_Backspace ? to : from);
    } else {
        cursor.setPosition(_reverse ? to : from);
        cursor.setPosition(_reverse ? from : to, QTextCursor::KeepAnchor);
    }
    edit->setTextCursorForced(cursor);
    const auto originalPosition = cursor.position();
    require(!edit->isReadOnly(), "editable production editor");
    QKeyEvent press(QEvent::KeyPress, _key, Qt::NoModifier, typing ? "X" : "");
    QKeyEvent release(QEvent::KeyRelease, _key, Qt::NoModifier, typing ? "X" : "");
    QApplication::sendEvent(edit, &press);
    QApplication::sendEvent(edit, &release);
    QApplication::processEvents();
    model->saveChanges();
    const auto after = model->document()->content();
    write(_root + '/' + name + "-after.xml", after);
    const auto expected = expectedTexts.join('\n');
    if (edit->toPlainText() != expected) {
        std::fprintf(stderr, "actual=[%s] expected=[%s]\n", qPrintable(edit->toPlainText()),
                     qPrintable(expected));
    }
    require(edit->toPlainText() == expected, "visible edit preserves hidden heading boundaries");
    if (_case.selection == "single-key") {
        require(edit->textCursor().position() == originalPosition,
                "boundary key retains caret at hidden beat boundary");
    }
    if (after != before) {
        edit->undo();
        QApplication::processEvents();
        model->saveChanges();
        require(model->document()->content() == before && edit->toPlainText() == original,
                "one Undo restores exact XML, identities and annotations");
        edit->redo();
        QApplication::processEvents();
        model->saveChanges();
        require(model->document()->content() == after && edit->toPlainText() == expected,
                "one Redo restores exact edited XML");
    }
    _fixture.save();
    require(model->document()->content() == after, "coordinator saves exact XML");
    edit->hide();
    std::printf("ok: %s, visibility, text, Undo/Redo and save\n", qPrintable(name));
    return { { "name", name },
             { "uuid", uuid.toString() },
             { "text", expected },
             { "hidden", !visible },
             { "singleKey", _case.selection == "single-key" } };
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Hidden beats regression");
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
        require(!manifest.isEmpty(), "read saved identities");
        for (const auto& value : manifest) {
            const auto entry = value.toObject();
            const auto uuid = QUuid(entry["uuid"].toString());
            const auto bytes = read(root + '/' + entry["name"].toString() + "-after.xml");
            require(StorageFacade::documentStorage()->document(uuid)->content() == bytes,
                    "fresh database preserves identity and exact XML");
            auto edit = fixture.select(uuid);
            edit->setBeatsVisible(true);
            QApplication::processEvents();
            require(edit->toPlainText() == entry["text"].toString(),
                    "reopened editor reveals exact retained outline and edited text");
            fixture.model->saveChanges();
            require(fixture.model->document()->content() == bytes,
                    "fresh model retains exact outline metadata");
            std::printf("ok: fresh-process reopen %s\n", qPrintable(entry["name"].toString()));
        }
        fixture.close();
        std::puts("PASS: fresh-process project/editor reopening");
        return 0;
    }
    fixture.save();
    fixture.close();
    Database::setCurrentFile(fixture.project.path());
    fixture.manager.loadCurrentProject(&fixture.project);
    const QVector<Case> cases{
        { "hidden-boundary", { "Alpha", "HIDDEN BEAT TITLE", "Beta" }, { 1 }, "boundary" },
        { "hidden-partial", { "Alpha", "HIDDEN BEAT TITLE", "Beta" }, { 1 }, "partial" },
        { "hidden-multiple",
          { "Alpha", "HIDDEN ONE", "Middle", "HIDDEN TWO", "Beta" },
          { 1, 3 },
          "partial" },
        { "hidden-adjacent", { "Alpha", "HIDDEN ONE", "HIDDEN TWO", "Beta" }, { 1, 2 }, "partial" },
        { "hidden-select-all", { "Alpha", "HIDDEN BEAT TITLE", "Beta" }, { 1 }, "all" },
        { "hidden-empty-before", { "", "HIDDEN BEAT TITLE", "Beta" }, { 1 }, "single-key" },
        { "hidden-empty-after", { "Alpha", "HIDDEN BEAT TITLE", "" }, { 1 }, "single-key" },
        { "hidden-single-key", { "Alpha", "HIDDEN BEAT TITLE", "Beta" }, { 1 }, "single-key" },
        { "same-beat-control",
          { "Alpha", "Beta", "HIDDEN BEAT TITLE", "Gamma" },
          { 2 },
          "same-beat" },
        { "visible-beat-control", { "Alpha", "VISIBLE BEAT TITLE", "Beta" }, { 1 }, "visible" }
    };
    QJsonArray manifest;
    for (const auto& test : cases) {
        const auto filter = qEnvironmentVariable("STARC_HIDDEN_BEATS_CASE");
        if (!filter.isEmpty() && filter != test.name)
            continue;
        for (bool reverse : { false, true }) {
            for (int key : { Qt::Key_Delete, Qt::Key_Backspace, Qt::Key_X }) {
                if (test.selection == "single-key" && (reverse || key == Qt::Key_X))
                    continue;
                manifest.append(checkCase(fixture, root, test, reverse, key));
            }
        }
    }
    require(!manifest.isEmpty(), "selected fixture cases exist");
    write(root + "/manifest.json", QJsonDocument(manifest).toJson());
    fixture.close();
    std::printf("PASS: %lld hidden-beat editor cases\n", static_cast<long long>(manifest.size()));
    return 0;
}
