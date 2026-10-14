#include <data_layer/database.h>

#include <QCoreApplication>
#include <QFile>
#include <QSqlQuery>
#include <QVariant>

#include <cstdio>
#include <cstdlib>

namespace {
void require(bool _condition, const char* _message)
{
    if (!_condition) {
        std::fprintf(stderr, "FAIL: %s\n", _message);
        std::exit(1);
    }
}

QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read disposable fixture");
    return file.readAll();
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion("0.8.4");
    require(argc == 3, "disposable directory and phase");
    const auto root = QString::fromLocal8Bit(argv[1]);
    const auto phase = QString::fromLocal8Bit(argv[2]);
    require(phase == "create" || phase == "open" || phase == "refuse", "known phase");
    using DatabaseLayer::Database;
    Database::setCurrentFile(root + "/story.starc");
    if (phase == "refuse") {
        require(Database::hasError() && !Database::lastError().isEmpty(),
                "failed migration reports an error");
        std::printf("ok: refused migration: %s\n", Database::lastError().toUtf8().constData());
    } else {
        require(!Database::hasError(), "database opens without errors");
        auto query = Database::query();
        require(
            query.exec("SELECT value FROM system_variables WHERE variable = 'application-version'")
                && query.next() && query.value(0).toString() == app.applicationVersion(),
            "current version is committed at creation or successful upgrade");
        query.finish();
        if (phase == "create") {
            require(query.exec("SELECT COUNT(*) FROM system_variables WHERE variable LIKE"
                               " 'application-updated-to-version-%'")
                        && query.next() && query.value(0).toInt() == 0,
                    "creation is not a legacy upgrade");
            query.finish();
            require(Database::transaction(), "begin fixture save");
            const QVector<QPair<QString, int>> documents = { { "structure", 1 },
                                                             { "script", 10104 },
                                                             { "comic", 10203 },
                                                             { "unchanged", 10104 },
                                                             { "unknown", 99999 } };
            for (const auto& [name, type] : documents) {
                query.prepare("INSERT INTO documents (uuid, type, content) VALUES (?, ?, ?)");
                query.addBindValue(name);
                query.addBindValue(type);
                query.addBindValue(read(root + '/' + name + ".bin"));
                require(query.exec(), "save exact document fixture");
            }
            require(query.exec("UPDATE documents SET synced_at = '2020-01-02T03:04:05'"
                               " WHERE uuid = 'script'"),
                    "retain existing synchronization metadata");
            query.prepare("INSERT INTO documents_changes (fk_document_uuid, uuid, undo_patch,"
                          " redo_patch, date_time, user_name, user_email, is_synced)"
                          " VALUES ('script', 'change', ?, ?, '2020-01-02T03:04:05',"
                          " 'Folder Writer', 'fake@example.invalid', 1)");
            query.addBindValue(QByteArray("undo folder\0\r\n", 14));
            query.addBindValue(QByteArray("redo folders\0\r\n", 15));
            require(query.exec() && Database::commit(), "commit fixture documents and history");
        }
    }
    Database::closeCurrentFile();
    std::puts("PASS: production database phase completed with ordinary exit");
    return 0;
}
