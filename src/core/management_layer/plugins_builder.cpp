#include "plugins_builder.h"

#include <interfaces/management_layer/i_document_manager.h>
#include <interfaces/ui/i_document_view.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QHash>
#include <QPluginLoader>
#include <QSet>
#include <QStandardPaths>
#include <QWidget>


namespace ManagementLayer {

namespace {

/**
 * @brief Тип представления для активации из плагина
 */
enum ViewType {
    Primary,
    Secondary,
    Window,
};

// clang-format off
/**
 * @brief Майм-типы плагинов
 */

const QString kProjectInformationMime = QStringLiteral("application/x-starc/editor/project/information");
const QString kProjectParametersMime = QStringLiteral("application/x-starc/editor/project/parameters");
const QString kProjectCollaboratorsMime = QStringLiteral("application/x-starc/editor/project/collaborators");

const QString kCharactersRelationsMime = QStringLiteral("application/x-starc/editor/characters/relations");
const QString kCharacterEditorMime = QStringLiteral("application/x-starc/editor/character/information");
const QString kCharacterNavigatorMime = QStringLiteral("application/x-starc/navigator/character/information");
const QString kCharacterDialoguesMime = QStringLiteral("application/x-starc/editor/character/dialogues");
//
const QString kLocationsMapMime = QStringLiteral("application/x-starc/editor/locations/map");
const QString kLocationEditorMime = QStringLiteral("application/x-starc/editor/location/information");
const QString kLocationNavigatorMime = QStringLiteral("application/x-starc/navigator/location/information");
const QString kLocationScenesMime = QStringLiteral("application/x-starc/editor/location/scenes");
//
const QString kSimpleTextFolderEditorMime = QStringLiteral("application/x-starc/editor/folder/text");
const QString kSimpleTextEditorMime = QStringLiteral("application/x-starc/editor/text/text");
const QString kSimpleTextNavigatorMime = QStringLiteral("application/x-starc/navigator/text/text");
//
const QString kScreenplayTitlePageEditorMime = QStringLiteral("application/x-starc/editor/screenplay/title-page");
const QString kScreenplayTreatmentEditorMime = QStringLiteral("application/x-starc/editor/screenplay/treatment/text");
const QString kScreenplayTreatmentNavigatorMime = QStringLiteral("application/x-starc/navigator/screenplay/treatment");
const QString kScreenplayTreatmentCardsMime = QStringLiteral("application/x-starc/editor/screenplay/treatment/cards");
const QString kScreenplayTextEditorMime = QStringLiteral("application/x-starc/editor/screenplay/text/text");
const QString kScreenplayTextNavigatorMime = QStringLiteral("application/x-starc/navigator/screenplay/text");
const QString kScreenplayTextCardsMime = QStringLiteral("application/x-starc/editor/screenplay/text/cards");
const QString kScreenplayTextTimelineMime = QStringLiteral("application/x-starc/editor/screenplay/text/timeline");
const QString kScreenplayTextBreakdownMime = QStringLiteral("application/x-starc/editor/screenplay/text/breakdown");
const QString kScreenplayBreakdownNavigatorMime = QStringLiteral("application/x-starc/navigator/screenplay/breakdown");
const QString kScreenplayStatisticsViewMime = QStringLiteral("application/x-starc/view/screenplay/statistics");
const QString kScreenplayStatisticsNavigatorMime = QStringLiteral("application/x-starc/navigator/screenplay/statistics");
//
const QString kMindMapMime = QStringLiteral("application/x-starc/editor/mind_map/mind_map");
const QString kImagesGalleryMime = QStringLiteral("application/x-starc/editor/images/images-gallery");
const QString kPresentationMime = QStringLiteral("application/x-starc/view/presentation");

/**
 * @brief Карта соотвествия майм-типов редактора к навигатору
 */
const QHash<QString, QString> kEditorToNavigator
    = { { kCharacterEditorMime, kCharacterNavigatorMime },
        { kLocationEditorMime, kLocationNavigatorMime },
        { kSimpleTextEditorMime, kSimpleTextNavigatorMime },
        { kScreenplayTreatmentEditorMime, kScreenplayTreatmentNavigatorMime },
        { kScreenplayTreatmentCardsMime, kScreenplayTreatmentNavigatorMime },
        { kScreenplayTextEditorMime, kScreenplayTextNavigatorMime },
        { kScreenplayTextCardsMime, kScreenplayTextNavigatorMime },
        { kScreenplayTextTimelineMime, kScreenplayTextNavigatorMime },
        { kScreenplayTextBreakdownMime, kScreenplayBreakdownNavigatorMime },
        { kScreenplayStatisticsViewMime, kScreenplayStatisticsNavigatorMime },
      };

/**
 * @brief Карта соответствия майм-типов документа к редакторам
 */
const QHash<QString, QVector<PluginsBuilder::EditorInfo>> kDocumentToEditors
    = { { "application/x-starc/document/project",    { { kProjectInformationMime, u8"\U000f02fd" },
                                                       { kProjectParametersMime, u8"\U000f0493" } ,
                                                       { kProjectCollaboratorsMime, u8"\U000f0b58" } } },
        //
        { "application/x-starc/document/screenplay", { { "application/x-starc/editor/screenplay/information", u8"\U000f02fd" },
                                                       { "application/x-starc/editor/screenplay/parameters", u8"\U000f0493" } } },
        { "application/x-starc/document/screenplay/title-page", { { kScreenplayTitlePageEditorMime, u8"\U000f09ed" } } },
        { "application/x-starc/document/screenplay/synopsis",   { { kSimpleTextEditorMime, u8"\U000f09ed" } } },
        { "application/x-starc/document/screenplay/treatment",  { { kScreenplayTreatmentEditorMime, u8"\U000f09ed" },
                                                                  { kScreenplayTreatmentCardsMime, u8"\U000f0554" },
                                                                  /*{ "application/x-starc/editor/screenplay/beatboard", u8"\U000F13D2" }*/ } },
        { "application/x-starc/document/screenplay/text",       { { kScreenplayTextEditorMime, u8"\U000f09ed" },
                                                                  { kScreenplayTextCardsMime, u8"\U000f0554" },
                                                                  { kScreenplayTextTimelineMime, u8"\U000F066C" },
                                                                  { kScreenplayTextBreakdownMime, u8"\U000F14DD" } } },
        { "application/x-starc/document/screenplay/statistics", { { kScreenplayStatisticsViewMime, u8"\U000f0127" } } },
        //
        { "application/x-starc/document/characters",  { { kCharactersRelationsMime, u8"\U000F0D3D" } } },
        { "application/x-starc/document/character",  { { kCharacterEditorMime, u8"\U000f02fd" },
                                                       { kCharacterDialoguesMime, u8"\U000F017C" } } },
        //
        { "application/x-starc/document/locations",  { { kLocationsMapMime, u8"\U000F0982" } } },
        { "application/x-starc/document/location",   { { kLocationEditorMime, u8"\U000f02fd" },
                                                       { kLocationScenesMime, u8"\U000F0571" } } },
        //
        { "application/x-starc/document/folder",     { { kSimpleTextFolderEditorMime, u8"\U000f09ed" } } },
        { "application/x-starc/document/text",       { { kSimpleTextEditorMime, u8"\U000f09ed" } } },
        { "application/x-starc/document/mind-map",  { { kMindMapMime, u8"\U000F04AA" } } },
        { "application/x-starc/document/images-gallery",  { { kImagesGalleryMime, u8"\U000F02F9" } } },
        { "application/x-starc/document/presentation",  { { kPresentationMime, u8"\U000F0428" } } },
        //,
        { "application/x-starc/document/recycle-bin",       { { "application/x-starc/editor/recycle-bin", u8"\U000f01b4" } } }
      };

/**
 * @brief Карта соответсвий майм-типов навигаторов/редакторов к названиям библиотек с плагинами
 * @note Нужно это для того, чтобы не прогружать каждый раз плагин со всеми зависимостями лишь для
 *       извлечения майм-типа навигатора/редактора, а подгружать точечно только то, что нужно
 */
const QHash<QString, QString> kMimeToPlugin
    = { { kProjectInformationMime, "*projectinformationplugin*" },
        { kProjectParametersMime, "*projectparametersplugin*" },
        { kProjectCollaboratorsMime, "*projectcollaboratorsplugin*" },
        //
        { kSimpleTextFolderEditorMime, "*simpletextplugin*" },
        { kSimpleTextEditorMime, "*simpletextplugin*" },
        { kSimpleTextNavigatorMime, "*simpletextstructureplugin*" },
        //
        { "application/x-starc/editor/screenplay/information", "*screenplayinformationplugin*" },
        { "application/x-starc/editor/screenplay/parameters", "*screenplayparametersplugin*" },
        { kScreenplayTitlePageEditorMime, "*titlepageplugin*" },
        { kScreenplayTreatmentEditorMime, "*screenplaytreatmentplugin*" },
        { kScreenplayTreatmentNavigatorMime, "*screenplaytreatmentstructureplugin*" },
        { kScreenplayTreatmentCardsMime, "*screenplaycardsplugin*" },
        { kScreenplayTextEditorMime, "*screenplaytextplugin*" },
        { kScreenplayTextNavigatorMime, "*screenplaytextstructureplugin*" },
        { kScreenplayTextCardsMime, "*screenplaycardsplugin*" },
        { kScreenplayTextTimelineMime, "*screenplaytimelineplugin*" },
        { kScreenplayTextBreakdownMime, "*screenplaybreakdownplugin*" },
        { kScreenplayBreakdownNavigatorMime, "*screenplaybreakdownstructureplugin*" },
        { kScreenplayStatisticsViewMime, "*screenplaystatisticsplugin*" },
        { kScreenplayStatisticsNavigatorMime, "*screenplaystatisticsstructureplugin*" },
        //
        { kCharactersRelationsMime, "*charactersrelationsplugin*" },
        { kCharacterEditorMime, "*characterinformationplugin*" },
        { kCharacterNavigatorMime, "*characterstructureplugin*" },
        { kCharacterDialoguesMime, "*characterdialoguesplugin*" },
        //
        { kLocationsMapMime, "*locationsmapplugin*" },
        { kLocationEditorMime, "*locationinformationplugin*" },
        { kLocationNavigatorMime, "*locationstructureplugin*" },
        { kLocationScenesMime, "*locationscenesplugin*" },
        //
        //
        { kMindMapMime, "*mindmapplugin*" },
        { kImagesGalleryMime, "*imagesgalleryplugin*" },
        { kPresentationMime, "*presentationplugin*" },
        //
        { "application/x-starc/editor/recycle-bin", "*recyclebinplugin*" },
      };
// clang-format on

} // namespace

class PluginsBuilder::Implementation
{
public:
    /**
     * @brief Path to the library with the plugin of the given type, empty if there's no such one
     */
    QString pluginPath(const QString& _mimeType);

