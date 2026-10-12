#include "screenplay_statistics_view.h"

#include <business_layer/model/screenplay/screenplay_statistics_model.h>
#include <business_layer/plots/screenplay/screenplay_characters_activity_plot.h>
#include <business_layer/plots/screenplay/screenplay_structure_analysis_plot.h>
#include <business_layer/reports/screenplay/screenplay_cast_report.h>
#include <business_layer/reports/screenplay/screenplay_dialogues_report.h>
#include <business_layer/reports/screenplay/screenplay_gender_report.h>
#include <business_layer/reports/screenplay/screenplay_location_report.h>
#include <business_layer/reports/screenplay/screenplay_scene_report.h>
#include <business_layer/reports/screenplay/screenplay_summary_report.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/plot/plot.h>
#include <utils/helpers/time_helper.h>

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include <cmath>


namespace Ui {

class ScreenplayStatisticsView::Implementation
{
public:
    QPointer<BusinessLayer::ScreenplayStatisticsModel> model;
    QComboBox* selector = nullptr;
    QPushButton* refreshButton = nullptr;
    QStackedWidget* pages = nullptr;
    QLabel* totals = nullptr;
    QLabel* genderTests = nullptr;
    QVector<QLabel*> headings;
    QVector<QTreeView*> tables;
    QVector<::Plot*> plots;

    QVBoxLayout* addPage()
    {
        auto area = new QScrollArea(pages);
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        auto content = new QWidget(area);
        auto layout = new QVBoxLayout(content);
        // Leave room for the project splitter's floating collapse control at the left edge.
        layout->setContentsMargins(40, 16, 16, 16);
        area->setWidget(content);
        pages->addWidget(area);
        return layout;
    }

    void addTable(QVBoxLayout* _layout, const char* _name)
    {
        auto heading = new QLabel;
        headings.append(heading);
        _layout->addWidget(heading);
        auto table = new QTreeView;
        table->setObjectName(QLatin1String(_name));
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);
        table->setWordWrap(true);
        table->setMinimumHeight(160);
        table->header()->setStretchLastSection(false);
        table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        _layout->addWidget(table, 1);
        tables.append(table);
    }

    void setTable(int _index, QAbstractItemModel* _model)
    {
        auto table = tables.at(_index);
        table->setModel(_model);
        table->expandAll();
    }

    void setPlot(int _index, const BusinessLayer::Plot& _data)
    {
        auto plot = plots.at(_index);
        plot->clearGraphs();
        plot->setPlotInfo(_data.info);
        for (const auto& series : _data.data) {
            auto graph = plot->addGraph();
            graph->setName(series.name);
            graph->setPen(QPen(series.color, 2));
            graph->setBrush(series.brushColor.isValid() ? QBrush(series.brushColor)
                                                        : QBrush(Qt::NoBrush));
            graph->setData(series.x, series.y);
        }
        plot->legend->setVisible(!_data.data.isEmpty());
        plot->rescaleAxes();
        if (_data.data.isEmpty()) {
            plot->xAxis->setRange(0, 1);
            plot->yAxis->setRange(0, 1);
        }
        if (_index == 1) {
            auto names = QSharedPointer<QCPAxisTickerText>::create();
            for (const auto& series : _data.data) {
                for (const auto value : series.y) {
                    if (std::isfinite(value)) {
                        names->addTick(value, series.name);
                        break;
                    }
                }
            }
            plot->yAxis->setTicker(names);
            const auto range = plot->yAxis->range();
            plot->yAxis->setRange(range.lower - 0.5, range.upper + 0.5);
        }
        plot->replot();
    }
};

ScreenplayStatisticsView::ScreenplayStatisticsView(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation)
{
    setObjectName("screenplayStatisticsView");
    d->selector = new QComboBox(this);
    d->selector->setObjectName("statisticsReportSelector");
    for (int index = 0; index < 8; ++index) {
        d->selector->addItem({});
    }
    d->refreshButton = new QPushButton(this);
    d->pages = new QStackedWidget(this);

    auto toolbar = new QHBoxLayout;
    toolbar->addWidget(d->selector, 1);
    toolbar->addWidget(d->refreshButton);
    auto layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(d->pages, 1);

    auto summary = d->addPage();
    d->totals = new QLabel;
    d->totals->setObjectName("statisticsTotals");
    d->totals->setWordWrap(true);
    summary->addWidget(d->totals);
    d->addTable(summary, "summaryText");
    d->addTable(summary, "summaryScenes");
    d->addTable(summary, "summaryLocations");
    d->addTable(summary, "summaryCharacters");
    d->addTable(d->addPage(), "sceneReport");
    d->addTable(d->addPage(), "locationReport");
    d->addTable(d->addPage(), "castReport");
    d->addTable(d->addPage(), "dialoguesReport");
    auto gender = d->addPage();
    d->genderTests = new QLabel;
    d->genderTests->setWordWrap(true);
    gender->addWidget(d->genderTests);
    d->addTable(gender, "genderScenes");
    d->addTable(gender, "genderDialogues");
    d->addTable(gender, "genderCharacters");
    for (int index = 0; index < 2; ++index) {
        auto page = d->addPage();
        auto plot = new ::Plot;
        plot->setMinimumSize(400, 350);
        plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        page->addWidget(plot, 1);
        d->plots.append(plot);
    }

    connect(d->selector, &QComboBox::currentIndexChanged, d->pages,
            &QStackedWidget::setCurrentIndex);
    connect(d->refreshButton, &QPushButton::clicked, this, &ScreenplayStatisticsView::refresh);
    updateTranslations();
}

