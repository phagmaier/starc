#include "project_models_facade.h"

#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/characters/characters_model.h>
#include <business_layer/model/images/images_gallery_model.h>
#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/locations/locations_model.h>
#include <business_layer/model/mind_map/mind_map_model.h>
#include <business_layer/model/presentation/presentation_model.h>
#include <business_layer/model/project/project_information_model.h>
#include <business_layer/model/recycle_bin/recycle_bin_model.h>
#include <business_layer/model/screenplay/screenplay_dictionaries_model.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/screenplay_statistics_model.h>
#include <business_layer/model/screenplay/screenplay_synopsis_model.h>
#include <business_layer/model/screenplay/screenplay_title_page_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/simple_text/simple_text_model.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <utils/logging.h>

#include <QSet>


namespace ManagementLayer {

class ProjectModelsFacade::Implementation
{
public:
    explicit Implementation(BusinessLayer::StructureModel* _projectStructureModel,
                            BusinessLayer::AbstractImageWrapper* _imageWrapper,
                            BusinessLayer::AbstractRawDataWrapper* _rawDataWrapper);


    BusinessLayer::StructureModel* projectStructureModel = nullptr;
    BusinessLayer::AbstractImageWrapper* imageWrapper = nullptr;
    BusinessLayer::AbstractRawDataWrapper* rawDataWrapper = nullptr;
    QHash<Domain::DocumentObject*, BusinessLayer::AbstractModel*> documentsToModels;
};

ProjectModelsFacade::Implementation::Implementation(
    BusinessLayer::StructureModel* _projectStructureModel,
    BusinessLayer::AbstractImageWrapper* _imageWrapper,
    BusinessLayer::AbstractRawDataWrapper* _rawDataWrapper)
    : projectStructureModel(_projectStructureModel)
    , imageWrapper(_imageWrapper)
    , rawDataWrapper(_rawDataWrapper)
{
}


// ****


ProjectModelsFacade::ProjectModelsFacade(BusinessLayer::StructureModel* _projectStructureModel,
                                         BusinessLayer::AbstractImageWrapper* _imageWrapper,
                                         BusinessLayer::AbstractRawDataWrapper* _rawDataWrapper,
                                         QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(_projectStructureModel, _imageWrapper, _rawDataWrapper))
{
}

ProjectModelsFacade::~ProjectModelsFacade()
{
    clear();
}

void ProjectModelsFacade::clear()
{
    //
    // Формируем список моделей для удаления, т.к. некоторые модели являются лишь ссылками на другие
    // модели, например модель тритмента - это ссылка на модель текста сценария
    //
    QSet<BusinessLayer::AbstractModel*> modelsToDelete;
    for (auto model : std::as_const(d->documentsToModels)) {
        modelsToDelete.insert(model);

        //
        // ... а заодно отсоединяем модели от клиентов и очищаем их
        //
        model->disconnect();
        model->clear();
    }

    //
    // Собственно удаляем модели
    //
    qDeleteAll(modelsToDelete);

    //
    // И очищаем список загруженных моделей
    //
    d->documentsToModels.clear();
}

BusinessLayer::AbstractModel* ProjectModelsFacade::modelFor(const QUuid& _uuid)
{
    return modelFor(DataStorageLayer::StorageFacade::documentStorage()->document(_uuid));
}

BusinessLayer::AbstractModel* ProjectModelsFacade::modelFor(Domain::DocumentObjectType _type)
{
    if (Domain::isUnsupportedDocumentType(_type)) {
        return nullptr;
    }
    return modelFor(DataStorageLayer::StorageFacade::documentStorage()->document(_type));
}

BusinessLayer::AbstractModel* ProjectModelsFacade::modelFor(Domain::DocumentObject* _document)
{
    if (_document == nullptr || Domain::isUnsupportedDocumentType(_document->type())) {
        return nullptr;
    }

    QVector<Domain::DocumentObject*> documentsToLoad = { _document };

    while (!documentsToLoad.isEmpty()) {
        auto documentToLoad = documentsToLoad.takeFirst();
        //
        // Является ли документы алиасом к другому, если да, то не исопльзуем его контент
        //
        bool isDocumentAlias = false;

        if (!d->documentsToModels.contains(documentToLoad)) {
            BusinessLayer::AbstractModel* model = nullptr;
            switch (documentToLoad->type()) {
            case Domain::DocumentObjectType::Project: {
                auto projectInformationModel = new BusinessLayer::ProjectInformationModel;
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::nameChanged, this,
                        &ProjectModelsFacade::projectNameChanged, Qt::UniqueConnection);
                //
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::
                            overrideCommonSettingsForScreenplayChanged,
                        this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                        Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::templateIdForScreenplayChanged,
                        this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                        Qt::UniqueConnection);
                connect(
                    projectInformationModel,
                    &BusinessLayer::ProjectInformationModel::showSceneNumbersForScreenplayChanged,
                    this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                    Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::
                            showSceneNumbersOnLeftForScreenplayChanged,
                        this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                        Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::
                            showSceneNumbersOnRightForScreenplayChanged,
                        this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                        Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::
                            showDialoguesNumbersForScreenplayChanged,
                        this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                        Qt::UniqueConnection);
                connect(
                    projectInformationModel,
                    &BusinessLayer::ProjectInformationModel::chronometerOptionsForScreenplayChanged,
                    this, &ProjectModelsFacade::projectScreenplaySettingsChanged,
                    Qt::UniqueConnection);
                //
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::collaboratorInviteRequested, this,
                        &ProjectModelsFacade::projectCollaboratorInviteRequested,
                        Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::collaboratorUpdateRequested, this,
                        &ProjectModelsFacade::projectCollaboratorUpdateRequested,
                        Qt::UniqueConnection);
                connect(projectInformationModel,
                        &BusinessLayer::ProjectInformationModel::collaboratorRemoveRequested, this,
                        &ProjectModelsFacade::projectCollaboratorRemoveRequested,
                        Qt::UniqueConnection);
                model = projectInformationModel;
                break;
            }

            case Domain::DocumentObjectType::RecycleBin: {
                auto recycleBinModel = new BusinessLayer::RecycleBinModel;

                auto updateChildCount = [this, recycleBinModel,
                                         documentUuid = documentToLoad->uuid()] {
                    auto recycleBinItem = d->projectStructureModel->itemForUuid(documentUuid);
                    if (recycleBinItem == nullptr) {
                        return;
                    }

                    std::function<int(BusinessLayer::StructureModelItem*)> countChildren;
                    countChildren = [&countChildren](BusinessLayer::StructureModelItem* _item) {
                        int childrenSize = 0;
                        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
                            childrenSize += 1 + countChildren(_item->childAt(childIndex));
                        }
                        return childrenSize;
                    };
                    recycleBinModel->setDocumentsToRemoveSize(countChildren(recycleBinItem));
                };
                updateChildCount();
                connect(d->projectStructureModel, &BusinessLayer::StructureModel::rowsInserted,
                        recycleBinModel, updateChildCount);
                connect(d->projectStructureModel, &BusinessLayer::StructureModel::rowsMoved,
                        recycleBinModel, updateChildCount);
                connect(d->projectStructureModel, &BusinessLayer::StructureModel::rowsRemoved,
                        recycleBinModel, updateChildCount);
                connect(recycleBinModel, &BusinessLayer::RecycleBinModel::emptyRecycleBinRequested,
                        this, &ProjectModelsFacade::emptyRecycleBinRequested);

                model = recycleBinModel;
                break;
            }

