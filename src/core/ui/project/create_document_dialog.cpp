#include "create_document_dialog.h"

#include "create_document_dialog_option.h"

#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <ui/design_system/design_system.h>
#include <ui/layouts/flow_layout/flow_layout.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/context_menu/context_menu.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/dialog_helper.h>
#include <utils/helpers/names_generator.h>
#include <utils/helpers/ui_helper.h>

#include <QFileDialog>
#include <QGridLayout>
#include <QScrollArea>
#include <QSettings>


namespace Ui {

namespace {
const QLatin1String kLastSelectedType("widgets/create-document-dialog/last-selected-type");
}

class CreateDocumentDialog::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    void updateDocumentInfo(Domain::DocumentObjectType _type, bool _isBlocked);


    QScrollArea* content = nullptr;
    Widget* optionsContainer = nullptr;
    QVBoxLayout* optionsLayout = nullptr;
    AbstractLabel* storyTitle;
    FlowLayout* storyOptionsLayout = nullptr;
    AbstractLabel* storyWorldTitle;
    AbstractLabel* otherTitle;
    QVector<CreateDocumentDialogOption*> options;
    /**
     * @brief Groups of the options with their titles
     */
    QVector<QPair<AbstractLabel*, QLayout*>> optionsGroups;

    H6Label* title = nullptr;
    Body1Label* documentInfo = nullptr;
    TextField* documentName = nullptr;
    bool insertIntoParentEnabled = false;
    TextField* importFilePath = nullptr;
    CheckBox* insertIntoParent = nullptr;

    Body1Label* blockedFeatureContainer = nullptr;
    Body1Label* blockedFeatureLabel = nullptr;

    QHBoxLayout* buttonsLayout = nullptr;
    Button* cancelButton = nullptr;
    Button* createButton = nullptr;
};

CreateDocumentDialog::Implementation::Implementation(QWidget* _parent)
    : content(new QScrollArea(_parent))
    , optionsContainer(new Widget(_parent))
    , optionsLayout(new QVBoxLayout(optionsContainer))
    , storyTitle(new Subtitle1Label(_parent))
    , storyOptionsLayout(new FlowLayout)
    , storyWorldTitle(new Subtitle1Label(_parent))
    , otherTitle(new Subtitle1Label(_parent))
    , title(new H6Label(_parent))
    , documentInfo(new Body1Label(_parent))
    , documentName(new TextField(_parent))
    , importFilePath(new TextField(_parent))
    , insertIntoParent(new CheckBox(_parent))
    , blockedFeatureContainer(new Body1Label(_parent))
    , blockedFeatureLabel(new Body1Label(_parent))
    , buttonsLayout(new QHBoxLayout)
    , cancelButton(new Button(_parent))
    , createButton(new Button(_parent))
{
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::transparent);
    palette.setColor(QPalette::Window, Qt::transparent);
    content->setPalette(palette);
    content->setFrameShape(QFrame::NoFrame);
    content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content->setVerticalScrollBar(new ScrollBar);

    auto makeOption = [this, &_parent](Domain::DocumentObjectType _type) {
        auto option = new CreateDocumentDialogOption(_type, _parent);
        options.append(option);
        return option;
    };

    optionsLayout->setContentsMargins({});
    optionsLayout->setSpacing(0);
    optionsLayout->addWidget(storyTitle);
    {
        storyOptionsLayout->setContentsMargins({});
        storyOptionsLayout->setSpacing(0);
        if (settingsValue(DataStorageLayer::kComponentsScreenplayAvailableKey).toBool()) {
            storyOptionsLayout->addWidget(makeOption(Domain::DocumentObjectType::Screenplay));
        }
        optionsLayout->addLayout(storyOptionsLayout, 1);
        optionsGroups.append({ storyTitle, storyOptionsLayout });
    }
    optionsLayout->addWidget(storyWorldTitle);
    {
        auto layout = new FlowLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addWidget(makeOption(Domain::DocumentObjectType::Character));
        layout->addWidget(makeOption(Domain::DocumentObjectType::Location));
        optionsLayout->addLayout(layout);
        optionsGroups.append({ storyWorldTitle, layout });
    }
    optionsLayout->addWidget(otherTitle);
    {
        auto layout = new FlowLayout;
        layout->setContentsMargins({});
        layout->setSpacing(0);
        if (settingsValue(DataStorageLayer::kComponentsSimpleTextAvailableKey).toBool()) {
            layout->addWidget(makeOption(Domain::DocumentObjectType::Folder));
            layout->addWidget(makeOption(Domain::DocumentObjectType::SimpleText));
        }
        layout->addWidget(makeOption(Domain::DocumentObjectType::MindMap));
        layout->addWidget(makeOption(Domain::DocumentObjectType::ImagesGallery));
        layout->addWidget(makeOption(Domain::DocumentObjectType::Presentation));
        optionsLayout->addLayout(layout);
        optionsGroups.append({ otherTitle, layout });
    }

    options.constFirst()->setChecked(true);

    content->setWidget(optionsContainer);
    content->setWidgetResizable(true);

    UiHelper::setFocusPolicyRecursively(content, Qt::NoFocus);

    documentName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    importFilePath->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    importFilePath->setTrailingIcon(u8"\U000f0256");

    insertIntoParent->hide();

    auto blockedFeaturesLayout = new QVBoxLayout;
    blockedFeaturesLayout->setContentsMargins({});
    blockedFeaturesLayout->setSpacing(0);
    blockedFeaturesLayout->addWidget(blockedFeatureLabel);
    blockedFeatureContainer->setLayout(blockedFeaturesLayout);

    buttonsLayout->setContentsMargins({});
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addWidget(createButton);
}

