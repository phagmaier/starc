#include "github_credential_store.h"

#include "settings_storage.h"
#include "storage_facade.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QPointer>
#include <QTimer>
#include <qt6keychain/keychain.h>

namespace {
using Result = GitHubCredentialBackend::Result;
Result result(QKeychain::Error _error)
{
    switch (_error) {
    case QKeychain::NoError:
        return Result::Success;
    case QKeychain::EntryNotFound:
        return Result::Missing;
    case QKeychain::AccessDenied:
    case QKeychain::AccessDeniedByUser:
        return Result::Denied;
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        return Result::Unavailable;
    default:
        return Result::Failed;
    }
}

class NativeBackend : public QObject, public GitHubCredentialBackend
{
public:
    explicit NativeBackend(const QString& _service)
        : m_service(_service)
    {
    }
    void read(Callback _callback) override
    {
        auto job = new QKeychain::ReadPasswordJob(m_service, this);
        configure(job);
        connect(job, &QKeychain::Job::finished, this, [job, _callback] {
            _callback(result(job->error()),
                      job->error() == QKeychain::NoError ? job->textData() : QString());
        });
        job->start();
    }
    void write(const QString& _token, Callback _callback) override
    {
        auto job = new QKeychain::WritePasswordJob(m_service, this);
        configure(job);
        job->setTextData(_token);
        connect(job, &QKeychain::Job::finished, this,
                [job, _callback] { _callback(result(job->error()), {}); });
        job->start();
    }
    void remove(Callback _callback) override
    {
        auto job = new QKeychain::DeletePasswordJob(m_service, this);
        configure(job);
        connect(job, &QKeychain::Job::finished, this,
                [job, _callback] { _callback(result(job->error()), {}); });
        job->start();
    }

private:
    static void configure(QKeychain::Job* _job)
    {
        _job->setKey(QStringLiteral("github-story-sync-token"));
        _job->setInsecureFallback(false);
    }
    QString m_service;
};
} // namespace

std::unique_ptr<GitHubCredentialBackend> nativeGitHubCredentialBackend(const QString& _service)
{
    return std::make_unique<NativeBackend>(_service);
}

GitHubCredentialStore::GitHubCredentialStore(std::unique_ptr<GitHubCredentialBackend> _backend,
                                             LegacySettings _legacy, QObject* _parent)
    : QObject(_parent)
    , m_backend(std::move(_backend))
    , m_legacySettings(std::move(_legacy))
{
}
GitHubCredentialStore::~GitHubCredentialStore() = default;

GitHubCredentialStore* GitHubCredentialStore::instance()
{
    static auto store = [] {
        QString service = QStringLiteral("app.starc.screenplay.github");
        // Development profiles never address the ordinary profile's credential.
        if (const auto profile = qEnvironmentVariable("STARC_PROFILE_DIR"); !profile.isEmpty()) {
            service += ".profile."
                + QString::fromLatin1(
                           QCryptographicHash::hash(profile.toUtf8(), QCryptographicHash::Sha256)
                               .toHex());
        }
        auto settings = DataStorageLayer::StorageFacade::settingsStorage();
        return new GitHubCredentialStore(
            nativeGitHubCredentialBackend(service),
            { [settings] { return settings->legacyGitHubToken(); },
              [settings] { return settings->removeLegacyGitHubToken(); } },
            qApp);
    }();
    return store;
}

void GitHubCredentialStore::start()
{
    if (m_started)
        return;
    m_started = true;
    m_legacy = m_legacySettings.read();
    load();
}

GitHubCredentialStore::Callback GitHubCredentialStore::completion(Callback _callback)
{
    const auto operation = ++m_operation;
    m_active = true;
    emit changed();
    return [guard = QPointer<GitHubCredentialStore>(this), operation,
            _callback](Result _result, const QString& _value) {
        if (!guard || !guard->m_active || guard->m_operation != operation)
            return;
        // Invalidate even duplicate deliveries before invoking the next operation.
        ++guard->m_operation;
        _callback(_result, _value);
    };
}

void GitHubCredentialStore::load()
{
    m_problem.clear();
    m_backend->read(completion([this](Result _result, const QString& _value) {
        m_loaded = true;
        if (_result != Result::Success && _result != Result::Missing) {
            if (!m_readKnown)
                m_token = m_legacy;
            m_readKnown = false;
            fail(_result, !m_legacy.isEmpty());
            return;
        }
        m_readKnown = true;
        // Even an explicitly empty native entry is authoritative over stale legacy data.
        m_keyringHasEntry = _result == Result::Success;
        m_token = m_keyringHasEntry ? _value : m_legacy;
        if (!m_legacy.isEmpty()) {
            if (!m_keyringHasEntry) {
                store(m_legacy, true);
                return;
            }
            // The keyring wins. Never replace it with possibly stale plaintext.
            removeLegacy();
        }
        finish();
    }));
}

