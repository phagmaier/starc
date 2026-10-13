#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <management_layer/content/github_sync/github_sync_manager.h>
#include <management_layer/content/projects/projects_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <utils/tools/github_files_client.h>
#include <utils/tools/project_sync.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {
using namespace ManagementLayer;
using namespace BusinessLayer;

void require(bool _ok, const char* _label)
{
    if (!_ok) {
        std::fprintf(stderr, "FAIL: %s\n", _label);
        std::exit(1);
    }
    std::printf("ok: %s\n", _label);
}

// Fake credentials keep the regression away from the native keyring.
class Backend : public GitHubCredentialBackend
{
public:
    void read(Callback _callback) override
    {
        _callback(Result::Success, "test-token");
    }
    void write(const QString&, Callback _callback) override
    {
        _callback(Result::Success, { });
    }
    void remove(Callback _callback) override
    {
        _callback(Result::Success, { });
    }
};

void waitUntil(std::function<bool()> _condition)
{
    QEventLoop loop;
    QTimer poll, deadline;
    poll.setInterval(10);
    deadline.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (_condition())
            loop.quit();
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    deadline.start(10000);
    if (!_condition())
        loop.exec();
    require(_condition(), "asynchronous operation completes");
}

void writeStory(const QString& _path, const QString& _content)
{
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "sync-fixture");
        db.setDatabaseName(_path);
        require(db.open(), "disposable SQLite story opens");
        QSqlQuery query(db);
        require(query.exec("CREATE TABLE IF NOT EXISTS documents "
                           "(uuid TEXT PRIMARY KEY, type INTEGER, content BLOB)"),
                "story schema exists");
        query.prepare("INSERT OR REPLACE INTO documents VALUES ('{script}', 10104, ?)");
        query.addBindValue(_content.toUtf8());
        require(query.exec(), "story edit is saved");
        db.close();
    }
    QSqlDatabase::removeDatabase("sync-fixture");
}

QByteArray bytes(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "story bytes can be read");
    return file.readAll();
}

QJsonObject links()
{
    return QJsonDocument::fromJson(
               QByteArray::fromHex(
                   settingsValue(DataStorageLayer::kGitHubSyncProjectsKey).toByteArray()))
        .object();
}

void checkLink(const QString& _path, const QByteArray& _content)
{
    const auto link = links().value(_path).toObject();
    require(link["sha"].toString().toLatin1() == ProjectSync::gitBlobSha(_content)
                && link["fingerprint"].toString().toLatin1()
                    == ProjectSync::fingerprint(_content).value,
            "persisted sync baseline matches the transferred story");
}

void uploadRemote(GitHubFilesClient& _client, const QString& _name, const QByteArray& _content,
                  const QByteArray& _previousSha = { })
{
    bool done = false;
    _client.upload(_name, _content, _previousSha, "Disposable test edit",
                   [&](const auto& _error, const auto& _sha) {
                       require(!_error.isValid() && _sha == ProjectSync::gitBlobSha(_content),
                               "mock remote edit succeeds");
                       done = true;
                   });
    waitUntil([&] { return done; });
}

void checkRemote(GitHubFilesClient& _client, const QString& _name, const QByteArray& _expected)
{
    bool done = false;
    _client.download(_name, [&](const auto& _error, const auto& _content) {
        require(!_error.isValid() && _content == _expected,
                "mock remote contains the exact expected bytes");
        done = true;
    });
    waitUntil([&] { return done; });
}

QList<QPointer<Dialog>> dialogs(QWidget& _host)
{
    QList<QPointer<Dialog>> result;
    for (auto dialog : _host.findChildren<Dialog*>())
        result.append(dialog);
    return result;
}

Dialog* nextDialog(QWidget& _host, const QList<QPointer<Dialog>>& _previous)
{
    for (auto dialog : _host.findChildren<Dialog*>()) {
        if (!_previous.contains(QPointer<Dialog>(dialog)))
            return dialog;
    }
    return nullptr;
}

