#include "database.h"

#include <QApplication>
#include <QDateTime>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QVariant>
#include <QXmlStreamReader>

#include <vector>

namespace DatabaseLayer {

namespace {
/**
 * @brief Название соединения с базой данных
 */
static QString s_connectionName = "local_database";

/**
 * @brief Плагин используемый для работы с базой
 */
static QString s_sqlDriver = "QSQLITE";

/**
 * @brief Имя файла базы данных, если не задан, то база будет размещаться во временной памяти
 */
static QString s_databaseName = ":memory:";

/**
 * @brief Текст ошибки открытия последнего загружаемого файла
 */
static QString s_openFileError;

/**
 * @brief Текст последней ошибки
 */
static QString s_lastError;

/**
 * @brief Счётчик открытых транзакций
 */
static int s_openedTransactions = 0;
static bool s_rollbackRequired = false;
static std::vector<std::function<void(bool)>> s_transactionCallbacks;

void finishTransaction(bool _committed)
{
    s_openedTransactions = 0;
    s_rollbackRequired = false;
    auto callbacks = std::move(s_transactionCallbacks);
    s_transactionCallbacks.clear();
    if (_committed) {
        for (const auto& callback : callbacks) {
            callback(true);
        }
    } else {
        for (auto callback = callbacks.rbegin(); callback != callbacks.rend(); ++callback) {
            (*callback)(false);
        }
    }
}

/**
 * @brief Получить ключ хранения номера версии приложения
 */
static QString applicationVersionKey()
{
    return "application-version";
}

QString unversionedDatabaseVersion(const QSqlDatabase& _database)
{
    // These columns were added by the last two schema migrations. New files from
    // earlier builds already have them, even though creation omitted the version.
    if (_database.record("documents_changes").contains("is_synced")) {
        return _database.record("documents").contains("synced_at") ? "0.6.2" : "0.2.4";
    }
    return "0.0.1";
}

bool renameXmlElements(QByteArray& _content, const QHash<QString, QString>& _names)
{
    if (_content.isEmpty()) {
        return true;
    }
    const auto bom
        = _content.startsWith("\xef\xbb\xbf") ? QByteArray("\xef\xbb\xbf") : QByteArray();
    const auto original = _content.mid(bom.size());
    const auto xml = QString::fromUtf8(original);
    if (xml.toUtf8() != original) {
        Database::setLastError("Cannot migrate a document that is not UTF-8 XML.");
        return false;
    }

    struct Rename {
        qsizetype start;
        qsizetype length;
        QString name;
    };
    QVector<Rename> renames;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        const auto previousOffset = reader.characterOffset();
        reader.readNext();
        if (!reader.isStartElement() && !reader.isEndElement()) {
            continue;
        }
        // An empty element has a synthetic EndElement at the same source offset.
        if (reader.isEndElement() && reader.characterOffset() == previousOffset) {
            continue;
        }
        const auto name = reader.qualifiedName().toString();
        const auto replacement = _names.constFind(name);
        if (replacement == _names.cend()) {
            continue;
        }
        const auto tokenEnd = reader.characterOffset();
        const auto tagStart = xml.lastIndexOf('<', tokenEnd - 1);
        const auto nameStart = tagStart + (reader.isEndElement() ? 2 : 1);
        // Elements expanded from an entity have no literal tag at this offset.
        // Refuse them rather than editing a preceding tag or the entity declaration.
        if (tokenEnd <= 0 || tokenEnd > xml.size() || xml.at(tokenEnd - 1) != '>' || tagStart < 0
            || xml.mid(nameStart, name.size()) != name
            || (!renames.isEmpty() && nameStart <= renames.last().start)) {
            Database::setLastError("Cannot locate an XML element for migration.");
            return false;
        }
        renames.append({ nameStart, name.size(), replacement.value() });
    }
    if (reader.hasError()) {
        Database::setLastError(reader.errorString());
        return false;
    }
    // Keep the original representation (CDATA, whitespace, attributes and comments).
    auto result = xml;
    for (auto rename = renames.crbegin(); rename != renames.crend(); ++rename) {
        result.replace(rename->start, rename->length, rename->name);
    }
    _content = bom + result.toUtf8();
    return true;
}
} // namespace

