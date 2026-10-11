#include <utils/tools/github_files_client.h>
#include <utils/tools/project_sync.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {
void require(bool _condition, const char* _message)
{
    if (!_condition) {
        std::fprintf(stderr, "FAIL: %s\n", _message);
        std::exit(1);
    }
    std::printf("ok: %s\n", _message);
}

/**
 * @brief Run SQL statements against a project file the way the application stores documents
 */
void execute(const QString& _path, const QStringList& _statements)
{
    {
        auto database = QSqlDatabase::addDatabase("QSQLITE", "test");
        database.setDatabaseName(_path);
        require(database.open(), "open test project");
        QSqlQuery query(database);
        for (const auto& statement : _statements) {
            if (!query.exec(statement)) {
                std::fprintf(stderr, "FAIL: %s\n", qPrintable(statement));
                std::exit(1);
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase("test");
}

QByteArray content(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read test project");
    return file.readAll();
}

/**
 * @brief Wait until the request started by the given function calls the done function
 */
void wait(const std::function<void(std::function<void()>)>& _request)
{
    QEventLoop loop;
    QTimer::singleShot(15000, &loop, [] {
        std::fprintf(stderr, "FAIL: request timed out\n");
        std::exit(1);
    });
    _request([&loop] { loop.quit(); });
    loop.exec();
}

void checkStates()
{
    using ProjectSync::State;

    require(ProjectSync::state({ "l", {}, {}, {} }) == State::LocalOnly, "no remote copy");
    require(ProjectSync::state({ "l", {}, "l", "r" }) == State::LocalOnly,
            "remote copy was removed");
    require(ProjectSync::state({ {}, "r", {}, {} }) == State::RemoteOnly, "no local file");
    require(ProjectSync::state({ "l", "r", "l", "r" }) == State::InSync, "nothing changed");
    require(ProjectSync::state({ "l2", "r", "l", "r" }) == State::LocalChanged,
            "only local changed");
    require(ProjectSync::state({ "l", "r2", "l", "r" }) == State::RemoteChanged,
            "only remote changed");
    require(ProjectSync::state({ "l2", "r2", "l", "r" }) == State::Diverged, "both changed");
    require(ProjectSync::state({ "l", "r", {}, {} }) == State::Diverged,
            "never synced copies need comparing");

    require(ProjectSync::resolveDiverged({ "l2", "r2", "l", "r" }, "l2") == State::InSync,
            "same changes on both sides");
    require(ProjectSync::resolveDiverged({ "l2", "r2", "l", "r" }, "l") == State::LocalChanged,
            "remote file rewritten without changes");
    require(ProjectSync::resolveDiverged({ "l2", "r2", "l", "r" }, "x") == State::Diverged,
            "different changes on both sides");
    require(ProjectSync::resolveDiverged({ "l", "r", {}, {} }, "l") == State::InSync,
            "never synced copies with the same content");
    require(ProjectSync::resolveDiverged({ "l", "r", {}, {} }, "x") == State::Diverged,
            "never synced copies with different content");
}

void checkFingerprint(const QString& _dir)
{
    require(ProjectSync::gitBlobSha({}) == "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391",
            "git sha of an empty file");
    require(ProjectSync::gitBlobSha("hello\n") == "ce013625030ba8dba906f756967f9e9ca394464a",
            "git sha of a text file");

    const auto path = _dir + "/story.starc";
    execute(path,
            { "CREATE TABLE system_variables (variable TEXT PRIMARY KEY, value TEXT NOT NULL)",
              "CREATE TABLE documents (id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT UNIQUE "
              "NOT NULL, type INTEGER NOT NULL DEFAULT(0), content BLOB DEFAULT(NULL), "
              "synced_at TEXT DEFAULT(NULL))",
              "INSERT INTO documents (uuid, type, content) VALUES ('{b}', 10104, "
              "CAST('<scene>INT. KITCHEN</scene>' AS BLOB))",
              "INSERT INTO documents (uuid, type, content) VALUES ('{a}', 1, "
              "CAST('<structure/>' AS BLOB))" });
    const auto original = ProjectSync::fingerprint(path);
    const auto originalContent = content(path);
    require(original.isValid(), "fingerprint of a project");
    require(ProjectSync::fingerprint(originalContent).value == original.value,
            "fingerprint of the project content equals the one of the file");

    //
    // What the application does with a project which was only opened
    //
    execute(path,
            { "INSERT INTO system_variables VALUES ('application-version', '0.8.4')",
              "UPDATE documents SET content = CAST(content AS TEXT) WHERE uuid = '{b}'",
              "INSERT INTO documents (uuid, type) VALUES ('{c}', 10103)" });
    require(content(path) != originalContent, "opened project file has other bytes");
    require(ProjectSync::fingerprint(path).value == original.value,
            "opened project has the same fingerprint");

    execute(path,
            { "UPDATE documents SET content = CAST('<scene>EXT. STREET</scene>' AS BLOB) WHERE "
              "uuid = '{b}'" });
    const auto edited = ProjectSync::fingerprint(path);
    require(edited.isValid() && edited.value != original.value,
            "edited project has another fingerprint");

    execute(path, { "DELETE FROM documents WHERE uuid = '{a}'" });
    require(ProjectSync::fingerprint(path).value != edited.value,
            "project without a document has another fingerprint");

    require(!ProjectSync::fingerprint(QByteArray("not a project")).isValid(),
            "broken content has no fingerprint");
    require(!ProjectSync::fingerprint(_dir + "/missing.starc").isValid(),
            "missing file has no fingerprint");

    QFile journal(path + "-journal");
    require(journal.open(QIODevice::WriteOnly) && journal.write("x") == 1,
            "create journal of unfinished changes");
    journal.close();
    require(!ProjectSync::fingerprint(path).isValid(),
            "project with unfinished changes has no fingerprint");
}

void checkClient()
{
    using Error = GitHubFilesClient::Error;

    GitHubFilesClient client;
    client.setRepository(" https://github.com/writer/stories.git ");
    client.setToken("test-token");

    wait([&](std::function<void()> _done) {
        client.loadRepositoryInfo(
            [_done](const Error& _error, const GitHubFilesClient::RepositoryInfo& _info) {
                require(!_error.isValid() && _info.isPrivate && _info.canPush,
                        "repository is available and private");
                _done();
            });
    });
    wait([&](std::function<void()> _done) {
        client.loadFiles(
            [_done](const Error& _error, const QVector<GitHubFilesClient::FileInfo>& _files) {
                require(!_error.isValid() && _files.isEmpty(), "empty repository has no files");
                _done();
            });
    });

    const QString name = "My story & more #1.starc";
    const QByteArray first("first version\0with zero", 23);
    const QByteArray second = "second version";
    wait([&](std::function<void()> _done) {
        client.upload(name, first, {}, "Add story from desk",
                      [&, _done](const Error& _error, const QByteArray& _sha) {
                          require(!_error.isValid() && _sha == ProjectSync::gitBlobSha(first),
                                  "new file is uploaded");
                          _done();
                      });
    });
    wait([&](std::function<void()> _done) {
        client.loadFiles(
            [&, _done](const Error& _error, const QVector<GitHubFilesClient::FileInfo>& _files) {
                require(!_error.isValid() && _files.size() == 1 && _files.first().name == name
                            && _files.first().sha == ProjectSync::gitBlobSha(first)
                            && _files.first().size == first.size(),
                        "uploaded file is listed");
                _done();
            });
    });
    wait([&](std::function<void()> _done) {
        client.download(name, [&, _done](const Error& _error, const QByteArray& _content) {
            require(!_error.isValid() && _content == first, "file is downloaded unchanged");
            _done();
        });
    });
    wait([&](std::function<void()> _done) {
        client.upload(name, second, {}, "Add story again",
                      [_done](const Error& _error, const QByteArray&) {
                          require(_error.type == Error::Type::Conflict,
                                  "existing file isn't replaced by a new one");
                          _done();
                      });
    });
    wait([&](std::function<void()> _done) {
        client.upload(name, second, ProjectSync::gitBlobSha("other"), "Update story",
                      [_done](const Error& _error, const QByteArray&) {
                          require(_error.type == Error::Type::Conflict,
                                  "file changed by someone else isn't replaced");
                          _done();
                      });
    });
    wait([&](std::function<void()> _done) {
        client.download(name, [&, _done](const Error& _error, const QByteArray& _content) {
            require(!_error.isValid() && _content == first, "refused uploads change nothing");
            _done();
        });
    });
    wait([&](std::function<void()> _done) {
        client.upload(name, second, ProjectSync::gitBlobSha(first), "Update story from laptop",
                      [&, _done](const Error& _error, const QByteArray& _sha) {
                          require(!_error.isValid() && _sha == ProjectSync::gitBlobSha(second),
                                  "known version is replaced");
                          _done();
                      });
    });
    wait([&](std::function<void()> _done) {
        client.loadLastChange(
            name, [_done](const Error& _error, const GitHubFilesClient::ChangeInfo& _info) {
                require(!_error.isValid() && _info.message == "Update story from laptop"
                            && _info.dateTime.isValid(),
                        "last change is known");
                _done();
            });
    });
    wait([&](std::function<void()> _done) {
        client.download("missing.starc", [_done](const Error& _error, const QByteArray&) {
            require(_error.type == Error::Type::NotFound, "missing file is reported");
            _done();
        });
    });

    client.setToken("wrong-token");
    wait([&](std::function<void()> _done) {
        client.loadRepositoryInfo(
            [_done](const Error& _error, const GitHubFilesClient::RepositoryInfo&) {
                require(_error.type == Error::Type::Unauthorized, "wrong token is reported");
                _done();
            });
    });

    client.setApiUrl("http://127.0.0.1:9");
    wait([&](std::function<void()> _done) {
        client.loadRepositoryInfo(
            [_done](const Error& _error, const GitHubFilesClient::RepositoryInfo&) {
                require(_error.type == Error::Type::Network, "unreachable server is reported");
                _done();
            });
    });
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    QTemporaryDir dir;
    require(dir.isValid(), "create temporary directory");

    checkStates();
    checkFingerprint(dir.path());
    checkClient();

    std::printf("PASS\n");
    return 0;
}
