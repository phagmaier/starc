#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <management_layer/content/github_sync/github_sync_manager.h>
#include <management_layer/content/projects/projects_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <utils/tools/github_files_client.h>
#include <utils/tools/project_sync.h>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QPointer>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryFile>
#include <QTimer>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>
#include <unistd.h>

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
        query.prepare("INSERT OR REPLACE INTO documents VALUES ('{review}', 10105, ?)");
        query.addBindValue(
            QByteArray("<review id=\"review-1\" from=\"2\" length=\"5\">Note</review>"));
        require(query.exec(), "authored review identity and bytes are saved");
        query.prepare("INSERT OR REPLACE INTO documents VALUES ('{opaque}', 99999, ?)");
        query.addBindValue(QByteArray::fromHex("00fffe5c5b0d0a80"));
        require(query.exec(), "opaque legacy document bytes are saved");
        require(
            query.exec("CREATE TABLE IF NOT EXISTS history (uuid TEXT PRIMARY KEY, patch BLOB)"),
            "history schema exists");
        query.prepare("INSERT OR REPLACE INTO history VALUES ('{history-1}', ?)");
        query.addBindValue(QByteArray::fromHex("005061746368ff0d0a"));
        require(query.exec(), "change history identity and exact patch are saved");
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

QString target, mode, externalTarget;
QByteArray competingBytes;
bool armed = false, faultFired = false, publicationSeen = false;
int limitedDescriptor = -1;

template<typename Function>
Function realFunction(const char* _name)
{
    const auto result = reinterpret_cast<Function>(dlsym(RTLD_NEXT, _name));
    require(result != nullptr, "native fault hook resolves the real function");
    return result;
}

template<typename Publish>
bool publish(Publish _publish)
{
    publicationSeen = true;
    QLockFile competitor(target + ".lock");
    require(!competitor.tryLock(), "destination lock is held through final publication");
    if (mode == "race-file") {
        QFile writer(target);
        require(writer.open(QIODevice::WriteOnly | QIODevice::NewOnly)
                    && writer.write(competingBytes) == competingBytes.size() && writer.flush(),
                "uncooperative writer publishes exact competing story before rename");
        writer.close();
        faultFired = true;
    } else if (mode == "race-symlink") {
        require(QFile::link(externalTarget, target), "dangling symlink appears before rename");
        faultFired = true;
    } else if (mode == "race-directory") {
        require(QDir().mkdir(target), "directory appears before rename");
        faultFired = true;
    }
    if (mode == "publish-failure") {
        const auto folder = QFile::encodeName(QFileInfo(target).absolutePath());
        struct stat original{ };
        require(::stat(folder.constData(), &original) == 0
                    && ::chmod(folder.constData(), original.st_mode & ~0222) == 0,
                "publication directory becomes unwritable after staging");
        const auto result = _publish();
        require(::chmod(folder.constData(), original.st_mode & 07777) == 0 && !result,
                "real publication fails and permissions recover");
        faultFired = true;
        return result;
    }
    return _publish();
}
} // namespace

// Freeze only wall time in this test executable so separate-copy names are deterministic.
// The Qt event loop and its deadlines continue using their ordinary monotonic clock.
QDateTime QDateTime::currentDateTime()
{
    return QDateTime(QDate(2026, 10, 13), QTime(12, 34, 56));
}

bool QTemporaryFile::rename(const QString& _name)
{
    using Rename = bool (*)(QTemporaryFile*, const QString&);
    static const auto renameFile = realFunction<Rename>("_ZN14QTemporaryFile6renameERK7QString");
    const auto operation = [&] { return renameFile(this, _name); };
    return armed && _name == target ? publish(operation) : operation();
}

// On the old implementation the collision is inserted at its replacing open boundary.
// The safety assertion must fail there rather than silently skipping the old route.
bool QSaveFile::open(OpenMode _flags)
{
    using Open = bool (*)(QSaveFile*, OpenMode);
    static const auto openFile
        = realFunction<Open>("_ZN9QSaveFile4openE6QFlagsIN13QIODeviceBase12OpenModeFlagEE");
    const auto operation = [&] { return openFile(this, _flags); };
    return armed && fileName() == target ? publish(operation) : operation();
}

extern "C" ssize_t write(int _fd, const void* _data, size_t _size)
{
    using Write = ssize_t (*)(int, const void*, size_t);
    static const auto writeFile = reinterpret_cast<Write>(dlsym(RTLD_NEXT, "write"));
    if (!writeFile)
        std::abort();
    if (armed && mode == "write-failure") {
        // QTemporaryFile may use O_TMPFILE. Identify only a staging descriptor in this
        // disposable destination's directory, never a SQLite, lock, network or log write.
        char link[64], path[4096];
        std::snprintf(link, sizeof(link), "/proc/self/fd/%d", _fd);
        const auto count = ::readlink(link, path, sizeof(path));
        if (count > 0) {
            const std::string actual(path, static_cast<size_t>(count));
            const auto destination = target.toStdString();
            const auto folder = destination.substr(0, destination.rfind('/') + 1);
            if ((actual.compare(0, destination.size() + 1, destination + ".") == 0
                 && actual != destination + ".lock")
                || actual.compare(0, folder.size() + 1, folder + "#") == 0) {
                limitedDescriptor = _fd;
            }
        }
    }
    if (_fd != limitedDescriptor)
        return writeFile(_fd, _data, _size);
    struct rlimit original{ };
    if (getrlimit(RLIMIT_FSIZE, &original) != 0)
        std::abort();
    auto limit = original;
    limit.rlim_cur = 1;
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
        std::abort();
    const auto result = writeFile(_fd, _data, _size);
    if (setrlimit(RLIMIT_FSIZE, &original) != 0)
        std::abort();
    std::signal(SIGXFSZ, handler);
    faultFired = true;
    std::printf("fault: one-byte kernel limit staged write result=%lld requested=%zu\n",
                static_cast<long long>(result), _size);
    return result;
}