bool Database::canOpenFile(const QString& _databaseFileName)
{
    bool canOpen = true;

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(s_sqlDriver, "tmp_database");
        database.setDatabaseName(_databaseFileName);
        database.open();

        QSqlQuery q_checker(database);

        //
        // 1. Если файл был создан в более поздней версии приложения, его нельзя открывать
        //
        if (q_checker.exec(
                "SELECT value FROM system_variables WHERE variable = 'application-version' ")
            && q_checker.next()
            && q_checker.value("value").toString().split(" ").first()
                > QApplication::applicationVersion()) {
            canOpen = false;
            s_openFileError = QApplication::translate("DatabaseLayer::Database",
                                                      "Project was modified in a newer version. "
                                                      "Update to the latest version to open it.");
        }
    }

    QSqlDatabase::removeDatabase("tmp_database");

    return canOpen;
}

QString Database::openFileError()
{
    return s_openFileError;
}

bool Database::hasError()
{
    return s_rollbackRequired || !s_lastError.isEmpty();
}

QString Database::lastError()
{
    return s_lastError;
}

void Database::setLastError(const QString& _error)
{
    if (s_openedTransactions > 0 && !_error.isEmpty()) {
        s_rollbackRequired = true;
    }
    if (s_lastError != _error) {
        s_lastError = _error;
    }
}

void Database::clearLastError()
{
    // Clearing a message cannot make an aborted outer transaction writable again.
    if (s_rollbackRequired) {
        return;
    }
    s_lastError.clear();
}

void Database::setCurrentFile(const QString& _databaseFileName)
{
    //
    // Если использовалась база данных, то удалим старое соединение
    //
    closeCurrentFile();

    //
    // Установим текущее имя базы данных
    //
    if (s_databaseName != _databaseFileName) {
        s_databaseName = _databaseFileName;
        s_connectionName = "local_database [" + s_databaseName + "]";
    }

    //
    // Откроем базу данных, или создадим новую
    //
    instance();
}

void Database::closeCurrentFile()
{
    abortTransaction();
    if (QSqlDatabase::contains(s_connectionName)) {
        QSqlDatabase::removeDatabase(s_connectionName);
    }
    s_openedTransactions = 0;
}

QString Database::currentFile()
{
    return instance().databaseName();
}

QSqlQuery Database::query()
{
    return QSqlQuery(instance());
}

bool Database::transaction()
{
    if (s_rollbackRequired) {
        return false;
    }
    //
    // Для первого запроса открываем транзакцию
    //
    if (s_openedTransactions == 0) {
        auto database = instance();
        if (!database.transaction()) {
            setLastError(database.lastError().text());
            return false;
        }
    }

    //
    // Увеличиваем счётчик открытых транзакций
    //
    ++s_openedTransactions;
    return true;
}

bool Database::commit()
{
    if (s_openedTransactions == 0) {
        return true;
    }
    //
    // Уменьшаем счётчик транзакций
    //
    if (s_openedTransactions > 1) {
        --s_openedTransactions;
        return !s_rollbackRequired;
    }

    //
    // При закрытии корневой транзакции фиксируем изменения в базе данных
    //
    auto database = instance();
    if (s_rollbackRequired) {
        abortTransaction();
        return false;
    }
    if (!database.commit()) {
        setLastError(database.lastError().text());
        // A failed COMMIT may leave an active transaction or SQLite may have already
        // rolled it back. Explicit rollback gives both cases the same recoverable state.
        abortTransaction();
        return false;
    }
    finishTransaction(true);
    return true;
}

bool Database::isTransactionActive()
{
    return s_openedTransactions > 0;
}

void Database::onTransactionFinished(std::function<void(bool)> _callback)
{
    if (isTransactionActive()) {
        s_transactionCallbacks.push_back(std::move(_callback));
    } else {
        _callback(true);
    }
}

void Database::abortTransaction()
{
    if (!isTransactionActive()) {
        return;
    }
    // Do not overwrite the original write/COMMIT error when SQLite already aborted.
    instance().rollback();
    finishTransaction(false);
}

