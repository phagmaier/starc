#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/starcloud_api.h>
#include <management_layer/content/writing_session/writing_session_storage.h>

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

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
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    application.setApplicationName("Writing session storage regression");
    application.setOrganizationName("Fork checks");
    require(QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
            "create isolated data directory");
    require(DataStorageLayer::StorageFacade::settingsStorage()->accountEmail().isNull(),
            "fresh profile has no account email");

    DataStorageLayer::WritingSessionStorage storage;
    Domain::SessionStatistics session;
    session.uuid = QUuid::createUuid();
    session.projectUuid = QUuid::createUuid();
    session.projectName = QStringLiteral("Disposable screenplay");
    session.deviceUuid = QUuid::createUuid().toString();
    session.deviceName = QStringLiteral("Test device");
    session.startDateTime = QDateTime::fromSecsSinceEpoch(1700000000).toUTC();
    session.endDateTime = session.startDateTime.addSecs(120);
    session.words = 12;
    session.characters = 60;

    storage.saveSessionStatistics({ session });
    const auto anonymousUuid = session.uuid;
    const auto saved = storage.sessionStatistics();
    require(saved.size() == 1, "unset account email still saves a session");
    require(saved.first().uuid == anonymousUuid && saved.first().projectUuid == session.projectUuid
                && saved.first().projectName == session.projectName
                && saved.first().deviceUuid == session.deviceUuid
                && saved.first().startDateTime == session.startDateTime
                && saved.first().endDateTime == session.endDateTime && saved.first().words == 12
                && saved.first().characters == 60,
            "anonymous session data survives save/read");

    auto database = QSqlDatabase::database("session_statistics_database");
    {
        QSqlQuery query(database);
        require(query.exec("SELECT account_email, account_email IS NULL FROM sessions")
                    && query.next(),
                "read stored anonymous account label");
        require(query.value(0).toString().isEmpty() && !query.value(1).toBool(),
                "anonymous label is empty text, not SQL NULL");
    }

    setSettingsValue(DataStorageLayer::kAccountEmailKey, QStringLiteral(""));
    session.uuid = QUuid::createUuid();
    storage.saveSessionStatistics({ session });
    require(storage.sessionStatistics().size() == 2, "explicit empty email also saves");

    setSettingsValue(DataStorageLayer::kAccountEmailKey, QStringLiteral("writer@example.invalid"));
    session.uuid = QUuid::createUuid();
    storage.saveSessionStatistics({ session });
    require(storage.sessionStatistics().size() == 3,
            "current account sees its session and anonymous history");
    setSettingsValue(DataStorageLayer::kAccountEmailKey, QStringLiteral("second@example.invalid"));
    require(storage.sessionStatistics().size() == 2,
            "another account sees anonymous history without the first account's "
            "session");

    database.close();
    require(database.open(), "reopen database");
    require(storage.sessionStatistics().size() == 2, "anonymous history survives reopening");
    setSettingsValue(DataStorageLayer::kAccountEmailKey, QStringLiteral("writer@example.invalid"));
    const auto reopened = storage.sessionStatistics();
    require(reopened.size() == 3, "all prior rows retained after reopening");
    bool originalFound = false;
    for (const auto& record : reopened) {
        if (record.uuid == anonymousUuid) {
            originalFound = record.words == 12 && record.characters == 60
                && record.deviceUuid == session.deviceUuid;
        }
    }
    require(originalFound, "original anonymous session remains intact");
    std::puts("PASS: unset/empty email, session data, account filtering, "
              "retained rows, reopen");
    return 0;
}
