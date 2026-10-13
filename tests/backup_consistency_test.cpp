#include <sys/resource.h>
#include <utils/tools/backup_builder.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {
struct ReadGate {
    std::mutex mutex;
    std::condition_variable condition;
    bool paused = false;
    bool released = false;
    bool timedOut = false;
};
thread_local ReadGate* backupGate = nullptr;

void require(bool _ok, const char* _label)
{
    std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
    if (!_ok) {
        std::exit(1);
    }
}

QByteArray bytes(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "open byte oracle");
    return file.readAll();
}

QMap<QString, QByteArray> files(const QString& _directory)
{
    QMap<QString, QByteArray> result;
    for (const auto& file : QDir(_directory).entryInfoList(QDir::Files | QDir::Hidden)) {
        result.insert(file.fileName(), bytes(file.absoluteFilePath()));
    }
    return result;
}

void execute(QSqlDatabase& _database, const QString& _sql)
{
    QSqlQuery query(_database);
    if (!query.exec(_sql)) {
        std::fprintf(stderr, "%s: %s\n", qPrintable(_sql), qPrintable(query.lastError().text()));
        require(false, "fixture SQL succeeds");
    }
}

void writeGeneration(QSqlDatabase& _database, char _generation)
{
    QSqlQuery query(_database);
    query.prepare("UPDATE documents SET content=? WHERE id=17");
    query.addBindValue(QByteArray(256 * 1024, _generation));
    require(query.exec(), "write complete screenplay generation");
    query.prepare("UPDATE documents SET content=? WHERE id=42");
    query.addBindValue(QByteArray("annotation\0\r\n", 13) + _generation);
    require(query.exec(), "write annotation in the same transaction");
    query.prepare("UPDATE documents_changes SET undo_patch=?, redo_patch=? WHERE id=31");
    query.addBindValue(QByteArray("undo-") + _generation);
    query.addBindValue(QByteArray("redo-") + _generation);
    require(query.exec(), "write history in the same transaction");
}

void createStory(QSqlDatabase& _database)
{
    execute(_database,
            "CREATE TABLE documents (id INTEGER PRIMARY KEY, uuid TEXT UNIQUE, "
            "type INTEGER, content BLOB)");
    execute(_database,
            "CREATE TABLE documents_changes (id INTEGER PRIMARY KEY, "
            "fk_document_uuid TEXT, uuid TEXT, undo_patch BLOB, redo_patch BLOB)");
    execute(_database, "CREATE TABLE system_variables (variable TEXT PRIMARY KEY, value TEXT)");
    execute(_database, "CREATE TABLE unknown_legacy_data (id INTEGER PRIMARY KEY, payload BLOB)");
    execute(_database,
            "INSERT INTO documents VALUES (17,'script',10104,NULL), "
            "(42,'annotation',10106,NULL)");
    execute(_database, "INSERT INTO documents_changes VALUES (31,'script','history',NULL,NULL)");
    execute(_database, "INSERT INTO system_variables VALUES ('fixture','preserved')");
    execute(_database, "INSERT INTO unknown_legacy_data VALUES (99,x'000d0aff')");
    require(_database.transaction(), "begin original generation");
    writeGeneration(_database, 'A');
    require(_database.commit(), "commit original generation");
}

void checkRestored(const QString& _path, char _generation)
{
    const auto name = QStringLiteral("restored_") + _generation;
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", name);
        database.setDatabaseName(_path);
        database.setConnectOptions("QSQLITE_OPEN_READONLY");
        require(database.open(), "reopen standalone recovery database");
        QSqlQuery query(database);
        require(query.exec("PRAGMA integrity_check") && query.next()
                    && query.value(0).toString() == "ok",
                "restored database passes structural integrity");
        require(query.exec("SELECT id,uuid,type,content FROM documents ORDER BY id")
                    && query.next(),
                "read recovered documents");
        require(query.value(0).toInt() == 17 && query.value(1).toString() == "script"
                    && query.value(2).toInt() == 10104
                    && query.value(3).toByteArray() == QByteArray(256 * 1024, _generation),
                "recovered screenplay is exactly one complete committed generation");
        require(query.next() && query.value(0).toInt() == 42
                    && query.value(1).toString() == "annotation" && query.value(2).toInt() == 10106
                    && query.value(3).toByteArray()
                        == QByteArray("annotation\0\r\n", 13) + _generation,
                "recovered annotation and identity belong to the same generation");
        require(query.exec("SELECT id,fk_document_uuid,uuid,undo_patch,redo_patch "
                           "FROM documents_changes")
                    && query.next() && query.value(0).toInt() == 31
                    && query.value(1).toString() == "script"
                    && query.value(2).toString() == "history"
                    && query.value(3).toByteArray() == QByteArray("undo-") + _generation
                    && query.value(4).toByteArray() == QByteArray("redo-") + _generation,
                "recovered history has exact bytes and identities");
        require(query.exec("SELECT payload FROM unknown_legacy_data WHERE id=99") && query.next()
                    && query.value(0).toByteArray() == QByteArray::fromHex("000d0aff"),
                "opaque legacy data survives recovery exactly");
    }
    QSqlDatabase::removeDatabase(name);
}

