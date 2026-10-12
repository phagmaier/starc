#include "character_information_view.h"

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

class CharacterInformationView::Implementation
{
public:
    explicit Implementation(QWidget* _parent);


    QScrollArea* content = nullptr;
    //
    Card* characterInfo = nullptr;
    QGridLayout* characterInfoLayout = nullptr;
    TextField* characterName = nullptr;
    TextField* characterDescription = nullptr;
    TextField* characterNotes = nullptr;
};

CharacterInformationView::Implementation::Implementation(QWidget* _parent)
    : content(UiHelper::createScrollAreaWithGridLayout(_parent))
    , characterInfo(new Card(_parent))
    , characterInfoLayout(new QGridLayout)
    , characterName(new TextField(characterInfo))
    , characterDescription(new TextField(characterInfo))
    , characterNotes(new TextField(characterInfo))
{
    characterInfoLayout->setContentsMargins({});
    characterInfoLayout->setSpacing(0);
    characterInfoLayout->setRowMinimumHeight(kTopMarginRow, 1);
    characterInfoLayout->addWidget(characterName, kNameRow, 0);
    characterInfoLayout->addWidget(characterDescription, kDescriptionRow, 0);
    characterInfoLayout->addWidget(characterNotes, kNotesRow, 0);
    characterInfoLayout->setRowMinimumHeight(kBottomMarginRow, 1);
    characterInfo->setContentLayout(characterInfoLayout);

    characterName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    characterDescription->setEnterMakesNewLine(true);
    characterNotes->setEnterMakesNewLine(true);
    UiHelper::initSpellingFor({
        characterDescription,
        characterNotes,
    });
    UiHelper::initOptionsFor({
        characterName,
        characterDescription,
        characterNotes,
    });

    auto layout = qobject_cast<QGridLayout*>(content->widget()->layout());
    layout->addWidget(characterInfo, 0, 0);
    layout->setRowStretch(1, 1);
}


// ****


CharacterInformationView::CharacterInformationView(QWidget* _parent)
    : Widget(_parent)
    , d(new Implementation(this))
{
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(d->content);
    setLayout(layout);

    d->characterName->installEventFilter(this);

    connect(d->characterName, &TextField::textChanged, d->characterName, &TextField::clearError);
    connect(d->characterName, &TextField::enterPressed, this,
            [this] { emit nameEdited(d->characterName->text()); });
    connect(d->characterDescription, &TextField::textChanged, this,
            [this] { emit descriptionChanged(d->characterDescription->text()); });
    connect(d->characterNotes, &TextField::textChanged, this,
            [this] { emit notesChanged(d->characterNotes->text()); });
}

CharacterInformationView::~CharacterInformationView() = default;

QWidget* CharacterInformationView::asQWidget()
{
    return this;
}

void CharacterInformationView::setEditingMode(ManagementLayer::DocumentEditingMode _mode)
{
    const auto readOnly = _mode != ManagementLayer::DocumentEditingMode::Edit;
    d->characterName->setReadOnly(readOnly);
    d->characterDescription->setReadOnly(readOnly);
    d->characterNotes->setReadOnly(readOnly);
}

void CharacterInformationView::setName(const QString& _name)
{
    d->characterName->clearError();
    if (d->characterName->text() == _name) {
        return;
    }

    d->characterName->setText(_name);
}

void CharacterInformationView::setNameError(const QString& _error)
{
    d->characterName->setError(_error);
}

void CharacterInformationView::setDescription(const QString& _description)
{
    if (d->characterDescription->text() == _description) {
        return;
    }

    d->characterDescription->setText(_description);
}

void CharacterInformationView::setNotes(const QString& _notes)
{
    if (d->characterNotes->text() == _notes) {
        return;
    }

    d->characterNotes->setText(_notes);
}

bool CharacterInformationView::eventFilter(QObject* _watched, QEvent* _event)
{
    //
    // A name left for another field is applied like the one confirmed with Enter. It stays as
    // typed, when the focus goes to another window or to a menu instead
    //
    if (_watched == d->characterName && _event->type() == QEvent::FocusOut
        && !d->characterName->isReadOnly()) {
        const auto reason = static_cast<QFocusEvent*>(_event)->reason();
        if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason
            && reason != Qt::MenuBarFocusReason) {
            emit nameEdited(d->characterName->text());
        }
    }

    return Widget::eventFilter(_watched, _event);
}

void CharacterInformationView::updateTranslations()
{
    d->characterName->setLabel(tr("Character name"));
    d->characterName->setHelper(tr("Enter renames it in the screenplay too"));
    d->characterDescription->setLabel(tr("Description"));
    d->characterDescription->setHelper(tr("Who the character is, in a sentence or two"));
    d->characterNotes->setLabel(tr("Notes"));
}

void CharacterInformationView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
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

    d->characterInfo->setBackgroundColor(DesignSystem::color().background());
    for (auto textField : { d->characterName, d->characterDescription, d->characterNotes }) {
        textField->setBackgroundColor(Ui::DesignSystem::color().onBackground());
        textField->setTextColor(Ui::DesignSystem::color().onBackground());
    }
    d->characterInfoLayout->setVerticalSpacing(Ui::DesignSystem::compactLayout().px16());
    d->characterInfoLayout->setRowMinimumHeight(
        kTopMarginRow, static_cast<int>(Ui::DesignSystem::layout().px24()));
    d->characterInfoLayout->setRowMinimumHeight(
        kBottomMarginRow, static_cast<int>(Ui::DesignSystem::layout().px16()));
}

} // namespace Ui
