#include "github_sync_manager.h"

#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <management_layer/content/projects/projects_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <utils/logging.h>
#include <utils/tools/backup_builder.h>
#include <utils/tools/github_files_client.h>
#include <utils/tools/project_sync.h>

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLockFile>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>
#include <QWidget>

#include <algorithm>


namespace ManagementLayer {

namespace {

/**
 * @brief GitHub doesn't accept files bigger than 100 MB, keep a reserve for the encoded content
 */
constexpr qint64 kMaximumFileSize = 50 * 1024 * 1024;

/**
 * @brief How many versions replaced by the downloaded ones are kept in the backups folder
 */
constexpr int kReplacedVersionsQty = 5;

const QLatin1String kProjectExtension(".starc");

/**
 * @brief Separator of the paragraphs in the texts of the dialogs
 */
const QLatin1String kParagraphBreak("\n\n");
const QString kTaskId = QStringLiteral("github-sync");

/**
 * @brief What was synced on this computer for a story
 */
struct Link {
    QString remoteName;
    QByteArray sha;
    QByteArray fingerprint;
};

/**
 * @brief A story and its copy in the repository
 */
struct Entry {
    QString name() const
    {
        return QFileInfo(remoteName).completeBaseName();
    }

    /**
     * @brief Path of the local file, empty if the story is only in the repository
     */
    QString path;

    /**
     * @brief Name of the file in the repository
     */
    QString remoteName;

    ProjectSync::Snapshot snapshot;
    ProjectSync::State state = ProjectSync::State::LocalOnly;

    /**
     * @brief Why the story can't be synced
     */
    QString error;

    /**
     * @brief Who changed the copy in the repository last time and when
     */
    QString remoteChange;
};

enum class OperationType {
    Upload,
    Download,
    DownloadCopy,
};

struct Operation {
    OperationType type;
    Entry entry;
};

QString bullets(const QStringList& _lines)
{
    QStringList result;
    for (const auto& line : _lines) {
        result.append(QString::fromUtf8(u8"• ") + line);
    }
    return result.join('\n');
}

} // namespace


class GitHubSyncManager::Implementation
{
public:
    Implementation(GitHubSyncManager* _q, QWidget* _parentWidget,
                   ProjectsManager* _projectsManager);

    /**
     * @brief Load and save what was synced on this computer
     */
    void loadLinks();
    void saveLinks();

    /**
     * @brief Keep the persisted link and cached state at the same successful transfer
     */
    void recordSync(const QString& _path, const QString& _remoteName, const QByteArray& _sha,
                    const QByteArray& _fingerprint);

    //
    // State of the stories
    //

    /**
     * @brief Determine the state of every story, the callback receives the error if any
     */
    void refresh(std::function<void(const GitHubFilesClient::Error&)> _callback);
    void buildEntries(const QVector<GitHubFilesClient::FileInfo>& _files);
    void resolveDivergedEntries(int _fromIndex,
                                std::function<void(const GitHubFilesClient::Error&)> _callback);
    void loadRemoteChanges(int _fromIndex, std::function<void()> _callback);

    /**
     * @brief Show the states of the stories in the list
     */
    void updateStatuses();
    void setStatus(const QString& _path, BusinessLayer::ProjectSyncStatus _status);

    //
    // Operations
    //

    /**
     * @brief Run operations one by one, the callback receives descriptions of the failed ones
     */
    void run(const QVector<Operation>& _operations,
             std::function<void(const QStringList&)> _callback);
    void runNextOperation();

    /**
     * @brief Stop everything what is in progress
     */
    void cancel();

    void upload(const Entry& _entry, std::function<void(const QString&)> _callback);
    void download(const Entry& _entry, bool _asCopy, std::function<void(const QString&)> _callback);

    //
    // Interaction with the user
    //

    Dialog* showDialog(const QString& _text, const QVector<Dialog::ButtonInfo>& _buttons,
                       std::function<void(int)> _callback);
    void showMessage(const QString& _text);

    /**
     * @brief Offer to sync the stories according to their states
     * @param _byRequest - the user asked to sync, so everything should be reported
     */
    void offerSync(bool _byRequest);

    /**
     * @brief Run the operation chosen by the user for the story, if its state is still the same
     */
    void runForProject(const QString& _path, ProjectSync::State _expectedState,
                       OperationType _type);

    void startUploadBeforeExit();
    void finishUploadBeforeExit(const QStringList& _problems);


    GitHubSyncManager* q = nullptr;
    QWidget* parentWidget = nullptr;
    ProjectsManager* projectsManager = nullptr;
    std::function<bool(const QString&)> projectReplacementHandler;

    GitHubFilesClient client;
    GitHubCredentialStore* credentials = nullptr;
    bool waitingForCredentialClose = false;
    quint64 credentialCloseRequest = 0;
    QPointer<Dialog> credentialCloseDialog;
    quint64 operationGeneration = 0;

    /**
     * @brief Synced stories by the paths of their files
     */
    QHash<QString, Link> links;

    /**
     * @brief States of the stories determined by the last check
     */
    QVector<Entry> entries;

    /**
     * @brief Is the check or the sync in progress
     */
    bool isBusy = false;

    /**
     * @brief Operations in progress
     */
    QVector<Operation> operations;
    int operationIndex = 0;
    QStringList operationsErrors;
    std::function<void(const QStringList&)> operationsCallback;

