#include "screenplay_statistics_manager.h"

#include "screenplay_statistics_view.h"

#include <business_layer/model/screenplay/screenplay_statistics_model.h>

#include <QModelIndex>


namespace ManagementLayer {

ScreenplayStatisticsManager::ScreenplayStatisticsManager(QObject* _parent)
    : QObject(_parent)
{
}

ScreenplayStatisticsManager::~ScreenplayStatisticsManager() = default;

QObject* ScreenplayStatisticsManager::asQObject()
{
    return this;
}

Ui::IDocumentView* ScreenplayStatisticsManager::view()
{
    return m_view;
}

Ui::IDocumentView* ScreenplayStatisticsManager::view(BusinessLayer::AbstractModel* _model)
{
    if (m_view == nullptr) {
        m_view = static_cast<Ui::ScreenplayStatisticsView*>(createView(_model));
    } else {
        m_view->setModel(qobject_cast<BusinessLayer::ScreenplayStatisticsModel*>(_model));
    }
    return m_view;
}

Ui::IDocumentView* ScreenplayStatisticsManager::secondaryView()
{
    return m_secondaryView;
}

Ui::IDocumentView* ScreenplayStatisticsManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (m_secondaryView == nullptr) {
        m_secondaryView = static_cast<Ui::ScreenplayStatisticsView*>(createView(_model));
    } else {
        m_secondaryView->setModel(qobject_cast<BusinessLayer::ScreenplayStatisticsModel*>(_model));
    }
    return m_secondaryView;
}

Ui::IDocumentView* ScreenplayStatisticsManager::createView(BusinessLayer::AbstractModel* _model)
{
    auto view = new Ui::ScreenplayStatisticsView;
    m_views.append(view);
    view->setModel(qobject_cast<BusinessLayer::ScreenplayStatisticsModel*>(_model));
    return view;
}

void ScreenplayStatisticsManager::resetModels()
{
    for (auto view : m_views) {
        if (view != nullptr) {
            view->setModel(nullptr);
        }
    }
}

void ScreenplayStatisticsManager::bind(IDocumentManager* _manager)
{
    if (_manager == nullptr || _manager == this || _manager->asQObject() == nullptr) {
        return;
    }
    // The navigator is a separate plugin: connect its public signals without linking its library.
    auto object = _manager->asQObject();
    if (object->metaObject()->indexOfSignal("currentReportIndexChanged(QModelIndex)") < 0) {
        return;
    }
    connect(object, SIGNAL(currentReportIndexChanged(QModelIndex)), this,
            SLOT(selectReport(QModelIndex)), Qt::UniqueConnection);
    connect(object, SIGNAL(currentPlotIndexChanged(QModelIndex)), this,
            SLOT(selectPlot(QModelIndex)), Qt::UniqueConnection);
}

void ScreenplayStatisticsManager::selectReport(const QModelIndex& _index)
{
    if (m_view != nullptr && _index.isValid()) {
        m_view->selectPage(_index.row());
    }
}

void ScreenplayStatisticsManager::selectPlot(const QModelIndex& _index)
{
    if (m_view != nullptr && _index.isValid()) {
        m_view->selectPage(6 + _index.row());
    }
}

} // namespace ManagementLayer