void CreateDocumentDialog::Implementation::updateDocumentInfo(Domain::DocumentObjectType _type,
                                                              bool _isBlocked)
{
    const QHash<Domain::DocumentObjectType, QString> documenTypeToTitle = {
        { Domain::DocumentObjectType::Folder, tr("Add folder") },
        { Domain::DocumentObjectType::SimpleText, tr("Add text") },
        { Domain::DocumentObjectType::Character, tr("Add character") },
        { Domain::DocumentObjectType::Location, tr("Add location") },
        { Domain::DocumentObjectType::Screenplay, tr("Add screenplay") },
        { Domain::DocumentObjectType::MindMap, tr("Add mind map") },
        { Domain::DocumentObjectType::ImagesGallery, tr("Add image gallery") },
        { Domain::DocumentObjectType::Presentation, tr("Add presentation") },
    };
    const QHash<Domain::DocumentObjectType, QString> documenTypeToInfo = {
        { Domain::DocumentObjectType::Folder,
          tr("Create a folder to group documents inside the story.") },
        { Domain::DocumentObjectType::SimpleText,
          tr("Create a plain text document to write out ideas and notes.") },
        { Domain::DocumentObjectType::Character,
          tr("Create a document with full character's description to track his relations and "
             "follow his journey within the story.") },
        { Domain::DocumentObjectType::Location,
          tr("Create a document to note down the location's description and keep the details.") },
        { Domain::DocumentObjectType::Screenplay,
          tr("Create a document set to streamline your work on the feature film, series, or "
             "animation.") },
        { Domain::DocumentObjectType::MindMap,
          tr("Create a mind map to brainstorm ideas and plan your story development.") },
        { Domain::DocumentObjectType::ImagesGallery,
          tr("Create a moodboard with atmospheric images or photos.") },
        { Domain::DocumentObjectType::Presentation,
          tr("Create a presentation document to store valuable outside content with your "
             "project.") },
    };

    title->setText(documenTypeToTitle.value(_type));
    documentInfo->setText(documenTypeToInfo.value(_type));
    if (_type == Domain::DocumentObjectType::Character
        || _type == Domain::DocumentObjectType::Location) {
        insertIntoParent->hide();
    } else {
        insertIntoParent->setVisible(insertIntoParentEnabled);
    }

    if (_type == Domain::DocumentObjectType::Presentation) {
        importFilePath->show();
    } else {
        importFilePath->hide();
    }

    if (_type == Domain::DocumentObjectType::Character) {
        NamesGenerator::bind(documentName);
    } else {
        NamesGenerator::unbind(documentName);
    }

    if (_isBlocked) {
        documentName->hide();
        importFilePath->hide();
        insertIntoParent->hide();
        blockedFeatureContainer->show();
        createButton->setEnabled(false);
    } else {
        documentName->show();
        blockedFeatureContainer->hide();
        createButton->setEnabled(true);
        documentName->setFocus();
    }
}


// ****