void answer(Dialog* _dialog, const QString& _text)
{
    require(_dialog != nullptr, "sync dialog exists");
    const auto isClose = _text == "Close";
    emit _dialog->finished(
        { isClose ? 0 : 1, _text, isClose ? Dialog::RejectButton : Dialog::AcceptButton });
}

Dialog* check(GitHubSyncManager& _sync, QWidget& _host)
{
    const auto previous = dialogs(_host);
    bool checked = false;
    const auto connection
        = QObject::connect(&_sync, &GitHubSyncManager::checkFinished, &_host, [&](bool _success) {
              require(_success, "manager checks the mock repository");
              checked = true;
          });
    _sync.sync();
    waitUntil([&] { return checked && nextDialog(_host, previous) != nullptr; });
    QObject::disconnect(connection);
    return nextDialog(_host, previous);
}

void checkAction(GitHubSyncManager& _sync, ProjectsModelProjectItem* _project, const QString& _text,
                 bool _trigger = false)
{
    const auto actions = _sync.projectActions(_project);
    require(actions.size() == 1 && actions.first()->text() == _text,
            "context action matches the story state");
    if (_trigger)
        actions.first()->trigger();
    qDeleteAll(actions);
}

void checkInSync(GitHubSyncManager& _sync, ProjectsModelProjectItem* _project,
                 const QString& _otherPath)
{
    require(_project->syncStatus() == ProjectSyncStatus::InSync,
            "successful transfer immediately reports InSync");
    checkAction(_sync, _project, "Sync with GitHub");
    _sync.updateProjectState(_project->path());
    _sync.updateProjectState(_project->path());
    _sync.updateProjectState(_otherPath);
    require(_project->syncStatus() == ProjectSyncStatus::InSync,
            "unchanged transferred story stays InSync after own and other story updates");
    checkAction(_sync, _project, "Sync with GitHub");
}
} // namespace

