#include "location_information_view.h"

#include <interfaces/management_layer/i_document_manager.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/card/card.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/ui_helper.h>

#include <QFocusEvent>
#include <QGridLayout>
#include <QScrollArea>


namespace Ui {

namespace {
enum {
    kTopMarginRow = 0,
    kNameRow,
    kDescriptionRow,
    kNotesRow,
    kBottomMarginRow,
};
} // namespace

class LocationInformationView::Implementation
{
public:
    explicit Implementation(QWidget* _parent);


    QScrollArea* content = nullptr;
    //
    Card* locationInfo = nullptr;
    QGridLayout* locationInfoLayout = nullptr;
    TextField* locationName = nullptr;
    TextField* locationDescription = nullptr;
    TextField* locationNotes = nullptr;
};

LocationInformationView::Implementation::Implementation(QWidget* _parent)
    : content(UiHelper::createScrollAreaWithGridLayout(_parent))
    , locationInfo(new Card(_parent))
    , locationInfoLayout(new QGridLayout)
    , locationName(new TextField(locationInfo))
    , locationDescription(new TextField(locationInfo))
    , locationNotes(new TextField(locationInfo))
{
    locationInfoLayout->setContentsMargins({});
    locationInfoLayout->setSpacing(0);
    locationInfoLayout->setRowMinimumHeight(kTopMarginRow, 1);
    locationInfoLayout->addWidget(locationName, kNameRow, 0);
    locationInfoLayout->addWidget(locationDescription, kDescriptionRow, 0);
    locationInfoLayout->addWidget(locationNotes, kNotesRow, 0);
    locationInfoLayout->setRowMinimumHeight(kBottomMarginRow, 1);
    locationInfo->setContentLayout(locationInfoLayout);

    locationName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    locationDescription->setEnterMakesNewLine(true);
    locationNotes->setEnterMakesNewLine(true);
    UiHelper::initSpellingFor({
        locationDescription,
        locationNotes,
    });
    UiHelper::initOptionsFor({
        locationName,
        locationDescription,
        locationNotes,
    });

    auto layout = qobject_cast<QGridLayout*>(content->widget()->layout());
    layout->addWidget(locationInfo, 0, 0);
    layout->setRowStretch(1, 1);
}


// ****


LocationInformationView::LocationInformationView(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(d->content);
    setLayout(layout);

    d->locationName->installEventFilter(this);

    connect(d->locationName, &TextField::textChanged, d->locationName, &TextField::clearError);
    connect(d->locationName, &TextField::enterPressed, this,
            [this] { emit nameEdited(d->locationName->text()); });
    connect(d->locationDescription, &TextField::textChanged, this,
            [this] { emit descriptionChanged(d->locationDescription->text()); });
    connect(d->locationNotes, &TextField::textChanged, this,
            [this] { emit notesChanged(d->locationNotes->text()); });
}

LocationInformationView::~LocationInformationView() = default;

QWidget* LocationInformationView::asQWidget()
{
    return this;
}

void LocationInformationView::setEditingMode(ManagementLayer::DocumentEditingMode _mode)
{
    const auto readOnly = _mode != ManagementLayer::DocumentEditingMode::Edit;
    d->locationName->setReadOnly(readOnly);
    d->locationDescription->setReadOnly(readOnly);
    d->locationNotes->setReadOnly(readOnly);
}

void LocationInformationView::setName(const QString& _name)
{
    d->locationName->clearError();
    if (d->locationName->text() == _name) {
        return;
    }

    d->locationName->setText(_name);
}

void LocationInformationView::setNameError(const QString& _error)
{
    d->locationName->setError(_error);
}

void LocationInformationView::setDescription(const QString& _description)
{
    if (d->locationDescription->text() == _description) {
        return;
    }

    d->locationDescription->setText(_description);
}

void LocationInformationView::setNotes(const QString& _notes)
{
    if (d->locationNotes->text() == _notes) {
        return;
    }

    d->locationNotes->setText(_notes);
}

bool LocationInformationView::eventFilter(QObject* _watched, QEvent* _event)
{
    //
    // A name left for another field is applied like the one confirmed with Enter. It stays as
    // typed, when the focus goes to another window or to a menu instead
    //
    if (_watched == d->locationName && _event->type() == QEvent::FocusOut
        && !d->locationName->isReadOnly()) {
        const auto reason = static_cast<QFocusEvent*>(_event)->reason();
        if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason
            && reason != Qt::MenuBarFocusReason) {
            emit nameEdited(d->locationName->text());
        }
    }

    return Widget::eventFilter(_watched, _event);
}

void LocationInformationView::updateTranslations()
{
    d->locationName->setLabel(tr("Location name"));
    d->locationName->setHelper(tr("Enter renames it in the scene headings too"));
    d->locationDescription->setLabel(tr("Description"));
    d->locationDescription->setHelper(tr("What the place is, in a sentence or two"));
    d->locationNotes->setLabel(tr("Notes"));
}

void LocationInformationView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    Widget::designSystemChangeEvent(_event);

    setBackgroundColor(Ui::DesignSystem::color().surface());

    d->content->widget()->layout()->setContentsMargins(
        QMarginsF(Ui::DesignSystem::layout().px24()
                      + (isLeftToRight() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  Ui::DesignSystem::compactLayout().topContentMargin(),
                  Ui::DesignSystem::layout().px24()
                      + (isRightToLeft() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  Ui::DesignSystem::compactLayout().px24())
            .toMargins());

    d->locationInfo->setBackgroundColor(DesignSystem::color().background());
    for (auto textField : { d->locationName, d->locationDescription, d->locationNotes }) {
        textField->setBackgroundColor(Ui::DesignSystem::color().onBackground());
        textField->setTextColor(Ui::DesignSystem::color().onBackground());
    }
    d->locationInfoLayout->setVerticalSpacing(Ui::DesignSystem::compactLayout().px16());
    d->locationInfoLayout->setRowMinimumHeight(kTopMarginRow,
                                               static_cast<int>(Ui::DesignSystem::layout().px24()));
    d->locationInfoLayout->setRowMinimumHeight(kBottomMarginRow,
                                               static_cast<int>(Ui::DesignSystem::layout().px16()));
}

} // namespace Ui
