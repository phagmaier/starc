#include "onboarding_view.h"

#include <ui/design_system/design_system.h>


namespace Ui {

OnboardingView::OnboardingView(QWidget* _parent)
    : Widget(_parent)
{
}

void OnboardingView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Q_UNUSED(_event);

    setBackgroundColor(DesignSystem::color().surface());
}

} // namespace Ui