int main(int _argc, char** _argv)
{
    QApplication app(_argc, _argv);
    app.setApplicationName("GitHub sync manager regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        QFontDatabase::addApplicationFont(font);
    }
    const auto root = qEnvironmentVariable("STARC_PROFILE_DIR");
    require(!root.isEmpty(), "test uses an isolated profile");
    const auto path = root + "/story.starc";
    const auto otherPath = root + "/observer.starc";
    const auto remotePath = root + "/remote-fixture.starc";
    QDir().mkpath(root + "/downloads");
    writeStory(path, "A");
    writeStory(otherPath, "Observer");
    GitHubFilesClient client;
    client.setRepository("writer/stories");
    client.setToken("test-token");
    uploadRemote(client, "story.starc", bytes(path));
    uploadRemote(client, "observer.starc", bytes(otherPath));
    QJsonObject initialLinks;
    for (const auto& storyPath : { path, otherPath }) {
        const auto content = bytes(storyPath);
        initialLinks[storyPath]
            = QJsonObject{ { "remote", QFileInfo(storyPath).fileName() },
                           { "sha", QString::fromLatin1(ProjectSync::gitBlobSha(content)) },
                           { "fingerprint",
                             QString::fromLatin1(ProjectSync::fingerprint(content).value) } };
    }
    setSettingsValue(DataStorageLayer::kGitHubSyncProjectsKey,
                     QJsonDocument(initialLinks).toJson(QJsonDocument::Compact).toHex());
    setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, "writer/stories");
    setSettingsValue(DataStorageLayer::kApplicationBackupsFolderKey, root + "/backups");
    setSettingsValue(DataStorageLayer::kProjectSaveFolderKey, root + "/downloads");
    QWidget host;
    host.resize(1400, 950);
    TaskBar::registerTaskBar(&host, Qt::black, Qt::white, Qt::blue);
    ProjectsManager projects(nullptr, &host);
    auto project = projects.addLocalProject(path);
    projects.addLocalProject(otherPath);
    GitHubCredentialStore credentials(std::make_unique<Backend>(),
                                      { [] { return QString(); }, [] { return true; } });
    GitHubSyncManager sync(nullptr, &host, &projects, &credentials);
    const auto scenario = qEnvironmentVariable("STARC_SYNC_STATUS_CASE");

    if (scenario != "download") {
        writeStory(path, "B");
        auto dialog = check(sync, host);
        require(project->syncStatus() == ProjectSyncStatus::LocalChanged,
                "saved local edit needs upload");
        answer(dialog, "Sync");
        waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::InSync; });
        checkLink(path, bytes(path));
        checkInSync(sync, project, otherPath);
        if (scenario == "upload")
            return 0;

        writeStory(path, "B with a later local edit");
        sync.updateProjectState(path);
        require(project->syncStatus() == ProjectSyncStatus::LocalChanged,
                "local edit after upload is LocalChanged");
        checkAction(sync, project, "Upload changes to GitHub", true);
        waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::InSync; });
        checkRemote(client, "story.starc", bytes(path));
        checkInSync(sync, project, otherPath);
    }

    const auto beforeDownload = bytes(path);
    writeStory(remotePath, "C from another computer");
    const auto downloaded = bytes(remotePath);
    uploadRemote(client, "story.starc", downloaded, ProjectSync::gitBlobSha(beforeDownload));
    auto dialog = check(sync, host);
    require(project->syncStatus() == ProjectSyncStatus::RemoteChanged,
            "remote-only edit needs download");
    checkAction(sync, project, "Download the newer version from GitHub");
    answer(dialog, "Sync");
    waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::InSync; });
    require(bytes(path) == downloaded, "replacement download preserves exact remote bytes");
    const auto backups = QDir(root + "/backups").entryList({ "*.starc" }, QDir::Files);
    require(backups.size() == 1 && bytes(root + "/backups/" + backups.first()) == beforeDownload,
            "replacement download keeps the exact previous story backup");
    checkLink(path, downloaded);
    // Reach the second original reproduction directly in the download-only baseline run.
    if (scenario != "download")
        checkInSync(sync, project, otherPath);
    writeStory(path, "D local edit after download");
    sync.updateProjectState(path);
    require(project->syncStatus() == ProjectSyncStatus::LocalChanged,
            "local-only edit after download is LocalChanged rather than Diverged");
    checkAction(sync, project, "Upload changes to GitHub", true);
    waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::InSync; });
    checkRemote(client, "story.starc", bytes(path));
    checkInSync(sync, project, otherPath);
    checkLink(path, bytes(path));

    // A remote-only entry must acquire its new local path and baseline.
    uploadRemote(client, "new-story.starc", downloaded);
    dialog = check(sync, host);
    answer(dialog, "Sync");
    const auto newPath = root + "/downloads/new-story.starc";
    waitUntil([&] { return projects.project(newPath) != nullptr; });
    auto newProject = projects.project(newPath);
    require(bytes(newPath) == downloaded, "new remote story downloads unchanged");
    checkLink(newPath, downloaded);
    checkInSync(sync, newProject, path);
    writeStory(newPath, "New story local edit");
    sync.updateProjectState(newPath);
    require(newProject->syncStatus() == ProjectSyncStatus::LocalChanged,
            "newly downloaded story tracks its local edit");
    writeStory(newPath, "C from another computer");
    sync.updateProjectState(newPath);
    checkInSync(sync, newProject, path);

    // A genuine conflict and Download Copy must retain the original baseline.
    const auto originalLink = links().value(path);
    const auto syncedBytes = bytes(path);
    writeStory(path, "Local conflicting edit");
    const auto localConflict = bytes(path);
    writeStory(remotePath, "Remote conflicting edit");
    const auto remoteConflict = bytes(remotePath);
    uploadRemote(client, "story.starc", remoteConflict, ProjectSync::gitBlobSha(syncedBytes));
    dialog = check(sync, host);
    require(project->syncStatus() == ProjectSyncStatus::Diverged,
            "real local and remote changes still report Diverged");
    answer(dialog, "Close");
    auto actions = sync.projectActions(project);
    require(actions.size() == 3, "real conflict offers three resolution actions");
    actions.last()->trigger();
    qDeleteAll(actions);
    waitUntil([&] { return projects.localProjects().size() == 4; });
    ProjectsModelProjectItem* copy = nullptr;
    for (auto item : projects.localProjects()) {
        if (item->path().contains("(GitHub copy "))
            copy = item;
    }
    require(copy && bytes(copy->path()) == remoteConflict
                && copy->syncStatus() == ProjectSyncStatus::NotSynced,
            "separate GitHub copy stays unlinked and byte-exact");
    require(bytes(path) == localConflict && links().value(path) == originalLink
                && !links().contains(copy->path()),
            "Download Copy preserves original bytes and sync baseline");
    sync.updateProjectState(path);
    require(project->syncStatus() == ProjectSyncStatus::Diverged,
            "Download Copy preserves original conflict status");

    // A rejected upload must not record a successful sync.
    dialog = check(sync, host);
    answer(dialog, "Sync"); // Upload the unlinked copy; the original conflict remains.
    waitUntil([&] { return copy->syncStatus() == ProjectSyncStatus::InSync; });
    checkInSync(sync, copy, path);
    actions = sync.projectActions(project);
    const auto previousDialogs = dialogs(host);
    actions.first()->trigger();
    qDeleteAll(actions);
    waitUntil([&] { return nextDialog(host, previousDialogs) != nullptr; });
    dialog = nextDialog(host, previousDialogs);
    writeStory(remotePath, "Remote edit during confirmation");
    const auto racingRemote = bytes(remotePath);
    uploadRemote(client, "story.starc", racingRemote, ProjectSync::gitBlobSha(remoteConflict));
    answer(dialog, "Replace");
    waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::Failed; });
    require(bytes(path) == localConflict && links().value(path) == originalLink,
            "failed upload preserves local bytes and sync baseline");
    checkRemote(client, "story.starc", racingRemote);
    sync.updateProjectState(path);
    require(project->syncStatus() == ProjectSyncStatus::Diverged,
            "failed upload does not advance the cached baseline");

    // A refused replacement download likewise leaves both baselines intact.
    sync.setProjectReplacementHandler([](const QString&) { return false; });
    actions = sync.projectActions(project);
    auto beforeReplacement = dialogs(host);
    actions.at(1)->trigger();
    qDeleteAll(actions);
    waitUntil([&] { return nextDialog(host, beforeReplacement) != nullptr; });
    answer(nextDialog(host, beforeReplacement), "Replace");
    waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::Failed; });
    require(bytes(path) == localConflict && links().value(path) == originalLink,
            "refused download preserves local bytes and sync baseline");
    sync.updateProjectState(path);
    require(project->syncStatus() == ProjectSyncStatus::Diverged,
            "refused download does not advance the cached baseline");

    sync.setProjectReplacementHandler([](const QString&) { return true; });
    actions = sync.projectActions(project);
    beforeReplacement = dialogs(host);
    actions.at(1)->trigger();
    qDeleteAll(actions);
    waitUntil([&] { return nextDialog(host, beforeReplacement) != nullptr; });
    answer(nextDialog(host, beforeReplacement), "Replace");
    waitUntil([&] { return project->syncStatus() == ProjectSyncStatus::InSync; });
    require(bytes(path) == racingRemote, "chosen conflict download keeps exact remote bytes");
    checkLink(path, racingRemote);
    checkInSync(sync, project, otherPath);

    GitHubSyncManager fresh(nullptr, &host, &projects, &credentials);
    writeStory(path, "Local edit after manager reload");
    fresh.updateProjectState(path);
    require(project->syncStatus() == ProjectSyncStatus::LocalChanged,
            "a fresh manager retains the persisted transferred baseline");
    std::puts("GitHub sync manager regression passed.");
    return 0;
}