ScreenplayStatisticsView::~ScreenplayStatisticsView() = default;

QWidget* ScreenplayStatisticsView::asQWidget()
{
    return this;
}

void ScreenplayStatisticsView::setModel(BusinessLayer::ScreenplayStatisticsModel* _model)
{
    if (d->model != nullptr) {
        disconnect(d->model, nullptr, this, nullptr);
    }
    d->model = _model;
    if (d->model != nullptr) {
        // Show all existing cast columns; these are report settings, not project data.
        d->model->setCastReportParameters(true, true, 0);
        connect(d->model, &QObject::destroyed, this, [this] { setModel(nullptr); });
    }
    refresh();
}

void ScreenplayStatisticsView::selectPage(int _index)
{
    if (_index >= 0 && _index < d->pages->count()) {
        d->selector->setCurrentIndex(_index);
    }
}

void ScreenplayStatisticsView::refresh()
{
    const bool hasModel = d->model != nullptr && d->model->textModel() != nullptr;
    d->refreshButton->setEnabled(hasModel);
    if (!hasModel) {
        d->totals->clear();
        d->genderTests->clear();
        for (auto table : d->tables) {
            table->setModel(nullptr);
        }
        for (int index = 0; index < d->plots.size(); ++index) {
            d->setPlot(index, {});
        }
        return;
    }

    d->model->updateReports();
    const auto& summary = d->model->summaryReport();
    const auto characters = summary.charactersCount();
    d->totals->setText(tr("Pages: %1    Words: %2    Duration: %3\n"
                          "Text characters: %4 with spaces, %5 without spaces")
                           .arg(summary.pagesCount())
                           .arg(summary.wordsCount())
                           .arg(TimeHelper::toString(summary.duration()))
                           .arg(characters.withSpaces)
                           .arg(characters.withoutSpaces));
    d->setTable(0, summary.textInfoModel());
    d->setTable(1, summary.scenesInfoModel());
    d->setTable(2, summary.locationsInfoModel());
    d->setTable(3, summary.charactersInfoModel());
    d->setTable(4, d->model->sceneReport().sceneModel());
    d->setTable(5, d->model->locationReport().locationModel());
    d->setTable(6, d->model->castReport().castModel());
    d->setTable(7, d->model->dialoguesReport().dialoguesModel());
    const auto& gender = d->model->genderReport();
    d->genderTests->setText(tr("Bechdel test passing scenes: %1\n"
                               "Reverse Bechdel test passing scenes: %2")
                                .arg(gender.bechdelTest())
                                .arg(gender.reverseBechdelTest()));
    d->setTable(8, gender.scenesInfoModel());
    d->setTable(9, gender.dialoguesInfoModel());
    d->setTable(10, gender.charactersInfoModel());
    d->setPlot(0, d->model->structureAnalysisPlot().plot());
    d->setPlot(1, d->model->charactersActivityPlot().plot());
}

void ScreenplayStatisticsView::updateTranslations()
{
    const QStringList reports = {
        tr("Summary statistics"),       tr("Scene report"),
        tr("Location report"),          tr("Cast report"),
        tr("Dialogues report"),         tr("Gender analysis"),
        tr("Story structure analysis"), tr("Characters activity"),
    };
    for (int index = 0; index < reports.size(); ++index) {
        d->selector->setItemText(index, reports.at(index));
    }
    d->refreshButton->setText(tr("Refresh"));
    const QStringList headings = {
        tr("Text"),
        tr("Scenes"),
        tr("Locations"),
        tr("Characters"),
        tr("Scenes"),
        tr("Locations"),
        tr("Characters"),
        tr("Dialogues"),
        tr("Scenes by gender"),
        tr("Dialogues by gender"),
        tr("Characters by gender"),
    };
    for (int index = 0; index < headings.size(); ++index) {
        d->headings.at(index)->setText(headings.at(index));
    }
    for (auto plot : d->plots) {
        plot->xAxis->setLabel(tr("Script time (minutes)"));
    }
    d->plots.at(0)->yAxis->setLabel(tr("Minutes / count"));
    d->plots.at(1)->yAxis->setLabel(tr("Characters"));
}

void ScreenplayStatisticsView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);
    const auto background = DesignSystem::color().background();
    const auto foreground = DesignSystem::color().onBackground();
    setBackgroundColor(background);
    auto colors = palette();
    colors.setColor(QPalette::Window, background);
    colors.setColor(QPalette::Base, background);
    colors.setColor(QPalette::AlternateBase, DesignSystem::color().primary());
    colors.setColor(QPalette::Text, foreground);
    colors.setColor(QPalette::WindowText, foreground);
    colors.setColor(QPalette::Button, DesignSystem::color().primary());
    colors.setColor(QPalette::ButtonText, foreground);
    colors.setColor(QPalette::Highlight, DesignSystem::color().accent());
    setPalette(colors);
    for (auto plot : d->plots) {
        plot->setBackground(background);
        plot->legend->setBrush(background);
        plot->legend->setTextColor(foreground);
        for (auto axis : { plot->xAxis, plot->yAxis }) {
            axis->setBasePen(QPen(foreground));
            axis->setTickPen(QPen(foreground));
            axis->setTickLabelColor(foreground);
            axis->setLabelColor(foreground);
        }
        plot->replot();
    }
}

} // namespace Ui