    /**
     * @brief Can the plugin of the given type be loaded
     * @note The library is looked up without loading it, a library that failed to load is
     *       remembered as unavailable
     */
    bool isPluginAvailable(const QString& _mimeType);

    /**
     * @brief Mime-types of the documents that have at least one editor to be opened with
     */
    QSet<QString> documentsWithEditors();

    /**
     * @brief Инициировать плагин заданного типа
     */
    bool initPlugin(const QString& _mimeType);

    /**
     * @brief Активировать плагин заданного типа указанной моделью
     */
    Ui::IDocumentView* activatePlugin(const QString& _mimeType,
                                      BusinessLayer::AbstractModel* _model, ViewType _type);

    /**
     * @brief Загруженные плагины <mime, plugin>
     */
    QHash<QString, ManagementLayer::IDocumentManager*> plugins;

    /**
     * @brief Paths to the libraries of the plugins <mime, path>, empty for the missing ones
     */
    QHash<QString, QString> pluginsPaths;

    /**
     * @brief Plugins which libraries are in place, but can't be loaded
     */
    QSet<QString> brokenPlugins;

    /**
     * @brief Находится ли текущий проект в команде
     */
    bool isProjectInTeam = false;

    /**
     * @brief Права доступа к конкретным документам
     */
    QHash<QUuid, DocumentEditingMode> editingPermissions;