void checkRace(const QString& _root, const QString& _mode)
{
    const auto directory = _root + '/' + _mode;
    require(QDir().mkpath(directory), "create isolated concurrency case");
    const auto path = directory + "/source.starc";
    auto database = QSqlDatabase::addDatabase("QSQLITE", _mode);
    database.setDatabaseName(path);
    database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=0");
    require(database.open(), "open disposable writer");
    execute(database, "PRAGMA journal_mode=" + _mode);
    createStory(database);
    const auto before = bytes(path);
    ReadGate gate;
    BackupBuilder::BackupResult result;
    std::thread worker([&] {
        backupGate = &gate;
        result = BackupBuilder::saveAt(path, directory + "/backups", { }, 7,
                                       QDateTime(QDate(2026, 10, 13), QTime(12, 0)));
        backupGate = nullptr;
    });
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        require(
            gate.condition.wait_for(lock, std::chrono::seconds(10), [&] { return gate.paused; }),
            "pause production worker after its first 64 KiB source read");
    }
    // Check the operating-system lock too, independently of SQLite's in-process locks.
    QProcess outsider;
    outsider.start("python3",
                   { "-c", R"PY(
import sqlite3,sys
c=sqlite3.connect(sys.argv[1],timeout=0)
try:
    c.execute('BEGIN EXCLUSIVE')
except sqlite3.OperationalError as e:
    print('external lock refusal:',e)
    sys.exit(0 if e.sqlite_errorcode == sqlite3.SQLITE_BUSY else 2)
else:
    c.rollback()
    print('external writer acquired exclusive lock during backup')
    sys.exit(1)
)PY",
                     path });
    const bool outsiderFinished = outsider.waitForFinished(5000);
    const bool externallyLocked = outsiderFinished && outsider.exitStatus() == QProcess::NormalExit
        && outsider.exitCode() == 0;
    std::printf("%s", outsider.readAllStandardOutput().constData());
    require(database.transaction(), "begin second save while backup is paused");
    writeGeneration(database, 'B');
    const bool earlyCommit = database.commit();
    const auto commitError = database.lastError().text();
    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.released = true;
        gate.condition.notify_all();
    }
    worker.join();
    std::printf("mode=%s early_commit=%d external_blocked=%d backup_success=%d status=%s\n",
                qPrintable(_mode), earlyCommit, externallyLocked, result.success,
                qPrintable(result.status));
    if (!earlyCommit) {
        require(database.commit(), "second Save commits after source copying finishes");
    }
    require(!gate.timedOut && result.success, "backup completes without a gate timeout");
    const auto backup = result.status.section('"', 1, 1);
    const auto copied = bytes(backup);
    std::printf("source_before=%lld source_after=%lld backup=%lld A=%lld B=%lld\n",
                static_cast<long long>(before.size()), static_cast<long long>(bytes(path).size()),
                static_cast<long long>(copied.size()), static_cast<long long>(copied.count('A')),
                static_cast<long long>(copied.count('B')));
    require(externallyLocked, "another process cannot acquire a write lock during copying");
    std::printf("competing commit error: %s\n", qPrintable(commitError));
    require(!earlyCommit && commitError.contains("locked"),
            "second commit cannot change pages during copying");
    require(copied == before, "recovery copy matches original committed database byte for byte");
    const auto restored = directory + "/restored.starc";
    require(QFile::copy(backup, restored), "restore the first generation to a separate story");
    checkRestored(restored, 'A');
    const auto second = BackupBuilder::saveAt(path, directory + "/backups", { }, 7,
                                              QDateTime(QDate(2026, 10, 13), QTime(12, 0, 1)));
    require(second.success, "later backup succeeds after the competing Save");
    const auto secondPath = second.status.section('"', 1, 1);
    require(bytes(secondPath) == bytes(path), "later recovery copy matches new database exactly");
    checkRestored(secondPath, 'B');
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(_mode);
}