CreateDocumentDialog::CreateDocumentDialog(QWidget* _parent)
    : AbstractDialog(_parent)
    , d(new Implementation(this))
{
    setAcceptButton(d->createButton);
    setRejectButton(d->cancelButton);

    contentsLayout()->setContentsMargins({});
    contentsLayout()->setSpacing(0);
    int row = 0;
    contentsLayout()->addWidget(d->title, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->documentInfo, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->documentName, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->importFilePath, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->insertIntoParent, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->blockedFeatureContainer, row++, 1, 1, 1);
    contentsLayout()->setRowStretch(row++, 1);
    contentsLayout()->addLayout(d->buttonsLayout, row++, 1, 1, 1);
    contentsLayout()->addWidget(d->content, 0, 0, row, 1);
    contentsLayout()->setColumnStretch(0, 2);
    contentsLayout()->setColumnStretch(1, 1);


    for (auto option : std::as_const(d->options)) {
        connect(option, &CreateDocumentDialogOption::checkedChanged, this, [this](bool _isChecked) {
            if (!_isChecked) {
                return;
            }

            for (auto option : std::as_const(d->options)) {
                if (option != sender()) {
                    option->setChecked(false);
                    continue;
                }

                d->updateDocumentInfo(option->documentType(), option->isBlocked());
            }
        });
    }
    connect(d->documentName, &TextField::textChanged, this,
            [this] { d->documentName->clearError(); });
    connect(d->importFilePath, &TextField::textChanged, d->importFilePath, &TextField::clearError);
    connect(d->importFilePath, &TextField::trailingIconPressed, this, [this] {
        const auto projectImportFolder
            = settingsValue(DataStorageLayer::kProjectImportFolderKey).toString();
        const auto filePath = QFileDialog::getOpenFileName(
            this, tr("Choose the file to import"), projectImportFolder, DialogHelper::pdfFilter());
        d->importFilePath->setText(filePath);
        if (!filePath.isEmpty()) {
            setSettingsValue(DataStorageLayer::kProjectImportFolderKey, filePath);
        }
    });
    connect(d->createButton, &Button::clicked, this, [this] {
        Domain::DocumentObjectType documentType = Domain::DocumentObjectType::Undefined;
        for (auto option : std::as_const(d->options)) {
            if (option->isChecked()) {
                documentType = option->documentType();
                break;
            }
        }

        //
        // Персонажи и локации нельзя создавать без названия
        //
        if (d->documentName->text().isEmpty()) {
            QString errorMessage;
            if (documentType == Domain::DocumentObjectType::Character) {
                errorMessage = tr("The character should have a name");
            } else if (documentType == Domain::DocumentObjectType::Location) {
                errorMessage = tr("The location should have a name");
            }
            if (!errorMessage.isEmpty()) {
                d->documentName->setFocus();
                d->documentName->setError(errorMessage);
                return;
            }
        }

        //
        // Презентацию нельзя создать без исходного файла
        //
        if (documentType == Domain::DocumentObjectType::Presentation) {
            if (d->importFilePath->text().isEmpty()) {
                d->importFilePath->setError(tr("Please, select file for importing"));
                return;
            }
        }

        //
        // Если задан файл, проверим что он доступен
        //
        QString filePath;
        if (d->importFilePath->isVisibleTo(this) && !d->importFilePath->text().isEmpty()) {
            QFile file(d->importFilePath->text());
            if (!file.exists()) {
                setImportFileError(tr("The file doesn't exist"));
                return;
            }
            if (file.open(QIODevice::ReadOnly)) {
                file.close();
                filePath = d->importFilePath->text();
            } else {
                setImportFileError(tr("Can't read data from the file"));
                return;
            }
        }

        emit createPressed(documentType, d->documentName->text(), filePath);
    });
    connect(d->cancelButton, &Button::clicked, this, &CreateDocumentDialog::hideDialog);
}

CreateDocumentDialog::~CreateDocumentDialog() = default;

Domain::DocumentObjectType CreateDocumentDialog::lastSelectedType() const
{
    const auto type
        = static_cast<Domain::DocumentObjectType>(QSettings().value(kLastSelectedType).toInt());
    //
    // A remembered choice of a retired format falls back to the screenplay
    //
    return Domain::isUnsupportedDocumentType(type) ? Domain::DocumentObjectType::Screenplay : type;
}

void CreateDocumentDialog::saveSelectedType()
{
    Domain::DocumentObjectType documentType = Domain::DocumentObjectType::Undefined;
    for (auto option : std::as_const(d->options)) {
        if (option->isChecked()) {
            documentType = option->documentType();
            break;
        }
    }
    if (documentType == Domain::DocumentObjectType::Undefined) {
        return;
    }

    QSettings().setValue(kLastSelectedType, static_cast<int>(documentType));
}