            case Domain::DocumentObjectType::Screenplay: {
                const auto screenplayItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (screenplayItem == nullptr) {
                    return nullptr;
                }

                auto screenplayModel = new BusinessLayer::ScreenplayInformationModel;
                connect(screenplayModel,
                        &BusinessLayer::ScreenplayInformationModel::titlePageVisibleChanged, this,
                        [this, screenplayModel](bool _visible) {
                            emit screenplayTitlePageVisibilityChanged(screenplayModel, _visible);
                        });
                connect(screenplayModel,
                        &BusinessLayer::ScreenplayInformationModel::synopsisVisibleChanged, this,
                        [this, screenplayModel](bool _visible) {
                            emit screenplaySynopsisVisibilityChanged(screenplayModel, _visible);
                        });
                connect(screenplayModel,
                        &BusinessLayer::ScreenplayInformationModel::treatmentVisibleChanged, this,
                        [this, screenplayModel](bool _visible) {
                            emit screenplayTreatmentVisibilityChanged(screenplayModel, _visible);
                        });
                connect(screenplayModel,
                        &BusinessLayer::ScreenplayInformationModel::screenplayTextVisibleChanged,
                        this, [this, screenplayModel](bool _visible) {
                            emit screenplayTextVisibilityChanged(screenplayModel, _visible);
                        });
                connect(
                    screenplayModel,
                    &BusinessLayer::ScreenplayInformationModel::screenplayStatisticsVisibleChanged,
                    this, [this, screenplayModel](bool _visible) {
                        emit screenplayStatisticsVisibilityChanged(screenplayModel, _visible);
                    });

                model = screenplayModel;
                break;
            }