extern "C" int close(int _fd)
{
    using Close = int (*)(int);
    static const auto closeFile = reinterpret_cast<Close>(dlsym(RTLD_NEXT, "close"));
    if (!closeFile)
        std::abort();
    if (_fd == limitedDescriptor)
        limitedDescriptor = -1;
    return closeFile(_fd);
}

int main(int _argc, char** _argv)
{
    QApplication app(_argc, _argv);
    app.setApplicationName("GitHub download safety regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        QFontDatabase::addApplicationFont(font);
    }
    const auto root = qEnvironmentVariable("STARC_PROFILE_DIR");
    require(!root.isEmpty(), "test uses a disposable profile");
    mode = qEnvironmentVariable("STARC_DOWNLOAD_CASE");
    const auto asCopy = qEnvironmentVariable("STARC_DOWNLOAD_ROUTE") == "copy";
    const auto folder = root + "/downloads";
    require(QDir().mkpath(folder), "disposable download folder exists");
    const auto localPath = root + "/story.starc";
    const auto remotePath = root + "/remote-fixture.starc";
    const auto competitorPath = root + "/competitor.starc";
    externalTarget = root + "/external.starc";
    target = asCopy ? root + "/story (GitHub copy 2026-10-13 12.34.56).starc"
                    : folder + "/new-story.starc";
    qputenv("STARC_DOWNLOAD_TARGET", target.toUtf8());
    writeStory(remotePath, "REMOTE script: café — 雪\n\nOriginal annotations and IDs");
    writeStory(competitorPath, "CONCURRENT writer owns these exact script bytes");
    const auto downloaded = bytes(remotePath);
    competingBytes = bytes(competitorPath);
    GitHubFilesClient client;
    client.setRepository("writer/stories");
    client.setToken("test-token");
    const auto remoteName = asCopy ? QString("story.starc") : QString("new-story.starc");
    uploadRemote(client, remoteName, downloaded);
    QJsonObject initialLinks;
    if (asCopy) {
        writeStory(localPath, "BASE version");
        const auto base = bytes(localPath);
        initialLinks[localPath]
            = QJsonObject{ { "remote", remoteName },
                           { "sha", QString::fromLatin1(ProjectSync::gitBlobSha(base)) },
                           { "fingerprint",
                             QString::fromLatin1(ProjectSync::fingerprint(base).value) } };
        writeStory(localPath, "LOCAL conflicting script with retained edits");
    }
    setSettingsValue(DataStorageLayer::kGitHubSyncProjectsKey,
                     QJsonDocument(initialLinks).toJson(QJsonDocument::Compact).toHex());
    setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, "writer/stories");
    setSettingsValue(DataStorageLayer::kProjectSaveFolderKey, folder);
    setSettingsValue(DataStorageLayer::kApplicationBackupsFolderKey, root + "/backups");
    const auto baseline = settingsValue(DataStorageLayer::kGitHubSyncProjectsKey);
    const auto original = asCopy ? bytes(localPath) : QByteArray();
    QWidget host;
    host.resize(1400, 950);
    TaskBar::registerTaskBar(&host, Qt::black, Qt::white, Qt::blue);
    ProjectsManager projects(nullptr, &host);
    auto localProject = asCopy ? projects.addLocalProject(localPath) : nullptr;
    GitHubCredentialStore credentials(std::make_unique<Backend>(),
                                      { [] { return QString(); }, [] { return true; } });
    GitHubSyncManager sync(nullptr, &host, &projects, &credentials);
    auto offer = check(sync, host);

    // Create collisions only after the manager classified the remote entry.
    QLockFile owner(target + ".lock");
    if (mode == "locked-absent" || mode == "locked-existing")
        require(owner.tryLock(), "other instance owns the destination lock");
    if (mode == "exists" || mode == "locked-existing")
        require(QFile::copy(competitorPath, target), "competing story appears after remote check");
    if (mode == "symlink" || mode == "dangling") {
        if (mode == "symlink")
            require(QFile::copy(competitorPath, externalTarget), "symlink target exists");
        require(QFile::link(externalTarget, target), "destination symlink exists");
    }
    if (mode == "directory")
        require(QDir().mkdir(target), "destination is a directory");
    const auto targetFolder = QFileInfo(target).absolutePath();
    if (mode == "unwritable-stage")
        require(QFile::setPermissions(targetFolder, QFile::ReadOwner | QFile::ExeOwner),
                "staging directory is unwritable");

    auto startDownload = [&](Dialog* _offer) {
        if (asCopy) {
            require(localProject->syncStatus() == ProjectSyncStatus::Diverged,
                    "original story remains a conflict");
            answer(_offer, "Close");
            const auto actions = sync.projectActions(localProject);
            require(actions.size() == 3, "conflict offers separate-copy action");
            actions.last()->trigger();
            qDeleteAll(actions);
        } else {
            answer(_offer, "Sync");
        }
    };
    const auto before = dialogs(host);
    armed = true;
    startDownload(offer);
    waitUntil(
        [&] { return projects.project(target) != nullptr || nextDialog(host, before) != nullptr; });
    armed = false;
    if (mode == "unwritable-stage")
        require(QFile::setPermissions(targetFolder,
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
                "staging permissions recover");
    if (mode.startsWith("race-") || mode == "write-failure" || mode == "publish-failure")
        require(faultFired, "requested native fault actually fired");

    const auto expectSuccess = mode == "success";
    require((projects.project(target) != nullptr) == expectSuccess,
            "only successful publication registers a new local story");
    if (!expectSuccess) {
        require(nextDialog(host, before) != nullptr, "refusal reports a sync error");
        require(settingsValue(DataStorageLayer::kGitHubSyncProjectsKey) == baseline,
                "refusal preserves exact persisted baseline");
        require(projects.localProjects().size() == (asCopy ? 1 : 0),
                "refusal does not register a phantom project");
        if (mode == "exists" || mode == "locked-existing" || mode == "race-file")
            require(bytes(target) == competingBytes, "competing story bytes survive refusal");
        else if (mode == "symlink" || mode == "dangling" || mode == "race-symlink") {
            require(QFileInfo(target).isSymLink(), "refusal preserves the symlink");
            require(mode == "symlink" ? bytes(externalTarget) == competingBytes
                                      : !QFileInfo::exists(externalTarget),
                    "refusal leaves symlink target unchanged or absent");
        } else if (mode == "directory" || mode == "race-directory")
            require(QFileInfo(target).isDir(), "refusal preserves competing directory");
        else
            require(!QFileInfo::exists(target), "failed download creates no destination");
        require(!QFileInfo::exists(root + "/backups"), "new-download refusal creates no backup");
        if (owner.isLocked()) {
            require(QFileInfo::exists(target + ".lock"), "competing lock survives refusal");
            owner.unlock();
        }
        // Retain the failed destination for independent byte/SQLite/symlink inspection.
        if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) {
            if (QFileInfo(target).isDir())
                require(QDir().rename(target, target + ".refused"), "retain refused directory");
            else
                require(QFile::rename(target, target + ".refused"), "retain refused destination");
        }
        QLockFile released(target + ".lock");
        require(released.tryLock(), "failed download releases its destination lock");
        released.unlock();
        answer(nextDialog(host, before), "Close");
        mode = "success";
        publicationSeen = false;
        armed = true;
        const auto retryOffer = check(sync, host);
        const auto retryBefore = dialogs(host);
        startDownload(retryOffer);
        waitUntil([&] {
            return projects.project(target) != nullptr || nextDialog(host, retryBefore) != nullptr;
        });
        armed = false;
        require(projects.project(target) != nullptr,
                "remote entry remains downloadable after removing collision or fault");
    }
    require(publicationSeen && bytes(target) == downloaded,
            "successful publication is locked and byte-exact");
    if (asCopy) {
        require(bytes(localPath) == original
                    && settingsValue(DataStorageLayer::kGitHubSyncProjectsKey) == baseline
                    && projects.project(target)->syncStatus() == ProjectSyncStatus::NotSynced,
                "copy preserves original bytes, baseline and unlinked status");
        sync.updateProjectState(localPath);
        require(localProject->syncStatus() == ProjectSyncStatus::Diverged,
                "copy preserves original cached conflict baseline");
    } else {
        checkLink(target, downloaded);
        sync.updateProjectState(target);
        require(projects.project(target)->syncStatus() == ProjectSyncStatus::InSync,
                "new story installs its cached sync baseline only after publication");
    }
    checkRemote(client, remoteName, downloaded);
    QLockFile released(target + ".lock");
    require(released.tryLock(), "successful download releases destination lock");
    released.unlock();
    auto leftover = QDir(QFileInfo(target).absolutePath())
                        .entryList({ QFileInfo(target).fileName() + ".*" },
                                   QDir::Files | QDir::Hidden | QDir::System);
    leftover.removeAll(QFileInfo(target).fileName() + ".refused");
    require(leftover.isEmpty(), "successful and failed downloads leave no staging files or locks");
    QFile manifest(root + "/target.txt");
    require(manifest.open(QIODevice::WriteOnly) && manifest.write(target.toUtf8()) > 0,
            "retain target for independent fresh-process oracle");
    std::puts("GitHub download safety passed; ordinary exit.");
    return 0;
}