void CreateDocumentDialog::setDocumentTypesFilter(
    const std::function<bool(Domain::DocumentObjectType)>& _filter)
{
    //
    // Remove the options of the rejected documents, and the titles of the groups left empty
    //
    bool isOptionRemoved = false;
    for (const auto& group : std::as_const(d->optionsGroups)) {
        const auto title = group.first;
        const auto layout = group.second;
        for (int index = layout->count() - 1; index >= 0; --index) {
            auto option
                = qobject_cast<CreateDocumentDialogOption*>(layout->itemAt(index)->widget());
            if (option == nullptr || _filter(option->documentType())) {
                continue;
            }

            layout->removeWidget(option);
            d->options.removeOne(option);
            delete option;
            isOptionRemoved = true;
        }
        title->setVisible(layout->count() > 0);
    }
    //
    // ... the groups left are kept together at the top, without a gap in place of the removed
    //
    if (isOptionRemoved) {
        d->optionsLayout->setStretchFactor(d->storyOptionsLayout, 0);
        d->optionsLayout->addStretch();
    }

    //
    // Nothing can be created, if there are no options left
    //
    if (d->options.isEmpty()) {
        d->createButton->setEnabled(false);
        return;
    }

    //
    // If the selected option was removed, select the first of the remaining ones
    //
    if (std::none_of(
            d->options.begin(), d->options.end(),
            [](const CreateDocumentDialogOption* _option) { return _option->isChecked(); })) {
        d->options.constFirst()->setChecked(true);
    }
}

void CreateDocumentDialog::setBlockedDocumentTypes(
    const QVector<Domain::DocumentObjectType>& _blockedDocumentTypes)
{
    for (auto option : std::as_const(d->options)) {
        option->setBlocked(_blockedDocumentTypes.contains(option->documentType()));
    }

    for (auto option : std::as_const(d->options)) {
        if (option->isChecked()) {
            d->updateDocumentInfo(option->documentType(), option->isBlocked());
            break;
        }
    }
}

void CreateDocumentDialog::setDocumentType(Domain::DocumentObjectType _type)
{
    for (auto option : std::as_const(d->options)) {
        if (option->documentType() == _type) {
            option->setChecked(true);
            //
            // Скорректируем прокрутку для опций идущих за первой группой
            //
            if (option->documentType() >= Domain::DocumentObjectType::Characters) {
                d->content->ensureWidgetVisible(option);
            }
            break;
        }
    }
}

void CreateDocumentDialog::setNameError(const QString& _error)
{
    d->documentName->setError(_error);
}

void CreateDocumentDialog::setImportFileError(const QString& _error)
{
    d->importFilePath->setError(_error);
}

void CreateDocumentDialog::setInsertionParent(const QString& _parentName)
{
    if (_parentName.isEmpty()) {
        d->insertIntoParentEnabled = false;
        d->insertIntoParent->hide();
        return;
    }

    d->insertIntoParentEnabled = true;
    d->insertIntoParent->setText(QString("%1 \"%2\"").arg(tr("Insert into"), _parentName));
    d->insertIntoParent->setChecked(true);
    d->insertIntoParent->show();
}

bool CreateDocumentDialog::needInsertIntoParent() const
{
    return d->insertIntoParent->isChecked();
}

QWidget* CreateDocumentDialog::focusedWidgetAfterShow() const
{
    return d->documentName;
}

QWidget* CreateDocumentDialog::lastFocusableWidget() const
{
    return d->createButton;
}

void CreateDocumentDialog::updateTranslations()
{
    d->storyTitle->setText(tr("Story"));
    d->storyWorldTitle->setText(tr("Characters & locations"));
    d->otherTitle->setText(tr("Structure & notes"));

    d->title->setText(tr("Add document"));
    for (auto option : std::as_const(d->options)) {
        if (option->isChecked()) {
            d->updateDocumentInfo(option->documentType(), option->isBlocked());
            break;
        }
    }
    d->documentName->setLabel(tr("Name"));
    d->importFilePath->setLabel(tr("Choose file for importing"));
    d->importFilePath->setTrailingIconToolTip(tr("Choose file for importing"));
    d->blockedFeatureLabel->setText(tr("This document is unavailable."));
    d->cancelButton->setText(tr("Cancel"));
    d->createButton->setText(tr("Create"));
}

