#pragma once

#include <interfaces/ui/i_document_view.h>
#include <ui/widgets/widget/widget.h>


namespace Ui {

/**
 * @brief The page of a character: its name, description and notes
 */
class CharacterInformationView : public Widget, public IDocumentView
{
    Q_OBJECT

public:
    explicit CharacterInformationView(QWidget* _parent = nullptr);
    ~CharacterInformationView() override;

    /**
     * @brief Реализация интерфейса IDocumentView
     */
    /** @{ */
    QWidget* asQWidget() override;
    void setEditingMode(ManagementLayer::DocumentEditingMode _mode) override;
    /** @} */

    /**
     * @brief The name is applied as a whole when its editing is finished, not while it's typed:
     *        renaming a character changes the screenplay
     */
    void setName(const QString& _name);
    void setNameError(const QString& _error);
    Q_SIGNAL void nameEdited(const QString& _name);

    void setDescription(const QString& _description);
    Q_SIGNAL void descriptionChanged(const QString& _description);

    void setNotes(const QString& _notes);
    Q_SIGNAL void notesChanged(const QString& _notes);

protected:
    /**
     * @brief Finish editing of the name when the user moves on to another field
     */
    bool eventFilter(QObject* _watched, QEvent* _event) override;

    /**
     * @brief Обновить переводы
     */
    void updateTranslations() override;

    /**
     * @brief Обновляем виджет при изменении дизайн системы
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