void checkRefusals(const QString& _root)
{
    const auto directory = _root + "/refusals";
    const auto path = directory + "/source.starc";
    const auto backups = directory + "/backups";
    require(QDir().mkpath(backups), "create failure case");
    auto database = QSqlDatabase::addDatabase("QSQLITE", "refusals");
    database.setDatabaseName(path);
    require(database.open(), "open failure-case database");
    createStory(database);
    for (int second = 0; second < 12; ++second) {
        const auto result = BackupBuilder::saveAt(
            path, backups, { }, 12, QDateTime(QDate(2026, 1, 1), QTime(0, 0, second)));
        require(result.success, "seed twelve old recovery copies");
    }
    const auto before = files(backups);
    const auto now = QDateTime(QDate(2026, 10, 13), QTime(12, 0));
    execute(database, "PRAGMA journal_mode=WAL");
    require(database.transaction(), "begin WAL generation");
    writeGeneration(database, 'B');
    require(database.commit(), "commit WAL-only generation");
    const auto result = BackupBuilder::saveAt(path, backups, { }, 7, now);
    std::printf("WAL refusal: %s\n", qPrintable(result.status));
    require(!result.success && result.status.contains("WAL"), "WAL backup is explicitly refused");
    require(files(backups) == before, "refusal cannot publish, prune or leak a temporary copy");
    execute(database, "PRAGMA journal_mode=DELETE");
    const auto sourceBeforeFailure = bytes(path);
    rlimit originalLimit;
    require(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0, "read process-local file size limit");
    auto shortLimit = originalLimit;
    shortLimit.rlim_cur = 32768;
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    require(setrlimit(RLIMIT_FSIZE, &shortLimit) == 0, "inject a genuine short staging write");
    const auto shortWrite = BackupBuilder::saveAt(path, backups, { }, 7, now);
    require(setrlimit(RLIMIT_FSIZE, &originalLimit) == 0, "restore file size limit");
    std::signal(SIGXFSZ, previousSignal);
    require(!shortWrite.success && shortWrite.status.contains("Can't complete backup"),
            "short SQLite staging write reports failure");
    require(files(backups) == before && bytes(path) == sourceBeforeFailure,
            "short write preserves source and every old backup without temporary files");
    require(database.transaction(), "begin Save after failed backup");
    writeGeneration(database, 'A');
    require(database.commit(), "failed backup releases its read lock for a subsequent Save");
    const auto recovered = BackupBuilder::saveAt(path, backups, { }, 7, now);
    require(recovered.success, "backup succeeds immediately after staging capacity is restored");
    require(files(backups).size() == 7, "only successful recovery publication permits pruning");
    checkRestored(recovered.status.section('"', 1, 1), 'A');
    const auto afterRetry = files(backups);
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase("refusals");
    QFile broken(path);
    require(broken.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "prepare malformed SQLite source");
    require(broken.write(QByteArray("SQLite format 3\0", 16) + QByteArray(4096, 'X')) == 4112,
            "write malformed SQLite fixture");
    broken.close();
    const auto corrupt = BackupBuilder::saveAt(path, backups, { }, 7, now);
    require(!corrupt.success && !corrupt.status.isEmpty(),
            "unreadable database refuses its snapshot");
    require(files(backups) == afterRetry,
            "failed read transaction preserves every old recovery copy");
}
} // namespace

// Fault injection only: pause the production copy, never alter its input bytes.
extern "C" ssize_t read(int _fd, void* _buffer, size_t _count)
{
    using Read = ssize_t (*)(int, void*, size_t);
    static const auto realRead = reinterpret_cast<Read>(dlsym(RTLD_NEXT, "read"));
    const auto length = realRead(_fd, _buffer, _count);
    if (backupGate && length >= 65536) {
        std::unique_lock<std::mutex> lock(backupGate->mutex);
        if (!backupGate->paused) {
            backupGate->paused = true;
            backupGate->condition.notify_all();
            backupGate->timedOut = !backupGate->condition.wait_for(
                lock, std::chrono::seconds(15), [&] { return backupGate->released; });
        }
    }
    return length;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    require(argc == 2, "disposable evidence directory supplied");
    const auto root = QString::fromLocal8Bit(argv[1]);
    for (const auto& mode : { "DELETE", "TRUNCATE", "PERSIST" }) {
        checkRace(root, QString::fromLatin1(mode));
    }
    checkRefusals(root);
    std::puts("PASS: complete committed backup generations and safe refusals");
    return 0;
}
