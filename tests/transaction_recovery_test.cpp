#include <data_layer/database.h>
#include <data_layer/storage/document_change_storage.h>
#include <data_layer/storage/document_image_storage.h>
#include <data_layer/storage/document_raw_data_storage.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_change_object.h>
#include <domain/document_object.h>

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

#include <cstdio>
#include <cstdlib>

using DatabaseLayer::Database;
using DataStorageLayer::StorageFacade;

namespace {
void require(bool _ok, const char* _label)
{
    std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
    if (!_ok) {
        std::exit(1);
    }
}

QByteArray fileBytes(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable story bytes");
    return file.readAll();
}

void retain(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain independent byte oracle");
}

QByteArray png(Qt::GlobalColor _color)
{
    QPixmap image(12, 12);
    image.fill(_color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    require(buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG"),
            "create authored PNG fixture");
    return bytes;
}

void runCase(const QString& _root, const QString& _mode)
{
    require(QDir().mkpath(_root), "create isolated storage fixture");
    const auto path = _root + "/story.starc";
    Database::clearLastError();
    Database::setCurrentFile(path);
    auto documents = StorageFacade::documentStorage();
    auto changes = StorageFacade::documentChangeStorage();
    DataStorageLayer::DocumentImageStorage images;
    DataStorageLayer::DocumentRawDataStorage raw;
    auto script = documents->createDocument(QUuid::createUuid(),
                                            Domain::DocumentObjectType::ScreenplayText);
    script->setContent(QByteArray("old-script\x00\r\n", 13));
    auto later
        = documents->createDocument(QUuid::createUuid(), Domain::DocumentObjectType::SimpleText);
    later->setContent("old-notes");
    const auto removedImage = QUuid::createUuid();
    images.save(removedImage, png(Qt::red));
    const auto removedRaw = raw.save("old-attachment");
    require(Database::transaction() && documents->saveDocument(script)
                && documents->saveDocument(later),
            "begin baseline save");
    images.saveChanges();
    raw.saveChanges();
    changes->appendDocumentChange(script->uuid(), QUuid::createUuid(), "baseline-undo",
                                  "baseline-redo", "", "Writer");
    changes->store();
    require(Database::commit() && !Database::hasError(),
            "commit baseline with attachments and history");
    const auto baseline = fileBytes(path);
    retain(_root + "/before.starc", baseline);
    const auto scriptId = script->id();
    const auto laterId = later->id();
    auto inserted
        = documents->createDocument(QUuid::createUuid(), Domain::DocumentObjectType::SimpleText);
    inserted->setContent("new-document");
    const auto insertedUuid = inserted->uuid();
    script->setContent(QByteArray("new-script\x00\r\n", 13));
    later->setContent(_mode == "full" ? QByteArray(2 * 1024 * 1024, 'X') : QByteArray("new-notes"));
    const auto newImage = png(Qt::green);
    const auto addedImage = QUuid::createUuid();
    images.save(addedImage, newImage);
    const auto addedRaw = raw.save(QByteArray("new-attachment\x00\r\n", 17));
    images.remove(removedImage);
    raw.remove(removedRaw);
    auto firstChange = changes->appendDocumentChange(script->uuid(), QUuid::createUuid(),
                                                     "first-undo", "first-redo", "", "Writer");
    auto secondChange = changes->appendDocumentChange(script->uuid(), QUuid::createUuid(),
                                                      "second-undo", "second-redo", "", "Writer");
    const auto firstUuid = firstChange->uuid();
    const auto secondUuid = secondChange->uuid();

    auto query = Database::query();
    if (_mode != "full" && _mode != "commit") {
        require(query.exec(
                    QString("CREATE TRIGGER reject_later BEFORE UPDATE ON documents "
                            "WHEN OLD.type=100002 BEGIN SELECT RAISE(%1, 'late save fault'); END")
                        .arg(_mode == "rollback" ? "ROLLBACK" : "ABORT")),
                "install late statement fault");
    } else if (_mode == "commit") {
        require(
            query.exec("PRAGMA foreign_keys=ON")
                && query.exec("CREATE TABLE parent(id INTEGER PRIMARY KEY)")
                && query.exec(
                    "CREATE TABLE child(id REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED)")
                && query.exec("CREATE TRIGGER reject_later AFTER UPDATE ON documents "
                              "WHEN OLD.type=100002 BEGIN INSERT INTO child VALUES(404); END"),
            "install root COMMIT fault");
    }
    const auto beforeSave = fileBytes(path);

    auto attempt = [&] {
        Database::clearLastError();
        require(Database::transaction() && documents->saveDocument(script)
                    && documents->saveDocument(inserted),
                "accept update and insert before the fault");
        images.saveChanges();
        raw.saveChanges();
        changes->store();
        require(
            !Database::hasError() && firstChange->id().isValid() && secondChange->id().isValid(),
            "accept attachments, deletions and multiple nested history inserts before the fault");
        if (_mode == "full") {
            require(query.exec("PRAGMA page_count") && query.next(), "read current page count");
            const auto pages = query.value(0).toInt();
            query.finish();
            require(query.exec(QString("PRAGMA max_page_count=%1").arg(pages)),
                    "bound database capacity without filling the physical disk");
            query.finish();
        }
        const auto updated = documents->saveDocument(later);
        const auto error = Database::lastError();
        if (_mode != "commit") {
            require(!updated && !error.isEmpty(), "later statement fails");
            if (_mode == "full") {
                require(error.contains("full"), "genuine SQLITE_FULL capacity error");
            }
            Database::clearLastError();
            require(Database::hasError() && Database::lastError() == error,
                    "clearing an error cannot revive an aborted outer transaction");
            require(
                !documents->saveDocument(insertedUuid),
                "UUID save refuses failed transaction before looking up a rolled-back insertion");
            auto blocked = changes->appendDocumentChange(
                script->uuid(), QUuid::createUuid(), "blocked-undo", "blocked-redo", "", "Writer");
            changes->store();
            require(!blocked->id().isValid(),
                    "no history write autocommits after the transaction failed");
        } else {
            require(updated && error.isEmpty(), "all statements succeed before rejected COMMIT");
        }
        require(!Database::commit() && Database::hasError() && !Database::isTransactionActive(),
                "failed root save ends transaction and retains its original error");
        if (_mode != "commit") {
            require(Database::lastError() == error,
                    "rollback does not replace the original statement error");
        }
        require(!script->isChangesStored() && !inserted->isChangesStored()
                    && !inserted->id().isValid() && script->id() == scriptId
                    && later->id() == laterId,
                "lost updates become dirty and insertion state resets without changing existing "
                "identities");
        require(documents->document(insertedUuid) == inserted && documents->document(removedImage)
                    && documents->document(removedRaw) && !firstChange->id().isValid()
                    && !secondChange->id().isValid() && firstChange->uuid() == firstUuid
                    && secondChange->uuid() == secondUuid,
                "restore document ownership, deletions, history queues and UUIDs");
        require(fileBytes(path) == beforeSave,
                "failed transaction preserves the entire committed database");
    };
    attempt();
    script->setContent(QByteArray("new-script plus later edit\x00\r\n", 29));
    auto extraChange = changes->appendDocumentChange(script->uuid(), QUuid::createUuid(),
                                                     "extra-undo", "extra-redo", "", "Writer");
    attempt();
    require(!extraChange->id().isValid(), "another failed Retry retains later history too");
    require(
        query.exec(_mode == "full" ? "PRAGMA max_page_count=1048576" : "DROP TRIGGER reject_later"),
        "repair underlying storage fault");
    query.finish();
    if (_mode == "commit") {
        require(query.exec("INSERT INTO parent VALUES(404)"), "repair deferred constraint");
    }
    Database::clearLastError();
    require(Database::transaction() && documents->saveDocument(script)
                && documents->saveDocument(inserted) && documents->saveDocument(later),
            "resubmit all live document bytes after recovery");
    images.saveChanges();
    raw.saveChanges();
    changes->store();
    require(Database::commit() && !Database::hasError() && script->isChangesStored()
                && inserted->isChangesStored() && later->isChangesStored(),
            "recovered Save commits all previously lost writes");
    require(firstChange->id().value() < secondChange->id().value()
                && secondChange->id().value() < extraChange->id().value(),
            "history order is retained across repeated rollback");
    retain(_root + "/script.bin", script->content());
    retain(_root + "/later.bin", later->content());
    retain(_root + "/image.png", newImage);
    retain(_root + "/attachment.bin", raw.load(addedRaw));
    retain(_root + "/identities.txt",
           QString("%1\n%2\n%3\n%4\n%5\n%6\n%7\n%8\n")
               .arg(script->uuid().toString(), later->uuid().toString(), insertedUuid.toString(),
                    addedImage.toString(), addedRaw.toString(), firstUuid.toString(),
                    secondUuid.toString(), extraChange->uuid().toString())
               .toUtf8());
    StorageFacade::clearStorages();
    Database::closeCurrentFile();
    Database::setCurrentFile(path);
    require(documents->document(insertedUuid)->content() == "new-document"
                && documents->document(addedImage)->content() == fileBytes(_root + "/image.png")
                && documents->document(addedRaw)->content() == fileBytes(_root + "/attachment.bin")
                && !documents->document(removedImage) && !documents->document(removedRaw),
            "reopen preserves inserts and attachments, and applies retained deletions");
    StorageFacade::clearStorages();
    query = QSqlQuery();
    Database::closeCurrentFile();
    std::printf("PASS: %s\n", qPrintable(_mode));
}
} // namespace

int main(int _argc, char** _argv)
{
    QApplication app(_argc, _argv);
    app.setApplicationVersion("0.8.4");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    require(_argc == 2, "disposable evidence root supplied");
    for (const auto& mode :
         { QString("full"), QString("rollback"), QString("abort"), QString("commit") }) {
        runCase(QString::fromLocal8Bit(_argv[1]) + '/' + mode, mode);
    }
    return 0;
}
