#pragma once

#include <domain/starcloud_api.h>

#include <QObject>


namespace ManagementLayer {

/**
 * @brief Stable release metadata for the retained application update-download flow
 */
class NotificationsManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationsManager(QObject* _parent = nullptr);
    ~NotificationsManager() override;

    /**
     * @brief Cache device-relevant stable releases received from the cloud feed
     */
    void processNotifications(const QVector<Domain::Notification>& _notifications);

    /**
     * @brief Ссылка на загрузку самой последней версии
     */
    QString lastVersionDownloadLink() const;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