            case Domain::DocumentObjectType::ScreenplayTitlePage: {
                const auto titlePageItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (titlePageItem == nullptr) {
                    return nullptr;
                }

                Q_ASSERT(titlePageItem->parent());

                auto titlePageModel = new BusinessLayer::ScreenplayTitlePageModel;
                const auto parentUuid = titlePageItem->parent()->uuid();

                //
                // Добавляем в модель титульной страницы, модель информации о сценарие
                //
                auto informationModel = qobject_cast<BusinessLayer::ScreenplayInformationModel*>(
                    modelFor(parentUuid));
                Q_ASSERT(informationModel);
                if (informationModel == nullptr) {
                    Log::warning("Information model isn't initialized for screenplay title page "
                                 "with uuid %1",
                                 documentToLoad->uuid().toString());
                    titlePageModel->deleteLater();
                    return nullptr;
                }
                titlePageModel->setInformationModel(informationModel);

                model = titlePageModel;
                break;
            }

            case Domain::DocumentObjectType::ScreenplaySynopsis: {
                const auto synopsisItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (synopsisItem == nullptr) {
                    return nullptr;
                }
                Q_ASSERT(synopsisItem->parent());

                auto synopsisModel = new BusinessLayer::ScreenplaySynopsisModel;
                const auto parentUuid = synopsisItem->parent()->uuid();

                //
                // Добавляем в модель синопсиса, модель информации о сценарие
                //
                auto informationModel = qobject_cast<BusinessLayer::ScreenplayInformationModel*>(
                    modelFor(parentUuid));
                Q_ASSERT(informationModel);
                if (informationModel == nullptr) {
                    Log::warning(
                        "Information model isn't initialized for screenplay synopsis with uuid %1",
                        documentToLoad->uuid().toString());
                    synopsisModel->deleteLater();
                    return nullptr;
                }
                synopsisModel->setInformationModel(informationModel);

                model = synopsisModel;
                break;
            }

            case Domain::DocumentObjectType::ScreenplayTreatment: {
                //
                // Модель по сути является алиасом к тексту сценария, поэтому используем модель
                // сценария
                //
                isDocumentAlias = true;

                const auto treatmentItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (treatmentItem == nullptr) {
                    return nullptr;
                }
                Q_ASSERT(treatmentItem->parent());

                //
                // ... модель сценария
                //
                BusinessLayer::StructureModelItem* screenplayItem = nullptr;
                for (int index = 0; index < treatmentItem->parent()->childCount(); ++index) {
                    auto childItem = treatmentItem->parent()->childAt(index);
                    if (childItem->type() == Domain::DocumentObjectType::ScreenplayText) {
                        screenplayItem = childItem;
                        break;
                    }
                }
                Q_ASSERT(screenplayItem);
                model = modelFor(screenplayItem->uuid());
                //
                // ... если не удалось получить модель сценария, то значит она ещё не была загружена
                //     из облака, возвращаем тут пустую модель, т.к. это нормальная штатная ситуация
                //
                if (model == nullptr) {
                    return nullptr;
                }

                break;
            }

