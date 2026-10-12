#include <business_layer/document/text/text_document.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
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
#include <utils/tools/backup_builder.h>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLockFile>
#include <QMap>
#include <QSortFilterProxyModel>

#include <cstdio>
#include <cstdlib>

namespace {
void require(bool _ok, const char* _label)
{
    if (!_ok) {
        std::fprintf(stderr, "FAIL: %s\n", _label);
        std::exit(1);
    }
    std::printf("ok: %s\n", _label);
}
QByteArray read(const QString& _path)
{
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "FAIL: read %s\n", qPrintable(_path));
        std::exit(1);
    }
    return file.readAll();
}
void write(const QString& _path, const QByteArray& _bytes)
{
    QDir::root().mkpath(QFileInfo(_path).absolutePath());
    QFile file(_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(_bytes) != _bytes.size()) {
        std::fprintf(stderr, "FAIL: write %s\n", qPrintable(_path));
        std::exit(1);
    }
}
QString identity(const QString& _source)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        QDir::cleanPath(QFileInfo(_source).absoluteFilePath()).toUtf8(),
        QCryptographicHash::Sha256).toHex());
}
QString prefix(const QString& _source)
{
    return QFileInfo(_source).completeBaseName() + '_' + identity(_source) + '_';
}
QMap<QString, QByteArray> contents(const QString& _dir)
{
    QMap<QString, QByteArray> result;
    for (const auto& file : QDir(_dir).entryInfoList(QDir::Files | QDir::Hidden)) {
        if (!file.isSymLink()) {
            result.insert(file.fileName(), read(file.absoluteFilePath()));
        }
    }
    return result;
}
QDateTime time(int _day, int _second = 0)
{
    return QDateTime(QDate(2026, 4, 1).addDays(_day), QTime(12, 0).addSecs(_second));
}
QString save(const QString& _source, const QString& _dir, const QDateTime& _time, int _qty = 7)
{
    const auto result = BackupBuilder::saveAt(_source, _dir, {}, _qty, _time);
    if (!result.success) {
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(result.status));
        std::exit(1);
    }
    return result.status.section('"', 1, 1);
}
void checkRecent(const QString& _root)
{
    const auto source = _root + "/recent/story.starc";
    const auto dir = _root + "/recent/backups";
    QStringList paths;
    // Identical timestamps force numeric collision ordinals, including 9, 10 and 11.
    for (int i = 0; i < 12; ++i) {
        write(source, QByteArray::number(i) + QByteArray("\0\r\n", 3));
        paths.append(save(source, dir, time(0), 2));
    }
    require(contents(dir).size() == 7, "many same-second saves retain seven recent copies");
    for (int i = 0; i < paths.size(); ++i) {
        require(QFile::exists(paths[i]) == (i >= 5), "numeric timestamp ties retain the newest saves");
        if (i >= 5) {
            require(read(paths[i]) == QByteArray::number(i) + QByteArray("\0\r\n", 3),
                    "tied saves have distinct exact bytes, without replacement");
        }
    }
    for (int i = 0; i < 12; ++i) {
        save(source, dir, time(0, i + 1), 9);
    }
    require(contents(dir).size() == 9, "higher existing recent-copy preference is preserved");
}
void checkDays(const QString& _root)
{
    const auto source = _root + "/days/story.starc";
    const auto dir = _root + "/days/backups";
    QStringList daily;
    for (int day = 0; day < 36; ++day) {
        for (int i = 0; i < 12; ++i) {
            write(source, QByteArray::number(day) + ':' + QByteArray::number(i));
            const auto path = save(source, dir, time(day, i));
            if (i == 11) {
                daily.append(path);
            }
        }
    }
    require(contents(dir).size() == 36, "daily and recent union has 30 days plus six recent extras");
    for (int day = 0; day < 36; ++day) {
        require(QFile::exists(daily[day]) == (day >= 6), "inclusive 30-day boundary retains correct days");
        if (day >= 6) {
            require(read(daily[day]) == QByteArray::number(day) + ":11",
                    "daily representative is the newest successful save of that day");
        }
    }
    const auto sparse = _root + "/sparse/backups";
    for (int day = 0; day < 7; ++day) {
        save(source, sparse, time(day));
    }
    require(contents(sparse).size() == 7, "daily/recent overlap does not duplicate files");
    save(source, sparse, time(50));
    require(contents(sparse).size() == 7, "recent seven survive even outside the daily horizon");
}
void checkIsolation(const QString& _root)
{
    const auto a = _root + "/isolation/a/Story [one]+.starc";
    const auto b = _root + "/isolation/b/Story [one]+.starc";
    const auto dir = _root + "/isolation/backups";
    write(a, "A");
    write(b, "B");
    for (int i = 0; i < 12; ++i) {
        save(b, dir, time(0, i));
    }
    const auto otherStory = contents(dir);
    const QStringList unrelated {
        "notes.txt", "Story [one]+_2020_01_01_00_00_00.starc",
        ".Story [one]+_2020_01_01_00_00_00.starc.icloud", "Story [one]+.tmp.starc",
        prefix(a) + "2026_02_30_12_00_00.starc",
        prefix(a) + "2026_04_01_25_00_00.starc",
        prefix(a) + "2026_04_01_12_00_00_0.starc",
        prefix(a) + "2026_04_01_12_00_00_999999999999999999999.starc",
        prefix(a) + "2026_04_01_12_00_00.starc.bak"
    };
    for (const auto& name : unrelated) {
        write(dir + '/' + name, QByteArray("unrelated\0bytes", 15));
    }
    const auto occupied = dir + '/' + prefix(a) + "2026_04_01_12_00_00.starc";
    require(QDir().mkpath(occupied), "directory occupying snapshot name");
    const auto link = dir + '/' + prefix(a) + "2026_04_01_12_00_00_1.starc";
    require(QFile::link(dir + "/missing-target", link), "broken symlink occupying snapshot name");
    const auto first = save(a, dir, time(0));
    require(first.endsWith("_2.starc"), "collisions with directories and broken symlinks are skipped");
    for (int i = 1; i < 12; ++i) {
        save(a, dir, time(0, i));
    }
    const auto after = contents(dir);
    for (auto it = otherStory.cbegin(); it != otherStory.cend(); ++it) {
        require(after.value(it.key()) == it.value(), "same-named story in another folder is untouched");
    }
    for (const auto& name : unrelated) {
        require(after.contains(name) && after.value(name) == QByteArray("unrelated\0bytes", 15),
                "legacy, malformed and unrelated files are untouched");
    }
    require(QFileInfo(occupied).isDir() && QFileInfo(link).isSymLink(),
            "unclassified directories and symlinks survive pruning");
}
void checkFailures(const QString& _root)
{
    const auto source = _root + "/failure/story.starc";
    const auto dir = _root + "/failure/backups";
    // Seed excess old copies: a failure must not opportunistically prune them.
    for (int i = 0; i < 12; ++i) {
        write(dir + '/' + prefix(source) + time(0, i).toString("yyyy_MM_dd_hh_mm_ss") + ".starc",
              QByteArray::number(i));
    }
    const auto before = contents(dir);
    auto result = BackupBuilder::saveAt(source, dir, {}, 7, time(50));
    require(!result.success && !result.status.isEmpty(), "missing source reports copy failure");
    require(contents(dir) == before, "failed copy leaves every existing recovery copy intact");
    write(source, "source");
    QLockFile lock(dir + "/.backup-" + identity(source) + ".lock");
    require(lock.tryLock(), "hold backup lock");
    result = BackupBuilder::saveAt(source, dir, {}, 7, time(50));
    require(!result.success && result.status.contains("lock"), "concurrent operation reports locked family");
    lock.unlock();
    require(contents(dir) == before, "lock failure leaves recovery copies intact");
    result = BackupBuilder::saveAt(source, dir, {}, 7, {});
    require(!result.success && contents(dir) == before, "invalid clock cannot prune recovery copies");
    const auto blocked = _root + "/failure/not-a-folder";
    write(blocked, "blocker");
    result = BackupBuilder::saveAt(source, blocked, {}, 7, time(50));
    require(!result.success && read(blocked) == "blocker", "folder creation failure preserves blocker");
    result = BackupBuilder::saveAt(_root + "/failure", dir, {}, 7, time(50));
    require(!result.success && contents(dir) == before, "directory source cannot produce empty recovery copy");
    result = BackupBuilder::saveAt(source, dir, "../outside", 7, time(50));
    require(!result.success && contents(dir) == before, "unsafe custom name cannot escape backup directory");
}
Ui::ScreenplayTextEdit* editor(const QUuid& _uuid)
{
    for (auto widget : QApplication::allWidgets()) {
        auto edit = qobject_cast<Ui::ScreenplayTextEdit*>(widget);
        auto document = edit ? qobject_cast<BusinessLayer::TextDocument*>(edit->document()) : nullptr;
        if (document && document->model() && document->model()->document()->uuid() == _uuid) {
            return edit;
        }
    }
    return nullptr;
}
void checkProject(const QString& _root)
{
    using Type = Domain::DocumentObjectType;
    const auto path = _root + "/project/disposable.starc";
    QDir().mkpath(QFileInfo(path).absolutePath());
    DatabaseLayer::Database::setCurrentFile(path);
    ManagementLayer::PluginsBuilder plugins;
    QWidget host;
    ManagementLayer::ProjectManager manager(nullptr, &host, plugins);
    BusinessLayer::StructureModel* structure = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::structureModelChanged,
                     [&](BusinessLayer::AbstractModel* model) {
                         structure = qobject_cast<BusinessLayer::StructureModel*>(model);
                     });
    BusinessLayer::ProjectsModelProjectItem project;
    project.setProjectType(BusinessLayer::ProjectType::Local);
    project.setPath(path);
    project.setName("Disposable backup check");
    auto storage = DataStorageLayer::StorageFacade::documentStorage();
    const auto close = [&] {
        manager.closeCurrentProject(project.path());
        storage->clear();
        DatabaseLayer::Database::closeCurrentFile();
    };
    manager.loadCurrentProject(&project);
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    close();
    DatabaseLayer::Database::setCurrentFile(path);
    manager.loadCurrentProject(&project); // Complete upstream schema upgrade before comparisons.
    const auto story = structure->itemForIndex(structure->addDocument(Type::Screenplay, "Backup script"));
    require(story, "create disposable screenplay");
    QUuid scriptUuid;
    for (int i = 0; i < story->childCount(); ++i) {
        if (story->childAt(i)->type() == Type::ScreenplayText) {
            scriptUuid = story->childAt(i)->uuid();
        }
    }
    auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    const auto select = [&] {
        navigator->setCurrentIndex(proxy->mapFromSource(
            structure->indexForItem(structure->itemForUuid(scriptUuid))));
        QApplication::processEvents();
    };
    select();
    auto edit = editor(scriptUuid);
    require(edit, "open disposable screenplay in real editor");
    const QString text = "INT. BACKUP ROOM - DAY";
    for (const auto ch : text) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(edit, &press);
        QApplication::sendEvent(edit, &release);
        // Run the normal queued input callbacks before the fixture closes their models.
        // This is event dispatch, not a timing delay or a save-serialization workaround.
        QApplication::processEvents();
    }
    // ProjectManager stores document contents; explicitly serialize the changed models first.
    manager.firstScriptModel()->saveChanges();
    structure->saveChanges();
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    require(!DatabaseLayer::Database::hasError(), "commit screenplay before backup");
    const auto scriptBytes = storage->document(scriptUuid)->content();
    const auto savedText = edit->toPlainText();
    require(savedText.contains(text), "typed fixture text exists before save");
    const auto fileBytes = read(path);
    const auto backup = save(path, _root + "/project/backups", time(0));
    require(read(backup) == fileBytes, "backup matches complete disposable project bytes exactly");
    close();
    // Restoration follows the existing workflow: copy the .starc snapshot, then open it.
    const auto restored = _root + "/project/restored.starc";
    require(QFile::copy(backup, restored), "restore snapshot to a disposable project file");
    require(read(restored) == fileBytes, "restored file retains exact snapshot bytes before open");
    project.setPath(restored);
    DatabaseLayer::Database::setCurrentFile(restored);
    manager.loadCurrentProject(&project);
    require(storage->document(scriptUuid)->content() == scriptBytes, "restored document has exact serialized bytes");
    select();
    require(editor(scriptUuid) && editor(scriptUuid)->toPlainText() == savedText,
            "restored project opens successfully with saved screenplay text");
    manager.firstScriptModel()->saveChanges();
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    close();
    DatabaseLayer::Database::setCurrentFile(restored);
    manager.loadCurrentProject(&project);
    require(storage->document(scriptUuid)->content() == scriptBytes, "restored screenplay survives save and reopen");
    close();
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Backup retention regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--copy-only") {
        const auto result = BackupBuilder::saveAt(QString::fromLocal8Bit(argv[2]),
            QString::fromLocal8Bit(argv[3]), {}, 7, time(50));
        std::printf("%s\n", qPrintable(result.status));
        return result.success ? 1 : 0;
    }
    require(argc == 2, "evidence directory argument");
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        require(QFontDatabase::addApplicationFont(font) >= 0, "embedded font");
    }
    const auto root = QString::fromLocal8Bit(argv[1]);
    checkRecent(root);
    checkDays(root);
    checkIsolation(root);
    checkFailures(root);
    checkProject(root);
    std::puts("PASS: backup retention and restoration");
    return 0;
}