void Database::rollback()
{
    if (!isTransactionActive()) {
        return;
    }
    s_rollbackRequired = true;
    //
    // Уменьшаем счётчик транзакций
    //
    --s_openedTransactions;

    //
    // При закрытии корневой транзакции отменяем изменения в базе данных
    //
    if (s_openedTransactions == 0) {
        instance().rollback();
        finishTransaction(false);
    }
}

void Database::vacuum()
{
    auto query = Database::query();
    query.exec("VACUUM");
}

// ****

QSqlDatabase Database::instance()
{
    QSqlDatabase database;

    if (!QSqlDatabase::contains(s_connectionName)) {
        open(database, s_connectionName, s_databaseName);
    } else {
        database = QSqlDatabase::database(s_connectionName);
    }

    return database;
}

void Database::open(QSqlDatabase& _database, const QString& _connectionName,
                    const QString& _databaseName)
{
    s_lastError.clear();

    _database = QSqlDatabase::addDatabase(s_sqlDriver, _connectionName);
    _database.setDatabaseName(_databaseName);
    _database.open();

    Database::States states = checkState(_database);

    if (!states.testFlag(SchemeFlag))
        createTables(_database);
    if (hasError())
        return;
    if (!states.testFlag(IndexesFlag))
        createIndexes(_database);
    if (!states.testFlag(EnumsFlag))
        createEnums(_database);
    if (states.testFlag(OldVersionFlag))
        updateDatabase(_database);
    if (hasError())
        return;

    validateDatabase(_database);
}

// Проверка состояния базы данных
// например:
// - БД отсутствует
// - БД пуста
// - БД имеет старую версию
// - БД имеет последнюю версию
// - и т.д.
Database::States Database::checkState(QSqlDatabase& _database)
{
    QSqlQuery q_checker(_database);
    Database::States states = Database::EmptyFlag;

    //
    // Созданы ли таблицы
    //
    if (q_checker.exec("SELECT COUNT(*) as size FROM sqlite_master WHERE type = 'table' ")
        && q_checker.next() && q_checker.record().value("size").toInt()) {
        //
        // Все остальные проверки имеют смысл, только если проходит данная проверка
        //
        states = states | Database::SchemeFlag;

        //
        // Созданы ли индексы
        //
        if (q_checker.exec("SELECT COUNT(*) as size FROM sqlite_master WHERE type = 'index' ")
            && q_checker.next() && q_checker.record().value("size").toInt()) {
            states = states | Database::IndexesFlag;
        }

        //
        // Проверка версии
        //
        if (q_checker.exec(
                QString("SELECT value as version FROM system_variables WHERE variable = '%1' ")
                    .arg(applicationVersionKey()))) {
            //
            // Если версия не задана, или она не соответствует текущей, то надо обновить файл
            //
            if (!q_checker.next()
                || q_checker.record().value("version").toString()
                    != QApplication::applicationVersion()) {
                states = states | Database::OldVersionFlag;
            }
        }
    }

    return states;
}

void Database::createTables(QSqlDatabase& _database)
{
    QSqlQuery query(_database);
    if (!_database.transaction()) {
        setLastError(_database.lastError().text());
        return;
    }

    //
    // Таблица системных переменных
    //
    if (!query.exec("CREATE TABLE system_variables "
                    "("
                    "variable TEXT PRIMARY KEY ON CONFLICT REPLACE, "
                    "value TEXT NOT NULL "
                    ");")) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }

    //
    // Таблица с документами
    //
    if (!query.exec("CREATE TABLE documents "
                    "("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "uuid TEXT UNIQUE NOT NULL, "
                    "type INTEGER NOT NULL DEFAULT(0), "
                    "content BLOB DEFAULT(NULL), "
                    "synced_at TEXT DEFAULT(NULL) "
                    ")")) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }

    //
    // Таблица с изменениями документов
    //
    if (!query.exec("CREATE TABLE documents_changes "
                    "("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "fk_document_uuid TEXT NOT NULL, "
                    "uuid TEXT UNIQUE NOT NULL, "
                    "undo_patch BLOB NOT NULL, " // отмена изменения
                    "redo_patch BLOB NOT NULL, " // повтор изменения
                    "date_time TEXT NOT NULL, " // yyyy.mm.dd.hh.mm.ss.zzz
                    "user_name TEXT NOT NULL, "
                    "user_email TEXT DEFAULT(NULL), "
                    "is_synced INTEGER NOT NULL DEFAULT(0) "
                    ")")) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }

    // A new database already uses the current schema and must never run legacy
    // content migrations on its first reopening.
    query.prepare("INSERT INTO system_variables (variable, value) VALUES (?, ?)");
    query.addBindValue(applicationVersionKey());
    query.addBindValue(QApplication::applicationVersion());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }
    if (!_database.commit()) {
        setLastError(_database.lastError().text());
        _database.rollback();
    }
}

