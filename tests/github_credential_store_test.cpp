#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <management_layer/content/github_sync/github_sync_manager.h>
#include <management_layer/content/onboarding/onboarding_manager.h>
#include <management_layer/content/projects/projects_manager.h>
#include <management_layer/content/settings/settings_manager.h>
#include <management_layer/content/shortcuts/shortcuts_manager.h>
#include <management_layer/plugins_builder.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/settings/settings_tool_bar.h>
#include <ui/settings/settings_view.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/tools/github_files_client.h>

#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSettings>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using Store = GitHubCredentialStore;
using Backend = GitHubCredentialBackend;
using Result = Backend::Result;
void require(bool ok, const char* label)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
    std::printf("ok: %s\n", label);
}
struct Vault { QString value; };
struct Legacy {
    QString value;
    bool removeOk = true;
    int removals = 0;
    Store::LegacySettings callbacks()
    {
        return { [this] { return value; }, [this] {
            ++removals;
            if (!removeOk) return false;
            value.clear();
            return true;
        } };
    }
};
class ControlledBackend : public Backend
{
public:
    struct Operation { char kind; QString value; Callback callback; bool done = false; };
    explicit ControlledBackend(std::shared_ptr<Vault> state) : vault(state) { }
    void read(Callback callback) override { operations.push_back({ 'r', {}, callback }); }
    void write(const QString& value, Callback callback) override
    { operations.push_back({ 'w', value, callback }); }
    void remove(Callback callback) override { operations.push_back({ 'd', {}, callback }); }
    void complete(size_t index, Result result = Result::Success)
    {
        require(index < operations.size() && !operations[index].done, "one completion per active job");
        auto operation = operations[index];
        operations[index].done = true;
        QString value;
        if (result == Result::Success) {
            if (operation.kind == 'w') vault->value = operation.value;
            if (operation.kind == 'd') vault->value.clear();
            if (operation.kind == 'r') {
                value = vault->value;
                if (value.isEmpty()) result = Result::Missing;
            }
        }
        operation.callback(result, value);
    }
    void drain()
    {
        for (size_t i = 0; i < operations.size(); ++i) {
            require(i < 100, "bounded serialized jobs");
            if (!operations[i].done) complete(i);
        }
    }
    std::shared_ptr<Vault> vault;
    std::vector<Operation> operations;
};
struct Fixture {
    std::shared_ptr<Vault> vault = std::make_shared<Vault>();
    Legacy legacy;
    ControlledBackend* backend = nullptr;
    std::unique_ptr<Store> store;
    Fixture(const QString& secure = {}, const QString& old = {})
    {
        vault->value = secure;
        legacy.value = old;
        auto adapter = std::make_unique<ControlledBackend>(vault);
        backend = adapter.get();
        store = std::make_unique<Store>(std::move(adapter), legacy.callbacks());
        store->start();
    }
};
void waitFor(std::function<bool()> predicate, QObject* context, std::function<void()> start)
{
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto store = qobject_cast<Store*>(context);
    if (store) QObject::connect(store, &Store::settled, &loop, &QEventLoop::quit);
    start();
    if (!predicate()) { deadline.start(15000); loop.exec(); }
    require(predicate(), "asynchronous operation reaches its expected boundary");
}
void deterministic()
{
    {
        Fixture f;
        require(f.store->isBusy() && !f.store->isReady(), "loading differs from absent credential");
        f.backend->complete(0);
        f.store->stageToken("fake-first-token");
        require(f.backend->operations.size() == 1, "typing stages in memory without keyring jobs");
        f.store->save(); f.backend->drain();
        require(f.store->token() == "fake-first-token" && !f.store->hasPendingChange(), "first storage confirmed");
        auto freshBackend = std::make_unique<ControlledBackend>(f.vault);
        auto freshRaw = freshBackend.get();
        Store fresh(std::move(freshBackend), f.legacy.callbacks());
        fresh.start(); freshRaw->drain();
        require(fresh.token() == "fake-first-token", "retrieval through a fresh store instance");
    }
    {
        Fixture f({}, "fake-legacy-token");
        f.backend->complete(0);
        require(f.legacy.removals == 0, "migration retains plaintext until storage confirmation");
        f.backend->complete(1);
        require(f.legacy.removals == 0, "successful write alone does not retire plaintext");
        f.backend->complete(2);
        require(f.legacy.value.isEmpty() && f.store->token() == "fake-legacy-token", "successful migration removes legacy value");
    }
    {
        Fixture f({}, "fake-retained-token");
        f.backend->complete(0); f.backend->complete(1, Result::Denied);
        require(f.store->isReady() && f.store->token() == f.legacy.value && !f.store->problem().isEmpty(), "failed migration retains a usable legacy token and feedback");
        f.store->save();
        require(f.store->isBusy(), "saving unchanged fields retries migration rather than falsely claiming secure storage");
        f.backend->drain();
        require(f.legacy.value.isEmpty() && f.store->problem().isEmpty(), "failed migration can be retried");
    }
    {
        Fixture f("fake-newer-keyring-token", "fake-stale-legacy-token");
        f.backend->drain();
        require(f.backend->operations.size() == 1 && f.store->token() == "fake-newer-keyring-token" && f.legacy.value.isEmpty(), "keyring wins conflict without an overwriting write");
    }
    {
        Fixture f({}, "fake-stale-after-native-clear");
        f.backend->operations[0].done = true;
        f.backend->operations[0].callback(Result::Success, {});
        require(f.store->token().isEmpty() && f.legacy.value.isEmpty() && f.backend->operations.size() == 1, "explicitly empty keyring entry cannot resurrect stale legacy token");
    }
    {
        Fixture f({}, "fake-kept-until-readback");
        f.backend->complete(0); f.backend->complete(1); f.backend->complete(2, Result::Denied);
        require(f.legacy.removals == 0 && f.store->token() == f.legacy.value && !f.store->problem().isEmpty(), "failed storage confirmation cannot remove legacy credential");
        f.store->retry(); f.backend->drain();
        require(f.legacy.value.isEmpty(), "confirmation retry reads existing secure copy without stale overwrite");
    }
    {
        Fixture f("fake-secure-token", "fake-old-token");
        f.legacy.removeOk = false; f.backend->drain();
        require(!f.legacy.value.isEmpty() && !f.store->problem().isEmpty(), "settings removal failure is reported and retains legacy");
        f.store->stageToken({}); f.store->save();
        require(f.backend->operations.size() == 1 && f.vault->value == "fake-secure-token", "failed legacy cleanup cannot resurrect a cleared token");
        f.legacy.removeOk = true; f.store->retry(); f.backend->drain();
        require(f.vault->value.isEmpty() && f.legacy.value.isEmpty(), "clear succeeds after durable legacy cleanup");
    }
    {
        Fixture f("fake-previous-token"); f.backend->drain();
        f.store->stageToken("fake-replacement-token"); f.store->save();
        f.backend->complete(1, Result::Unavailable);
        require(f.store->token() == "fake-previous-token" && f.vault->value == f.store->token() && f.store->hasPendingChange(), "failed write preserves previous credential and unsaved edit");
        f.store->retry(); f.backend->drain();
        require(f.store->token() == "fake-replacement-token" && !f.store->hasPendingChange(), "replacement retried and confirmed");
        f.store->stageToken({}); f.store->save();
        f.backend->complete(f.backend->operations.size() - 1, Result::Failed);
        require(f.store->token() == "fake-replacement-token" && f.store->hasPendingChange(), "failed delete retains previous credential");
        f.store->retry(); f.backend->drain();
        require(f.store->token().isEmpty() && f.vault->value.isEmpty(), "clear and subsequent missing read confirmed");
    }
    {
        Fixture f("fake-original-token");
        auto delayedRead = f.backend->operations[0].callback;
        f.store->stageToken("fake-first-edit"); f.store->save();
        f.backend->complete(0);
        auto delayedWrite = f.backend->operations[1].callback;
        f.store->stageToken("fake-second-edit"); f.store->save();
        f.backend->complete(1); f.backend->complete(2);
        require(f.backend->operations.size() == 4 && f.backend->operations[3].value == "fake-second-edit" && !f.store->isReady(), "newer edit waits behind confirmation of the previous job");
        f.store->stageToken({}); f.store->save(); f.backend->drain();
        delayedWrite(Result::Success, {}); delayedRead(Result::Success, "fake-original-token");
        require(f.store->token().isEmpty() && f.store->fieldToken().isEmpty() && f.vault->value.isEmpty(), "out-of-order duplicate completions cannot overwrite edits or resurrect cleared token");
    }
    {
        Fixture f; f.backend->drain();
        f.store->stageToken("fake-first-in-flight-write"); f.store->save();
        f.store->stageToken({}); f.store->save(); f.backend->drain();
        require(f.store->token().isEmpty() && f.vault->value.isEmpty() && !f.store->hasPendingChange(), "clear during the first write cannot be mistaken for unchanged empty credentials");
        Fixture reverting("fake-prior-token"); reverting.backend->drain();
        reverting.store->stageToken("fake-in-flight-replacement"); reverting.store->save();
        reverting.store->stageToken("fake-prior-token"); reverting.store->save(); reverting.backend->drain();
        require(reverting.store->token() == "fake-prior-token" && reverting.vault->value == "fake-prior-token", "newer revert to previous token wins over in-flight replacement");
    }
    {
        Fixture f({}, "fake-kept-on-read-failure");
        f.backend->complete(0, Result::Unavailable);
        require(f.store->token() == f.legacy.value && f.backend->operations.size() == 1, "unavailable read preserves legacy without blindly overwriting unknown keyring");
        f.vault->value = "fake-newer-external-token";
        f.store->retry(); f.backend->drain();
        require(f.store->token() == f.vault->value && f.backend->operations.size() == 2, "retry discovers newer native credential before migration");
    }
    {
        Fixture f("fake-initial-loading-token");
        f.store->stageToken({}); f.store->save(); f.backend->drain();
        require(f.store->token().isEmpty() && f.vault->value.isEmpty(), "clear queued during initial loading wins over the older read");
        Fixture destroyed;
        auto completion = destroyed.backend->operations[0].callback;
        destroyed.store.reset(); completion(Result::Success, "fake-too-late");
        require(true, "late completion after store destruction is safely ignored");
    }
}
void persistedMigration(std::unique_ptr<Backend> backend, bool native)
{
    auto settings = DataStorageLayer::StorageFacade::settingsStorage();
    QSettings disk;
    const auto key = DataStorageLayer::kGitHubSyncTokenKey.toUtf8().toHex();
    disk.setValue(key, "fake-disk-legacy-token"); disk.sync();
    require(disk.status() == QSettings::NoError, "seed isolated legacy settings file");
    auto raw = dynamic_cast<ControlledBackend*>(backend.get());
    Store store(std::move(backend), { [settings] { return settings->legacyGitHubToken(); },
                                    [settings] { return settings->removeLegacyGitHubToken(); } });
    waitFor([&] { return store.isReady(); }, &store, [&] {
        store.start(); if (raw) raw->drain();
    });
    require(store.problem().isEmpty() && store.token() == "fake-disk-legacy-token", native ? "native migration confirmed" : "injected migration confirmed with real settings writer");
    settings->sync();
    QSettings reopened(disk.fileName(), QSettings::IniFormat);
    QFile bytes(disk.fileName()); require(bytes.open(QIODevice::ReadOnly), "read isolated settings bytes");
    require(!reopened.contains(key) && !bytes.readAll().contains("fake-disk-legacy-token"), "no plaintext token in settings after successful migration and sync");
    setSettingsValue(DataStorageLayer::kGitHubSyncTokenKey, "fake-forbidden-new-token");
    settings->sync();
    QSettings check(disk.fileName(), QSettings::IniFormat);
    require(!check.contains(key), "ordinary settings cannot create a second credential path");
    store.stageToken({});
    waitFor([&] { return store.isReady() && !store.hasPendingChange(); }, &store,
            [&] { store.save(); if (raw) raw->drain(); });
}
void settingsRemovalRetry()
{
    auto settings = DataStorageLayer::StorageFacade::settingsStorage();
    QSettings disk;
    const auto key = DataStorageLayer::kGitHubSyncTokenKey.toUtf8().toHex();
    disk.setValue(key, "fake-permission-legacy-token"); disk.sync();
    auto backend = std::make_unique<ControlledBackend>(std::make_shared<Vault>());
    auto raw = backend.get();
    Store store(std::move(backend), { [settings] { return settings->legacyGitHubToken(); },
                                    [settings] { return settings->removeLegacyGitHubToken(); } });
    store.start(); raw->complete(0); raw->complete(1);
    const auto directory = QFileInfo(disk.fileName()).absolutePath();
    const auto permissions = QFile::permissions(directory);
    require(QFile::setPermissions(directory, QFile::ReadOwner | QFile::ExeOwner), "make only the disposable settings directory read-only");
    QFile probe(directory + "/permission-probe");
    require(!probe.open(QIODevice::WriteOnly), "filesystem denies atomic settings publication");
    setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, "permission-test/repository");
    raw->complete(2);
    QFile bytes(disk.fileName()); require(bytes.open(QIODevice::ReadOnly), "inspect failed cleanup's isolated disk bytes");
    require(bytes.readAll().contains("fake-permission-legacy-token") && !store.problem().isEmpty(), "real settings publication failure retains plaintext and reports incomplete migration");
    bytes.close();
    require(QFile::setPermissions(directory, permissions), "restore disposable settings permissions");
    store.retry(); raw->drain();
    require(store.problem().isEmpty() && bytes.open(QIODevice::ReadOnly) && !bytes.readAll().contains("fake-permission-legacy-token"), "fresh settings writer retries successfully after sticky QSettings access error");
    store.stageToken({}); store.save(); raw->drain();
}
void uiAndSync()
{
    // View pointers are retained by these managers: the host must outlive all managers.
    QWidget host; host.resize(1400, 950);
    TaskBar::registerTaskBar(&host, Qt::black, Qt::white, Qt::blue);
    Fixture f("test-token");
    ManagementLayer::PluginsBuilder plugins;
    ManagementLayer::ShortcutsManager shortcuts;
    ManagementLayer::SettingsManager settings(nullptr, &host, plugins, &shortcuts, f.store.get());
    ManagementLayer::OnboardingManager onboarding(nullptr, &host, f.store.get());
    onboarding.showWelcomePage(); settings.reloadSettings();
    auto view = qobject_cast<Ui::SettingsView*>(settings.view());
    auto token = view->findChild<TextField*>("githubToken");
    require(token && !token->isEnabled() && f.backend->operations.size() == 1, "settings loading field disabled without writes");
    f.backend->drain(); QApplication::processEvents();
    require(token->text() == "test-token" && f.backend->operations.size() == 1 && !f.store->hasPendingChange(), "programmatic settings and onboarding population cause no unintended write");
    view->setApplicationGitHubSyncToken("fake-view-only-token");
    require(!f.store->hasPendingChange(), "programmatic setter cannot stage deletion or replacement");
    settings.reloadSettings();
    token->setText("fake-ui-edited-token"); QApplication::processEvents();
    require(f.store->hasPendingChange() && f.backend->operations.size() == 1, "field edit stages without a job per keystroke");
    view->findChild<Button*>("githubTokenSave")->click(); f.backend->drain();
    require(f.store->token() == "fake-ui-edited-token", "explicit settings Save stores edit");
    view->findChild<Button*>("githubTokenClear")->click(); f.backend->drain();
    require(f.store->token().isEmpty() && token->text().isEmpty(), "settings Clear removes credential and field");
    token->setText("fake-close-edit");
    auto toolbar = qobject_cast<Ui::SettingsToolBar*>(settings.toolBar());
    emit toolbar->backPressed(); f.backend->drain();
    require(f.store->token() == "fake-close-edit", "leaving settings settles staged edit");

    Fixture syncing("test-token");
    ManagementLayer::ProjectsManager projects(nullptr, &host);
    ManagementLayer::GitHubSyncManager sync(nullptr, &host, &projects, syncing.store.get());
    setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, "test-owner/test-repository");
    int refreshed = 0;
    QObject::connect(&sync, &ManagementLayer::GitHubSyncManager::saveCurrentProjectRequested, &host, [&] { ++refreshed; });
    sync.checkOnStart(); QApplication::processEvents();
    require(refreshed == 0, "sync waits for credential readiness");
    QEventLoop loop; bool finished = false, success = false;
    QTimer deadline; deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&sync, &ManagementLayer::GitHubSyncManager::checkFinished, &loop, [&](bool ok) {
        finished = true; success = ok; loop.quit();
    });
    syncing.backend->drain(); deadline.start(15000); loop.exec();
    require(finished && success && refreshed == 1, "sync sends expected fake token to authenticated local mock API");

    GitHubFilesClient client;
    client.setRepository("echo/denied"); client.setToken("test-token");
    bool redacted = false;
    client.loadRepositoryInfo([&](const auto& error, const auto&) {
        redacted = error.isValid() && !error.text.contains("test-token"); loop.quit();
    });
    deadline.start(15000); loop.exec();
    require(redacted, "server-reflected credential excluded from diagnostics and UI errors");

    Fixture setup;
    ManagementLayer::OnboardingManager onboardingSetup(nullptr, &host, setup.store.get());
    setup.backend->drain(); bool onboarded = false;
    QObject::connect(&onboardingSetup, &ManagementLayer::OnboardingManager::finished, &host,
                     [&](bool configured) { onboarded = configured; });
    auto navigator = qobject_cast<Ui::OnboardingNavigator*>(onboardingSetup.navigator());
    emit navigator->setupGitHubSyncRequested("test-owner/test-repository", "fake-onboarding-token");
    setup.backend->complete(1, Result::Denied); QApplication::processEvents();
    require(!onboarded && setup.store->hasPendingChange(), "failed onboarding storage does not claim configured sync");
    emit navigator->setupGitHubSyncRequested("test-owner/test-repository", "fake-onboarding-token");
    setup.backend->drain(); QApplication::processEvents();
    require(onboarded && !setup.store->hasPendingChange(), "onboarding retry finishes only after confirmed storage");

    syncing.store->stageToken("fake-shutdown-edit");
    setSettingsValue(DataStorageLayer::kGitHubSyncUploadOnExitKey, false);
    bool closed = false;
    sync.uploadBeforeExit([&] { closed = true; });
    require(!closed && syncing.store->isBusy(), "ordinary close waits for pending credential change");
    syncing.backend->drain(); QApplication::processEvents();
    require(closed && syncing.store->token() == "fake-shutdown-edit", "shutdown callback follows confirmed storage");
    syncing.store->stageToken("fake-second-close-edit");
    bool cancelledClose = false, currentClose = false;
    sync.settleCredentialsBeforeExit([&] { cancelledClose = true; });
    auto dialogs = host.findChildren<Dialog*>();
    require(!dialogs.isEmpty(), "close wait has a keep-open action");
    emit dialogs.last()->finished({ 0, "Keep application open", Dialog::RejectButton });
    sync.settleCredentialsBeforeExit([&] { currentClose = true; });
    syncing.backend->drain(); QApplication::processEvents();
    require(!cancelledClose && currentClose, "older cancelled close completion cannot replace the newer close request");
}
void nativeChecks(const QString& service)
{
    require(qEnvironmentVariable("STARC_ISOLATED_KEYRING") == "1" && service.startsWith("app.starc.test."), "native checks restricted to isolated test service");
    Legacy empty;
    {
        Store first(nativeGitHubCredentialBackend(service), empty.callbacks());
        waitFor([&] { return first.isReady(); }, &first, [&] { first.start(); });
        require(first.problem().isEmpty() && first.token().isEmpty(), "native fresh entry missing");
        first.stageToken("fake-native-first-token");
        waitFor([&] { return first.isReady() && !first.hasPendingChange(); }, &first, [&] { first.save(); });
        require(first.problem().isEmpty(), "native first write and confirmation");
    }
    Store fresh(nativeGitHubCredentialBackend(service), empty.callbacks());
    waitFor([&] { return fresh.isReady(); }, &fresh, [&] { fresh.start(); });
    require(fresh.token() == "fake-native-first-token", "actual keyring retrieval through fresh instance");
    fresh.stageToken("fake-native-replacement");
    waitFor([&] { return fresh.isReady() && !fresh.hasPendingChange(); }, &fresh, [&] { fresh.save(); });
    require(fresh.token() == "fake-native-replacement", "actual keyring replacement");
    fresh.stageToken({});
    waitFor([&] { return fresh.isReady() && !fresh.hasPendingChange(); }, &fresh, [&] { fresh.save(); });
    require(fresh.token().isEmpty() && fresh.problem().isEmpty(), "actual keyring delete and confirmation");
    persistedMigration(nativeGitHubCredentialBackend(service + ".migration"), true);
}
void nativeUnavailable(const QString& service)
{
    require(qEnvironmentVariable("STARC_ISOLATED_KEYRING") == "1" && service.startsWith("app.starc.test."), "unavailable native check restricted to isolated service");
    Legacy empty;
    Store store(nativeGitHubCredentialBackend(service), empty.callbacks());
    waitFor([&] { return store.isReady(); }, &store, [&] { store.start(); });
    require(!store.problem().isEmpty() && store.token().isEmpty(), "actual missing Secret Service reports unavailable credentials");
    store.stageToken("fake-must-not-fall-back");
    waitFor([&] { return store.isReady(); }, &store, [&] { store.save(); });
    require(store.hasPendingChange() && !store.problem().isEmpty(), "actual unavailable keyring cannot falsely report storage success");
    auto settings = DataStorageLayer::StorageFacade::settingsStorage();
    settings->sync();
    QSettings disk;
    QFile bytes(disk.fileName());
    require(!bytes.exists() || (bytes.open(QIODevice::ReadOnly) && !bytes.readAll().contains("fake-must-not-fall-back")), "native adapter creates no plaintext fallback");
}
} // namespace
int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Credential regression"); app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    for (auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons", ":/fonts/font-awesome-brands" })
        require(QFontDatabase::addApplicationFont(font) >= 0, "embedded font available");
    if (app.arguments().size() == 3 && app.arguments()[1] == "--native-unavailable") {
        nativeUnavailable(app.arguments()[2]);
    } else if (app.arguments().size() == 3 && app.arguments()[1] == "--native") {
        nativeChecks(app.arguments()[2]);
    } else {
        deterministic();
        persistedMigration(std::make_unique<ControlledBackend>(std::make_shared<Vault>()), false);
        settingsRemovalRetry();
        uiAndSync();
    }
    std::puts("PASS: credential regression");
}
