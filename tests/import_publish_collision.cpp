// Linux test-only interposer: another writer creates the destination immediately
// before publication. It is loaded only for the disposable import-race case.
#include <QFile>
#include <QTemporaryFile>

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

bool QTemporaryFile::rename(const QString& _newName)
{
    using Rename = bool (*)(QTemporaryFile*, const QString&);
    static const auto renameFile
        = reinterpret_cast<Rename>(dlsym(RTLD_NEXT, "_ZN14QTemporaryFile6renameERK7QString"));
    if (!renameFile) {
        std::abort();
    }
    if (qEnvironmentVariable("STARC_STARTUP_TEST_MODE") == "import-race"
        && _newName.endsWith("/import-race/source.fountain.starc")) {
        QFile otherWriter(_newName);
        const QByteArray bytes("Another writer owns these bytes.\n");
        if (!otherWriter.open(QIODevice::WriteOnly | QIODevice::NewOnly)
            || otherWriter.write(bytes) != bytes.size() || !otherWriter.flush()) {
            std::abort();
        }
        otherWriter.close();
        std::puts("ok: another writer creates the destination immediately before publication");
    }
    return renameFile(this, _newName);
}