void Database::createIndexes(QSqlDatabase& _database)
{
    QSqlQuery query(_database);
    _database.transaction();

    //
    // Таблица с изменениями документов
    //
    query.exec("CREATE INDEX documents_changes_fk_document_uuid_idx "
               "ON documents_changes (fk_document_uuid)");
    query.exec("CREATE INDEX documents_changes_date_time_idx "
               "ON documents_changes (date_time)");

    _database.commit();
}

void Database::createEnums(QSqlDatabase& _database)
{
    Q_UNUSED(_database)
}

void Database::updateDatabase(QSqlDatabase& _database)
{
    QSqlQuery query(_database);

    //
    // Определим версию базы данных
    //
    query.prepare("SELECT value as version FROM system_variables WHERE variable = ? ");
    query.addBindValue(applicationVersionKey());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return;
    }
    auto databaseVersion = query.next() ? query.value(0).toString() : QString();
    if (databaseVersion.isEmpty()) {
        databaseVersion = unversionedDatabaseVersion(_database);
    }
    query.finish();
    //
    // Извлекаем информацию о dev-версии
    //
    bool isDevVersion = false;
    if (const auto devIndex = databaseVersion.indexOf(" dev "); devIndex > 0) {
        isDevVersion = true;
        databaseVersion = databaseVersion.left(devIndex);
    }
    const QStringList& versionParts = databaseVersion.split(".");
    int versionMajor = versionParts.value(0, "0").toInt();
    int versionMinor = versionParts.value(1, "0").toInt();
    int versionBuild = versionParts.value(2, "1").toInt();
    //
    // Корректируем номер версии если это dev-сборка
    //
    if (isDevVersion) {
        //
        // x.x.3 dev -> x.x.2
        //
        if (versionBuild > 0) {
            --versionBuild;
        }
        //
        // x.x.0 dev ->
        //
        else {
            versionBuild = 999;
            //
            // x.3.0 dev -> x.2.999
            //
            if (versionMinor > 0) {
                --versionMinor;
            }
            //
            // x.0.0 dev ->
            //
            else {
                versionMinor = 999;
                //
                // 1.0.0 dev -> 0.999.999
                //
                if (versionMajor > 0) {
                    --versionMajor;
                }
                //
                // 0.0.0 dev - такого не может быть!
                //
                else {
                    Q_ASSERT(false);
                }
            }
        }
    }

    if (!_database.transaction()) {
        setLastError(_database.lastError().text());
        return;
    }

    //
    // Вызываются необходимые процедуры обновления БД в зависимости от её версии
    //
    // 0.X.X
    //
    if (versionMajor <= 0) {
        //
        // 0.0.X
        //
        if (versionMajor < 0 || versionMinor <= 0) {
            //
            // 0.0.9
            //
            if (versionMajor < 0 || versionMinor < 0 || versionBuild <= 9) {
                updateDatabaseTo_0_0_10(_database);
                if (hasError()) {
                    _database.rollback();
                    return;
                }
            }
        }
        //
        // 0.1.X
        //
        if (versionMajor < 0 || versionMinor <= 1) {
            //
            // 0.1.2
            //
            if (versionMajor < 0 || versionMinor < 1 || versionBuild <= 2) {
                updateDatabaseTo_0_1_3(_database);
                if (hasError()) {
                    _database.rollback();
                    return;
                }
            }
        }
        //
        // 0.2.x
        //
        if (versionMajor < 0 || versionMinor <= 2) {
            //
            // 0.2.3
            //
            if (versionMajor < 0 || versionMinor < 2 || versionBuild <= 3) {
                updateDatabaseTo_0_2_4(_database);
                if (hasError()) {
                    _database.rollback();
                    return;
                }
            }
        }
        //
        // 0.6.x
        //
        if (versionMajor < 0 || versionMinor <= 6) {
            //
            // 0.6.1
            //
            if (versionMajor < 0 || versionMinor < 6 || versionBuild <= 1) {
                updateDatabaseTo_0_6_2(_database);
                if (hasError()) {
                    _database.rollback();
                    return;
                }
            }
        }
    }

    //
    // Сохраняем информацию об обновлении версии
    //
    query.prepare("INSERT INTO system_variables (variable, value) VALUES (?, ?)");
    query.addBindValue("application-updated-to-version-" + QApplication::applicationVersion());
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }

    //
    // Обновляется версия программы
    //
    query.prepare("INSERT INTO system_variables (variable, value) VALUES (?, ?)");
    query.addBindValue(applicationVersionKey());
    query.addBindValue(QApplication::applicationVersion());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        _database.rollback();
        return;
    }
    if (!_database.commit()) {
        setLastError(_database.lastError().text());
        _database.rollback();
    }
}