    /**
     * @brief Closing of the application is waiting for the upload
     */
    std::function<void()> exitCallback;
    QPointer<Dialog> exitDialog;
};

GitHubSyncManager::Implementation::Implementation(GitHubSyncManager* _q, QWidget* _parentWidget,
                                                  ProjectsManager* _projectsManager)
    : q(_q)
    , parentWidget(_parentWidget)
    , projectsManager(_projectsManager)
{
    loadLinks();
}

void GitHubSyncManager::Implementation::loadLinks()
{
    const auto data = settingsValue(DataStorageLayer::kGitHubSyncProjectsKey).toByteArray();
    const auto json = QJsonDocument::fromJson(QByteArray::fromHex(data)).object();
    for (auto iter = json.begin(); iter != json.end(); ++iter) {
        const auto linkJson = iter.value().toObject();
        links.insert(iter.key(),
                     { linkJson["remote"].toString(), linkJson["sha"].toString().toLatin1(),
                       linkJson["fingerprint"].toString().toLatin1() });
    }
}

void GitHubSyncManager::Implementation::saveLinks()
{
    QJsonObject json;
    for (auto iter = links.begin(); iter != links.end(); ++iter) {
        QJsonObject linkJson;
        linkJson["remote"] = iter->remoteName;
        linkJson["sha"] = QString::fromLatin1(iter->sha);
        linkJson["fingerprint"] = QString::fromLatin1(iter->fingerprint);
        json[iter.key()] = linkJson;
    }
    setSettingsValue(DataStorageLayer::kGitHubSyncProjectsKey,
                     QJsonDocument(json).toJson(QJsonDocument::Compact).toHex());
}

void GitHubSyncManager::Implementation::recordSync(const QString& _path, const QString& _remoteName,
                                                   const QByteArray& _sha,
                                                   const QByteArray& _fingerprint)
{
    links.insert(_path, { _remoteName, _sha, _fingerprint });
    saveLinks();

    for (auto& entry : entries) {
        if (entry.remoteName != _remoteName || (!entry.path.isEmpty() && entry.path != _path)) {
            continue;
        }

        // A remote-only download now has a local path. Remember the transferred version,
        // so later local saves compare against it rather than the pre-transfer snapshot.
        entry.path = _path;
        entry.snapshot = { _fingerprint, _sha, _fingerprint, _sha };
        entry.state = ProjectSync::State::InSync;
        entry.error.clear();
        entry.remoteChange.clear();
        break;
    }
}

void GitHubSyncManager::Implementation::refresh(
    std::function<void(const GitHubFilesClient::Error&)> _callback)
{
    // Keep the private per-story action route behind the same readiness boundary.
    if (credentials->isBusy()) {
        const auto generation = operationGeneration;
        credentials->whenSettled(q, [this, generation, _callback] {
            if (operationGeneration == generation) refresh(_callback);
        });
        return;
    }
    if (credentials->token().trimmed().isEmpty()) {
        _callback({ GitHubFilesClient::Error::Type::Unauthorized,
                    tr("Set the GitHub access token in application settings before syncing.") });
        return;
    }
    //
    // The state is determined by the files, so all changes should be there
    //
    emit q->saveCurrentProjectRequested();

    client.setRepository(settingsValue(DataStorageLayer::kGitHubSyncRepositoryKey).toString());
    client.setToken(credentials->token());

    auto fail = [this, _callback](const GitHubFilesClient::Error& _error) {
        Log::warning("[GitHubSyncManager] Stories can't be checked: %1", _error.text);
        entries.clear();
        updateStatuses();
        _callback(_error);
    };

    Log::info("[GitHubSyncManager] Check stories");
    client.loadRepositoryInfo([this, _callback,
                               fail](const GitHubFilesClient::Error& _error,
                                     const GitHubFilesClient::RepositoryInfo& _info) {
        if (_error.isValid()) {
            fail(_error);
            return;
        }
        //
        // Stories must not become visible for everyone
        //
        if (!_info.isPrivate) {
            fail({ GitHubFilesClient::Error::Type::Other,
                   tr("The repository is public, so everyone would be able to read the stories. "
                      "Make the repository private on GitHub, or set another one in the "
                      "application settings.") });
            return;
        }
        if (!_info.canPush) {
            fail({ GitHubFilesClient::Error::Type::Unauthorized,
                   tr("The access token doesn't allow writing to the repository.") });
            return;
        }

        client.loadFiles([this, _callback,
                          fail](const GitHubFilesClient::Error& _error,
                                const QVector<GitHubFilesClient::FileInfo>& _files) {
            if (_error.isValid()) {
                fail(_error);
                return;
            }

            buildEntries(_files);
            resolveDivergedEntries(0,
                                   [this, _callback, fail](const GitHubFilesClient::Error& _error) {
                                       if (_error.isValid()) {
                                           fail(_error);
                                           return;
                                       }

                                       loadRemoteChanges(0, [this, _callback] {
                                           //
                                           // The user chooses the version of such stories in the
                                           // list of stories, so they should be there even if they
                                           // were hidden from it
                                           //
                                           for (const auto& entry : std::as_const(entries)) {
                                               if (entry.state == ProjectSync::State::Diverged
                                                   && !entry.path.isEmpty()) {
                                                   projectsManager->addLocalProject(entry.path);
                                               }
                                           }

                                           saveLinks();
                                           updateStatuses();
                                           _callback({});
                                       });
                                   });
        });
    });
}

void GitHubSyncManager::Implementation::buildEntries(
    const QVector<GitHubFilesClient::FileInfo>& _files)
{
    entries.clear();

    QHash<QString, GitHubFilesClient::FileInfo> remoteFiles;
    for (const auto& file : _files) {
        if (file.name.endsWith(kProjectExtension, Qt::CaseInsensitive)) {
            remoteFiles.insert(file.name, file);
        }
    }

    QSet<QString> usedRemoteNames;
    auto addLocalEntry = [this, &remoteFiles, &usedRemoteNames](const QString& _path) {
        Entry entry;
        entry.path = _path;

        const auto link = links.value(_path);
        entry.remoteName
            = link.remoteName.isEmpty() ? QFileInfo(_path).fileName() : link.remoteName;
        //
        // Stories placed in different folders can have the same file name,
        // but they can't share one file in the repository
        //
        if (usedRemoteNames.contains(entry.remoteName)) {
            const auto baseName = QFileInfo(entry.remoteName).completeBaseName();
            int index = 2;
            do {
                entry.remoteName
                    = QString("%1 (%2)%3").arg(baseName).arg(index++).arg(kProjectExtension);
            } while (usedRemoteNames.contains(entry.remoteName)
                     || remoteFiles.contains(entry.remoteName));
        }
        usedRemoteNames.insert(entry.remoteName);

        const auto fingerprint = ProjectSync::fingerprint(_path);
        entry.error = fingerprint.error;
        entry.snapshot.localFingerprint = fingerprint.value;
        entry.snapshot.remoteSha = remoteFiles.value(entry.remoteName).sha;
        entry.snapshot.baseFingerprint = link.fingerprint;
        entry.snapshot.baseSha = link.sha;
        entry.state = ProjectSync::state(entry.snapshot);
        //
        // The copy was removed from the repository, so there is nothing to compare with anymore
        //
        if (entry.state == ProjectSync::State::LocalOnly) {
            links.remove(_path);
            entry.snapshot.baseFingerprint.clear();
            entry.snapshot.baseSha.clear();
        }
        entries.append(entry);
    };

    //
    // Synced stories go first, they already own their names in the repository
    //
    auto projects = projectsManager->localProjects();
    std::stable_partition(projects.begin(), projects.end(),
                          [this](BusinessLayer::ProjectsModelProjectItem* _project) {
                              return links.contains(_project->path());
                          });
    for (const auto project : std::as_const(projects)) {
        if (QFileInfo::exists(project->path())
            && project->path().endsWith(kProjectExtension, Qt::CaseInsensitive)) {
            addLocalEntry(project->path());
        }
    }

    //
    // Stories which are only in the repository
    //
    const auto projectsFolder = settingsValue(DataStorageLayer::kProjectSaveFolderKey).toString();
    for (const auto& file : _files) {
        if (!remoteFiles.contains(file.name) || usedRemoteNames.contains(file.name)) {
            continue;
        }

        //
        // The file of the story can be on this computer without being in the list of stories
        //
        const auto path = QDir::toNativeSeparators(projectsFolder + "/" + file.name);
        if (QFileInfo::exists(path)) {
            addLocalEntry(path);
            continue;
        }

        Entry entry;
        entry.remoteName = file.name;
        entry.snapshot.remoteSha = file.sha;
        entry.state = ProjectSync::State::RemoteOnly;
        usedRemoteNames.insert(entry.remoteName);
        entries.append(entry);
    }
}

void GitHubSyncManager::Implementation::resolveDivergedEntries(
    int _fromIndex, std::function<void(const GitHubFilesClient::Error&)> _callback)
{
    for (int index = _fromIndex; index < entries.size(); ++index) {
        const auto& entry = entries.at(index);
        if (entry.state != ProjectSync::State::Diverged || !entry.error.isEmpty()) {
            continue;
        }

        //
        // Only the content of the copy can tell, if the story really differs from it
        //
        client.download(
            entry.remoteName,
            [this, index, _callback](const GitHubFilesClient::Error& _error,
                                     const QByteArray& _content) {
                if (_error.isValid()) {
                    _callback(_error);
                    return;
                }

                auto& entry = entries[index];
                const auto fingerprint = ProjectSync::fingerprint(_content);
                if (!fingerprint.isValid()) {
                    entry.remoteChange = tr("this copy can't be read");
                } else if (ProjectSync::gitBlobSha(_content) == entry.snapshot.remoteSha) {
                    entry.state = ProjectSync::resolveDiverged(entry.snapshot, fingerprint.value);
                    if (entry.state == ProjectSync::State::InSync) {
                        entry.snapshot.baseFingerprint = entry.snapshot.localFingerprint;
                        entry.snapshot.baseSha = entry.snapshot.remoteSha;
                    } else if (entry.state == ProjectSync::State::LocalChanged) {
                        entry.snapshot.baseSha = entry.snapshot.remoteSha;
                    }
                    if (entry.state != ProjectSync::State::Diverged) {
                        links.insert(entry.path,
                                     { entry.remoteName, entry.snapshot.baseSha,
                                       entry.snapshot.baseFingerprint });
                    }
                }

                resolveDivergedEntries(index + 1, _callback);
            });
        return;
    }

    _callback({});
}

void GitHubSyncManager::Implementation::loadRemoteChanges(int _fromIndex,
                                                          std::function<void()> _callback)
{
    for (int index = _fromIndex; index < entries.size(); ++index) {
        const auto& entry = entries.at(index);
        const auto isRemoteChanged = entry.state == ProjectSync::State::RemoteChanged
            || entry.state == ProjectSync::State::RemoteOnly
            || entry.state == ProjectSync::State::Diverged;
        if (!isRemoteChanged || !entry.error.isEmpty() || !entry.remoteChange.isEmpty()) {
            continue;
        }

        client.loadLastChange(
            entry.remoteName,
            [this, index, _callback](const GitHubFilesClient::Error& _error,
                                     const GitHubFilesClient::ChangeInfo& _info) {
                //
                // It's just an additional information, so the sync can go on without it
                //
                if (!_error.isValid() && _info.dateTime.isValid()) {
                    entries[index].remoteChange = QString("%1, %2").arg(
                        _info.message.section('\n', 0, 0),
                        QLocale().toString(_info.dateTime.toLocalTime(), QLocale::ShortFormat));
                }

                loadRemoteChanges(index + 1, _callback);
            });
        return;
    }

    _callback();
}

void GitHubSyncManager::Implementation::updateStatuses()
{
    QHash<QString, BusinessLayer::ProjectSyncStatus> statuses;
    for (const auto& entry : std::as_const(entries)) {
        if (entry.path.isEmpty()) {
            continue;
        }

        auto status = BusinessLayer::ProjectSyncStatus::Undefined;
        if (!entry.error.isEmpty()) {
            status = BusinessLayer::ProjectSyncStatus::Failed;
        } else {
            switch (entry.state) {
            case ProjectSync::State::LocalOnly: {
                status = BusinessLayer::ProjectSyncStatus::NotSynced;
                break;
            }
            case ProjectSync::State::InSync: {
                status = BusinessLayer::ProjectSyncStatus::InSync;
                break;
            }
            case ProjectSync::State::LocalChanged: {
                status = BusinessLayer::ProjectSyncStatus::LocalChanged;
                break;
            }
            case ProjectSync::State::RemoteChanged: {
                status = BusinessLayer::ProjectSyncStatus::RemoteChanged;
                break;
            }
            case ProjectSync::State::Diverged: {
                status = BusinessLayer::ProjectSyncStatus::Diverged;
                break;
            }
            case ProjectSync::State::RemoteOnly: {
                break;
            }
            }
        }
        statuses.insert(entry.path, status);
    }

    const auto projects = projectsManager->localProjects();
    for (auto project : projects) {
        project->setSyncStatus(
            statuses.value(project->path(), BusinessLayer::ProjectSyncStatus::Undefined));
        projectsManager->updateProject(project);
    }
}

void GitHubSyncManager::Implementation::setStatus(const QString& _path,
                                                  BusinessLayer::ProjectSyncStatus _status)
{
    auto project = projectsManager->project(_path);
    if (project == nullptr) {
        return;
    }

    project->setSyncStatus(_status);
    projectsManager->updateProject(project);
}

void GitHubSyncManager::Implementation::run(const QVector<Operation>& _operations,
                                            std::function<void(const QStringList&)> _callback)
{
    operations = _operations;
    operationIndex = 0;
    operationsErrors.clear();
    operationsCallback = _callback;

    if (!operations.isEmpty()) {
        TaskBar::addTask(kTaskId);
        TaskBar::setTaskTitle(kTaskId, tr("Syncing the stories with GitHub"));
    }
    runNextOperation();
}

void GitHubSyncManager::Implementation::runNextOperation()
{
    if (operationIndex >= operations.size()) {
        if (!TaskBar::isTaskFinished(kTaskId)) {
            TaskBar::finishTask(kTaskId);
        }
        operations.clear();
        const auto callback = operationsCallback;
        operationsCallback = {};
        callback(operationsErrors);
        return;
    }

    TaskBar::setTaskProgress(kTaskId, 100.0 * operationIndex / operations.size());

    const auto operation = operations.at(operationIndex);
    auto callback = [this, name = operation.entry.name()](const QString& _error) {
        if (!_error.isEmpty()) {
            Log::warning("[GitHubSyncManager] \"%1\" isn't synced: %2", name, _error);
            operationsErrors.append(QString("\"%1\": %2").arg(name, _error));
        }
        ++operationIndex;
        runNextOperation();
    };
    switch (operation.type) {
    case OperationType::Upload: {
        upload(operation.entry, callback);
        break;
    }
    case OperationType::Download: {
        download(operation.entry, false, callback);
        break;
    }
    case OperationType::DownloadCopy: {
        download(operation.entry, true, callback);
        break;
    }
    }
}

void GitHubSyncManager::Implementation::cancel()
{
    ++operationGeneration;
    client.abort();
    operations.clear();
    operationsCallback = {};
    isBusy = false;
    if (!TaskBar::isTaskFinished(kTaskId)) {
        TaskBar::finishTask(kTaskId);
    }
}

void GitHubSyncManager::Implementation::upload(const Entry& _entry,
                                               std::function<void(const QString&)> _callback)
{
    Log::info("[GitHubSyncManager] Upload \"%1\"", _entry.remoteName);

    //
    // Changes of the opened story should be in the file,
    // and the file should not be in the middle of writing
    //
    emit q->saveCurrentProjectRequested();
    QFile file(_entry.path);
    if (QFileInfo::exists(_entry.path + "-journal") || !file.open(QIODevice::ReadOnly)) {
        setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::Failed);
        _callback(tr("The file of the story can't be read."));
        return;
    }
    const auto content = file.readAll();
    file.close();
    if (content.size() > kMaximumFileSize) {
        setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::Failed);
        _callback(tr("The file of the story is bigger than %1 MB, GitHub doesn't accept such "
                     "files.")
                      .arg(kMaximumFileSize / 1024 / 1024));
        return;
    }

    //
    // Upload exactly what was checked
    //
    const auto fingerprint = ProjectSync::fingerprint(content);
    if (!fingerprint.isValid() || fingerprint.value != _entry.snapshot.localFingerprint) {
        _callback(tr("The story was changed during the sync. Sync once again."));
        return;
    }

    setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::InProgress);
    client.upload(_entry.remoteName, content, _entry.snapshot.remoteSha,
                  QString("Saved from %1").arg(QSysInfo::machineHostName()),
                  [this, _entry, fingerprint, _callback](const GitHubFilesClient::Error& _error,
                                                         const QByteArray& _sha) {
                      if (_error.isValid()) {
                          setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::Failed);
                          _callback(_error.type == GitHubFilesClient::Error::Type::Conflict
                                        ? tr("The copy on GitHub was changed during the sync, "
                                             "so it wasn't replaced. Sync once again.")
                                        : _error.text);
                          return;
                      }

                      recordSync(_entry.path, _entry.remoteName, _sha, fingerprint.value);
                      setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::InSync);
                      _callback({});
                  });
}

