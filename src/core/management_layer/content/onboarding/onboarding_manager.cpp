#include "onboarding_manager.h"

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


OnboardingManager::OnboardingManager(QObject* _parent, QWidget* _parentWidget)
    : QObject(_parent)
    , d(new Implementation(_parentWidget))
{
    connect(d->navigator, &Ui::OnboardingNavigator::setupGitHubSyncRequested, this,
            [this](const QString& _repository, const QString& _token) {
                setSettingsValue(DataStorageLayer::kGitHubSyncRepositoryKey, _repository);
                setSettingsValue(DataStorageLayer::kGitHubSyncTokenKey, _token);
                emit finished(true);
            });
    connect(d->navigator, &Ui::OnboardingNavigator::skipGitHubSyncRequested, this,
            [this] { emit finished(false); });
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
        settingsValue(DataStorageLayer::kGitHubSyncTokenKey).toString());
    d->navigator->showWelcomePage();
}

} // namespace ManagementLayer