void Database::updateDatabaseTo_0_0_10(QSqlDatabase& _database)
{
    //
    // В структуре хранились кривые цвета элементов проекта, поэтому удаляем их
    //

    QSqlQuery q_updater(_database);

    {
        //
        // Извлекаем структуру проекта
        //
        if (!q_updater.exec("SELECT content FROM documents WHERE type = 1")) {
            setLastError(q_updater.lastError().text());
            return;
        }
        QString structure;
        if (q_updater.next()) {
            structure = q_updater.record().value("content").toString();
        }
        //
        // ... удаляем цвета
        //
        structure = structure.remove("#000000");
        //
        // ... обновим данные
        //
        q_updater.prepare("UPDATE documents SET content = ? WHERE type = 1");
        q_updater.addBindValue(structure);
        if (!q_updater.exec()) {
            setLastError(q_updater.lastError().text());
        }
    }
}

void Database::updateDatabaseTo_0_1_3(QSqlDatabase& _database)
{
    QSqlQuery q_updater(_database);

    //
    // Изменить в комиксах блоки page_name & panel_name на page_heading & panel_heading
    //

    {
        //
        // Извлекаем документы комиксов
        //
        std::map<QString, QByteArray> comicBooks;
        if (!q_updater.exec("SELECT id, content FROM documents WHERE type = 10203")) {
            setLastError(q_updater.lastError().text());
            return;
        }
        while (q_updater.next()) {
            const auto original = q_updater.value("content").toByteArray();
            auto content = original;
            if (!renameXmlElements(
                    content,
                    { { "page_name", "page_heading" }, { "panel_name", "panel_heading" } })) {
                return;
            }
            if (content != original) {
                comicBooks[q_updater.value("id").toString()] = content;
            }
        }

        //
        // Обновим данные
        //
        for (const auto& [id, content] : comicBooks) {
            q_updater.prepare("UPDATE documents SET content = ? WHERE id = ?");
            q_updater.addBindValue(content);
            q_updater.addBindValue(id);
            if (!q_updater.exec()) {
                setLastError(q_updater.lastError().text());
                return;
            }
        }
    }

    //
    // Изменить в сценариях элементы
    // folder_header -> sequence_heading
    // folder_footer -> sequence_footer
    // folder -> sequence
    //

    {
        //
        // Извлекаем документы сценариев
        //
        std::map<QString, QByteArray> screenplays;
        if (!q_updater.exec("SELECT id, content FROM documents WHERE type = 10104")) {
            setLastError(q_updater.lastError().text());
            return;
        }
        while (q_updater.next()) {
            const auto original = q_updater.value("content").toByteArray();
            auto content = original;
            if (!renameXmlElements(content,
                                   { { "folder_header", "sequence_heading" },
                                     { "folder_footer", "sequence_footer" },
                                     { "folder", "sequence" } })) {
                return;
            }
            if (content != original) {
                screenplays[q_updater.value("id").toString()] = content;
            }
        }

        //
        // Обновим данные
        //
        for (const auto& [id, content] : screenplays) {
            q_updater.prepare("UPDATE documents SET content = ? WHERE id = ?");
            q_updater.addBindValue(content);
            q_updater.addBindValue(id);
            if (!q_updater.exec()) {
                setLastError(q_updater.lastError().text());
                return;
            }
        }
    }
}

