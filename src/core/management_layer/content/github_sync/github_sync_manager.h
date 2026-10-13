#pragma once

#include <QObject>

#include <functional>

class QAction;

namespace BusinessLayer {
class ProjectsModelProjectItem;
}


class GitHubCredentialStore;

namespace ManagementLayer {

class ProjectsManager;

/**
 * @brief Manager of syncing the local stories with a private GitHub repository
 *
 * Every story is one file in the root of the repository. The manager never replaces a version
 * it doesn't know: a file is uploaded only over the version it was synced with, and it is
 * downloaded only if the story wasn't changed on this computer since the last sync. When both
 * copies were changed, the user chooses which one to keep
 */
class GitHubSyncManager : public QObject
{
    Q_OBJECT

public:
    GitHubSyncManager(QObject* _parent, QWidget* _parentWidget, ProjectsManager* _projectsManager,
                      GitHubCredentialStore* _credentials = nullptr);
    ~GitHubSyncManager() override;

    /**
     * @brief Are the repository and the access token set
     */
    bool isConfigured() const;

    /**
     * @brief Check the stories after the application start and offer to sync the changed ones
     */
    void checkOnStart();

    /**
     * @brief Check the stories by the request of the user and offer to sync them
     */
    void sync();

    /**
     * @brief Upload the changed stories before closing the application
     * @param _callback - called when the application can be closed
     */
    void uploadBeforeExit(std::function<void()> _callback);

    // Also used for settings-driven restarts and update exits, without uploading stories.
    void settleCredentialsBeforeExit(std::function<void()> _callback);

    /**
     * @brief Show the state of the story after its file was changed
     */
    void updateProjectState(const QString& _path);

    /**
     * @brief Actions for the context menu of the story
     */
    QVector<QAction*> projectActions(BusinessLayer::ProjectsModelProjectItem* _project);

    /**
     * @brief Save and close an opened story before replacing its file; false prevents replacement
     */
    void setProjectReplacementHandler(std::function<bool(const QString&)> _handler);

signals:
    void checkFinished(bool _success);
    /**
     * @brief Changes of the opened story should be written to its file
     */
    void saveCurrentProjectRequested();

    /**
     * @brief The user wants to set the repository and the access token
     */
    void setupRequested();

private:
    void uploadReadyBeforeExit(std::function<void()> _callback);
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
