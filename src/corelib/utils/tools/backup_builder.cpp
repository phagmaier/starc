#include "backup_builder.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <limits>


namespace {
const QString kTimestampFormat = QStringLiteral("yyyy_MM_dd_hh_mm_ss");
constexpr int kRecentCopies = 7;
constexpr int kDailyDays = 30;

struct Snapshot {
    QString path;
    QString timestamp;
    QDate date;
    qint64 sequence = 0;
};

// Keep SQLite's shared read lock until the last source byte has been copied.
// The backup-family lock alone does not prevent a normal Save from committing.
class DatabaseReadLock
{
public:
    ~DatabaseReadLock()
    {
        if (m_connectionName.isEmpty()) {
            return;
        }
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QString acquire(const QString& _path)
    {
        m_connectionName = QStringLiteral("backup_") + QUuid::createUuid().toString(QUuid::Id128);
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_database.setDatabaseName(_path);
        m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!m_database.open() || !m_database.transaction()) {
            return m_database.lastError().text();
        }
        QSqlQuery query(m_database);
        // BEGIN is deferred: an actual read is required to acquire the shared lock.
        if (!query.exec(QStringLiteral("SELECT count(*) FROM sqlite_master")) || !query.next()) {
            return query.lastError().text();
        }
        query.finish();
        if (!query.exec(QStringLiteral("PRAGMA journal_mode")) || !query.next()) {
            return query.lastError().text();
        }
        // WAL readers do not freeze the main file, and committed pages may live only
        // in the WAL. Refuse rather than publish an incomplete recovery copy. The
        // application's own databases use rollback journaling.
        if (query.value(0).toString() == QLatin1String("wal")) {
            return QObject::tr("SQLite WAL mode can't be copied safely as a single file.",
                               "BackupBuilder");
        }
        return { };
    }

private:
    QString m_connectionName;
    QSqlDatabase m_database;
};
} // namespace

BackupBuilder::BackupResult BackupBuilder::save(const QString& _filePath, const QString& _backupDir,
                                                const QString& _newName, int _maximumBackups)
{
    return saveAt(_filePath, _backupDir, _newName, _maximumBackups, QDateTime::currentDateTime());
}