void CreateDocumentDialog::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    AbstractDialog::designSystemChangeEvent(_event);

    contentsLayout()->setColumnMinimumWidth(1, DesignSystem::layout().px(299));
    setContentFixedWidth(DesignSystem::layout().px(898));

    d->optionsContainer->setBackgroundColor(DesignSystem::color().surface());
    d->content->setFixedHeight(DesignSystem::layout().px(556));
    d->optionsLayout->setContentsMargins(
        isLeftToRight() ? DesignSystem::layout().px16() : 0.0, DesignSystem::layout().px8(),
        isLeftToRight() ? 0.0 : DesignSystem::layout().px16(), DesignSystem::layout().px16());
    for (auto title : std::vector<Widget*>{
             d->storyTitle,
             d->storyWorldTitle,
             d->otherTitle,
         }) {
        title->setBackgroundColor(DesignSystem::color().surface());
        title->setTextColor(DesignSystem::color().onSurface());
        title->setContentsMargins(DesignSystem::layout().px16(), DesignSystem::layout().px12(),
                                  DesignSystem::layout().px16(), DesignSystem::layout().px4());
    }
    for (auto option : std::as_const(d->options)) {
        option->setBackgroundColor(DesignSystem::color().surface());
        option->setTextColor(DesignSystem::color().onSurface());
        option->setContentsMargins(DesignSystem::layout().px8(), DesignSystem::layout().px8(),
                                   DesignSystem::layout().px8(), DesignSystem::layout().px8());
    }

    d->title->setBackgroundColor(DesignSystem::color().background());
    d->title->setTextColor(DesignSystem::color().onBackground());
    d->title->setContentsMargins(DesignSystem::layout().px24(), DesignSystem::layout().px24(),
                                 DesignSystem::layout().px24(), DesignSystem::layout().px16());

    for (auto textField : std::vector<TextField*>{ d->documentName, d->importFilePath }) {
        textField->setTextColor(DesignSystem::color().onBackground());
        textField->setBackgroundColor(DesignSystem::color().onBackground());
        textField->setCustomMargins(
            { DesignSystem::layout().px24(), DesignSystem::compactLayout().px8(),
              DesignSystem::layout().px24(), DesignSystem::compactLayout().px8() });
    }

    auto documentInfoMargins = DesignSystem::label().margins().toMargins();
    documentInfoMargins.setTop(0);
    d->documentInfo->setContentsMargins(documentInfoMargins);
    d->documentInfo->setTextColor(DesignSystem::color().onBackground());
    d->documentInfo->setBackgroundColor(DesignSystem::color().background());

    d->insertIntoParent->setTextColor(DesignSystem::color().onBackground());
    d->insertIntoParent->setBackgroundColor(DesignSystem::color().background());

    d->blockedFeatureContainer->setContentsMargins(DesignSystem::layout().px24(),
                                                   DesignSystem::compactLayout().px8(),
                                                   DesignSystem::layout().px24(), 0);
    d->blockedFeatureContainer->setBorderRadius(DesignSystem::textField().borderRadius());
    d->blockedFeatureContainer->setBackgroundColor(
        ColorHelper::transparent(DesignSystem::color().onBackground(),
                                 DesignSystem::textField().backgroundActiveColorOpacity()));
    d->blockedFeatureContainer->layout()->setContentsMargins(
        DesignSystem::layout().px16(), DesignSystem::layout().px16(), DesignSystem::layout().px8(),
        DesignSystem::layout().px8());
    d->blockedFeatureLabel->setContentsMargins(0, 0, DesignSystem::layout().px8(),
                                               DesignSystem::layout().px24());
    d->blockedFeatureLabel->setBackgroundColor(Qt::transparent);
    d->blockedFeatureLabel->setTextColor(DesignSystem::color().onBackground());

    for (auto button : {
             d->cancelButton,
             d->createButton,
         }) {
        button->setBackgroundColor(DesignSystem::color().accent());
        button->setTextColor(DesignSystem::color().accent());
    }
    UiHelper::initColorsFor(d->createButton, UiHelper::DialogAccept);
    UiHelper::initColorsFor(d->cancelButton, UiHelper::DialogDefault);

    d->buttonsLayout->setContentsMargins(
        QMarginsF(DesignSystem::layout().px12(), DesignSystem::layout().px12(),
                  DesignSystem::layout().px16(), DesignSystem::layout().px16())
            .toMargins());
}

} // namespace Ui
