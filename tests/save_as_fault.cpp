// Linux test-only interposer, restricted to the selected disposable destination.
#include <data_layer/storage/github_credential_store.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTimer>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <string>
#include <unistd.h>

namespace {
int limitedDescriptor = -1;

class MemoryCredentials : public GitHubCredentialBackend
{
public:
    void read(Callback _callback) override
    {
        const auto token = m_token;
        QTimer::singleShot(0, qApp, [_callback, token] {
            _callback(token.isEmpty() ? Result::Missing : Result::Success, token);
        });
    }
    void write(const QString& _token, Callback _callback) override
    {
        m_token = _token;
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }
    void remove(Callback _callback) override
    {
        m_token.clear();
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }

private:
    QString m_token;
};

bool selected(const QString& _path)
{
    const auto target = qEnvironmentVariable("STARC_SAVE_AS_TARGET");
    return !target.isEmpty() && (_path == target || _path.startsWith(target + "."));
}

void limitStaging(QFileDevice* _output)
{
    if (selected(_output->fileName())
        && qEnvironmentVariable("STARC_STARTUP_TEST_MODE").startsWith("save-as-write-failure")) {
        limitedDescriptor = _output->handle();
        if (limitedDescriptor < 0) {
            std::abort();
        }
    }
}

template<typename Operation>
ssize_t limitedWrite(Operation _write)
{
    struct rlimit original{ };
    if (getrlimit(RLIMIT_FSIZE, &original) != 0) {
        std::abort();
    }
    auto limited = original;
    limited.rlim_cur = 1;
    const auto handler = std::signal(SIGXFSZ, SIG_IGN);
    if (setrlimit(RLIMIT_FSIZE, &limited) != 0) {
        std::abort();
    }
    const auto result = _write();
    if (setrlimit(RLIMIT_FSIZE, &original) != 0) {
        std::abort();
    }
    std::signal(SIGXFSZ, handler);
    std::printf("fault: kernel-limited staged write returned %lld\n",
                static_cast<long long>(result));
    return result;
}

template<typename Operation>
bool publish(const QString& _target, Operation _publish)
{
    QLockFile competitor(_target + ".lock");
    const bool locked = !competitor.tryLock();
    std::printf("publication: destination lock held=%d\n", locked);
    if (!locked) {
        std::abort();
    }
    const auto mode = qEnvironmentVariable("STARC_STARTUP_TEST_MODE");
    if (mode == "save-as-race") {
        QFile writer(_target);
        const QByteArray bytes("Concurrent publisher owns this destination.\n");
        if (!writer.open(QIODevice::WriteOnly | QIODevice::NewOnly)
            || writer.write(bytes) != bytes.size() || !writer.flush()) {
            std::abort();
        }
        writer.close();
        std::puts("fault: competing destination published immediately before rename");
    }
    if (mode.startsWith("save-as-publish-failure")) {
        const auto folder = QFile::encodeName(QFileInfo(_target).absolutePath());
        struct stat original{ };
        if (::stat(folder.constData(), &original) != 0
            || ::chmod(folder.constData(), original.st_mode & ~0222) != 0) {
            std::abort();
        }
        const auto result = _publish();
        if (::chmod(folder.constData(), original.st_mode & 07777) != 0) {
            std::abort();
        }
        std::printf("fault: final publication refused=%d\n", !result);
        return result;
    }
    return _publish();
}
} // namespace

// No desktop bus or native provider is contacted by this Save As regression.
GitHubCredentialStore* GitHubCredentialStore::instance()
{
    static auto store = [] {
        std::puts("test: injected asynchronous memory credential store");
        return new GitHubCredentialStore(std::make_unique<MemoryCredentials>(),
                                         { [] { return QString(); }, [] { return true; } }, qApp);
    }();
    return store;
}

bool QSaveFile::open(OpenMode _mode)
{
    using Open = bool (*)(QSaveFile*, OpenMode);
    static const auto openFile = reinterpret_cast<Open>(
        dlsym(RTLD_NEXT, "_ZN9QSaveFile4openE6QFlagsIN13QIODeviceBase12OpenModeFlagEE"));
    if (!openFile) {
        std::abort();
    }
    const auto result = openFile(this, _mode);
    if (result) {
        limitStaging(this);
    }
    return result;
}

extern "C" ssize_t write(int _fd, const void* _data, size_t _length)
{
    using Write = ssize_t (*)(int, const void*, size_t);
    static const auto writeFile = reinterpret_cast<Write>(dlsym(RTLD_NEXT, "write"));
    if (!writeFile) {
        std::abort();
    }
    // QTemporaryFile's inline open uses Qt's internal vtable. Identify its actual
    // descriptor here, including an unnamed O_TMPFILE in the isolated case directory.
    const auto mode = std::getenv("STARC_STARTUP_TEST_MODE");
    const auto target = std::getenv("STARC_SAVE_AS_TARGET");
    if (limitedDescriptor < 0 && mode && target
        && std::string(mode) == "save-as-write-failure-new") {
        char link[64];
        char path[4096];
        std::snprintf(link, sizeof(link), "/proc/self/fd/%d", _fd);
        const auto count = ::readlink(link, path, sizeof(path));
        if (count > 0) {
            const std::string actual(path, static_cast<size_t>(count));
            const std::string destination(target);
            const auto folder = destination.substr(0, destination.rfind('/') + 1);
            if ((actual.compare(0, destination.size() + 1, destination + ".") == 0
                 && actual != destination + ".lock")
                || actual.compare(0, folder.size() + 1, folder + "#") == 0) {
                limitedDescriptor = _fd;
            }
        }
    }
    return _fd == limitedDescriptor ? limitedWrite([&] { return writeFile(_fd, _data, _length); })
                                    : writeFile(_fd, _data, _length);
}

extern "C" int close(int _fd)
{
    using Close = int (*)(int);
    static const auto closeFile = reinterpret_cast<Close>(dlsym(RTLD_NEXT, "close"));
    if (!closeFile) {
        std::abort();
    }
    if (_fd == limitedDescriptor) {
        limitedDescriptor = -1;
    }
    return closeFile(_fd);
}

bool QSaveFile::commit()
{
    using Commit = bool (*)(QSaveFile*);
    static const auto commitFile
        = reinterpret_cast<Commit>(dlsym(RTLD_NEXT, "_ZN9QSaveFile6commitEv"));
    if (!commitFile) {
        std::abort();
    }
    return selected(fileName()) ? publish(fileName(), [&] { return commitFile(this); })
                                : commitFile(this);
}

bool QTemporaryFile::rename(const QString& _target)
{
    using Rename = bool (*)(QTemporaryFile*, const QString&);
    static const auto renameFile
        = reinterpret_cast<Rename>(dlsym(RTLD_NEXT, "_ZN14QTemporaryFile6renameERK7QString"));
    if (!renameFile) {
        std::abort();
    }
    return selected(_target) ? publish(_target, [&] { return renameFile(this, _target); })
                             : renameFile(this, _target);
}