void GitHubSyncManager::Implementation::download(const Entry& _entry, bool _asCopy,
                                                 std::function<void(const QString&)> _callback)
{
    Log::info("[GitHubSyncManager] Download \"%1\"", _entry.remoteName);

    const auto isReplacing = !_asCopy && !_entry.path.isEmpty();
    if (isReplacing) {
        setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::InProgress);
    }
    client.download(
        _entry.remoteName,
        [this, _entry, _asCopy, isReplacing, _callback](const GitHubFilesClient::Error& _error,
                                                        const QByteArray& _content) {
            auto fail = [this, _entry, isReplacing, _callback](const QString& _text) {
                if (isReplacing) {
                    setStatus(_entry.path, BusinessLayer::ProjectSyncStatus::Failed);
                }
                _callback(_text);
            };

            if (_error.isValid()) {
                fail(_error.text);
                return;
            }

            //
            // Save exactly what was checked, and only if it is a story
            //
            if (ProjectSync::gitBlobSha(_content) != _entry.snapshot.remoteSha) {
                fail(tr("The copy on GitHub was changed during the sync. Sync once again."));
                return;
            }
            const auto fingerprint = ProjectSync::fingerprint(_content);
            if (!fingerprint.isValid()) {
                fail(tr(
                    "The copy on GitHub can't be read as a story, so nothing was changed on this "
                    "computer."));
                return;
            }

            QString path = _entry.path;
            QScopedPointer<QLockFile> lock;
            if (_asCopy) {
                const QFileInfo fileInfo(_entry.path);
                path = QDir::toNativeSeparators(
                    QString("%1/%2 (GitHub copy %3)%4")
                        .arg(fileInfo.absolutePath(), fileInfo.completeBaseName(),
                             QDateTime::currentDateTime().toString("yyyy-MM-dd hh.mm.ss"),
                             kProjectExtension));
            } else if (path.isEmpty()) {
                const auto projectsFolder
                    = settingsValue(DataStorageLayer::kProjectSaveFolderKey).toString();
                QDir::root().mkpath(projectsFolder);
                path = QDir::toNativeSeparators(projectsFolder + "/" + _entry.remoteName);
            } else {
                //
                // The file will be replaced, so nobody should work with it
                //
                if (projectReplacementHandler) {
                    if (!projectReplacementHandler(path)) {
                        fail(tr("The story couldn't be saved, so it was kept open and wasn't "
                                "replaced. Retry saving before syncing again."));
                        return;
                    }
                } else if (projectsManager->currentProject() != nullptr
                           && projectsManager->currentProject()->path() == path) {
                    fail(tr("The story is still open, so it wasn't replaced."));
                    return;
                }
                const QFileInfo fileInfo(path);
                lock.reset(new QLockFile(
                    QString("%1/%2.lock").arg(fileInfo.absolutePath(), fileInfo.fileName())));
                if (!lock->tryLock()) {
                    fail(tr("The story is opened in another copy of the application."));
                    return;
                }

                //
                // ... and it should be the same story which was checked
                //
                const auto localFingerprint = ProjectSync::fingerprint(path);
                if (!localFingerprint.isValid()
                    || localFingerprint.value != _entry.snapshot.localFingerprint) {
                    fail(tr("The story was changed during the sync. Sync once again."));
                    return;
                }

                //
                // ... and the replaced version should stay available
                //
                const auto backup = BackupBuilder::save(
                    path, settingsValue(DataStorageLayer::kApplicationBackupsFolderKey).toString(),
                    QString("%1 (before GitHub download)").arg(fileInfo.completeBaseName()),
                    kReplacedVersionsQty);
                if (!backup.success) {
                    fail(backup.status);
                    return;
                }
            }
            if (!isReplacing && QFileInfo::exists(path)) {
                fail(tr("The file \"%1\" already exists.").arg(path));
                return;
            }

            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(_content) != _content.size()
                || !file.commit()) {
                fail(tr("The file \"%1\" can't be written: %2").arg(path, file.errorString()));
                return;
            }
            if (!lock.isNull()) {
                lock->unlock();
            }

            auto project = projectsManager->addLocalProject(path);
            project->setLastEditTime(QDateTime::currentDateTime());
            if (_asCopy) {
                project->setSyncStatus(BusinessLayer::ProjectSyncStatus::NotSynced);
            } else {
                recordSync(path, _entry.remoteName, _entry.snapshot.remoteSha, fingerprint.value);
                project->setSyncStatus(BusinessLayer::ProjectSyncStatus::InSync);
            }
            projectsManager->updateProject(project);
            _callback({});
        });
}

