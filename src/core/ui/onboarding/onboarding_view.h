#pragma once

#include <ui/widgets/widget/widget.h>


namespace Ui {

/**
 * @brief Контентные страницы посадочного экрана
 */
class OnboardingView : public Widget
{
    Q_OBJECT

public:
    explicit OnboardingView(QWidget* _parent = nullptr);

protected:
    /**
     * @brief Обновляем внешний вид, когда обновилась дизайн система
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;
};

} // namespace Ui
