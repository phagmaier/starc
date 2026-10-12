#include "notifications_manager.h"

#include <utils/shugar.h>

#include <QJsonDocument>
#include <QJsonObject>


namespace ManagementLayer {

class NotificationsManager::Implementation
{
public:
    QVector<Domain::Notification> notifications;
};


// ****


NotificationsManager::NotificationsManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation)
{
}

NotificationsManager::~NotificationsManager() = default;

void NotificationsManager::processNotifications(const QVector<Domain::Notification>& _notifications)
{
    const QVector<Domain::NotificationType> stableUpdateTypes = {
        Domain::NotificationType::UpdateStableQt5Linux,
        Domain::NotificationType::UpdateStableQt5Mac,
        Domain::NotificationType::UpdateStableQt5Windows32,
        Domain::NotificationType::UpdateStableQt5Windows64,
        Domain::NotificationType::UpdateStableQt6Linux,
        Domain::NotificationType::UpdateStableQt6Mac,
        Domain::NotificationType::UpdateStableQt6Windows,
    };
    const QVector<Domain::NotificationType> irrelevantNotificationTypes = {
#if defined(Q_OS_LINUX)
        Domain::NotificationType::UpdateStableQt5Mac,
        Domain::NotificationType::UpdateStableQt5Windows32,
        Domain::NotificationType::UpdateStableQt5Windows64,
        Domain::NotificationType::UpdateStableQt6Mac,
        Domain::NotificationType::UpdateStableQt6Windows,
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        Domain::NotificationType::UpdateStableQt5Linux,
#else
        Domain::NotificationType::UpdateStableQt6Linux,
#endif
#elif defined(Q_OS_MACOS)
        Domain::NotificationType::UpdateStableQt5Linux,
        Domain::NotificationType::UpdateStableQt5Windows32,
        Domain::NotificationType::UpdateStableQt5Windows64,
        Domain::NotificationType::UpdateStableQt6Linux,
        Domain::NotificationType::UpdateStableQt6Windows,
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        Domain::NotificationType::UpdateStableQt5Mac,
#else
        Domain::NotificationType::UpdateStableQt6Mac,
#endif
#elif defined(Q_OS_WIN64)
        Domain::NotificationType::UpdateStableQt5Linux,
        Domain::NotificationType::UpdateStableQt5Mac,
        Domain::NotificationType::UpdateStableQt5Windows32,
        Domain::NotificationType::UpdateStableQt6Linux,
        Domain::NotificationType::UpdateStableQt6Mac,
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        Domain::NotificationType::UpdateStableQt5Windows64,
#else
        Domain::NotificationType::UpdateStableQt6Windows,
#endif
#elif defined(Q_OS_WIN32)
        Domain::NotificationType::UpdateStableQt5Linux,
        Domain::NotificationType::UpdateStableQt5Mac,
        Domain::NotificationType::UpdateStableQt5Windows64,
        Domain::NotificationType::UpdateStableQt6Linux,
        Domain::NotificationType::UpdateStableQt6Mac,
        Domain::NotificationType::UpdateStableQt6Windows,
#endif
    };

    for (const auto& notification : reversed(_notifications)) {
        //
        // Пропускаем уведомления нерелевантные для текущего устройства
        //
        if (!stableUpdateTypes.contains(notification.type)
            || irrelevantNotificationTypes.contains(notification.type)) {
            continue;
        }

        //
        // Пропускаем уведомления, которые уже были обработаны
        //
        if (d->notifications.contains(notification)) {
            continue;
        }

        d->notifications.prepend(notification);
    }
}

QString NotificationsManager::lastVersionDownloadLink() const
{
    //
    // Определим уведомление о последнем обновлении
    //
    Domain::Notification latestUpdate;
    for (const auto& notification : std::as_const(d->notifications)) {
        if (notification.type == Domain::NotificationType::UpdateStableQt5Linux
            || notification.type == Domain::NotificationType::UpdateStableQt5Mac
            || notification.type == Domain::NotificationType::UpdateStableQt5Windows32
            || notification.type == Domain::NotificationType::UpdateStableQt5Windows64
            || notification.type == Domain::NotificationType::UpdateStableQt6Linux
            || notification.type == Domain::NotificationType::UpdateStableQt6Mac
            || notification.type == Domain::NotificationType::UpdateStableQt6Windows) {
            latestUpdate = notification;
            break;
        }
    }
    if (latestUpdate.notification.isEmpty()) {
        return {};
    }

    //
    // Извлечём из него ссылку
    //
    const auto json = QJsonDocument::fromJson(latestUpdate.notification.toUtf8()).object();
    return json["download_link"].toString();
}

} // namespace ManagementLayer