Dialog* GitHubSyncManager::Implementation::showDialog(const QString& _text,
                                                      const QVector<Dialog::ButtonInfo>& _buttons,
                                                      std::function<void(int)> _callback)
{
    //
    // The label of the dialog reserves a bit less space for every line than it needs to draw
    // it, so the last line of a long text is cut. An empty line at the end gives that space
    //
    const auto text = _text.contains('\n') ? _text + '\n' : _text;

    auto dialog = new Dialog(parentWidget->topLevelWidget());
    dialog->setContentMaximumWidth(Ui::DesignSystem::dialog().normalWidth());
    dialog->showDialog(tr("Sync with GitHub"), text, _buttons);
    QObject::connect(dialog, &Dialog::finished, q,
                     [dialog, _callback](const Dialog::ButtonInfo& _button) {
                         dialog->hideDialog();
                         if (_callback) {
                             _callback(_button.id);
                         }
                     });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
    return dialog;
}

void GitHubSyncManager::Implementation::showMessage(const QString& _text)
{
    showDialog(_text, { { 0, tr("Close"), Dialog::RejectButton } }, {});
}

void GitHubSyncManager::Implementation::offerSync(bool _byRequest)
{
    const auto skippedFiles
        = settingsValue(DataStorageLayer::kGitHubSyncSkippedFilesKey).toStringList();

    QVector<Operation> operations;
    QStringList newRemote, changedRemote, newLocal, changedLocal, diverged, failed, offeredFiles;
    auto described = [](const Entry& _entry) {
        return _entry.remoteChange.isEmpty()
            ? _entry.name()
            : QString("%1 (%2)").arg(_entry.name(), _entry.remoteChange);
    };
    for (const auto& entry : std::as_const(entries)) {
        if (!entry.error.isEmpty()) {
            failed.append(QString("%1 (%2)").arg(entry.name(), entry.error));
            continue;
        }

        switch (entry.state) {
        case ProjectSync::State::RemoteOnly: {
            //
            // Don't offer the same stories at every start of the application
            //
            if (!_byRequest && skippedFiles.contains(entry.remoteName)) {
                break;
            }
            operations.append({ OperationType::Download, entry });
            newRemote.append(described(entry));
            offeredFiles.append(entry.remoteName);
            break;
        }
        case ProjectSync::State::RemoteChanged: {
            operations.append({ OperationType::Download, entry });
            changedRemote.append(described(entry));
            break;
        }
        case ProjectSync::State::LocalOnly: {
            //
            // A story gets to the repository only when the user asks for it
            //
            if (!_byRequest) {
                break;
            }
            operations.append({ OperationType::Upload, entry });
            newLocal.append(entry.name());
            break;
        }
        case ProjectSync::State::LocalChanged: {
            operations.append({ OperationType::Upload, entry });
            changedLocal.append(entry.name());
            break;
        }
        case ProjectSync::State::Diverged: {
            diverged.append(described(entry));
            break;
        }
        case ProjectSync::State::InSync: {
            break;
        }
        }
    }

    if (operations.isEmpty() && diverged.isEmpty() && (!_byRequest || failed.isEmpty())) {
        if (_byRequest) {
            showMessage(tr("All stories are in sync with GitHub."));
        }
        return;
    }

    QStringList sections;
    if (!changedRemote.isEmpty()) {
        sections.append(tr("Newer on GitHub, will be downloaded:") + "\n" + bullets(changedRemote));
    }
    if (!newRemote.isEmpty()) {
        sections.append(tr("Not on this computer yet, will be downloaded:") + "\n"
                        + bullets(newRemote));
    }
    if (!changedLocal.isEmpty()) {
        sections.append(tr("Changed on this computer, will be uploaded:") + "\n"
                        + bullets(changedLocal));
    }
    if (!newLocal.isEmpty()) {
        sections.append(tr("Not on GitHub yet, will be uploaded:") + "\n" + bullets(newLocal));
    }
    if (!diverged.isEmpty()) {
        sections.append(tr("Changed both on this computer and on GitHub, nothing will be done "
                           "with them. Right-click such a story to choose which version to keep:")
                        + "\n" + bullets(diverged));
    }
    if (!failed.isEmpty()) {
        sections.append(tr("Can't be synced:") + "\n" + bullets(failed));
    }

    if (operations.isEmpty()) {
        showMessage(sections.join(kParagraphBreak));
        return;
    }

    constexpr int cancelButtonId = 0;
    constexpr int syncButtonId = 1;
    showDialog(
        sections.join(kParagraphBreak),
        { { cancelButtonId, tr("Not now"), Dialog::RejectButton },
          { syncButtonId, tr("Sync"), Dialog::AcceptButton } },
        [this, operations, offeredFiles, _byRequest](int _buttonId) {
            if (_buttonId == cancelButtonId) {
                if (!_byRequest && !offeredFiles.isEmpty()) {
                    auto skippedFiles = settingsValue(DataStorageLayer::kGitHubSyncSkippedFilesKey)
                                            .toStringList();
                    skippedFiles.append(offeredFiles);
                    skippedFiles.removeDuplicates();
                    setSettingsValue(DataStorageLayer::kGitHubSyncSkippedFilesKey, skippedFiles);
                }
                return;
            }

            if (isBusy) {
                return;
            }
            isBusy = true;
            run(operations, [this](const QStringList& _errors) {
                isBusy = false;
                if (!_errors.isEmpty()) {
                    showMessage(tr("Not all stories were synced:") + kParagraphBreak
                                + _errors.join(kParagraphBreak));
                }
            });
        });
}

