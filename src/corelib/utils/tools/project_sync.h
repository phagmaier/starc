#pragma once

#include <QByteArray>
#include <QString>

#include <corelib_global.h>


/**
 * @brief Helpers for syncing a project file with its copy in a remote file storage
 */
namespace ProjectSync {

/**
 * @brief State of the local project file relative to its remote copy
 */
enum class State {
    //
    // There is no remote copy
    //
    LocalOnly,
    //
    // There is no local file
    //
    RemoteOnly,
    //
    // Nothing changed since the last sync
    //
    InSync,
    //
    // Only the local file changed since the last sync
    //
    LocalChanged,
    //
    // Only the remote copy changed since the last sync
    //
    RemoteChanged,
    //
    // Both copies changed since the last sync, or they were never synced with each other.
    // The content of the remote copy is needed to tell more, see resolveDiverged()
    //
    Diverged,
};

/**
 * @brief What is known about both copies of a project
 */
struct CORE_LIBRARY_EXPORT Snapshot {
    /**
     * @brief Fingerprint of the local file, empty if there is no local file
     */
    QByteArray localFingerprint;

    /**
     * @brief Identifier of the current remote copy, empty if there is no remote copy
     */
    QByteArray remoteSha;

    /**
     * @brief Fingerprint and remote identifier remembered at the last sync, empty if the copies
     *        were never synced with each other
     */
    QByteArray baseFingerprint;
    QByteArray baseSha;
};

/**
 * @brief Determine the state without looking at the remote content
 */
CORE_LIBRARY_EXPORT State state(const Snapshot& _snapshot);

/**
 * @brief Refine the diverged state using the fingerprint of the remote content
 */
CORE_LIBRARY_EXPORT State resolveDiverged(const Snapshot& _snapshot,
                                          const QByteArray& _remoteFingerprint);

/**
 * @brief Identifier git assigns to a file with the given content
 */
CORE_LIBRARY_EXPORT QByteArray gitBlobSha(const QByteArray& _content);

/**
 * @brief Fingerprint of the documents stored in a project file
 */
struct CORE_LIBRARY_EXPORT Fingerprint {
    bool isValid() const
    {
        return !value.isEmpty();
    }

    /**
     * @brief Hex digest, empty if the project can't be read
     */
    QByteArray value;

    /**
     * @brief Why the project can't be read
     */
    QString error;
};

/**
 * @brief Calculate the fingerprint of a project
 * @note The application rewrites a project file when it is just opened (version marks, storage
 *       format of the values), so the bytes of the file can't tell if the story was changed.
 *       The fingerprint depends only on the content of the project documents
 */
CORE_LIBRARY_EXPORT Fingerprint fingerprint(const QString& _projectFilePath);
CORE_LIBRARY_EXPORT Fingerprint fingerprint(const QByteArray& _projectFileContent);

} // namespace ProjectSync
