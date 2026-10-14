// Linux test-only fault at QSaveFile publication; only the selected disposable
// output is affected. SQLite and application settings remain writable.
#include <sys/resource.h>
#include <sys/stat.h>

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

bool QSaveFile::commit()
{
    using Commit = bool (*)(QSaveFile*);
    static const auto commitFile
        = reinterpret_cast<Commit>(dlsym(RTLD_NEXT, "_ZN9QSaveFile6commitEv"));
    if (!commitFile) {
        std::abort();
    }
    const auto fault = qEnvironmentVariable("STARC_EXPLICIT_EXPORT_FAULT");
    if (fault.isEmpty() || fileName() != qEnvironmentVariable("STARC_EXPLICIT_EXPORT_TARGET")) {
        return commitFile(this);
    }

    bool result = false;
    if (fault == "partial") {
        // The small authored output is buffered until commit. Allow a real prefix
        // write, then make the kernel reject the remainder with EFBIG.
        struct rlimit original{ };
        if (getrlimit(RLIMIT_FSIZE, &original) != 0) {
            std::abort();
        }
        auto limited = original;
        // The output size differs across format/input pairs. Reject the second half
        // of this actual output, rather than assuming it exceeds a fixed byte count.
        if (pos() < 2) {
            std::abort();
        }
        limited.rlim_cur = static_cast<rlim_t>(pos() / 2);
        const auto handler = std::signal(SIGXFSZ, SIG_IGN);
        if (setrlimit(RLIMIT_FSIZE, &limited) != 0) {
            std::abort();
        }
        result = commitFile(this);
        if (setrlimit(RLIMIT_FSIZE, &original) != 0) {
            std::abort();
        }
        std::signal(SIGXFSZ, handler);
    } else if (fault == "publish") {
        // Staging and all writes have succeeded. Refuse the actual final rename
        // by removing write permission from the disposable destination directory.
        const auto folder = QFile::encodeName(QFileInfo(fileName()).absolutePath());
        struct stat original{ };
        if (::stat(folder.constData(), &original) != 0
            || ::chmod(folder.constData(), original.st_mode & ~0222) != 0) {
            std::abort();
        }
        result = commitFile(this);
        if (::chmod(folder.constData(), original.st_mode & 07777) != 0) {
            std::abort();
        }
    } else {
        std::abort();
    }
    std::printf("fault: %s at explicit export publication; result=%d\n", qPrintable(fault), result);
    return result;
}
