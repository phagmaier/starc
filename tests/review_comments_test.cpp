#include <business_layer/document/text/text_document.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/content/project/project_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <management_layer/plugins/screenplay_text/text/screenplay_text_edit.h>
#include <management_layer/plugins_builder.h>
#include <ui/modules/comments/comments_model.h>
#include <ui/project/project_navigator.h>
#include <ui/widgets/tree/tree.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
        project.setName("Disposable review comments regression");
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

using Comment = TextModelTextItem::ReviewComment;
using Mark = TextModelTextItem::ReviewMark;

const QString kFirstDate = "2020-01-02T03:04:05";
const QString kOtherDate = "2021-02-03T04:05:06";

// A comments-panel row the writer must see afterwards, in positions of the whole text.
struct Thread {
    int from;
    int to;
    QStringList messages;
    QColor color = Qt::yellow;
    bool done = false;
};

struct Case {
    QString name;
    QVector<Mark> marks;
    int from;
    int to;
    QVector<Thread> threads;
    QString text = "second comment";
    QString author = "Writer";
    QColor color = Qt::yellow;
    bool revision = false;
    bool addition = false;
    bool removal = false;
    QStringList paragraphs = { "Alpha Beta Gamma" };
};

Mark mark(int _from, int _to, const QString& _text = "first comment",
          const QString& _author = "Writer", const QColor& _color = Qt::yellow,
          const QString& _date = kFirstDate, bool _revision = false, bool _addition = false,
          bool _removal = false)
{
    Mark result;
    result.from = _from;
    result.length = _to - _from;
    result.textColor = Qt::black;
    result.backgroundColor = _color;
    result.comments.append(
        { _author, _author + "@invalid.test", _date, _text, true, _revision, _addition, _removal });
    return result;
}

QJsonArray toJson(const QVector<Thread>& _threads)
{
    QJsonArray result;
    for (const auto& thread : _threads) {
        result.append(QJsonObject{ { "from", thread.from },
                                   { "to", thread.to },
                                   { "messages", QJsonArray::fromStringList(thread.messages) },
                                   { "color", thread.color.name() },
                                   { "done", thread.done } });
    }
    return result;
}

// The live or freshly loaded comments panel shows exactly the expected rows.
QVector<Comment> requireThreads(const CommentsModel& _comments, const QString& _text,
                                const QJsonArray& _threads)
{
    QVector<Comment> visible;
    for (int row = 0; row < _comments.rowCount(); ++row) {
        const auto index = _comments.index(row, 0);
        const auto messages
            = index.data(CommentsModel::ReviewMarkRepliesRole).value<QVector<Comment>>();
        std::printf("thread %d, source=%s, messages=%lld\n", row,
                    qPrintable(index.data(CommentsModel::ReviewMarkSourceTextRole).toString()),
                    static_cast<long long>(messages.size()));
        for (const auto& message : messages) {
            std::printf("message=%s, author=%s, date=%s\n", qPrintable(message.text),
                        qPrintable(message.author), qPrintable(message.date));
        }
        visible += messages;
    }
    require(_comments.rowCount() == _threads.size(), "expected comment thread count");
    for (int row = 0; row < _threads.size(); ++row) {
        const auto thread = _threads.at(row).toObject();
        const auto index = _comments.index(row, 0);
        const auto from = thread["from"].toInt();
        require(index.data(CommentsModel::ReviewMarkSourceTextRole).toString()
                    == _text.mid(from, thread["to"].toInt() - from),
                "thread is anchored to exactly the expected text");
        QStringList messages;
        for (const auto& message :
             index.data(CommentsModel::ReviewMarkRepliesRole).value<QVector<Comment>>()) {
            messages.append(message.text);
        }
        require(messages == thread["messages"].toVariant().toStringList(),
                "thread shows exactly the expected messages in order");
        require(index.data(CommentsModel::ReviewMarkColorRole).value<QColor>().name()
                    == thread["color"].toString(),
                "thread has the expected color");
        require(index.data(CommentsModel::ReviewMarkIsDoneRole).toBool() == thread["done"].toBool(),
                "thread has the expected resolved state");
    }
    return visible;
}

