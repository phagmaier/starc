#include "onboarding_navigator.h"

#include <include/custom_events.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/ui_helper.h>

#include <QBoxLayout>
#include <QScrollArea>
#include <QSignalBlocker>

namespace Ui {

class OnboardingNavigator::Implementation
{
public:
    explicit Implementation(OnboardingNavigator* _q);

    void initSyncPage();

    OnboardingNavigator* q = nullptr;

    Widget* syncPage = nullptr;
    QScrollArea* syncPageContainer = nullptr;
    ImageLabel* syncLogo = nullptr;
    H6Label* syncTitle = nullptr;
    Body2Label* syncSubtitle = nullptr;
    Subtitle1Label* syncDescription = nullptr;
    TextField* syncRepository = nullptr;
    TextField* syncToken = nullptr;
    Button* syncSetupButton = nullptr;
    Button* syncSkipButton = nullptr;
    Button* syncRetryButton = nullptr;
    Body2Label* syncStatus = nullptr;
};

OnboardingNavigator::Implementation::Implementation(OnboardingNavigator* _q)
    : q(_q)
    , syncPage(new Widget(q))
    , syncPageContainer(UiHelper::createScrollArea(syncPage))
    , syncLogo(new ImageLabel(syncPageContainer))
    , syncTitle(new H6Label(syncPageContainer))
    , syncSubtitle(new Body2Label(syncPageContainer))
    , syncDescription(new Subtitle1Label(syncPageContainer))
    , syncRepository(new TextField(syncPageContainer))
    , syncToken(new TextField(syncPageContainer))
    , syncSetupButton(new Button(syncPageContainer))
    , syncSkipButton(new Button(syncPageContainer))
    , syncRetryButton(new Button(syncPageContainer))
    , syncStatus(new Body2Label(syncPageContainer))
{
    initSyncPage();
}

void OnboardingNavigator::Implementation::initSyncPage()
{
    syncLogo->setImage(QPixmap(":/images/logo"));
    syncTitle->setAlignment(Qt::AlignCenter);
    syncSubtitle->setAlignment(Qt::AlignCenter);
    for (auto textField : { syncRepository, syncToken }) {
        textField->setSpellCheckPolicy(SpellCheckPolicy::Manual);
        textField->setCapitalizeWords(false);
        textField->setCorrectDoubleCapitals(false);
        textField->setCapitalizeSingleILetter(false);
    }
    syncToken->setPasswordModeEnabled(true);
    syncSetupButton->setContained(true);

    auto pageLayout = qobject_cast<QVBoxLayout*>(syncPageContainer->widget()->layout());
    pageLayout->addWidget(syncLogo, 0, Qt::AlignHCenter);
    pageLayout->addWidget(syncTitle);
    pageLayout->addWidget(syncSubtitle);
    pageLayout->addWidget(syncDescription);
    pageLayout->addWidget(syncRepository);
    pageLayout->addWidget(syncToken);
    pageLayout->addWidget(syncStatus);
    pageLayout->addWidget(syncRetryButton);
    syncToken->setObjectName("onboardingToken");
    syncSetupButton->setObjectName("onboardingSetup");
    pageLayout->addStretch();
    pageLayout->addWidget(syncSetupButton);
    pageLayout->addWidget(syncSkipButton, 0, Qt::AlignHCenter);

    auto layout = UiHelper::makeVBoxLayout();
    layout->addWidget(syncPageContainer);
    syncPage->setLayout(layout);
}

// ****

OnboardingNavigator::OnboardingNavigator(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{
    setCurrentWidget(d->syncPage);

    d->syncRepository->installEventFilter(this);
    d->syncToken->installEventFilter(this);

    for (auto textField : { d->syncRepository, d->syncToken }) {
        connect(textField, &TextField::textChanged, textField, &TextField::clearError);
    }
    connect(d->syncRepository, &TextField::enterPressed, this,
            [this] { d->syncToken->setFocus(); });
    connect(d->syncToken, &TextField::enterPressed, d->syncSetupButton, &Button::click);
    connect(d->syncSetupButton, &Button::clicked, this, [this] {
        const auto repository = d->syncRepository->text().trimmed();
        const auto token = d->syncToken->text().trimmed();
        if (repository.isEmpty()) {
            d->syncRepository->setError(tr("Set the repository to keep the stories in"));
        }
        if (token.isEmpty()) {
            d->syncToken->setError(tr("Set the access token of the repository"));
        }
        if (repository.isEmpty() || token.isEmpty()) {
            return;
        }

        emit setupGitHubSyncRequested(repository, token);
    });
    connect(d->syncRetryButton, &Button::clicked, this,
            &OnboardingNavigator::retryCredentialRequested);
    connect(d->syncSkipButton, &Button::clicked, this,
            &OnboardingNavigator::skipGitHubSyncRequested);
}

OnboardingNavigator::~OnboardingNavigator() = default;

void OnboardingNavigator::showWelcomePage()
{
    setCurrentWidget(d->syncPage);
}

void OnboardingNavigator::setGitHubSync(const QString& _repository, const QString& _token)
{
    const QSignalBlocker repositoryBlocker(d->syncRepository);
    const QSignalBlocker tokenBlocker(d->syncToken);
    d->syncRepository->setText(_repository);
    d->syncToken->setText(_token);
}

void OnboardingNavigator::setCredentialState(bool _busy, const QString& _status)
{
    d->syncRepository->setEnabled(!_busy);
    d->syncToken->setEnabled(!_busy);
    d->syncSetupButton->setEnabled(!_busy);
    d->syncRetryButton->setEnabled(!_busy);
    d->syncStatus->setText(_status);
}

bool OnboardingNavigator::eventFilter(QObject* _watched, QEvent* _event)
{
    //
    // Text corrections turned on in the settings are applied to all text fields of the
    // application, but they must not change the name of the repository and the access token
    //
    if ((_watched == d->syncRepository || _watched == d->syncToken)
        && _event->type() == static_cast<QEvent::Type>(EventType::TextEditingOptionsChangeEvent)) {
        return true;
    }

    return StackWidget::eventFilter(_watched, _event);
}

void OnboardingNavigator::updateTranslations()
{
    d->syncTitle->setText(tr("Welcome to the Story Architect"));
    d->syncSubtitle->setText(tr("Keep your stories on all your computers"));
    d->syncDescription->setText(
        tr("The application can keep copies of your stories in a private GitHub repository and "
           "sync them between your computers.\n\n"
           "You can skip this step and turn the sync on later in the application settings."));
    d->syncRepository->setLabel(tr("Private repository"));
    d->syncRepository->setHelper(tr("In the form owner/name"));
    d->syncToken->setLabel(tr("Access token"));
    d->syncToken->setHelper(tr("With read and write access to the contents of the repository"));
    d->syncSetupButton->setText(tr("Set up sync"));
    d->syncRetryButton->setText(tr("Retry keyring"));
    d->syncSkipButton->setText(tr("Set up later"));
}

void OnboardingNavigator::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    StackWidget::designSystemChangeEvent(_event);

    setBackgroundColor(DesignSystem::color().primary());
    setTextColor(DesignSystem::color().onPrimary());

    for (auto widget : std::vector<Widget*>{
             d->syncPage,
             d->syncLogo,
             d->syncTitle,
             d->syncSubtitle,
             d->syncDescription,
             d->syncStatus,
         }) {
        widget->setBackgroundColor(DesignSystem::color().primary());
        widget->setTextColor(DesignSystem::color().onPrimary());
    }
    const auto margin = DesignSystem::layout().px(48);
    d->syncLogo->setFixedSize(DesignSystem::layout().px62(), DesignSystem::layout().px62());
    d->syncTitle->setContentsMargins(margin / 2.0, DesignSystem::layout().px24(), margin / 2.0,
                                     DesignSystem::layout().px4());
    d->syncSubtitle->setContentsMargins(margin, 0, margin, 0);
    d->syncDescription->setContentsMargins(margin, DesignSystem::layout().px24(), margin, 0);

    for (auto textField : { d->syncRepository, d->syncToken }) {
        textField->setBackgroundColor(DesignSystem::color().onPrimary());
        textField->setTextColor(DesignSystem::color().onPrimary());
        textField->setCustomMargins({ margin, DesignSystem::layout().px24(), margin, 0 });
    }

    for (auto button : { d->syncSetupButton, d->syncSkipButton, d->syncRetryButton }) {
        button->setBackgroundColor(DesignSystem::color().accent());
        button->setTextColor(DesignSystem::color().onAccent());
        button->setContentsMargins(margin - DesignSystem::card().shadowMargins().left(), 0,
                                   margin - DesignSystem::card().shadowMargins().right(),
                                   DesignSystem::layout().px16());
    }
    d->syncSkipButton->setTextColor(DesignSystem::color().accent());
}

} // namespace Ui