void Database::updateDatabaseTo_0_2_4(QSqlDatabase& _database)
{
    QSqlQuery q_updater(_database);

    if (!q_updater.exec(
            "ALTER TABLE documents_changes ADD is_synced INTEGER NOT NULL DEFAULT(0)")) {
        setLastError(q_updater.lastError().text());
    }
}

void Database::updateDatabaseTo_0_6_2(QSqlDatabase& _database)
{
    QSqlQuery q_updater(_database);

    if (!q_updater.exec("ALTER TABLE documents ADD synced_at TEXT DEFAULT(NULL)")) {
        setLastError(q_updater.lastError().text());
    }
}

void Database::validateDatabase(QSqlDatabase& _database)
{
    //
    // Валидируем базу, чтобы документы заданных типов были в единственном экземпляре
    // NOTE: После некоторых проблемных ситуаций такое могло происходить, например после
    //       восстановления работы облачного сервиса с проектами, с которыми работали до утраты
    //       данных и в то же время после утраты
    // TODO: Выпилить в версии 1.0
    //
    // Retired Worlds records and their change history must be preserved, including duplicates.
    const QVector<int> singletonDocumentTypes = {
        1, // Domain::DocumentObjectType::Structure
        10001, // Domain::DocumentObjectType::RecycleBin
        10000, // Domain::DocumentObjectType::Project
        10105, // Domain::DocumentObjectType::ScreenplayDictionaries
        30000, // Domain::DocumentObjectType::Characters
        40000, // Domain::DocumentObjectType::Locations
    };

    _database.transaction();
    for (const auto& documentType : singletonDocumentTypes) {
        QSqlQuery documentsQuery(_database);
        documentsQuery.prepare(
            "SELECT documents.id, documents.uuid "
            "FROM documents "
            "LEFT JOIN documents_changes "
            "ON documents_changes.fk_document_uuid = documents.uuid "
            "WHERE documents.type = ? "
            "GROUP BY documents.id "
            "ORDER BY COALESCE(MAX(documents_changes.date_time), documents.synced_at, '') DESC, "
            "documents.id DESC");
        documentsQuery.addBindValue(static_cast<int>(documentType));
        if (!documentsQuery.exec()) {
            _database.rollback();
            setLastError(documentsQuery.lastError().text());
            return;
        }

        QVector<QPair<QVariant, QVariant>> duplicateDocuments;
        bool keepDocument = true;
        while (documentsQuery.next()) {
            if (keepDocument) {
                keepDocument = false;
                continue;
            }

            duplicateDocuments.append({ documentsQuery.value("id"), documentsQuery.value("uuid") });
        }
        documentsQuery.finish();

        for (const auto& [documentId, documentUuid] : std::as_const(duplicateDocuments)) {
            QSqlQuery removeChangesQuery(_database);
            removeChangesQuery.prepare("DELETE FROM documents_changes WHERE fk_document_uuid = ?");
            removeChangesQuery.addBindValue(documentUuid);
            if (!removeChangesQuery.exec()) {
                _database.rollback();
                setLastError(removeChangesQuery.lastError().text());
                return;
            }

            QSqlQuery removeDocumentQuery(_database);
            removeDocumentQuery.prepare("DELETE FROM documents WHERE id = ?");
            removeDocumentQuery.addBindValue(documentId);
            if (!removeDocumentQuery.exec()) {
                _database.rollback();
                setLastError(removeDocumentQuery.lastError().text());
                return;
            }
        }
    }
    _database.commit();
}

} // namespace DatabaseLayer