QJsonObject checkCase(Fixture& _fixture, const QString& _root, const Case& _case, bool _reverse)
{
    const auto name = _case.name + (_reverse ? "-reverse" : "-forward");
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
    const QString original = _case.paragraphs.join('\n');
    int position = 0;
    for (const auto& text : _case.paragraphs) {
        auto item = new ScreenplayTextModelTextItem(model);
        item->setParagraphType(TextParagraphType::Action);
        item->setText(text);
        QVector<Mark> marks;
        for (auto mark : _case.marks) {
            const auto from = std::max(position, mark.from);
            const auto to = std::min(position + int(text.size()), mark.end());
            if (from < to) {
                mark.from = from - position;
                mark.length = to - from;
                marks.append(mark);
            }
        }
        item->setReviewMarks(marks);
        if (position == 0) {
            // Emphasis splits a thread into several stored marks.
            QTextCharFormat bold;
            bold.setFontWeight(QFont::Bold);
            item->setFormats({ { 2, 6, bold } });
        }
        model->appendItem(item);
        position += text.size() + 1;
    }
    _fixture.editors.push_back(std::make_unique<Ui::ScreenplayTextEdit>());
    auto edit = _fixture.editors.back().get();
    edit->setCorrectionOptions(false, false);
    edit->resize(900, 700);
    edit->initWithModel(model);
    edit->show();
    QApplication::processEvents();
    require(edit->toPlainText() == original, "authored fixture text is exact");
    _fixture.save();
    const auto before = model->document()->content();
    write(_root + '/' + name + "-before.xml", before);
    CommentsModel comments;
    comments.setTextModel(model);
    setSettingsValue(DataStorageLayer::kAccountUserNameKey, _case.author);
    setSettingsValue(DataStorageLayer::kAccountEmailKey, _case.author + "@invalid.test");
    auto cursor = edit->textCursor();
    cursor.setPosition(_reverse ? _case.to : _case.from);
    cursor.setPosition(_reverse ? _case.from : _case.to, QTextCursor::KeepAnchor);
    edit->setTextCursorForced(cursor);
    edit->addReviewMark({ }, _case.color, _case.text, _case.revision, _case.addition,
                        _case.removal);
    QApplication::processEvents();
    model->saveChanges();
    const auto after = model->document()->content();
    write(_root + '/' + name + "-after.xml", after);
    require(edit->toPlainText() == original, "adding a mark preserves script text");
    const auto threads = toJson(_case.threads);
    const auto visible = requireThreads(comments, original, threads);
    if (!_case.text.isEmpty()) {
        int incomingCount = 0;
        for (const auto& comment : visible) {
            incomingCount += comment.text == _case.text && comment.author == _case.author
                && comment.authorEmail == _case.author + "@invalid.test" && !comment.isEdited
                && comment.date != kFirstDate && comment.date != kOtherDate;
        }
        require(incomingCount == 1, "new comment is stored and visible exactly once");
        for (const auto& oldMark : _case.marks) {
            for (const auto& oldComment : oldMark.comments) {
                require(oldComment.text.isEmpty() || visible.contains(oldComment),
                        "retain exact original comment/reply metadata");
            }
        }
    }
    for (const auto& comment : visible) {
        require(comment.isRevision == _case.revision && comment.isAddition == _case.addition
                    && comment.isRemoval == _case.removal,
                "every message keeps its exact revision flags");
    }
    // The editor's own character formats, independently of the panel.
    for (int pos = 0; pos < original.size(); ++pos) {
        if (original[pos] == '\n') {
            continue;
        }
        auto probe = edit->textCursor();
        probe.setPosition(pos);
        probe.setPosition(pos + 1, QTextCursor::KeepAnchor);
        const auto format = probe.charFormat();
        require((format.fontWeight() == QFont::Bold) == (pos >= 2 && pos < 8),
                "retain exact original emphasis");
        QStringList expected;
        bool isExpected = false;
        for (const auto& thread : _case.threads) {
            if (thread.from <= pos && pos < thread.to) {
                expected = thread.messages;
                isExpected = true;
            }
        }
        require(format.boolProperty(TextBlockStyle::PropertyIsReviewMark) == isExpected
                    && (!isExpected
                        || format.property(TextBlockStyle::PropertyComments).toStringList()
                            == expected),
                "every character carries exactly its expected thread");
    }
    if (qEnvironmentVariableIsSet("STARC_REVIEW_COMMENTS_SCREENSHOTS")) {
        require(edit->grab().save(_root + '/' + name + "-after.png"),
                "retain native editor screenshot");
    }
    edit->undo();
    QApplication::processEvents();
    model->saveChanges();
    require(model->document()->content() == before, "one Undo restores byte-exact original XML");
    edit->redo();
    QApplication::processEvents();
    model->saveChanges();
    require(model->document()->content() == after, "one Redo restores byte-exact commented XML");
    requireThreads(comments, original, threads);
    if (!_case.text.isEmpty()) {
        // Typing before the threads moves them all beyond their old ranges.
        auto prefix = edit->textCursor();
        prefix.setPosition(0);
        prefix.insertText(QString(10, 'X'), QTextCharFormat());
        QApplication::processEvents();
        auto moved = _case.threads;
        for (auto& thread : moved) {
            thread.from += 10;
            thread.to += 10;
        }
        requireThreads(comments, QString(10, 'X') + original, toJson(moved));
        edit->undo();
        QApplication::processEvents();
        model->saveChanges();
        require(model->document()->content() == after,
                "Undo of an anchor shift restores byte-exact comment XML");
        requireThreads(comments, original, threads);
    }
    _fixture.save();
    require(model->document()->content() == after, "coordinator saves exact commented XML");
    edit->hide();
    std::printf("ok: %s, payloads, ranges, Undo/Redo and save\n", qPrintable(name));
    return {
        { "name", name }, { "uuid", uuid.toString() }, { "text", original }, { "threads", threads }
    };
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Review comments regression");
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
                    "fresh database load preserves document identity and exact comment XML");
            auto edit = fixture.select(uuid);
            require(edit->toPlainText() == entry["text"].toString(), "reopen exact script text");
            fixture.model->saveChanges();
            require(fixture.model->document()->content() == bytes, "reopen exact comment metadata");
            CommentsModel comments;
            comments.setTextModel(fixture.model);
            requireThreads(comments, entry["text"].toString(), entry["threads"].toArray());
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
    auto resolved = mark(0, 6);
    resolved.isDone = true;
    resolved.comments.append(
        { "Reviewer", "Reviewer@invalid.test", kOtherDate, "existing reply", true });
    const QString first = "first comment";
    const QString second = "second comment";
    const QString reply = "existing reply";
    const QStringList twoParagraphs = { "Alpha Beta", "Gamma Delta" };
    Case differentColorAdjacent{ "different-color-adjacent",
                                 { mark(0, 6) },
                                 6,
                                 10,
                                 { { 0, 6, { first } }, { 6, 10, { second }, Qt::cyan } } };
    differentColorAdjacent.color = Qt::cyan;
    Case differentColorOverlap{ "different-color-overlap",
                                { mark(0, 6) },
                                3,
                                10,
                                { { 0, 10, { first, second } } } };
    differentColorOverlap.color = Qt::cyan;
    Case multiParagraphThread{ "multi-paragraph-thread",
                               { mark(2, 16) },
                               14,
                               22,
                               { { 2, 22, { first, second } } },
                               second,
                               "Reviewer" };
    multiParagraphThread.paragraphs = twoParagraphs;
    Case multiParagraphSelection{ "multi-paragraph-selection",
                                  { mark(0, 5) },
                                  3,
                                  16,
                                  { { 0, 16, { first, second } } } };
    multiParagraphSelection.paragraphs = twoParagraphs;
    Case multiParagraphAdjacent{ "multi-paragraph-adjacent",
                                 { mark(2, 10) },
                                 11,
                                 16,
                                 { { 2, 10, { first } }, { 11, 16, { second } } } };
    multiParagraphAdjacent.paragraphs = twoParagraphs;
    Case revisionControl{ "revision-control",
                          { mark(8, 10, "", "Writer", Qt::yellow, kFirstDate, true) },
                          10,
                          16,
                          { { 8, 16, { "" } } },
                          "" };
    revisionControl.revision = true;
    Case additionControl{ "addition-control",
                          { mark(8, 10, "", "Writer", Qt::yellow, kFirstDate, false, true) },
                          10,
                          16,
                          { { 8, 16, { "" } } },
                          "" };
    additionControl.addition = true;
    Case removalControl{ "removal-control",
                         { mark(8, 10, "", "Writer", Qt::yellow, kFirstDate, false, false, true) },
                         10,
                         16,
                         { { 8, 16, { "" } } },
                         "" };
    removalControl.removal = true;
    const QVector<Case> cases{
        // Touching selections share no character: every comment keeps its own text.
        { "adjacent", { mark(0, 6) }, 6, 10, { { 0, 6, { first } }, { 6, 10, { second } } } },
        { "adjacent-before",
          { mark(6, 10) },
          0,
          6,
          { { 0, 6, { second } }, { 6, 10, { first } } } },
        { "resolved-thread-adjacent",
          { resolved },
          6,
          10,
          { { 0, 6, { first, reply }, Qt::yellow, true }, { 6, 10, { second } } } },
        { "different-author-adjacent",
          { mark(0, 6) },
          6,
          10,
          { { 0, 6, { first } }, { 6, 10, { second } } },
          second,
          "Reviewer" },
        differentColorAdjacent,
        { "between-threads",
          { mark(0, 6), mark(11, 16, "distant thread", "Writer", Qt::yellow, kOtherDate) },
          6,
          10,
          { { 0, 6, { first } }, { 6, 10, { second } }, { 11, 16, { "distant thread" } } } },
        multiParagraphAdjacent,
        { "comment-beside-highlight",
          { mark(0, 6, "") },
          6,
          10,
          { { 0, 6, { "" } }, { 6, 10, { second } } } },
        // A character holds one thread: shared text continues the existing threads.
        { "overlap", { mark(0, 6) }, 3, 10, { { 0, 10, { first, second } } } },
        { "contained", { mark(0, 10) }, 3, 6, { { 0, 10, { first, second } } } },
        { "containing", { mark(3, 6) }, 0, 10, { { 0, 10, { first, second } } } },
        { "identical-range", { mark(0, 6) }, 0, 6, { { 0, 6, { first, second } } } },
        { "repeated-message", { mark(0, 6) }, 3, 10, { { 0, 10, { first, first } } }, first },
        { "resolved-thread-overlap",
          { resolved },
          3,
          10,
          { { 0, 10, { first, reply, second } } } },
        { "different-author-overlap",
          { mark(0, 6) },
          3,
          10,
          { { 0, 10, { first, second } } },
          second,
          "Reviewer" },
        differentColorOverlap,
        { "bridge-threads",
          { mark(0, 5), mark(11, 16, "other thread", "Reviewer", Qt::cyan, kOtherDate) },
          3,
          13,
          { { 0, 16, { first, "other thread", second } } } },
        multiParagraphThread,
        multiParagraphSelection,
        // A highlight has no text to keep; the comment takes the selected part of it.
        { "new-comment-control", { }, 3, 10, { { 3, 10, { second } } } },
        { "comment-in-highlight",
          { mark(0, 10, "") },
          3,
          6,
          { { 0, 3, { "" } }, { 3, 6, { second } }, { 6, 10, { "" } } } },
        { "comment-over-highlight", { mark(3, 6, "") }, 0, 10, { { 0, 10, { second } } } },
        // Highlights and revision marks without text still extend the mark they touch.
        { "highlight-control", { mark(8, 10, "") }, 10, 16, { { 8, 16, { "" } } }, "" },
        { "highlight-over-comment", { mark(8, 12) }, 10, 16, { { 8, 16, { first } } }, "" },
        { "highlight-covering-comment", { mark(10, 12) }, 8, 16, { { 8, 16, { first } } }, "" },
        revisionControl,
        additionControl,
        removalControl,
    };
    QJsonArray manifest;
    for (const auto& test : cases) {
        const auto filter = qEnvironmentVariable("STARC_REVIEW_COMMENTS_CASE");
        if (!filter.isEmpty() && filter != test.name) {
            continue;
        }
        for (bool reverse : { false, true }) {
            manifest.append(checkCase(fixture, root, test, reverse));
        }
    }
    require(!manifest.isEmpty(), "selected fixture cases exist");
    write(root + "/manifest.json", QJsonDocument(manifest).toJson());
    fixture.close();
    std::printf("PASS: %lld review-comment cases\n", static_cast<long long>(manifest.size()));
    return 0;
}