void GitHubCredentialStore::stageToken(const QString& _token)
{
    m_draft = _token.trimmed();
    ++m_revision;
    // An in-flight write may change the committed token after this edit. In particular,
    // clearing a first write must remain an intent even while the old token is still empty.
    m_dirty = m_active || !m_loaded || m_draft != m_token;
    emit changed();
}
void GitHubCredentialStore::save()
{
    m_saveRequested = true;
    if (!m_started) {
        start();
        return;
    }
    if (m_active)
        return;
    if (!m_dirty && !m_problem.isEmpty()) {
        retry();
        return;
    }
    m_problem.clear();
    if (!m_readKnown) {
        load();
        return;
    }
    process();
}
void GitHubCredentialStore::retry()
{
    if (m_active)
        return;
    m_saveRequested = m_dirty;
    if (!m_started) {
        start();
        return;
    }
    load();
}
void GitHubCredentialStore::process()
{
    if (m_saveRequested && m_dirty && m_readKnown) {
        m_saveRequested = false;
        store(m_draft, false);
        return;
    }
    m_saveRequested = false;
    m_active = false;
    emit changed();
    emit settled();
}

bool GitHubCredentialStore::removeLegacy()
{
    if (m_legacy.isEmpty())
        return true;
    if (!m_legacySettings.remove()) {
        m_problem
            = tr("The system keyring entry is confirmed, but the old settings value could not "
                 "be removed. Check settings-file permissions and retry.");
        return false;
    }
    m_legacy.clear();
    return true;
}

void GitHubCredentialStore::store(const QString& _token, bool _migration)
{
    const auto revision = m_revision;
    // Removing the sole legacy credential before a failed delete would lose it.
    // Migration must establish a confirmed secure copy before clearing is possible.
    if (_token.isEmpty() && !m_legacy.isEmpty() && (!m_keyringHasEntry || !removeLegacy())) {
        if (m_problem.isEmpty())
            m_problem = tr("The existing token must be secured before it can "
                           "be cleared. Unlock the system keyring and retry.");
        finish();
        return;
    }
    auto written = completion([this, _token, _migration, revision](Result _result, const QString&) {
        if (_result != Result::Success && !(_token.isEmpty() && _result == Result::Missing)) {
            // A newer edit may still be queued, but don't retry the failed edit in a loop.
            if (m_revision == revision)
                m_saveRequested = false;
            fail(_result, _migration);
            return;
        }
        // Confirm through a separate native read before removing plaintext or claiming success.
        m_backend->read(
            completion([this, _token, _migration, revision](Result _result, const QString& _value) {
                const bool confirmed = _token.isEmpty()
                    ? _result == Result::Missing || (_result == Result::Success && _value.isEmpty())
                    : _result == Result::Success && _value == _token;
                if (!confirmed) {
                    m_readKnown = false;
                    m_saveRequested = false;
                    fail(_result == Result::Success ? Result::Failed : _result, _migration);
                    return;
                }
                m_token = _token;
                m_keyringHasEntry = !_token.isEmpty();
                m_problem.clear();
                if (!_token.isEmpty())
                    removeLegacy();
                if (!_migration && m_revision == revision)
                    m_dirty = false;
                finish();
            }));
    });
    if (_token.isEmpty())
        m_backend->remove(std::move(written));
    else
        m_backend->write(_token, std::move(written));
}

void GitHubCredentialStore::fail(Result _result, bool _migration)
{
    if (_result == Result::Denied) {
        m_problem = tr("The system keyring is locked or access was denied. Unlock it and retry.");
    } else if (_result == Result::Unavailable) {
        m_problem = tr("No system keyring is available. Start a Secret Service or KWallet provider "
                       "and retry. Local story editing remains available.");
    } else {
        m_problem = tr("The system keyring change could not be confirmed. Retry before closing.");
    }
    if (_migration)
        m_problem += tr(" The old token is retained in settings and remains usable.");
    else if (m_dirty)
        m_problem += m_token.isEmpty()
            ? tr(" The token edit is not saved.")
            : tr(" The previous confirmed token is still used; the edit is not saved.");
    finish();
}
void GitHubCredentialStore::finish()
{
    m_active = false;
    // Edits accepted while a write/read was in flight are serialized after that operation.
    process();
}
bool GitHubCredentialStore::isBusy() const
{
    return !m_started || m_active;
}
bool GitHubCredentialStore::isReady() const
{
    return m_loaded && !isBusy();
}
bool GitHubCredentialStore::hasPendingChange() const
{
    return m_dirty;
}
QString GitHubCredentialStore::token() const
{
    return m_token;
}
QString GitHubCredentialStore::fieldToken() const
{
    return m_dirty ? m_draft : m_token;
}
QString GitHubCredentialStore::problem() const
{
    return m_problem;
}
QString GitHubCredentialStore::statusText() const
{
    if (isBusy())
        return tr("Waiting for the system keyring...");
    if (!m_problem.isEmpty())
        return m_problem;
    if (m_dirty)
        return tr("Token change is not saved. Choose Save token, or clear the token.");
    return m_token.isEmpty() ? tr("No GitHub token stored.")
                             : tr("Token stored in the system keyring.");
}
void GitHubCredentialStore::whenSettled(QObject* _context, std::function<void()> _callback)
{
    start();
    if (!isBusy()) {
        QTimer::singleShot(0, _context, std::move(_callback));
        return;
    }
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(this, &GitHubCredentialStore::settled, _context, [connection, _callback] {
        QObject::disconnect(*connection);
        _callback();
    });
}
