#include "project_information_view.h"

#include <interfaces/management_layer/i_document_manager.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/card/card.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/text_helper.h>
#include <utils/helpers/ui_helper.h>

#include <QGridLayout>
#include <QScrollArea>


namespace Ui {

class ProjectInformationView::Implementation
{
public:
    explicit Implementation(QWidget* _parent);


    QScrollArea* parametersPage = nullptr;
    //
    Card* projectInfo = nullptr;
    QGridLayout* projectInfoLayout = nullptr;
    TextField* projectName = nullptr;
    TextField* projectLogline = nullptr;
};

ProjectInformationView::Implementation::Implementation(QWidget* _parent)
    : parametersPage(UiHelper::createScrollAreaWithGridLayout(_parent))
    , projectInfo(new Card(_parent))
    , projectInfoLayout(new QGridLayout)
    , projectName(new TextField(projectInfo))
    , projectLogline(new TextField(projectInfo))
{
    projectInfoLayout->setContentsMargins({});
    projectInfoLayout->setSpacing(0);
    int row = 0;
    projectInfoLayout->setRowMinimumHeight(row++, 1); // добавляем пустую строку над названием
    projectInfoLayout->addWidget(projectName, row++, 0);
    projectInfoLayout->addWidget(projectLogline, row++, 0);
    projectInfoLayout->setRowMinimumHeight(row++, 1); // добавляем пустую строку под логлайном
    projectInfo->setContentLayout(projectInfoLayout);

    projectName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    projectLogline->setEnterMakesNewLine(true);
    UiHelper::initSpellingFor(projectLogline);
    UiHelper::initOptionsFor({
        projectName,
        projectLogline,
    });

    auto layout = qobject_cast<QGridLayout*>(parametersPage->widget()->layout());
    layout->addWidget(projectInfo, 0, 0);
    layout->setRowStretch(1, 1);
}


// ****


ProjectInformationView::ProjectInformationView(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(d->parametersPage);
    setLayout(layout);

    connect(d->projectName, &TextField::textChanged, this,
            [this] { emit nameChanged(d->projectName->text()); });
    connect(d->projectLogline, &TextField::textChanged, this,
            [this] { emit loglineChanged(d->projectLogline->text()); });
}

ProjectInformationView::~ProjectInformationView() = default;

QWidget* ProjectInformationView::asQWidget()
{
    return this;
}

void ProjectInformationView::setEditingMode(ManagementLayer::DocumentEditingMode _mode)
{
    const auto readOnly = _mode != ManagementLayer::DocumentEditingMode::Edit;
    d->projectName->setReadOnly(readOnly);
    d->projectLogline->setReadOnly(readOnly);
}

void ProjectInformationView::setName(const QString& _name)
{
    if (d->projectName->text() == _name) {
        return;
    }

    d->projectName->setText(_name);
}

void ProjectInformationView::setLogline(const QString& _logline)
{
    if (d->projectLogline->text() == _logline) {
        return;
    }

    d->projectLogline->setText(_logline);
}

void ProjectInformationView::updateTranslations()
{
    d->projectName->setLabel(tr("Project name"));
    d->projectLogline->setLabel(tr("Short description"));
    d->projectLogline->setHelper(tr("Tagline, logline or something similar"));
}

void ProjectInformationView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);

    setBackgroundColor(Ui::DesignSystem::color().surface());

    d->parametersPage->widget()->layout()->setContentsMargins(
        QMarginsF(Ui::DesignSystem::layout().px24()
                      + (isLeftToRight() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  Ui::DesignSystem::compactLayout().topContentMargin(),
                  Ui::DesignSystem::layout().px24()
                      + (isRightToLeft() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  Ui::DesignSystem::compactLayout().px24())
            .toMargins());

    d->projectInfo->setBackgroundColor(DesignSystem::color().background());
    for (auto textField : { d->projectName, d->projectLogline }) {
        textField->setBackgroundColor(Ui::DesignSystem::color().onBackground());
        textField->setTextColor(Ui::DesignSystem::color().onBackground());
    }
    d->projectInfoLayout->setVerticalSpacing(Ui::DesignSystem::compactLayout().px16());
    d->projectInfoLayout->setRowMinimumHeight(0,
                                              static_cast<int>(Ui::DesignSystem::layout().px24()));
    d->projectInfoLayout->setRowMinimumHeight(3,
                                              static_cast<int>(Ui::DesignSystem::layout().px16()));
}

} // namespace Ui