            case Domain::DocumentObjectType::ScreenplayText: {
                const auto screenplayItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (screenplayItem == nullptr) {
                    return nullptr;
                }
                Q_ASSERT(screenplayItem->parent());

                auto screenplayModel = new BusinessLayer::ScreenplayTextModel;
                const auto parentUuid = screenplayItem->parent()->uuid();

                //
                // Добавляем в модель сценария, модель информации о сценарие
                //
                auto informationModel = qobject_cast<BusinessLayer::ScreenplayInformationModel*>(
                    modelFor(parentUuid));
                Q_ASSERT(informationModel);
                if (informationModel == nullptr) {
                    Log::warning(
                        "Information model isn't initialized for screenplay text with uuid %1",
                        documentToLoad->uuid().toString());
                    screenplayModel->deleteLater();
                    return nullptr;
                }
                screenplayModel->setInformationModel(informationModel);
                //
                // ... модель титульной страницы
                //
                const auto titlePageIndex = 0;
                auto titlePageItem = screenplayItem->parent()->childAt(titlePageIndex);
                Q_ASSERT(titlePageItem);
                Q_ASSERT(titlePageItem->type() == Domain::DocumentObjectType::ScreenplayTitlePage);
                auto titlePageModel = qobject_cast<BusinessLayer::SimpleTextModel*>(
                    modelFor(titlePageItem->uuid()));
                Q_ASSERT(titlePageModel);
                if (titlePageModel == nullptr) {
                    Log::warning(
                        "Title page model isn't initialized for screenplay text with uuid %1",
                        documentToLoad->uuid().toString());
                    screenplayModel->deleteLater();
                    return nullptr;
                }
                screenplayModel->setTitlePageModel(titlePageModel);
                //
                // ... модель синопсиса
                //
                const auto synopsisIndex = 1;
                auto synopsisItem = screenplayItem->parent()->childAt(synopsisIndex);
                Q_ASSERT(synopsisItem);
                Q_ASSERT(synopsisItem->type() == Domain::DocumentObjectType::ScreenplaySynopsis);
                auto synopsisModel
                    = qobject_cast<BusinessLayer::SimpleTextModel*>(modelFor(synopsisItem->uuid()));
                Q_ASSERT(synopsisModel);
                if (synopsisModel == nullptr) {
                    Log::warning(
                        "Synopsis model isn't initialized for screenplay text with uuid %1",
                        documentToLoad->uuid().toString());
                    screenplayModel->deleteLater();
                    return nullptr;
                }
                screenplayModel->setSynopsisModel(synopsisModel);
                //
                // ... модель справочников сценариев
                //
                auto dictionariesModel = qobject_cast<BusinessLayer::ScreenplayDictionariesModel*>(
                    modelFor(Domain::DocumentObjectType::ScreenplayDictionaries));
                Q_ASSERT(dictionariesModel);
                screenplayModel->setDictionariesModel(dictionariesModel);
                //
                // ... модель персонажей
                //
                auto charactersModel = qobject_cast<BusinessLayer::CharactersModel*>(
                    modelFor(Domain::DocumentObjectType::Characters));
                Q_ASSERT(charactersModel);
                screenplayModel->setCharactersModel(charactersModel);
                //
                // ... и модель локаций
                //
                auto locationsModel = qobject_cast<BusinessLayer::LocationsModel*>(
                    modelFor(Domain::DocumentObjectType::Locations));
                Q_ASSERT(locationsModel);
                screenplayModel->setLocationsModel(locationsModel);
                //
                // ... обновим словари рантайма
                //
                screenplayModel->updateRuntimeDictionariesIfNeeded();

                model = screenplayModel;
                break;
            }

            case Domain::DocumentObjectType::ScreenplayDictionaries: {
                model = new BusinessLayer::ScreenplayDictionariesModel;
                break;
            }