void GitHubSyncManager::Implementation::runForProject(const QString& _path,
                                                      ProjectSync::State _expectedState,
                                                      OperationType _type)
{
    if (isBusy) {
        return;
    }

    isBusy = true;
    refresh([this, _path, _expectedState, _type](const GitHubFilesClient::Error& _error) {
        isBusy = false;
        if (_error.isValid()) {
            showMessage(_error.text);
            return;
        }

        const auto entryIter
            = std::find_if(entries.cbegin(), entries.cend(),
                           [_path](const Entry& _entry) { return _entry.path == _path; });
        if (entryIter == entries.cend() || !entryIter->error.isEmpty()
            || entryIter->state != _expectedState) {
            showMessage(tr("The state of the story has changed since it was checked last time. "
                           "Look at the story and choose the action again."));
            return;
        }

        const auto entry = *entryIter;
        auto runOperation = [this, entry, _type] {
            if (isBusy) {
                return;
            }
            isBusy = true;
            run({ { _type, entry } }, [this](const QStringList& _errors) {
                isBusy = false;
                if (!_errors.isEmpty()) {
                    showMessage(_errors.join(kParagraphBreak));
                }
            });
        };

        //
        // One of the changed versions is going to be replaced, so make sure it's intended
        //
        if (_expectedState == ProjectSync::State::Diverged
            && _type != OperationType::DownloadCopy) {
            QStringList text;
            if (_type == OperationType::Upload) {
                text.append(tr("The version of \"%1\" on GitHub will be replaced with the version "
                               "from this computer.")
                                .arg(entry.name()));
            } else {
                text.append(tr("The version of \"%1\" on this computer will be replaced with the "
                               "version from GitHub.")
                                .arg(entry.name()));
            }
            if (!entry.remoteChange.isEmpty()) {
                text.append(tr("The version on GitHub: %1.").arg(entry.remoteChange));
            }
            if (_type == OperationType::Upload) {
                text.append(tr("The replaced version will stay in the history of the repository."));
            } else {
                text.append(tr("The replaced version will be saved to the backups folder \"%1\".")
                                .arg(settingsValue(DataStorageLayer::kApplicationBackupsFolderKey)
                                         .toString()));
            }
            constexpr int replaceButtonId = 1;
            showDialog(text.join(kParagraphBreak),
                       { { 0, tr("Cancel"), Dialog::RejectButton },
                         { replaceButtonId, tr("Replace"), Dialog::AcceptCriticalButton } },
                       [runOperation](int _buttonId) {
                           if (_buttonId == replaceButtonId) {
                               runOperation();
                           }
                       });
            return;
        }

        runOperation();
    });
}

