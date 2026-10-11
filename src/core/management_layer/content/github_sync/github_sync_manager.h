#pragma once

#include <QObject>

#include <functional>

class QAction;

namespace BusinessLayer {
class ProjectsModelProjectItem;
}


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
    GitHubSyncManager(QObject* _parent, QWidget* _parentWidget, ProjectsManager* _projectsManager);
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

    /**
     * @brief Show the state of the story after its file was changed
     */
    void updateProjectState(const QString& _path);

    /**
     * @brief Actions for the context menu of the story
     */
    QVector<QAction*> projectActions(BusinessLayer::ProjectsModelProjectItem* _project);

signals:
    /**
     * @brief Changes of the opened story should be written to its file
     */
    void saveCurrentProjectRequested();

    /**
     * @brief The file of the story will be replaced, so the story should be closed
     */
    void closeProjectRequested(const QString& _path);

    /**
     * @brief The user wants to set the repository and the access token
     */
    void setupRequested();

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
