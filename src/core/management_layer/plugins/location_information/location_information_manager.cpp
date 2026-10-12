#include "location_information_manager.h"

#include "location_information_view.h"

#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/locations/locations_model.h>
#include <utils/helpers/text_helper.h>

#include <QPointer>


namespace ManagementLayer {

class LocationInformationManager::Implementation
{
public:
    /**
     * @brief Создать представление
     */
    Ui::LocationInformationView* createView(BusinessLayer::AbstractModel* _model);

    /**
     * @brief Связать заданную модель и представление
     */
    void setModelForView(BusinessLayer::AbstractModel* _model, Ui::LocationInformationView* _view);

    /**
     * @brief Rename the location to the name entered in the view, if the name can be used
     */
    void rename(BusinessLayer::LocationModel* _location, Ui::LocationInformationView* _view,
                const QString& _name) const;


    /**
     * @brief The locations of the project
     */
    QPointer<BusinessLayer::LocationsModel> locations;

    /**
     * @brief Предаставление для основного окна
     */
    Ui::LocationInformationView* view = nullptr;
    Ui::LocationInformationView* secondaryView = nullptr;

    /**
     * @brief Все созданные представления с моделями, которые в них отображаются
     */
    struct ViewAndModel {
        QPointer<Ui::LocationInformationView> view;
        QPointer<BusinessLayer::LocationModel> model;
    };
    QVector<ViewAndModel> allViews;
};

Ui::LocationInformationView* LocationInformationManager::Implementation::createView(
    BusinessLayer::AbstractModel* _model)
{
    auto view = new Ui::LocationInformationView;
    setModelForView(_model, view);
    return view;
}

void LocationInformationManager::Implementation::setModelForView(
    BusinessLayer::AbstractModel* _model, Ui::LocationInformationView* _view)
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
    auto model = qobject_cast<BusinessLayer::LocationModel*>(_model);

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

        connect(model, &BusinessLayer::LocationModel::modelReset, _view, reloadModel);
        connect(model, &BusinessLayer::LocationModel::nameChanged, _view,
                &Ui::LocationInformationView::setName);
        connect(model, &BusinessLayer::LocationModel::oneSentenceDescriptionChanged, _view,
                &Ui::LocationInformationView::setDescription);
        connect(model, &BusinessLayer::LocationModel::longDescriptionChanged, _view,
                &Ui::LocationInformationView::setNotes);
        //
        connect(_view, &Ui::LocationInformationView::nameEdited, model,
                [this, _view, model](const QString& _name) { rename(model, _view, _name); });
        connect(_view, &Ui::LocationInformationView::descriptionChanged, model,
                &BusinessLayer::LocationModel::setOneSentenceDescription);
        connect(_view, &Ui::LocationInformationView::notesChanged, model,
                &BusinessLayer::LocationModel::setLongDescription);
    }
}

void LocationInformationManager::Implementation::rename(BusinessLayer::LocationModel* _location,
                                                        Ui::LocationInformationView* _view,
                                                        const QString& _name) const
{
    //
    // Locations are named in capitals, like the ones collected from the script
    //
    const auto name = TextHelper::smartToUpper(_name.simplified());
    if (name == _location->name()) {
        _view->setName(name);
        return;
    }

    if (name.isEmpty()) {
        _view->setNameError(LocationInformationManager::tr("The location should have a name"));
        return;
    }

    //
    // A location renamed to the name of another one is removed from the project as its
    // duplicate, together with the description and notes it has, so such a name isn't accepted.
    // A name that can't be checked isn't applied either
    //
    if (locations.isNull()) {
        _view->setName(_location->name());
        return;
    }
    for (int row = 0; row < locations->rowCount(); ++row) {
        const auto location = locations->location(row);
        if (location != _location && TextHelper::smartToUpper(location->name()) == name) {
            _view->setNameError(
                LocationInformationManager::tr("Location with this name already exists"));
            return;
        }
    }

    _location->setName(name);
}


// ****


LocationInformationManager::LocationInformationManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation)
{
}

LocationInformationManager::~LocationInformationManager() = default;

QObject* LocationInformationManager::asQObject()
{
    return this;
}

void LocationInformationManager::configureModelProcessing(BusinessLayer::AbstractModel* _model)
{
    if (auto locations = qobject_cast<BusinessLayer::LocationsModel*>(_model)) {
        d->locations = locations;
    }
}

Ui::IDocumentView* LocationInformationManager::view()
{
    return d->view;
}

Ui::IDocumentView* LocationInformationManager::view(BusinessLayer::AbstractModel* _model)
{
    if (d->view == nullptr) {
        d->view = d->createView(_model);
    } else {
        d->setModelForView(_model, d->view);
    }

    return d->view;
}

Ui::IDocumentView* LocationInformationManager::secondaryView()
{
    return d->secondaryView;
}

Ui::IDocumentView* LocationInformationManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (d->secondaryView == nullptr) {
        d->secondaryView = d->createView(_model);
    } else {
        d->setModelForView(_model, d->secondaryView);
    }

    return d->secondaryView;
}

Ui::IDocumentView* LocationInformationManager::createView(BusinessLayer::AbstractModel* _model)
{
    return d->createView(_model);
}

void LocationInformationManager::resetModels()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        d->setModelForView(nullptr, viewAndModel.view);
    }
    d->locations.clear();
}

void LocationInformationManager::setEditingMode(DocumentEditingMode _mode)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setEditingMode(_mode);
    }
}

} // namespace ManagementLayer
