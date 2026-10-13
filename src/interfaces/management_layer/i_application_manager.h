#pragma once

#include <QtPlugin>

class QString;

namespace ManagementLayer {

/**
 * @brief Интерфейс менеджера приложения
 */
class IApplicationManager
{
public:
    virtual ~IApplicationManager() = default;

    /**
     * @brief Получить путь к файлу с логами
     */
    virtual QString logFilePath() const = 0;

    /**
     * @brief Запустить приложение
     */
    virtual void exec(const QString& _fileToOpenPath) = 0;

    /**
     * @brief Открыть проект по заданному пути
     * @return True only when opened synchronously. Startup buffering or a save question
     *         can defer opening; cancellation and failed saves leave the current story open.
     */
    virtual bool openProject(const QString& _path) = 0;
};

} // namespace ManagementLayer

Q_DECLARE_INTERFACE(ManagementLayer::IApplicationManager,
                    "app.starc.ManagementLayer.IApplicationManager")