void GitHubSyncManager::Implementation::startUploadBeforeExit()
{
    isBusy = true;
    exitDialog
        = showDialog(tr("Uploading the changed stories to GitHub..."),
                     { { 0, tr("Close without uploading"), Dialog::RejectButton } }, [this](int) {
                         exitDialog.clear();
                         cancel();
                         finishUploadBeforeExit({});
                     });

    refresh([this](const GitHubFilesClient::Error& _error) {
        if (_error.isValid()) {
            finishUploadBeforeExit({ _error.text });
            return;
        }

        QVector<Operation> operations;
        QStringList problems;
        for (const auto& entry : std::as_const(entries)) {
            if (entry.path.isEmpty() || !entry.error.isEmpty()) {
                continue;
            }

            if (entry.state == ProjectSync::State::LocalChanged) {
                operations.append({ OperationType::Upload, entry });
            } else if (entry.state == ProjectSync::State::Diverged) {
                problems.append(tr("\"%1\" was changed both on this computer and on GitHub. "
                                   "Choose which version to keep when you start the application "
                                   "next time.")
                                    .arg(entry.name()));
            }
        }
        run(operations, [this, problems](const QStringList& _errors) {
            finishUploadBeforeExit(problems + _errors);
        });
    });
}

void GitHubSyncManager::Implementation::finishUploadBeforeExit(const QStringList& _problems)
{
    isBusy = false;
    if (!exitDialog.isNull()) {
        exitDialog->hideDialog();
        exitDialog.clear();
    }

    //
    // The user should know that this computer has changes which are not on GitHub
    //
    if (!_problems.isEmpty()) {
        constexpr int retryButtonId = 0;
        showDialog(tr("Not all changes were uploaded to GitHub:") + kParagraphBreak
                       + _problems.join(kParagraphBreak),
                   { { retryButtonId, tr("Try again"), Dialog::NormalButton },
                     { 1, tr("Close the application"), Dialog::AcceptButton } },
                   [this](int _buttonId) {
                       if (_buttonId == retryButtonId) {
                           startUploadBeforeExit();
                       } else {
                           finishUploadBeforeExit({});
                       }
                   });
        return;
    }

    const auto callback = exitCallback;
    exitCallback = {};
    if (callback) {
        callback();
    }
}


