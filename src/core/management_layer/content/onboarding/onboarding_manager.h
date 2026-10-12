#pragma once

#include <QObject>


class GitHubCredentialStore;

namespace ManagementLayer {

/**
 * @brief Менеджер посадочного экрана
 */
class OnboardingManager : public QObject
{
    Q_OBJECT

public:
    OnboardingManager(QObject* _parent, QWidget* _parentWidget,
                      GitHubCredentialStore* _credentials = nullptr);
    ~OnboardingManager() override;

    QWidget* toolBar() const;
    QWidget* navigator() const;
    QWidget* view() const;

    /**
     * @brief Показать начальную страницу онбординга
     */
    void showWelcomePage();

signals:
    /**
     * @brief Посадка окончена
     * @param _syncWithGitHub - the user set the repository to sync the stories with
     */
    void finished(bool _syncWithGitHub);

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
