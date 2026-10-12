#include "onboarding_manager.h"

#include <data_layer/storage/github_credential_store.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <ui/onboarding/onboarding_navigator.h>
#include <ui/onboarding/onboarding_tool_bar.h>
#include <ui/onboarding/onboarding_view.h>


namespace ManagementLayer {

class OnboardingManager::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    GitHubCredentialStore* credentials = nullptr;
    bool setupPending = false;
    Ui::OnboardingToolBar* toolBar = nullptr;
    Ui::OnboardingNavigator* navigator = nullptr;
    Ui::OnboardingView* view = nullptr;
};

OnboardingManager::Implementation::Implementation(QWidget* _parent)
    : toolBar(new Ui::OnboardingToolBar(_parent))
    , navigator(new Ui::OnboardingNavigator(_parent))
    , view(new Ui::OnboardingView(_parent))
{
    toolBar->hide();
    navigator->hide();
    view->hide();
}


//****


OnboardingManager::OnboardingManager(QObject* _parent, QWidget* _parentWidget,
                                     GitHubCredentialStore* _credentials)
    : QObject(_parent)
    , d(new Implementation(_parentWidget))
{
    d->credentials = _credentials ? _credentials : GitHubCredentialStore::instance();
    connect(d->credentials, &GitHubCredentialStore::changed, this, [this] {
        d->navigator->setCredentialState(d->credentials->isBusy(), d->credentials->statusText());
    });
    connect(d->navigator, &Ui::OnboardingNavigator::retryCredentialRequested, d->credentials,
            &GitHubCredentialStore::retry);
    d->credentials->start();
    connect(d->navigator, &Ui::OnboardingNavigator::setupGitHubSyncRequested, this,
            [this](const QString& _repository, const QString& _token) {
                setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, _repository);
                d->setupPending = true;
                d->credentials->stageToken(_token);
                d->credentials->save();
                d->credentials->whenSettled(this, [this] {
                    if (!d->setupPending)
                        return;
                    d->setupPending = false;
                    if (!d->credentials->hasPendingChange()
                        && d->credentials->problem().isEmpty()) {
                        emit finished(true);
                    }
                });
            });
    connect(d->navigator, &Ui::OnboardingNavigator::skipGitHubSyncRequested, this, [this] {
        d->setupPending = false;
        emit finished(false);
    });
}

OnboardingManager::~OnboardingManager() = default;

QWidget* OnboardingManager::toolBar() const
{
    return d->toolBar;
}

QWidget* OnboardingManager::navigator() const
{
    return d->navigator;
}

QWidget* OnboardingManager::view() const
{
    return d->view;
}

void OnboardingManager::showWelcomePage()
{
    //
    // The sync is already set, if the settings of the application were reset to the default ones
    //
    d->navigator->setGitHubSync(
        settingsValue(DataStorageLayer::kGitHubSyncRepositoryKey).toString(),
        d->credentials->fieldToken());
    d->navigator->setCredentialState(d->credentials->isBusy(), d->credentials->statusText());
    d->credentials->whenSettled(this, [this] {
        d->navigator->setGitHubSync(
            settingsValue(DataStorageLayer::kGitHubSyncRepositoryKey).toString(),
            d->credentials->fieldToken());
    });
    d->navigator->showWelcomePage();
}

} // namespace ManagementLayer
