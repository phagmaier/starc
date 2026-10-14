#pragma once

#include <data_layer/storage/github_credential_store.h>

#include <QApplication>
#include <QTimer>

// Application fixtures use no native keyring. Keep its asynchronous lifecycle
// while returning no credential from this disposable in-memory backend.
namespace {
class IsolatedGitHubCredentials final : public GitHubCredentialBackend
{
public:
    void read(Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Missing, { }); });
    }
    void write(const QString&, Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }
    void remove(Callback _callback) override
    {
        QTimer::singleShot(0, qApp, [_callback] { _callback(Result::Success, { }); });
    }
};
} // namespace

GitHubCredentialStore* GitHubCredentialStore::instance()
{
    static auto store
        = new GitHubCredentialStore(std::make_unique<IsolatedGitHubCredentials>(),
                                    { [] { return QString(); }, [] { return true; } }, qApp);
    return store;
}
