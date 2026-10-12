#pragma once

#include "../manager_plugin_global.h"

#include <interfaces/management_layer/i_document_manager.h>

#include <QObject>
#include <QPointer>

namespace Ui {
class ScreenplayStatisticsView;
}

namespace ManagementLayer {

class MANAGER_PLUGIN_EXPORT ScreenplayStatisticsManager : public QObject, public IDocumentManager
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "app.starc.ManagementLayer.IDocumentManager")
    Q_INTERFACES(ManagementLayer::IDocumentManager)

public:
    explicit ScreenplayStatisticsManager(QObject* _parent = nullptr);
    ~ScreenplayStatisticsManager() override;

    QObject* asQObject() override;
    Ui::IDocumentView* view() override;
    Ui::IDocumentView* view(BusinessLayer::AbstractModel* _model) override;
    Ui::IDocumentView* secondaryView() override;
    Ui::IDocumentView* secondaryView(BusinessLayer::AbstractModel* _model) override;
    Ui::IDocumentView* createView(BusinessLayer::AbstractModel* _model) override;
    void resetModels() override;
    void bind(IDocumentManager* _manager) override;

private slots:
    void selectReport(const QModelIndex& _index);
    void selectPlot(const QModelIndex& _index);

private:
    QPointer<Ui::ScreenplayStatisticsView> m_view;
    QPointer<Ui::ScreenplayStatisticsView> m_secondaryView;
    QVector<QPointer<Ui::ScreenplayStatisticsView>> m_views;
};

} // namespace ManagementLayer