            case Domain::DocumentObjectType::ScreenplayStatistics: {
                const auto statisticsItem
                    = d->projectStructureModel->itemForUuid(documentToLoad->uuid());
                if (statisticsItem == nullptr) {
                    return nullptr;
                }

                auto statisticsModel = new BusinessLayer::ScreenplayStatisticsModel;
                const auto screenplayItem = statisticsItem->parent();
                Q_ASSERT(screenplayItem);
                QUuid screenplayTextItemUuid;
                for (int childIndex = 0; childIndex < screenplayItem->childCount(); ++childIndex) {
                    const auto childItem = screenplayItem->childAt(childIndex);
                    if (childItem->type() == Domain::DocumentObjectType::ScreenplayText) {
                        screenplayTextItemUuid = childItem->uuid();
                        break;
                    }
                }
                Q_ASSERT(!screenplayTextItemUuid.isNull());
                auto screenplayModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(
                    modelFor(screenplayTextItemUuid));
                Q_ASSERT(screenplayModel);
                if (screenplayModel == nullptr) {
                    Log::warning(
                        "Text model isn't initialized for screenplay statistics with uuid %1",
                        documentToLoad->uuid().toString());
                    statisticsModel->deleteLater();
                    return nullptr;
                }
                statisticsModel->setScreenplayTextModel(screenplayModel);

                model = statisticsModel;
                break;
            }

            case Domain::DocumentObjectType::Characters: {
                auto charactersModel = new BusinessLayer::CharactersModel;

                const auto characterDocuments
                    = DataStorageLayer::StorageFacade::documentStorage()->documents(
                        Domain::DocumentObjectType::Character);
                for (const auto characterDocument : characterDocuments) {
                    const auto characterItem
                        = d->projectStructureModel->itemForUuid(characterDocument->uuid());
                    if (characterItem == nullptr
                        || characterItem->parent()->uuid() != documentToLoad->uuid()) {
                        continue;
                    }

                    auto characterModel = modelFor(characterDocument);
                    charactersModel->addCharacterModel(
                        qobject_cast<BusinessLayer::CharacterModel*>(characterModel));
                }

                connect(charactersModel, &BusinessLayer::CharactersModel::createCharacterRequested,
                        this, &ProjectModelsFacade::createCharacterRequested);

                model = charactersModel;
                break;
            }

            case Domain::DocumentObjectType::Character: {
                auto characterModel = new BusinessLayer::CharacterModel;

                connect(characterModel, &BusinessLayer::CharacterModel::nameChanged, this,
                        [this, characterModel](const QString& _oldName, const QString& _newName) {
                            emit characterNameChanged(characterModel, _oldName, _newName);
                        });
                connect(characterModel, &BusinessLayer::CharacterModel::renameRequested, this,
                        [this, characterModel](const QString& _name) {
                            emit characterRenameRequested(characterModel, _name);
                        });
                connect(characterModel, &BusinessLayer::CharacterModel::dialoguesUpdateRequested,
                        this, [this, characterModel] {
                            emit characterDialoguesUpdateRequested(characterModel);
                        });

                model = characterModel;
                break;
            }

            case Domain::DocumentObjectType::Locations: {
                auto locationsModel = new BusinessLayer::LocationsModel;

                const auto locationDocuments
                    = DataStorageLayer::StorageFacade::documentStorage()->documents(
                        Domain::DocumentObjectType::Location);
                for (const auto locationDocument : locationDocuments) {
                    const auto locationItem
                        = d->projectStructureModel->itemForUuid(locationDocument->uuid());
                    if (locationItem == nullptr
                        || locationItem->parent()->uuid() != documentToLoad->uuid()) {
                        continue;
                    }

                    auto locationModel = modelFor(locationDocument);
                    locationsModel->addLocationModel(
                        qobject_cast<BusinessLayer::LocationModel*>(locationModel));
                }

                connect(locationsModel, &BusinessLayer::LocationsModel::createLocationRequested,
                        this, &ProjectModelsFacade::createLocationRequested);

                model = locationsModel;
                break;
            }

            case Domain::DocumentObjectType::Location: {
                auto locationModel = new BusinessLayer::LocationModel;

                connect(locationModel, &BusinessLayer::LocationModel::nameChanged, this,
                        [this, locationModel](const QString& _oldName, const QString& _newName) {
                            emit locationNameChanged(locationModel, _oldName, _newName);
                        });
                connect(
                    locationModel, &BusinessLayer::LocationModel::scenesUpdateRequested, this,
                    [this, locationModel] { emit locationScenesUpdateRequested(locationModel); });

                model = locationModel;
                break;
            }