    /**
     * @brief Количество кредитов доступных для использования с ИИ инструментами
     */
};

QString PluginsBuilder::Implementation::pluginPath(const QString& _mimeType)
{
    if (const auto path = pluginsPaths.constFind(_mimeType); path != pluginsPaths.constEnd()) {
        return path.value();
    }

    //
    // Смотрим папку с данными приложения на компе
    // NOTE: В Debug-режим работает с папкой сборки приложения
    //
    // TODO: Когда дорастём включить этот функционал, плюс продумать, как быть в режиме
    // разработки
    //
    const QString pluginsDirName = "plugins";
    QDir pluginsDir(
        // #ifndef QT_NO_DEBUG
        QApplication::applicationDirPath()
        // #else
        //                     QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        // #endif
    );

#if defined(Q_OS_MAC)
    pluginsDir.cdUp();
#endif

    //
    // Ищем библиотеку плагина, если папка с плагинами есть
    //
    const QString extensionFilter =
#ifdef Q_OS_WIN
        ".dll";
#elif defined(Q_OS_LINUX)
        ".so";
#elif defined(Q_OS_MAC)
        ".dylib";
#else
        "";
#endif
    QString path;
    const auto pluginNameFilter = kMimeToPlugin.value(_mimeType);
    if (!pluginNameFilter.isEmpty() && pluginsDir.cd(pluginsDirName)) {
        const QStringList libCorePluginEntries
            = pluginsDir.entryList({ pluginNameFilter + extensionFilter }, QDir::Files);
        if (libCorePluginEntries.size() > 1) {
            qCritical() << "Found more than 1 plugins for mime-type:" << _mimeType;
        } else if (!libCorePluginEntries.isEmpty()) {
            path = pluginsDir.absoluteFilePath(libCorePluginEntries.constFirst());
        }
    }

    pluginsPaths.insert(_mimeType, path);
    return path;
}

bool PluginsBuilder::Implementation::isPluginAvailable(const QString& _mimeType)
{
    if (const auto plugin = plugins.constFind(_mimeType); plugin != plugins.constEnd()) {
        return plugin.value() != nullptr;
    }

    return !brokenPlugins.contains(_mimeType) && !pluginPath(_mimeType).isEmpty();
}

QSet<QString> PluginsBuilder::Implementation::documentsWithEditors()
{
    QSet<QString> documents;
    for (auto editors = kDocumentToEditors.constBegin(); editors != kDocumentToEditors.constEnd();
         ++editors) {
        for (const auto& editor : editors.value()) {
            if (isPluginAvailable(editor.mimeType)) {
                documents.insert(editors.key());
                break;
            }
        }
    }
    return documents;
}

bool PluginsBuilder::Implementation::initPlugin(const QString& _mimeType)
{
    if (plugins.contains(_mimeType)) {
        return true;
    }

    //
    // Если плагина нет, идём лесом
    //
    const auto pluginPath = this->pluginPath(_mimeType);
    if (pluginPath.isEmpty()) {
        qCritical() << "Plugin isn't found for mime-type:" << _mimeType;
        return false;
    }

    //
    // Подгружаем плагин
    //
    QPluginLoader pluginLoader(pluginPath);
    QObject* pluginObject = pluginLoader.instance();
    if (pluginObject == nullptr) {
        qCritical() << pluginLoader.errorString();
        brokenPlugins.insert(_mimeType);
        return false;
    }

    auto plugin = qobject_cast<ManagementLayer::IDocumentManager*>(pluginObject);
    plugins.insert(_mimeType, plugin);

    return true;
}

Ui::IDocumentView* PluginsBuilder::Implementation::activatePlugin(
    const QString& _mimeType, BusinessLayer::AbstractModel* _model, ViewType _type)
{
    //
    // Инициилизируем плагин
    //
    const auto isPluginInitialized = initPlugin(_mimeType);
    if (!isPluginInitialized) {
        return nullptr;
    }

    //
    // Получим плагин для нужного типа редактора
    //
    auto plugin = plugins.value(_mimeType);
    if (plugin == nullptr) {
        return nullptr;
    }

    //
    // Получим представление (при необходимости новое будет создано)
    //
    Ui::IDocumentView* view = nullptr;
    switch (_type) {
    case Primary: {
        view = plugin->view(_model);
        break;
    }

    case Secondary: {
        view = plugin->secondaryView(_model);
        break;
    }

    default: {
        view = plugin->createView(_model);
        break;
    }
    }

    //
    // Настроим доступность для редактирования в плагине
    //
    plugin->checkAvailabilityToEdit(isProjectInTeam);
    plugin->setEditingPermissions(editingPermissions);
    //
    // ... the documents that can be opened, for the controls that belong to them
    //
    plugin->setDocumentsWithEditors(documentsWithEditors());
    //
    // ... а также доступные кредиты для работы с ИИ
    //

    return view;
}


// ****


PluginsBuilder::PluginsBuilder()
    : d(new Implementation)
{
}

PluginsBuilder::~PluginsBuilder() = default;

bool PluginsBuilder::initPlugin(const QString& _mimeType) const
{
    return d->initPlugin(_mimeType);
}

IDocumentManager* PluginsBuilder::plugin(const QString& _mimeType) const
{
    auto plugin = d->plugins.find(_mimeType);
    if (plugin == d->plugins.end()) {
        return nullptr;
    }

    return plugin.value();
}

QVector<PluginsBuilder::EditorInfo> PluginsBuilder::editorsInfoFor(const QString& _documentMimeType,
                                                                   bool _isProjectRemote) const
{
    auto editors = kDocumentToEditors.value(_documentMimeType);
    if (!_isProjectRemote) {
        editors.erase(std::remove_if(editors.begin(), editors.end(),
                                     [](const auto& _editorInfo) {
                                         return _editorInfo.mimeType == kProjectCollaboratorsMime;
                                     }),
                      editors.end());
    }
    //
    // An editor is offered only when its plugin is a part of this build
    //
    editors.erase(std::remove_if(editors.begin(), editors.end(),
                                 [this](const auto& _editorInfo) {
                                     return !d->isPluginAvailable(_editorInfo.mimeType);
                                 }),
                  editors.end());
    return editors;
}

QString PluginsBuilder::editorDescription(const QString& _documentMimeType,
                                          const QString& _editorMimeType) const
{
    // clang-format off
    const QHash<QString, QHash<QString, QString>> descriptions
        = { { "application/x-starc/document/project",
              { { kProjectInformationMime,
                  QApplication::translate("ProjectPluginsBuilder", "Information about project") },
                { kProjectParametersMime,
                  QApplication::translate("ProjectPluginsBuilder", "Project parameters") },
                { kProjectCollaboratorsMime,
                  QApplication::translate("ProjectPluginsBuilder", "Project collaborators") } } },
            //
            { "application/x-starc/document/screenplay",
              { { "application/x-starc/editor/screenplay/information",
                  QApplication::translate("ProjectPluginsBuilder", "Information about screenplay") },
                { "application/x-starc/editor/screenplay/parameters",
                  QApplication::translate("ProjectPluginsBuilder", "Screenplay parameters") } } },
            { "application/x-starc/document/screenplay/title-page",
              { { kScreenplayTitlePageEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Title page text") } } },
            { "application/x-starc/document/screenplay/synopsis",
              { { kSimpleTextEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Synopsis text") } } },
            { "application/x-starc/document/screenplay/treatment",
              { { kScreenplayTreatmentEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Treatment text") },
                { "application/x-starc/editor/screenplay/treatment/beatboard",
                  QApplication::translate("ProjectPluginsBuilder", "Beats") },
                { kScreenplayTreatmentCardsMime,
                  QApplication::translate("ProjectPluginsBuilder", "Cards") } } },
            { "application/x-starc/document/screenplay/text",
              { { kScreenplayTextEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Screenplay text") },
                { kScreenplayTextCardsMime,
                  QApplication::translate("ProjectPluginsBuilder", "Cards") },
                { kScreenplayTextTimelineMime,
                  QApplication::translate("ProjectPluginsBuilder", "Timeline") },
                { kScreenplayTextBreakdownMime,
                  QApplication::translate("ProjectPluginsBuilder", "Breakdown") } } },
            { "application/x-starc/document/screenplay/statistics",
              { { kScreenplayStatisticsViewMime,
                  QApplication::translate("ProjectPluginsBuilder", "Statistics") } } },
            //
            { "application/x-starc/document/characters",
              { { kCharactersRelationsMime,
                  QApplication::translate("ProjectPluginsBuilder", "Characters relations") } } },
            { "application/x-starc/document/character",
              { { kCharacterEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Character information") },
                { kCharacterDialoguesMime,
                  QApplication::translate("ProjectPluginsBuilder", "Character dialogues") } } },
            //
            { "application/x-starc/document/locations",
              { { kLocationsMapMime,
                  QApplication::translate("ProjectPluginsBuilder", "Locations map") } } },
            { "application/x-starc/document/location",
              { { kLocationEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Location information") },
                { kLocationScenesMime,
                  QApplication::translate("ProjectPluginsBuilder", "Location scenes") } } },
            //
            { "application/x-starc/document/folder",
              { { kSimpleTextFolderEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Folder text") } } },
            { "application/x-starc/document/text",
              { { kSimpleTextEditorMime,
                  QApplication::translate("ProjectPluginsBuilder", "Document text") } } },
            //,
            { "application/x-starc/document/recycle-bin",
              { { "application/x-starc/editor/recycle-bin",
                  QApplication::translate("ProjectPluginsBuilder", "Recycle bin summary info") } } },
            { "application/x-starc/document/mind-map",
              { { kMindMapMime,
                  QApplication::translate("ProjectPluginsBuilder", "Mind map") } } },
            { "application/x-starc/document/images-gallery",
              { { kImagesGalleryMime,
                  QApplication::translate("ProjectPluginsBuilder", "Images gallery") } } },
            { "application/x-starc/document/presentation",
              { { kPresentationMime,
                  QApplication::translate("ProjectPluginsBuilder", "Presentation") } } },
             };
    // clang-format on
    return descriptions.value(_documentMimeType).value(_editorMimeType);
}

QString PluginsBuilder::navigatorMimeTypeFor(const QString& _editorMimeType) const
{
    return kEditorToNavigator.value(_editorMimeType);
}

Ui::IDocumentView* PluginsBuilder::activateView(const QString& _viewMimeType,
                                                BusinessLayer::AbstractModel* _model) const
{
    return d->activatePlugin(_viewMimeType, _model, ViewType::Primary);
}

Ui::IDocumentView* PluginsBuilder::activateSecondView(const QString& _viewMimeType,
                                                      BusinessLayer::AbstractModel* _model) const
{
    return d->activatePlugin(_viewMimeType, _model, ViewType::Secondary);
}

Ui::IDocumentView* PluginsBuilder::activateWindowView(const QString& _viewMimeType,
                                                      BusinessLayer::AbstractModel* _model) const
{
    return d->activatePlugin(_viewMimeType, _model, ViewType::Window);
}

void PluginsBuilder::bind(const QString& _viewMimeType, const QString& _navigatorMimeType) const
{
    auto viewPlugin = d->plugins.value(_viewMimeType);
    Q_ASSERT(viewPlugin);

    auto navigatorPlugin = d->plugins.value(_navigatorMimeType);
    Q_ASSERT(navigatorPlugin);

    viewPlugin->bind(navigatorPlugin);
    navigatorPlugin->bind(viewPlugin);
}

void PluginsBuilder::syncModelAndBindEditors(const QString& _viewMimeType,
                                             BusinessLayer::AbstractModel* _model,
                                             bool _isPrimaryView) const
{
    if (_viewMimeType.isEmpty()) {
        return;
    }

    //
    // Определеим группу редакторо, чтобы их связать
    //
    auto viewGroupMimeType = _viewMimeType;
    const auto groupsCount = viewGroupMimeType.count('/') + 1;
    //
    // ... если групп меньше пяти, то не связываем, чтобы не привязать несовместимые плагины
    //
    if (groupsCount < 5) {
        return;
    }
    //
    // ... если групп всего пять, то для связи используем именно этот майм тип
    //
    else if (groupsCount == 5) {
    }
    //
    // ... если групп шесть, то связываем на предыдущем уровне майма
    //
    else if (groupsCount == 6) {
        viewGroupMimeType = viewGroupMimeType.left(_viewMimeType.lastIndexOf("/"));
    }
    //
    // ... такого, чтобы групп было больше шести пока нет
    //
    else {
        Q_ASSERT(false);
    }
    //
    // Соберём все редакторы заданной группы
    //
    QHash<QString, IDocumentManager*> pluginsFiltered;
    for (auto iter = d->plugins.begin(); iter != d->plugins.end(); ++iter) {
        if (iter.key().startsWith(viewGroupMimeType)) {
            pluginsFiltered.insert(iter.key(), iter.value());
        }
    }
    //
    // Установим им единую модель
    //
    for (auto i = pluginsFiltered.begin(); i != pluginsFiltered.end(); ++i) {
        if (_isPrimaryView) {
            if (i.value()->view() != nullptr) {
                i.value()->view(_model);
            }
        } else {
            if (i.value()->secondaryView() != nullptr) {
                i.value()->secondaryView(_model);
            }
        }
    }
    //
    // И свяжем их
    //
    for (auto i = pluginsFiltered.begin(); i != pluginsFiltered.end(); ++i) {
        for (auto j = pluginsFiltered.begin(); j != pluginsFiltered.end(); ++j) {
            if (i.key() == j.key()) {
                continue;
            }

            i.value()->bind(j.value());
        }
    }
}

void PluginsBuilder::toggleViewFullScreen(bool _isFullScreen, const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->view();
    if (view != nullptr) {
        view->toggleFullScreen(_isFullScreen);
    }
}

void PluginsBuilder::toggleSecondaryViewFullScreen(bool _isFullScreen,
                                                   const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->secondaryView();
    if (view != nullptr) {
        view->toggleFullScreen(_isFullScreen);
    }
}

void PluginsBuilder::setViewCursors(const QVector<Domain::CursorInfo>& _cursors,
                                    const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->view();
    if (view != nullptr) {
        view->setCursors(_cursors);
    }
}

void PluginsBuilder::setSecondaryViewCursors(const QVector<Domain::CursorInfo>& _cursors,
                                             const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->secondaryView();
    if (view != nullptr) {
        view->setCursors(_cursors);
    }
}

void PluginsBuilder::setViewCurrentCursor(const Domain::CursorInfo& _cursor,
                                          const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->view();
    if (view != nullptr) {
        view->setCurrentCursor(_cursor);
    }
}

void PluginsBuilder::setSecondaryViewCurrentCursor(const Domain::CursorInfo& _cursor,
                                                   const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->secondaryView();
    if (view != nullptr) {
        view->setCurrentCursor(_cursor);
    }
}

void PluginsBuilder::setViewCurrentIndex(const QModelIndex& _index,
                                         const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->view();
    if (view != nullptr) {
        view->setCurrentModelIndex(_index);
    }
}

void PluginsBuilder::setSecondaryViewCurrentIndex(const QModelIndex& _index,
                                                  const QString& _viewMimeType) const
{
    if (!d->plugins.contains(_viewMimeType)) {
        return;
    }

    auto view = d->plugins.value(_viewMimeType)->secondaryView();
    if (view != nullptr) {
        view->setCurrentModelIndex(_index);
    }
}

void PluginsBuilder::reconfigureAll() const
{
    for (auto plugin : std::as_const(d->plugins)) {
        plugin->reconfigure({});
    }
}

void PluginsBuilder::reconfigurePlugin(const QString& _mimeType,
                                       const QStringList& _changedSettingsKeys) const
{
    auto plugin = this->plugin(_mimeType);
    if (!plugin) {
        return;
    }

    plugin->reconfigure(_changedSettingsKeys);
}

void PluginsBuilder::reconfigureSimpleTextEditor(const QStringList& _changedSettingsKeys) const
{
    reconfigurePlugin(kSimpleTextEditorMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTitlePageEditorMime, _changedSettingsKeys);
}

void PluginsBuilder::reconfigureSimpleTextNavigator() const
{
    reconfigurePlugin(kSimpleTextNavigatorMime, {});
}

void PluginsBuilder::reconfigureScreenplayEditor(const QStringList& _changedSettingsKeys) const
{
    reconfigurePlugin(kScreenplayTitlePageEditorMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTreatmentEditorMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTreatmentCardsMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTextEditorMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTextCardsMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTextTimelineMime, _changedSettingsKeys);
    reconfigurePlugin(kScreenplayTextBreakdownMime, _changedSettingsKeys);
}

void PluginsBuilder::reconfigureScreenplayNavigator() const
{
    reconfigurePlugin(kScreenplayTreatmentNavigatorMime, {});
    reconfigurePlugin(kScreenplayTextNavigatorMime, {});
    reconfigurePlugin(kScreenplayBreakdownNavigatorMime, {});
}

void PluginsBuilder::setProjectInfo(
    bool _isRemote, bool _isOwner, bool _allowGrantAccessToProject, bool _canBeSentForChecking,
    const QVector<BusinessLayer::ComplianceRule>& _complianceRules) const
{
    for (auto plugin : std::as_const(d->plugins)) {
        plugin->setProjectInfo(_isRemote, _isOwner, _allowGrantAccessToProject,
                               _canBeSentForChecking, _complianceRules);
    }
}

void PluginsBuilder::checkAvailabilityToEdit(bool _projectInTeam) const
{
    d->isProjectInTeam = _projectInTeam;
    for (auto plugin : std::as_const(d->plugins)) {
        plugin->checkAvailabilityToEdit(d->isProjectInTeam);
    }
}

void PluginsBuilder::setEditingPermissions(
    const QHash<QUuid, DocumentEditingMode>& _permissions) const
{
    if (d->editingPermissions == _permissions) {
        return;
    }

    d->editingPermissions = _permissions;
    for (auto plugin : std::as_const(d->plugins)) {
        plugin->setEditingPermissions(d->editingPermissions);
    }
}

void PluginsBuilder::resetModels() const
{
    for (auto plugin : std::as_const(d->plugins)) {
        plugin->saveSettings();
        plugin->resetModels();
    }
}

Ui::IDocumentView* PluginsBuilder::projectCollaboratorsView(
    BusinessLayer::AbstractModel* _model) const
{
    return d->activatePlugin(kProjectCollaboratorsMime, _model, Primary);
}

} // namespace ManagementLayer
