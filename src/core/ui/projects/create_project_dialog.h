#pragma once

#include <ui/widgets/dialog/abstract_dialog.h>

namespace Ui {

/**
 * @brief Диалог создания нового проекта
 */
class CreateProjectDialog : public AbstractDialog
{
    Q_OBJECT

public:
    explicit CreateProjectDialog(QWidget* _parent = nullptr);
    ~CreateProjectDialog() override;

    /**
     * @brief Тип проекта
     */
    int projectType() const;
    void setProjectType(int _type);

    /**
     * @brief Название нового проекта
     */
    QString projectName() const;
    void setProjectName(const QString& _name);

    /**
     * @brief Папка, куда будет сохраняться новый проект
     */
    QString projectFolder() const;
    void setProjectFolder(const QString& _path);

    /**
     * @brief Папка, из которой будут выбираться проекты для импорта
     */
    QString importFilePath() const;
    void setImportFolder(const QString& _path);

    /**
     * @brief Показать проблему создания проекта в заданной папке
     */
    void showProjectFolderError();

signals:
    /**
     * @brief Пользователь нажал на кнопку создания проекта
     */
    void createProjectPressed();

protected:
    /**
     * @brief Определим виджет, который необходимо сфокусировать после отображения диалога
     */
    QWidget* focusedWidgetAfterShow() const override;

    /**
     * @brief Опеределим последний фокусируемый виджет в диалоге
     */
    QWidget* lastFocusableWidget() const override;

    /**
     * @brief Обновить переводы
     */
    void updateTranslations() override;

    /**
     * @brief Обновляем UI при изменении дизайн системы
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