            case Domain::DocumentObjectType::Folder:
            case Domain::DocumentObjectType::SimpleText: {
                model = new BusinessLayer::SimpleTextModel;
                break;
            }

            case Domain::DocumentObjectType::MindMap: {
                model = new BusinessLayer::MindMapModel;
                break;
            }

            case Domain::DocumentObjectType::ImagesGallery: {
                model = new BusinessLayer::ImagesGalleryModel;
                break;
            }

            case Domain::DocumentObjectType::Presentation: {
                model = new BusinessLayer::PresentationModel;
                break;
            }

            default: {
                Q_ASSERT(false);
                return nullptr;
            }
            }
            Q_ASSERT(model);

            model->setImageWrapper(d->imageWrapper);
            model->setRawDataWrapper(d->rawDataWrapper);

            //
            // Если документ не является алиасом, то загрузим из него модели и соединим модель со
            // всеми необходимыми обработчиками событий модели
            //
            if (!isDocumentAlias) {
                model->setDocument(documentToLoad);

                connect(
                    model, &BusinessLayer::AbstractModel::documentNameChanged, this,
                    [this, model](const QString& _name) { emit modelNameChanged(model, _name); });
                connect(
                    model, &BusinessLayer::AbstractModel::documentColorChanged, this,
                    [this, model](const QColor& _color) { emit modelColorChanged(model, _color); });
                connect(model, &BusinessLayer::AbstractModel::contentsChanged, this,
                        [this, model](const QByteArray& _undo, const QByteArray& _redo) {
                            emit modelContentChanged(model, _undo, _redo);
                        });
                connect(
                    model, &BusinessLayer::AbstractModel::undoRequested, this,
                    [this, model](int _undoStep) { emit modelUndoRequested(model, _undoStep); });
                connect(model, &BusinessLayer::AbstractModel::removeRequested, this,
                        [this, model] { emit modelRemoveRequested(model); });
            }

            d->documentsToModels.insert(documentToLoad, model);
        }
    }

    return d->documentsToModels.value(_document);
}

QVector<BusinessLayer::AbstractModel*> ProjectModelsFacade::modelsFor(
    Domain::DocumentObjectType _type)
{
    QVector<BusinessLayer::AbstractModel*> models;
    const auto documents = DataStorageLayer::StorageFacade::documentStorage()->documents(_type);
    for (const auto document : documents) {
        auto model = modelFor(document);
        if (model == nullptr) {
            continue;
        }

        models.append(model);
    }
    return models;
}

void ProjectModelsFacade::removeModelFor(Domain::DocumentObject* _document)
{
    if (!d->documentsToModels.contains(_document)) {
        return;
    }

    auto model = d->documentsToModels.take(_document);
    model->disconnect();
    model->clear();
    model->deleteLater();
}

QVector<BusinessLayer::AbstractModel*> ProjectModelsFacade::loadedModels() const
{
    QVector<BusinessLayer::AbstractModel*> models;
    for (auto model : std::as_const(d->documentsToModels)) {
        models.append(model);
    }
    return models;
}

QVector<BusinessLayer::AbstractModel*> ProjectModelsFacade::loadedModelsFor(
    Domain::DocumentObjectType _type) const
{
    QVector<BusinessLayer::AbstractModel*> models;
    for (auto iter = d->documentsToModels.begin(); iter != d->documentsToModels.end(); ++iter) {
        if (iter.key()->type() == _type) {
            models.append(iter.value());
        }
    }
    return models;
}

QVector<Domain::DocumentObject*> ProjectModelsFacade::loadedDocuments() const
{
    QVector<Domain::DocumentObject*> documents;
    for (auto iter = d->documentsToModels.cbegin(); iter != d->documentsToModels.cend(); ++iter) {
        documents.append(iter.key());
    }
    return documents;
}

} // namespace ManagementLayer
