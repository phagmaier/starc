#pragma once

#include <QObject>

#include <corelib_global.h>
#include <functional>
#include <memory>

/** Native jobs and deterministic test backends share this asynchronous interface. */
class CORE_LIBRARY_EXPORT GitHubCredentialBackend
{
public:
    enum class Result { Success, Missing, Denied, Unavailable, Failed };
    using Callback = std::function<void(Result, const QString&)>;
    virtual ~GitHubCredentialBackend() = default;
    virtual void read(Callback _callback) = 0;
    virtual void write(const QString& _token, Callback _callback) = 0;
    virtual void remove(Callback _callback) = 0;
};

/** QtKeychain adapter. No plaintext fallback; service/key never contain a credential. */
CORE_LIBRARY_EXPORT std::unique_ptr<GitHubCredentialBackend> nativeGitHubCredentialBackend(
    const QString& _service);

/** One credential boundary for onboarding, preferences, sync, and normal shutdown. */
class CORE_LIBRARY_EXPORT GitHubCredentialStore : public QObject
{
    Q_OBJECT
public:
    struct LegacySettings {
        std::function<QString()> read;
        // Must confirm durable removal, and preserve the value on failure.
        std::function<bool()> remove;
    };
    GitHubCredentialStore(std::unique_ptr<GitHubCredentialBackend> _backend, LegacySettings _legacy,
                          QObject* _parent = nullptr);
    ~GitHubCredentialStore() override;
    static GitHubCredentialStore* instance();

    void start();
    void stageToken(const QString& _token); // memory only, never one job per keystroke
    void save();
    void retry();
    bool isBusy() const;
    bool isReady() const;
    bool hasPendingChange() const;
    QString token() const; // last confirmed credential, or retained legacy on migration failure
    QString fieldToken() const; // keeps a newer unsaved edit across asynchronous completions
    QString problem() const;
    QString statusText() const;
    void whenSettled(QObject* _context, std::function<void()> _callback);

signals:
    void changed();
    void settled();

private:
    using Result = GitHubCredentialBackend::Result;
    using Callback = GitHubCredentialBackend::Callback;
    Callback completion(std::function<void(Result, const QString&)> _callback);
    void load();
    void store(const QString& _token, bool _migration);
    void process();
    bool removeLegacy();
    void finish();
    void fail(Result _result, bool _migration);

    std::unique_ptr<GitHubCredentialBackend> m_backend;
    LegacySettings m_legacySettings;
    QString m_token, m_legacy, m_draft, m_problem;
    bool m_started = false, m_active = false, m_loaded = false, m_readKnown = false;
    bool m_keyringHasEntry = false, m_dirty = false, m_saveRequested = false;
    quint64 m_operation = 0, m_revision = 0;
};
