#pragma once

#include <QVector>

class QAction;
class QModelIndex;
class QPixmap;
class QWidget;

namespace BusinessLayer {
struct ComplianceRule;
}

namespace Domain {
struct CursorInfo;
}

namespace ManagementLayer {
enum class DocumentEditingMode;
}

namespace Ui {

/**
 * @brief Интерфейс редактора документа
 */
class IDocumentView
{
public:
    virtual ~IDocumentView() = default;

    /**
     * @brief Вернуть указатель на себя как QWidget, если представление является наследником QWidget
     */
    virtual QWidget* asQWidget() = 0;

    /**
     * @brief Включить/отключить полноэкранный режим
     */
    virtual void toggleFullScreen(bool /*_isFullScreen*/)
    {
    }

    /**
     * @brief Список опций редактора
     */
    virtual QVector<QAction*> options() const
    {
        return {};
    }

    /**
     * @brief Задать параметры проекта
     */
    virtual void setProjectInfo(bool /*_isRemote*/, bool /*_isOwner*/,
                                bool /*_allowGrantAccessToProject*/, bool /*_canBeSentForChecking*/,
                                const QVector<BusinessLayer::ComplianceRule>& /*_complianceRules*/)
    {
    }

    /**
     * @brief Задать режим редактирования
     */
    virtual void setEditingMode(ManagementLayer::DocumentEditingMode /*_editingMode*/)
    {
    }

    /**
     * @brief Задать список курсоров соавторов в редакторе
     */
    virtual void setCursors(const QVector<Domain::CursorInfo>& /*_cursors*/)
    {
    }

    /**
     * @brief Пользователь хочет увидеть заданный курсор
     */
    virtual void setCurrentCursor(const Domain::CursorInfo& /*_cursor*/)
    {
    }

    /**
     * @brief Сделать элемент модели с заданным индексом текущим
     */
    virtual void setCurrentModelIndex(const QModelIndex& /*_index*/)
    {
    }



    //
    // Сигналы, на которые может реагировать менеджер приложения
    //

    //
    // @brief Изменилась позиция курсора
    //
    // void cursorChanged(const QByteArray& _cursorData);

    //
    // @brief Пользователь отправил документ на проверку
    //
    // void sendForCheckingRequested();
};

} // namespace Ui
