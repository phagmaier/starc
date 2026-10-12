#pragma once

#include <QDateTime>
#include <QString>

#include <corelib_global.h>


/**
 * @brief Класс для организации создания резервных копий
 */
namespace BackupBuilder {

/**
 * @brief Результат выполнения операции по созданию бекапа
 */
struct CORE_LIBRARY_EXPORT BackupResult {
    bool success = false;
    QString status;
};


/**
 * @brief Save a complete snapshot before pruning this source file's backup family.
 *
 * Keep at least seven recent copies (or _maximumBackups if greater), plus the newest
 * copy on each of the last 30 local calendar days, including today. The selections
 * overlap. Legacy names without a source-path identity and unrecognized files are
 * left alone. Moving/renaming a source starts a new family; its old copies remain.
 */
CORE_LIBRARY_EXPORT extern BackupResult save(const QString& _filePath, const QString& _backupDir,
                                             const QString& _newName, int _maximumBackups);

/**
 * @brief The same operation with an explicit clock boundary for deterministic checks.
 */
CORE_LIBRARY_EXPORT extern BackupResult saveAt(const QString& _filePath, const QString& _backupDir,
                                               const QString& _newName, int _maximumBackups,
                                               const QDateTime& _now);

} // namespace BackupBuilder
