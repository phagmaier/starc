#pragma once

#include <ui/widgets/stack_widget/stack_widget.h>


namespace Ui {

/**
 * @brief Навигатор по посадочным страницам
 */
class OnboardingNavigator : public StackWidget
{
    Q_OBJECT

public:
    explicit OnboardingNavigator(QWidget* _parent = nullptr);
    ~OnboardingNavigator() override;

    /**
     * @brief Показать приветственную страницу
     */
    void showWelcomePage();

    /**
     * @brief Set the repository and the access token, which are already used for the sync
     */
    void setGitHubSync(const QString& _repository, const QString& _token);
    void setCredentialState(bool _busy, const QString& _status);

signals:
    void retryCredentialRequested();
    /**
     * @brief The user wants to sync the stories with the GitHub repository
     */
    void setupGitHubSyncRequested(const QString& _repository, const QString& _token);

    /**
     * @brief The user wants to set up the sync with GitHub later
     */
    void skipGitHubSyncRequested();

protected:
    /**
     * @brief Keep the GitHub repository and the access token exactly as they were entered
     */
    bool eventFilter(QObject* _watched, QEvent* _event) override;

    /**
     * @brief Обновить перевод
     */
    void updateTranslations() override;

    /**
     * @brief Обновляем навигатор при изменении дизайн системы
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