BackupBuilder::BackupResult BackupBuilder::saveAt(const QString& _filePath,
                                                  const QString& _backupDir,
                                                  const QString& _newName, int _maximumBackups,
                                                  const QDateTime& _now)
{
    auto failure = [](const QString& _status) { return BackupResult{ false, _status }; };
    if (!_now.isValid()) {
        return failure(QObject::tr("Can't create backup: invalid date.", "BackupBuilder"));
    }
    if (!QDir::root().mkpath(_backupDir)) {
        return failure(QObject::tr("Can't create backups folder \"%1\". Please check permissions.",
                                   "BackupBuilder")
                           .arg(_backupDir));
    }

    const QFileInfo sourceInfo(_filePath);
    const auto baseName = _newName.isEmpty() ? sourceInfo.completeBaseName() : _newName;
    if (baseName.contains('/') || baseName.contains('\\')) {
        return failure(QObject::tr("Can't create backup: invalid snapshot name.", "BackupBuilder"));
    }
    // Legacy names identify only a basename, which two different stories can share.
    // A full source-path digest lets us prune each story independently without guessing
    // ownership of old copies. Keep the existing directory and ordinary file suffix.
    const auto identity = QString::fromLatin1(
        QCryptographicHash::hash(QDir::cleanPath(sourceInfo.absoluteFilePath()).toUtf8(),
                                 QCryptographicHash::Sha256)
            .toHex());
    const auto prefix = baseName + '_' + identity + '_';
    const auto suffix
        = sourceInfo.completeSuffix().isEmpty() ? QString() : '.' + sourceInfo.completeSuffix();
    const QDir directory(_backupDir);
    QLockFile lock(directory.filePath(".backup-" + identity + ".lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        return failure(QObject::tr("Can't lock backups for \"%1\". Another backup may be running "
                                   "or the backups folder is not writable.",
                                   "BackupBuilder")
                           .arg(_filePath));
    }

    const auto now = _now.toLocalTime();
    const auto timestamp = now.toString(kTimestampFormat);
    const QRegularExpression pattern(
        "\\A" + QRegularExpression::escape(prefix)
        + "([0-9]{4}_[0-9]{2}_[0-9]{2}_[0-9]{2}_[0-9]{2}_[0-9]{2})(?:_([1-9][0-9]*))?"
        + QRegularExpression::escape(suffix) + "\\z");
    QVector<Snapshot> snapshots;
    qint64 sequence = 0;
    const auto files
        = directory.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto& file : files) {
        // Symlinks and iCloud placeholders are not complete local recovery snapshots.
        if (!file.isFile() || file.isSymLink()) {
            continue;
        }
        const auto match = pattern.match(file.fileName());
        if (!match.hasMatch()) {
            continue;
        }
        const auto stamp = match.captured(1);
        const auto date = QDate::fromString(stamp.left(10), "yyyy_MM_dd");
        const auto time = QTime::fromString(stamp.mid(11), "hh_mm_ss");
        bool validSequence = true;
        const auto ordinal
            = match.captured(2).isEmpty() ? 0 : match.captured(2).toLongLong(&validSequence);
        if (!date.isValid() || !time.isValid() || !validSequence) {
            continue;
        }
        snapshots.append({ file.absoluteFilePath(), stamp, date, ordinal });
        if (stamp == timestamp) {
            if (ordinal == std::numeric_limits<qint64>::max()) {
                return failure(QObject::tr("Can't create backup: snapshot sequence exhausted.",
                                           "BackupBuilder"));
            }
            sequence = std::max(sequence, ordinal + 1);
        }
    }

    // Write privately to a unique temporary file in the same directory. Failed reads,
    // writes, flushes or publication leave every existing recovery copy untouched.
    QFile source(_filePath);
    QTemporaryFile temporary(directory.filePath(".backup-XXXXXX.tmp"));
    if (!sourceInfo.isFile() || !source.open(QIODevice::ReadOnly | QIODevice::Unbuffered)
        || !temporary.open()) {
        return failure(
            QObject::tr("Can't copy project \"%1\" to backups folder \"%2\": %3", "BackupBuilder")
                .arg(_filePath, _backupDir,
                     source.isOpen() ? temporary.errorString() : source.errorString()));
    }
    const auto header = source.read(16);
    if (source.error() != QFileDevice::NoError || !source.seek(0)) {
        return failure(QObject::tr("Can't complete backup of \"%1\": %2", "BackupBuilder")
                           .arg(_filePath, source.errorString()));
    }
    // Read again from zero under the lock. Unbuffered I/O prevents pre-lock bytes
    // from being reused, and the descriptor stays open until copying is complete.
    DatabaseReadLock databaseLock;
    if (header == QByteArray("SQLite format 3\0", 16)) {
        const auto error = databaseLock.acquire(sourceInfo.absoluteFilePath());
        if (!error.isEmpty()) {
            return failure(QObject::tr("Can't complete backup of \"%1\": %2", "BackupBuilder")
                               .arg(_filePath, error));
        }
    }
    while (!source.atEnd()) {
        const auto bytes = source.read(64 * 1024);
        if (source.error() != QFileDevice::NoError || temporary.write(bytes) != bytes.size()) {
            return failure(QObject::tr("Can't complete backup of \"%1\": %2", "BackupBuilder")
                               .arg(_filePath,
                                    source.error() != QFileDevice::NoError
                                        ? source.errorString()
                                        : temporary.errorString()));
        }
    }
    // No more source reads occur after this close. On POSIX it can release the
    // operating-system shared lock, so never close/reopen the file during copying.
    source.close();
    if (!temporary.flush()) {
        return failure(QObject::tr("Can't flush backup of \"%1\": %2", "BackupBuilder")
                           .arg(_filePath, temporary.errorString()));
    }
    temporary.close();
    QString backupPath;
    for (;;) {
        const auto number = sequence == 0 ? QString() : '_' + QString::number(sequence);
        backupPath = directory.absoluteFilePath(prefix + timestamp + number + suffix);
        // Include broken symlinks and directories in collision detection. Never replace
        // another file, including an unclassified name. rename also refuses overwrite.
        const QFileInfo destination(backupPath);
        if (!destination.exists() && !destination.isSymLink()) {
            break;
        }
        if (sequence == std::numeric_limits<qint64>::max()) {
            return failure(
                QObject::tr("Can't create backup: snapshot sequence exhausted.", "BackupBuilder"));
        }
        ++sequence;
    }
    if (!temporary.rename(backupPath)) {
        return failure(QObject::tr("Can't publish backup \"%1\": %2", "BackupBuilder")
                           .arg(backupPath, temporary.errorString()));
    }
    temporary.setAutoRemove(false);
    snapshots.append({ backupPath, timestamp, now.date(), sequence });

    std::sort(snapshots.begin(), snapshots.end(), [](const Snapshot& _lhs, const Snapshot& _rhs) {
        return _lhs.timestamp == _rhs.timestamp ? _lhs.sequence > _rhs.sequence
                                                : _lhs.timestamp > _rhs.timestamp;
    });
    QSet<QString> retained;
    QSet<QDate> daily;
    const auto oldestDay = now.date().addDays(1 - kDailyDays);
    const auto recentCopies = std::max(kRecentCopies, _maximumBackups);
    for (int i = 0; i < snapshots.size(); ++i) {
        const auto& snapshot = snapshots.at(i);
        if (i < recentCopies) {
            retained.insert(snapshot.path);
        }
        if (snapshot.date >= oldestDay && snapshot.date <= now.date()
            && !daily.contains(snapshot.date)) {
            retained.insert(snapshot.path);
            daily.insert(snapshot.date);
        }
    }
    // Even when the clock moves backwards, the just-created copy is never pruned.
    retained.insert(backupPath);
    int removed = 0;
    for (const auto& snapshot : snapshots) {
        if (retained.contains(snapshot.path)) {
            continue;
        }
        if (!QFile::remove(snapshot.path)) {
            return failure(QObject::tr("Backup created at \"%1\", but can't remove old backup "
                                       "\"%2\". Please check permissions for the backups folder.",
                                       "BackupBuilder")
                               .arg(backupPath, snapshot.path));
        }
        ++removed;
    }
    return { true,
             QString("Backup created: path=\"%1\", matching backups=%2, retained=%3, removed=%4")
                 .arg(backupPath)
                 .arg(snapshots.size())
                 .arg(retained.size())
                 .arg(removed) };
}