// ****


GitHubSyncManager::GitHubSyncManager(QObject* _parent, QWidget* _parentWidget,
                                     ProjectsManager* _projectsManager,
                                     GitHubCredentialStore* _credentials)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget, _projectsManager))
{
    d->credentials = _credentials ? _credentials : GitHubCredentialStore::instance();
    connect(d->credentials, &GitHubCredentialStore::changed, this, [this] {
        if (d->credentials->isReady() && d->client.setToken(d->credentials->token())) {
            d->cancel();
            d->entries.clear();
            d->updateStatuses();
        }
    });
    d->credentials->start();
}

GitHubSyncManager::~GitHubSyncManager() = default;

void GitHubSyncManager::setProjectReplacementHandler(
    std::function<bool(const QString&)> _handler)
{
    d->projectReplacementHandler = std::move(_handler);
}

bool GitHubSyncManager::isConfigured() const
{
    return !settingsValue(DataStorageLayer::kGitHubSyncRepositoryKey).toString().trimmed().isEmpty()
        && d->credentials->isReady() && !d->credentials->token().trimmed().isEmpty();
}

void GitHubSyncManager::checkOnStart()
{
    if (d->credentials->isBusy()) {
        d->credentials->whenSettled(this, [this] { checkOnStart(); });
        return;
    }
    if (!d->credentials->problem().isEmpty())
        d->showMessage(d->credentials->problem());
    if (!isConfigured() || d->isBusy) {
        return;
    }

    d->isBusy = true;
    d->refresh([this](const GitHubFilesClient::Error& _error) {
        d->isBusy = false;
        emit checkFinished(!_error.isValid());
        if (_error.isValid()) {
            //
            // There is no need to disturb the user, if the computer is just offline
            //
            if (_error.type != GitHubFilesClient::Error::Type::Network) {
                d->showMessage(tr("The stories can't be synced with GitHub.") + kParagraphBreak
                               + _error.text);
            }
            return;
        }

        d->offerSync(false);
    });
}

void GitHubSyncManager::sync()
{
    if (d->credentials->isBusy()) {
        d->credentials->whenSettled(this, [this] { sync(); });
        return;
    }
    if (!isConfigured() && !d->credentials->problem().isEmpty()) {
        d->showMessage(d->credentials->problem());
        return;
    }
    if (!isConfigured()) {
        constexpr int setupButtonId = 1;
        d->showDialog(tr("The application can keep copies of your stories in a private GitHub "
                         "repository and sync them between your computers. To turn it on, set "
                         "the repository and the access token in the application settings."),
                      { { 0, tr("Not now"), Dialog::RejectButton },
                        { setupButtonId, tr("Open settings"), Dialog::AcceptButton } },
                      [this](int _buttonId) {
                          if (_buttonId == setupButtonId) {
                              emit setupRequested();
                          }
                      });
        return;
    }

    if (d->isBusy) {
        return;
    }

    d->isBusy = true;
    TaskBar::addTask(kTaskId);
    TaskBar::setTaskTitle(kTaskId, tr("Checking the stories on GitHub"));
    TaskBar::setTaskIndeterminate(kTaskId, true);
    d->refresh([this](const GitHubFilesClient::Error& _error) {
        TaskBar::finishTask(kTaskId);
        d->isBusy = false;
        emit checkFinished(!_error.isValid());
        if (_error.isValid()) {
            d->showMessage(_error.text);
            return;
        }

        d->offerSync(true);
    });
}

