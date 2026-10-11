#include "project_sync.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QUuid>
#include <QVariant>


namespace ProjectSync {

State state(const Snapshot& _snapshot)
{
    if (_snapshot.remoteSha.isEmpty()) {
        return State::LocalOnly;
    }
    if (_snapshot.localFingerprint.isEmpty()) {
        return State::RemoteOnly;
    }

    //
    // Copies which were never synced with each other can be compared only by their content
    //
    if (_snapshot.baseFingerprint.isEmpty() || _snapshot.baseSha.isEmpty()) {
        return State::Diverged;
    }

    const auto isLocalChanged = _snapshot.localFingerprint != _snapshot.baseFingerprint;
    const auto isRemoteChanged = _snapshot.remoteSha != _snapshot.baseSha;
    if (isLocalChanged && isRemoteChanged) {
        return State::Diverged;
    }
    if (isLocalChanged) {
        return State::LocalChanged;
    }
    if (isRemoteChanged) {
        return State::RemoteChanged;
    }
    return State::InSync;
}

State resolveDiverged(const Snapshot& _snapshot, const QByteArray& _remoteFingerprint)
{
    //
    // The same changes were made in both copies
    //
    if (_remoteFingerprint == _snapshot.localFingerprint) {
        return State::InSync;
    }

    //
    // The remote file was rewritten, but its documents are the same as at the last sync
    //
    if (!_snapshot.baseFingerprint.isEmpty() && _remoteFingerprint == _snapshot.baseFingerprint) {
        return State::LocalChanged;
    }

    return State::Diverged;
}

QByteArray gitBlobSha(const QByteArray& _content)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray("blob ") + QByteArray::number(_content.size()) + '\0');
    hash.addData(_content);
    return hash.result().toHex();
}

Fingerprint fingerprint(const QString& _projectFilePath)
{
    Fingerprint result;

    if (!QFileInfo::exists(_projectFilePath)) {
        result.error = QStringLiteral("file doesn't exist");
        return result;
    }

    //
    // A journal left near the file means that the last write to the project wasn't finished,
    // such file is restored by the application when the project is opened
    //
    if (QFileInfo::exists(_projectFilePath + QLatin1String("-journal"))) {
        result.error = QStringLiteral("project has unfinished changes");
        return result;
    }

    const auto connectionName
        = QLatin1String("project_sync_") + QUuid::createUuid().toString(QUuid::Id128);
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(_projectFilePath);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!database.open()) {
            result.error = database.lastError().text();
        } else {
            QSqlQuery query(database);
            query.setForwardOnly(true);
            if (!query.exec(QStringLiteral("PRAGMA quick_check")) || !query.next()
                || query.value(0).toString() != QLatin1String("ok")) {
                result.error = QStringLiteral("file is damaged or isn't a project");
            } else if (!query.exec(QStringLiteral(
                           "SELECT uuid, type, content FROM documents ORDER BY uuid"))) {
                result.error = query.lastError().text();
            } else {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                hash.addData(QByteArrayLiteral("starc-project-fingerprint-v1\n"));
                while (query.next()) {
                    //
                    // Rows of the documents which have no content yet are created just by
                    // navigating through the project
                    //
                    const auto content = query.value(2).toByteArray();
                    if (content.isEmpty()) {
                        continue;
                    }

                    hash.addData(query.value(0).toString().toUtf8());
                    hash.addData(QByteArrayLiteral("\n"));
                    hash.addData(QByteArray::number(query.value(1).toInt()));
                    hash.addData(QByteArrayLiteral("\n"));
                    hash.addData(QByteArray::number(content.size()));
                    hash.addData(QByteArrayLiteral("\n"));
                    hash.addData(content);
                    hash.addData(QByteArrayLiteral("\n"));
                }
                if (query.lastError().isValid()) {
                    result.error = query.lastError().text();
                } else {
                    result.value = hash.result().toHex();
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return result;
}

Fingerprint fingerprint(const QByteArray& _projectFileContent)
{
    QTemporaryFile file;
    if (!file.open() || file.write(_projectFileContent) != _projectFileContent.size()
        || !file.flush()) {
        Fingerprint result;
        result.error = file.errorString();
        return result;
    }

    return fingerprint(file.fileName());
}

} // namespace ProjectSync
