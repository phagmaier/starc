#pragma once

#include <interfaces/ui/i_document_view.h>
#include <ui/widgets/widget/widget.h>

namespace BusinessLayer {
class ScreenplayStatisticsModel;
}

namespace Ui {

class ScreenplayStatisticsView : public Widget, public IDocumentView
{
    Q_OBJECT

public:
    explicit ScreenplayStatisticsView(QWidget* _parent = nullptr);
    ~ScreenplayStatisticsView() override;

    QWidget* asQWidget() override;
    void setModel(BusinessLayer::ScreenplayStatisticsModel* _model);
    void selectPage(int _index);

protected:
    void updateTranslations() override;
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    void refresh();

    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