void GitHubSyncManager::settleCredentialsBeforeExit(std::function<void()> _callback)
{
    // Settle an intended field edit even if the user closes without leaving settings first.
    if (d->waitingForCredentialClose)
        return;
    if (d->credentials->hasPendingChange() && d->credentials->problem().isEmpty()) {
        d->credentials->save();
    }
    if (d->credentials->isBusy()) {
        const auto request = ++d->credentialCloseRequest;
        d->waitingForCredentialClose = true;
        d->credentialCloseDialog = d->showDialog(
            tr("Waiting for GitHub credential storage before closing..."),
            { { 0, tr("Keep application open"), Dialog::RejectButton } }, [this](int) {
                d->waitingForCredentialClose = false;
                ++d->credentialCloseRequest;
                d->credentialCloseDialog.clear();
            });
        d->credentials->whenSettled(this, [this, _callback, request] {
            if (!d->waitingForCredentialClose || d->credentialCloseRequest != request)
                return;
            d->waitingForCredentialClose = false;
            if (d->credentialCloseDialog)
                d->credentialCloseDialog->hideDialog();
            d->credentialCloseDialog.clear();
            settleCredentialsBeforeExit(_callback);
        });
        return;
    }
    if (d->credentials->hasPendingChange()) {
        d->waitingForCredentialClose = true;
        d->showDialog(tr("The GitHub token change has not been saved.") + kParagraphBreak
                          + d->credentials->problem(),
                      { { 0, tr("Try again"), Dialog::NormalButton },
                        { 1, tr("Keep application open"), Dialog::RejectButton },
                        { 2, tr("Close without saving token"), Dialog::AcceptButton } },
                      [this, _callback](int _choice) {
                          d->waitingForCredentialClose = false;
                          if (_choice == 0) {
                              d->credentials->retry();
                              settleCredentialsBeforeExit(_callback);
                          } else if (_choice == 2) {
                              _callback();
                          }
                      });
        return;
    }
    _callback();
}

void GitHubSyncManager::uploadBeforeExit(std::function<void()> _callback)
{
    settleCredentialsBeforeExit([this, _callback] { uploadReadyBeforeExit(_callback); });
}

void GitHubSyncManager::uploadReadyBeforeExit(std::function<void()> _callback)
{
    if (!isConfigured() || !settingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey).toBool()) {
        _callback();
        return;
    }

    //
    // The user already sees what is going on with the upload
    //
    if (d->exitCallback) {
        return;
    }

    //
    // Closing of the application is more important than the sync which is in progress
    //
    d->cancel();

    //
    // Don't touch the network, if there is nothing to upload
    //
    bool hasChanges = false;
    const auto projects = d->projectsManager->localProjects();
    for (const auto project : projects) {
        const auto link = d->links.value(project->path());
        if (link.fingerprint.isEmpty()) {
            continue;
        }

        const auto fingerprint = ProjectSync::fingerprint(project->path());
        if (fingerprint.isValid() && fingerprint.value != link.fingerprint) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges) {
        _callback();
        return;
    }

    d->exitCallback = _callback;
    d->startUploadBeforeExit();
}

void GitHubSyncManager::updateProjectState(const QString& _path)
{
    if (!isConfigured()) {
        return;
    }

    const auto fingerprint = ProjectSync::fingerprint(_path);
    if (!fingerprint.isValid()) {
        return;
    }

    //
    // If the copy in the repository was checked, the state can be determined completely
    //
    for (auto& entry : d->entries) {
        if (entry.path != _path) {
            continue;
        }

        entry.snapshot.localFingerprint = fingerprint.value;
        entry.state = ProjectSync::state(entry.snapshot);
        d->updateStatuses();
        return;
    }

    //
    // ... otherwise it's known only if the story has changes which were not uploaded
    //
    const auto link = d->links.value(_path);
    if (!link.fingerprint.isEmpty() && link.fingerprint != fingerprint.value) {
        d->setStatus(_path, BusinessLayer::ProjectSyncStatus::LocalChanged);
    }
}

QVector<QAction*> GitHubSyncManager::projectActions(
    BusinessLayer::ProjectsModelProjectItem* _project)
{
    if (!isConfigured() || _project->projectType() != BusinessLayer::ProjectType::Local) {
        return {};
    }

    const auto path = _project->path();
    auto makeAction
        = [this](const QString& _icon, const QString& _text, std::function<void()> _callback) {
              auto action = new QAction;
              action->setIconText(_icon);
              action->setText(_text);
              connect(action, &QAction::triggered, this, _callback);
              return action;
          };
    switch (_project->syncStatus()) {
    case BusinessLayer::ProjectSyncStatus::NotSynced: {
        return { makeAction(u8"\U000F0167", tr("Upload to GitHub"), [this, path] {
            d->runForProject(path, ProjectSync::State::LocalOnly, OperationType::Upload);
        }) };
    }

    case BusinessLayer::ProjectSyncStatus::LocalChanged: {
        return { makeAction(u8"\U000F0167", tr("Upload changes to GitHub"), [this, path] {
            d->runForProject(path, ProjectSync::State::LocalChanged, OperationType::Upload);
        }) };
    }

    case BusinessLayer::ProjectSyncStatus::RemoteChanged: {
        return { makeAction(
            u8"\U000F0162", tr("Download the newer version from GitHub"), [this, path] {
                d->runForProject(path, ProjectSync::State::RemoteChanged, OperationType::Download);
            }) };
    }

    case BusinessLayer::ProjectSyncStatus::Diverged: {
        return {
            makeAction(u8"\U000F0167", tr("Keep this version, replace the one on GitHub"),
                       [this, path] {
                           d->runForProject(path, ProjectSync::State::Diverged,
                                            OperationType::Upload);
                       }),
            makeAction(u8"\U000F0162", tr("Keep the GitHub version, replace this one"),
                       [this, path] {
                           d->runForProject(path, ProjectSync::State::Diverged,
                                            OperationType::Download);
                       }),
            makeAction(u8"\U000F018F", tr("Download the GitHub version as a separate story"),
                       [this, path] {
                           d->runForProject(path, ProjectSync::State::Diverged,
                                            OperationType::DownloadCopy);
                       }),
        };
    }

    default: {
        return { makeAction(u8"\U000F063F", tr("Sync with GitHub"), [this] { sync(); }) };
    }
    }
}

} // namespace ManagementLayer
