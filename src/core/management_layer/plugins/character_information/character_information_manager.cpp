#include "character_information_manager.h"

#include "character_information_view.h"

#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/characters/characters_model.h>
#include <utils/helpers/text_helper.h>

#include <QPointer>


namespace ManagementLayer {

class CharacterInformationManager::Implementation
{
public:
    /**
     * @brief Создать представление
     */
    Ui::CharacterInformationView* createView(BusinessLayer::AbstractModel* _model);

    /**
     * @brief Связать заданную модель и представление
     */
    void setModelForView(BusinessLayer::AbstractModel* _model, Ui::CharacterInformationView* _view);

    /**
     * @brief Rename the character to the name entered in the view, if the name can be used
     */
    void rename(BusinessLayer::CharacterModel* _character, Ui::CharacterInformationView* _view,
                const QString& _name) const;


    /**
     * @brief The characters of the project
     */
    QPointer<BusinessLayer::CharactersModel> characters;

    /**
     * @brief Предаставление для основного окна
     */
    Ui::CharacterInformationView* view = nullptr;
    Ui::CharacterInformationView* secondaryView = nullptr;

    /**
     * @brief Все созданные представления с моделями, которые в них отображаются
     */
    struct ViewAndModel {
        QPointer<Ui::CharacterInformationView> view;
        QPointer<BusinessLayer::CharacterModel> model;
    };
    QVector<ViewAndModel> allViews;
};

Ui::CharacterInformationView* CharacterInformationManager::Implementation::createView(
    BusinessLayer::AbstractModel* _model)
{
    auto view = new Ui::CharacterInformationView;
    setModelForView(_model, view);
    return view;
}

void CharacterInformationManager::Implementation::setModelForView(
    BusinessLayer::AbstractModel* _model, Ui::CharacterInformationView* _view)
{
    constexpr int invalidIndex = -1;
    int viewIndex = invalidIndex;
    for (int index = 0; index < allViews.size(); ++index) {
        if (allViews[index].view == _view) {
            if (allViews[index].model == _model) {
                return;
            }

            viewIndex = index;
            break;
        }
    }

    //
    // Разрываем соединения со старой моделью
    //
    if (viewIndex != invalidIndex && allViews[viewIndex].model != nullptr) {
        _view->disconnect(allViews[viewIndex].model);
        allViews[viewIndex].model->disconnect(_view);
    }

    //
    // Определяем новую модель
    //
    auto model = qobject_cast<BusinessLayer::CharacterModel*>(_model);

    //
    // Обновляем связь представления с моделью
    //
    if (viewIndex != invalidIndex) {
        allViews[viewIndex].model = model;
    }
    //
    // Или сохраняем связь представления с моделью
    //
    else {
        allViews.append({ _view, model });
    }

    //
    // Настраиваем соединения с новой моделью
    //
    if (model != nullptr) {
        auto reloadModel = [_view, model] {
            _view->setName(model->name());
            _view->setDescription(model->oneSentenceDescription());
            _view->setNotes(model->longDescription());
        };
        reloadModel();

        connect(model, &BusinessLayer::CharacterModel::modelReset, _view, reloadModel);
        connect(model, &BusinessLayer::CharacterModel::nameChanged, _view,
                &Ui::CharacterInformationView::setName);
        connect(model, &BusinessLayer::CharacterModel::renameRejected, _view,
                &Ui::CharacterInformationView::setNameError);
        connect(model, &BusinessLayer::CharacterModel::oneSentenceDescriptionChanged, _view,
                &Ui::CharacterInformationView::setDescription);
        connect(model, &BusinessLayer::CharacterModel::longDescriptionChanged, _view,
                &Ui::CharacterInformationView::setNotes);
        //
        connect(_view, &Ui::CharacterInformationView::nameEdited, model,
                [this, _view, model](const QString& _name) { rename(model, _view, _name); });
        connect(_view, &Ui::CharacterInformationView::descriptionChanged, model,
                &BusinessLayer::CharacterModel::setOneSentenceDescription);
        connect(_view, &Ui::CharacterInformationView::notesChanged, model,
                &BusinessLayer::CharacterModel::setLongDescription);
    }
}

void CharacterInformationManager::Implementation::rename(BusinessLayer::CharacterModel* _character,
                                                         Ui::CharacterInformationView* _view,
                                                         const QString& _name) const
{
    //
    // Characters are named in capitals, like the ones collected from the script
    //
    const auto name = TextHelper::smartToUpper(_name.simplified());
    if (name == _character->name()) {
        _view->setName(name);
        return;
    }

    if (name.isEmpty()) {
        _view->setNameError(CharacterInformationManager::tr("The character should have a name"));
        return;
    }

    //
    // A character renamed to the name of another one is removed from the project as its
    // duplicate, together with the description and notes it has, so such a name isn't accepted.
    // A name that can't be checked isn't applied either
    //
    if (characters.isNull()) {
        _view->setName(_character->name());
        return;
    }
    for (int row = 0; row < characters->rowCount(); ++row) {
        const auto character = characters->character(row);
        if (character != _character && TextHelper::smartToUpper(character->name()) == name) {
            _view->setNameError(
                CharacterInformationManager::tr("Character with this name already exists"));
            return;
        }
    }

    //
    // The project applies the name: the screenplays name the character too, and it asks first
    // when their text mentions the character
    //
    _character->requestRename(name);
}


// ****


CharacterInformationManager::CharacterInformationManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation)
{
}

CharacterInformationManager::~CharacterInformationManager() = default;

QObject* CharacterInformationManager::asQObject()
{
    return this;
}

void CharacterInformationManager::configureModelProcessing(BusinessLayer::AbstractModel* _model)
{
    if (auto characters = qobject_cast<BusinessLayer::CharactersModel*>(_model)) {
        d->characters = characters;
    }
}

Ui::IDocumentView* CharacterInformationManager::view()
{
    return d->view;
}

Ui::IDocumentView* CharacterInformationManager::view(BusinessLayer::AbstractModel* _model)
{
    if (d->view == nullptr) {
        d->view = d->createView(_model);
    } else {
        d->setModelForView(_model, d->view);
    }

    return d->view;
}

Ui::IDocumentView* CharacterInformationManager::secondaryView()
{
    return d->secondaryView;
}

Ui::IDocumentView* CharacterInformationManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (d->secondaryView == nullptr) {
        d->secondaryView = d->createView(_model);
    } else {
        d->setModelForView(_model, d->secondaryView);
    }

    return d->secondaryView;
}

Ui::IDocumentView* CharacterInformationManager::createView(BusinessLayer::AbstractModel* _model)
{
    return d->createView(_model);
}

void CharacterInformationManager::resetModels()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        d->setModelForView(nullptr, viewAndModel.view);
    }
    d->characters.clear();
}

void CharacterInformationManager::setEditingMode(DocumentEditingMode _mode)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setEditingMode(_mode);
    }
}

} // namespace ManagementLayer
