#include "export_manager.h"

#include <business_layer/export/characters/character_docx_exporter.h>
#include <business_layer/export/characters/character_export_options.h>
#include <business_layer/export/characters/character_pdf_exporter.h>
#include <business_layer/export/characters/characters_docx_exporter.h>
#include <business_layer/export/characters/characters_export_options.h>
#include <business_layer/export/characters/characters_pdf_exporter.h>
#include <business_layer/export/export_options.h>
#include <business_layer/export/locations/location_docx_exporter.h>
#include <business_layer/export/locations/location_export_options.h>
#include <business_layer/export/locations/location_pdf_exporter.h>
#include <business_layer/export/locations/locations_docx_exporter.h>
#include <business_layer/export/locations/locations_pdf_exporter.h>
#include <business_layer/export/screenplay/screenplay_docx_exporter.h>
#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_fdx_exporter.h>
#include <business_layer/export/screenplay/screenplay_fountain_exporter.h>
#include <business_layer/export/screenplay/screenplay_pdf_exporter.h>
#include <business_layer/export/simple_text/simple_text_docx_exporter.h>
#include <business_layer/export/simple_text/simple_text_markdown_exporter.h>
#include <business_layer/export/simple_text/simple_text_pdf_exporter.h>
#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/characters/characters_model.h>
#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/locations/locations_model.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/simple_text/simple_text_model.h>
#include <business_layer/templates/simple_text_template.h>
#include <business_layer/templates/templates_facade.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <ui/export/character_export_dialog.h>
#include <ui/export/characters_export_dialog.h>
#include <ui/export/location_export_dialog.h>
#include <ui/export/locations_export_dialog.h>
#include <ui/export/screenplay_export_dialog.h>
#include <ui/export/simple_text_export_dialog.h>
#include <ui/widgets/dialog/standard_dialog.h>
#include <utils/helpers/dialog_helper.h>
#include <utils/helpers/extension_helper.h>
#include <utils/helpers/platform_helper.h>
#include <utils/logging.h>

#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>


namespace ManagementLayer {

namespace {

/**
 * @brief Ключ для сохранения пути экспортируемого документа
 */
QString exportModelKey(BusinessLayer::AbstractModel* _model)
{
    return QString("%1/%2").arg(DataStorageLayer::kProjectExportFolderKey,
                                _model->document()->uuid().toString());
}

QString buildExportFilePath(BusinessLayer::ExportFileFormat _fileFormat, const QString& _modelName,
                            const QString& _draftName, BusinessLayer::AbstractModel* _model,
                            QWidget* _topLevelWidget)
{
    using namespace BusinessLayer;

    //
    // Определим расширение и фильтр для диалога выбора файлов
    //
    QString exportFilter;
    QString exportExtension;
    switch (_fileFormat) {
    default:
    case ExportFileFormat::Pdf: {
        exportFilter = DialogHelper::pdfFilter();
        exportExtension = ExtensionHelper::pdf();
        break;
    }
    case ExportFileFormat::Docx: {
        exportFilter = DialogHelper::msWordFilter();
        exportExtension = ExtensionHelper::msOfficeOpenXml();
        break;
    }
    case ExportFileFormat::Fdx: {
        exportFilter = DialogHelper::finalDraftFilter();
        exportExtension = ExtensionHelper::finalDraft();
        break;
    }
    case ExportFileFormat::Fountain: {
        exportFilter = DialogHelper::fountainFilter();
        exportExtension = ExtensionHelper::fountain();
        break;
    }
    case ExportFileFormat::Markdown: {
        exportFilter = DialogHelper::markdownFilter();
        exportExtension = ExtensionHelper::markdown();
        break;
    }
    }

    //
    // Определим путь до экспортируемого файла
    //
    QString modelExportFile;
    if (const auto modelExportFileValue = settingsValue(exportModelKey(_model));
        !modelExportFileValue.isNull()) {
        modelExportFile = modelExportFileValue.toString();
    } else {
        const auto projectExportFolder
            = settingsValue(DataStorageLayer::kProjectExportFolderKey).toString();
        modelExportFile = QString("%1/%2.%3").arg(projectExportFolder, _modelName, exportExtension);
    }
    if (!modelExportFile.endsWith(exportExtension)) {
        const auto dotIndex = modelExportFile.lastIndexOf('.');
        if (dotIndex == -1) {
            modelExportFile += '.';
        } else {
            modelExportFile = modelExportFile.mid(0, dotIndex + 1);
        }
        modelExportFile += exportExtension;
    }

    //
    // Добавим суффикс к экспортируемому файлу
    //
    QString modelExportFileSuffix;
    if (!_draftName.isEmpty()) {
        modelExportFileSuffix += QString("_%1").arg(_draftName);
    }
    modelExportFileSuffix
        += QString("_%1").arg(QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss"));
    modelExportFile.insert(modelExportFile.length() - exportExtension.length() - 1,
                           modelExportFileSuffix);

    //
    // Определим выбор пользователя
    //
    const auto exportFilePath = QFileDialog::getSaveFileName(
        _topLevelWidget, QObject::tr("Choose the file to export", "ManagementLayer::ExportManager"),
        PlatformHelper::systemSavebleFileName(modelExportFile), exportFilter);
    if (exportFilePath.isEmpty()) {
        return {};
    }

    //
    // Сохраним файл, в который экспортировали данную модель, но при этом стираем суффикс, чтобы при
    // следующем экспорте он не добавлялся к имени файла
    //
    auto exportFilePathWithoutSuffix = exportFilePath;
    exportFilePathWithoutSuffix.remove(modelExportFileSuffix);
    setSettingsValue(exportModelKey(_model), exportFilePathWithoutSuffix);

    return exportFilePath;
}

void showExportError(const QString& _path, QWidget* _topLevelWidget)
{
    StandardDialog::information(
        _topLevelWidget, QObject::tr("Export error", "ManagementLayer::ExportManager"),
        QObject::tr("The document could not be written to \"%1\". Check file and folder "
                    "permissions and available space, then retry the export. Any existing "
                    "output has been kept.",
                    "ManagementLayer::ExportManager")
            .arg(_path));
}

} // namespace

class ExportManager::Implementation
{
public:
    explicit Implementation(ExportManager* _parent, QWidget* _topLevelWidget);

    /**
     * @brief Экспортировать документ
     */
    void exportScreenplays(const QVector<QPair<QString, BusinessLayer::AbstractModel*>>& _models,
                           int _currentModelIndex);
    bool exportScreenplay(BusinessLayer::AbstractModel* _model,
                          const BusinessLayer::ScreenplayExportOptions& _options);
    //
    void exportSimpleText(BusinessLayer::AbstractModel* _model);
    void exportCharacter(BusinessLayer::AbstractModel* _model);
    void exportCharacters(BusinessLayer::AbstractModel* _model);
    void exportLocation(BusinessLayer::AbstractModel* _model);
    void exportLocations(BusinessLayer::AbstractModel* _model);

    //
    // Данные
    //

    ExportManager* q = nullptr;

    QWidget* topLevelWidget = nullptr;

    Ui::ScreenplayExportDialog* screenplayExportDialog = nullptr;
    Ui::SimpleTextExportDialog* simpleTextExportDialog = nullptr;
    Ui::CharacterExportDialog* characterExportDialog = nullptr;
    Ui::CharactersExportDialog* charactersExportDialog = nullptr;
    Ui::LocationExportDialog* locationExportDialog = nullptr;
    Ui::LocationsExportDialog* locationsExportDialog = nullptr;
};

ExportManager::Implementation::Implementation(ExportManager* _parent, QWidget* _topLevelWidget)
    : q(_parent)
    , topLevelWidget(_topLevelWidget)
{
}

void ExportManager::Implementation::exportScreenplays(
    const QVector<QPair<QString, BusinessLayer::AbstractModel*>>& _models, int _currentModelIndex)
{
    using namespace BusinessLayer;

    if (screenplayExportDialog == nullptr) {
        const auto currentDraft = _models.constFirst().second;
        screenplayExportDialog = new Ui::ScreenplayExportDialog(
            currentDraft->document()->uuid().toString(), topLevelWidget);
        connect(screenplayExportDialog, &Ui::ScreenplayExportDialog::exportRequested,
                screenplayExportDialog, [this, _models] {
                    //
                    // Выбираем файл для экспорта
                    //
                    auto model = _models.at(screenplayExportDialog->selectedDraftIndex()).second;
                    const auto textModel = qobject_cast<BusinessLayer::ScreenplayTextModel*>(model);
                    auto exportOptions = screenplayExportDialog->exportOptions();
                    const auto exportFilePath = buildExportFilePath(
                        exportOptions.fileFormat, textModel->informationModel()->name(),
                        screenplayExportDialog->selectedDraftName(), textModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    if (!exportScreenplay(textModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (screenplayExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    screenplayExportDialog->hideDialog();
                });
        connect(screenplayExportDialog, &Ui::ScreenplayExportDialog::canceled,
                screenplayExportDialog, &Ui::ScreenplayExportDialog::hideDialog);
        connect(screenplayExportDialog, &Ui::ScreenplayExportDialog::disappeared,
                screenplayExportDialog, [this] {
                    screenplayExportDialog->deleteLater();
                    screenplayExportDialog = nullptr;
                });
    }

    QVector<QString> drafts;
    for (const auto& draft : _models) {
        drafts.append(draft.first);
    }
    screenplayExportDialog->setDrafts(drafts, _currentModelIndex);
    screenplayExportDialog->showDialog();
}

bool ExportManager::Implementation::exportScreenplay(
    BusinessLayer::AbstractModel* _model, const BusinessLayer::ScreenplayExportOptions& _options)
{
    using namespace BusinessLayer;

    ScreenplayExportOptions exportOptions = _options;

    //
    // ... донастроим параметры экспорта
    //
    const auto screenplayTextModel = qobject_cast<ScreenplayTextModel*>(_model);
    const auto screenplayInformation = screenplayTextModel->informationModel();
    exportOptions.templateId = screenplayInformation->templateId();
    exportOptions.showScenesNumbers = screenplayInformation->showSceneNumbers();
    exportOptions.showScenesNumbersOnLeft = screenplayInformation->showSceneNumbersOnLeft();
    exportOptions.showScenesNumbersOnRight = screenplayInformation->showSceneNumbersOnRight();
    exportOptions.showDialoguesNumbers = screenplayInformation->showDialoguesNumbers();
    exportOptions.header = screenplayInformation->header();
    exportOptions.printHeaderOnTitlePage = screenplayInformation->printHeaderOnTitlePage();
    exportOptions.footer = screenplayInformation->footer();
    exportOptions.printFooterOnTitlePage = screenplayInformation->printFooterOnTitlePage();
    //
    // ... при необходимости подсветить персонажей, делаем это
    //
    if (exportOptions.highlightCharacters) {
        const auto charactersModel = screenplayTextModel->charactersList();
        for (int row = 0; row < charactersModel->rowCount(); ++row) {
            const auto characterName = charactersModel->index(row, 0).data().toString();
            const auto character = screenplayTextModel->character(characterName);
            if (character != nullptr && character->color().isValid()) {
                exportOptions.highlightCharactersList.insert(character->name(), character->color());
            }
        }
    }
    //
    // ... обновим папку, куда в следующий раз он предположительно опять будет
    //     экспортировать
    //
    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                     QFileInfo(exportOptions.filePath).dir().absolutePath());
    //
    // ... и экспортируем документ
    //
    QScopedPointer<ScreenplayExporter> exporter;
    switch (exportOptions.fileFormat) {
    default:
    case ExportFileFormat::Pdf: {
        exporter.reset(new ScreenplayPdfExporter);
        break;
    }
    case ExportFileFormat::Docx: {
        exporter.reset(new ScreenplayDocxExporter);
        break;
    }
    case ExportFileFormat::Fdx: {
        exporter.reset(new ScreenplayFdxExporter);
        break;
    }
    case ExportFileFormat::Fountain: {
        exporter.reset(new ScreenplayFountainExporter);
        break;
    }
    }
    if (exporter.isNull()) {
        return false;
    }
    return exporter->exportTo(screenplayTextModel, exportOptions);
}

void ExportManager::Implementation::exportSimpleText(BusinessLayer::AbstractModel* _model)
{
    using namespace BusinessLayer;

    if (simpleTextExportDialog == nullptr) {
        simpleTextExportDialog
            = new Ui::SimpleTextExportDialog(_model->document()->uuid().toString(), topLevelWidget);
        connect(simpleTextExportDialog, &Ui::SimpleTextExportDialog::exportRequested,
                simpleTextExportDialog, [this, _model] {
                    //
                    // Выбираем файл для экспорта
                    //
                    const auto textModel = qobject_cast<BusinessLayer::SimpleTextModel*>(_model);
                    auto exportOptions = simpleTextExportDialog->exportOptions();
                    const auto exportFilePath = buildExportFilePath(
                        exportOptions.fileFormat, textModel->name(), {}, textModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    //
                    // ... донастроим параметры экспорта
                    //
                    exportOptions.templateId = TemplatesFacade::simpleTextTemplate().id();
                    exportOptions.includeTitlePage = false;
                    exportOptions.includeSynopsis = false;
                    exportOptions.includeText = true;
                    //
                    // ... обновим папку, куда в следующий раз он предположительно опять будет
                    //     экспортировать
                    //
                    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                                     QFileInfo(exportFilePath).dir().absolutePath());
                    //
                    // ... и экспортируем документ
                    //
                    QScopedPointer<BusinessLayer::AbstractExporter> exporter;
                    switch (exportOptions.fileFormat) {
                    default:
                    case ExportFileFormat::Pdf: {
                        exporter.reset(new BusinessLayer::SimpleTextPdfExporter);
                        break;
                    }
                    case ExportFileFormat::Docx: {
                        exporter.reset(new BusinessLayer::SimpleTextDocxExporter);
                        break;
                    }
                    case ExportFileFormat::Markdown: {
                        exporter.reset(new BusinessLayer::SimpleTextMarkdownExporter);
                        break;
                    }
                    }
                    if (exporter.isNull()) {
                        return;
                    }
                    if (!exporter->exportTo(textModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (simpleTextExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    simpleTextExportDialog->hideDialog();
                });
        connect(simpleTextExportDialog, &Ui::SimpleTextExportDialog::canceled,
                simpleTextExportDialog, &Ui::SimpleTextExportDialog::hideDialog);
        connect(simpleTextExportDialog, &Ui::SimpleTextExportDialog::disappeared,
                simpleTextExportDialog, [this] {
                    simpleTextExportDialog->deleteLater();
                    simpleTextExportDialog = nullptr;
                });
    }

    simpleTextExportDialog->showDialog();
}

void ExportManager::Implementation::exportCharacter(BusinessLayer::AbstractModel* _model)
{
    using namespace BusinessLayer;

    if (characterExportDialog == nullptr) {
        characterExportDialog
            = new Ui::CharacterExportDialog(_model->document()->uuid().toString(), topLevelWidget);
        connect(characterExportDialog, &Ui::CharacterExportDialog::exportRequested,
                characterExportDialog, [this, _model] {
                    //
                    // Выбираем файл для экспорта
                    //
                    const auto characterModel
                        = qobject_cast<BusinessLayer::CharacterModel*>(_model);
                    auto exportOptions = characterExportDialog->exportOptions();
                    const auto exportFilePath
                        = buildExportFilePath(exportOptions.fileFormat, characterModel->name(), {},
                                              characterModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    //
                    // ... обновим папку, куда в следующий раз он предположительно опять будет
                    //     экспортировать
                    //
                    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                                     QFileInfo(exportFilePath).dir().absolutePath());
                    //
                    // ... и экспортируем документ
                    //
                    QScopedPointer<BusinessLayer::AbstractExporter> exporter;
                    switch (exportOptions.fileFormat) {
                    default:
                    case ExportFileFormat::Pdf: {
                        exporter.reset(new BusinessLayer::CharacterPdfExporter);
                        break;
                    }
                    case ExportFileFormat::Docx: {
                        exporter.reset(new BusinessLayer::CharacterDocxExporter);
                        break;
                    }
                    }
                    if (exporter.isNull()) {
                        return;
                    }
                    if (!exporter->exportTo(characterModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (characterExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    characterExportDialog->hideDialog();
                });
        connect(characterExportDialog, &Ui::CharacterExportDialog::canceled, characterExportDialog,
                &Ui::CharacterExportDialog::hideDialog);
        connect(characterExportDialog, &Ui::CharacterExportDialog::disappeared,
                characterExportDialog, [this] {
                    characterExportDialog->deleteLater();
                    characterExportDialog = nullptr;
                });
    }

    characterExportDialog->showDialog();
}

void ExportManager::Implementation::exportCharacters(BusinessLayer::AbstractModel* _model)
{
    using namespace BusinessLayer;

    if (charactersExportDialog == nullptr) {
        charactersExportDialog
            = new Ui::CharactersExportDialog(_model->document()->uuid().toString(), topLevelWidget);
        connect(charactersExportDialog, &Ui::CharactersExportDialog::exportRequested,
                charactersExportDialog, [this, _model] {
                    //
                    // Выбираем файл для экспорта
                    //
                    const auto charactersModel
                        = qobject_cast<BusinessLayer::CharactersModel*>(_model);
                    auto exportOptions = charactersExportDialog->exportOptions();
                    const auto exportFilePath
                        = buildExportFilePath(exportOptions.fileFormat, tr("Characters"), {},
                                              charactersModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    //
                    // ... обновим папку, куда в следующий раз он предположительно опять будет
                    //     экспортировать
                    //
                    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                                     QFileInfo(exportFilePath).dir().absolutePath());
                    //
                    // ... и экспортируем документ
                    //
                    QScopedPointer<BusinessLayer::AbstractExporter> exporter;
                    switch (exportOptions.fileFormat) {
                    default:
                    case ExportFileFormat::Pdf: {
                        exporter.reset(new BusinessLayer::CharactersPdfExporter);
                        break;
                    }
                    case ExportFileFormat::Docx: {
                        exporter.reset(new BusinessLayer::CharactersDocxExporter);
                        break;
                    }
                    }
                    if (exporter.isNull()) {
                        return;
                    }
                    if (!exporter->exportTo(charactersModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (charactersExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    charactersExportDialog->hideDialog();
                });
        connect(charactersExportDialog, &Ui::CharactersExportDialog::canceled,
                charactersExportDialog, &Ui::CharactersExportDialog::hideDialog);
        connect(charactersExportDialog, &Ui::CharactersExportDialog::disappeared,
                charactersExportDialog, [this] {
                    charactersExportDialog->deleteLater();
                    charactersExportDialog = nullptr;
                });
    }

    charactersExportDialog->setModel(_model);
    charactersExportDialog->showDialog();
}

void ExportManager::Implementation::exportLocation(BusinessLayer::AbstractModel* _model)
{
    using namespace BusinessLayer;

    if (locationExportDialog == nullptr) {
        locationExportDialog
            = new Ui::LocationExportDialog(_model->document()->uuid().toString(), topLevelWidget);
        connect(locationExportDialog, &Ui::LocationExportDialog::exportRequested,
                locationExportDialog, [this, _model] {
                    //
                    // Выбираем файл для экспорта
                    //
                    const auto locationModel = qobject_cast<BusinessLayer::LocationModel*>(_model);
                    auto exportOptions = locationExportDialog->exportOptions();
                    const auto exportFilePath
                        = buildExportFilePath(exportOptions.fileFormat, locationModel->name(), {},
                                              locationModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    //
                    // ... обновим папку, куда в следующий раз он предположительно опять будет
                    //     экспортировать
                    //
                    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                                     QFileInfo(exportFilePath).dir().absolutePath());
                    //
                    // ... и экспортируем документ
                    //
                    QScopedPointer<BusinessLayer::AbstractExporter> exporter;
                    switch (exportOptions.fileFormat) {
                    default:
                    case ExportFileFormat::Pdf: {
                        exporter.reset(new BusinessLayer::LocationPdfExporter);
                        break;
                    }
                    case ExportFileFormat::Docx: {
                        exporter.reset(new BusinessLayer::LocationDocxExporter);
                        break;
                    }
                    }
                    if (exporter.isNull()) {
                        return;
                    }
                    if (!exporter->exportTo(locationModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (locationExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    locationExportDialog->hideDialog();
                });
        connect(locationExportDialog, &Ui::LocationExportDialog::canceled, locationExportDialog,
                &Ui::LocationExportDialog::hideDialog);
        connect(locationExportDialog, &Ui::LocationExportDialog::disappeared, locationExportDialog,
                [this] {
                    locationExportDialog->deleteLater();
                    locationExportDialog = nullptr;
                });
    }

    locationExportDialog->showDialog();
}

void ExportManager::Implementation::exportLocations(BusinessLayer::AbstractModel* _model)
{
    using namespace BusinessLayer;

    if (locationsExportDialog == nullptr) {
        locationsExportDialog
            = new Ui::LocationsExportDialog(_model->document()->uuid().toString(), topLevelWidget);
        connect(locationsExportDialog, &Ui::LocationsExportDialog::exportRequested,
                locationsExportDialog, [this, _model] {
                    //
                    // Выбираем файл для экспорта
                    //
                    const auto locationsModel
                        = qobject_cast<BusinessLayer::LocationsModel*>(_model);
                    auto exportOptions = locationsExportDialog->exportOptions();
                    const auto exportFilePath
                        = buildExportFilePath(exportOptions.fileFormat, tr("Locations"), {},
                                              locationsModel, topLevelWidget);
                    if (exportFilePath.isEmpty()) {
                        return;
                    }

                    //
                    // Если файл был выбран
                    //
                    exportOptions.filePath = exportFilePath;
                    //
                    // ... обновим папку, куда в следующий раз он предположительно опять будет
                    //     экспортировать
                    //
                    setSettingsValue(DataStorageLayer::kProjectExportFolderKey,
                                     QFileInfo(exportFilePath).dir().absolutePath());
                    //
                    // ... и экспортируем документ
                    //
                    QScopedPointer<BusinessLayer::AbstractExporter> exporter;
                    switch (exportOptions.fileFormat) {
                    default:
                    case ExportFileFormat::Pdf: {
                        exporter.reset(new BusinessLayer::LocationsPdfExporter);
                        break;
                    }
                    case ExportFileFormat::Docx: {
                        exporter.reset(new BusinessLayer::LocationsDocxExporter);
                        break;
                    }
                    }
                    if (exporter.isNull()) {
                        return;
                    }
                    if (!exporter->exportTo(locationsModel, exportOptions)) {
                        showExportError(exportOptions.filePath, topLevelWidget);
                        return;
                    }

                    //
                    // Если необходимо, откроем экспортированный документ
                    //
                    if (locationsExportDialog->openDocumentAfterExport()) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(exportOptions.filePath));
                    }
                    //
                    // ... и закрываем диалог экспорта
                    //
                    locationsExportDialog->hideDialog();
                });
        connect(locationsExportDialog, &Ui::LocationsExportDialog::canceled, locationsExportDialog,
                &Ui::LocationsExportDialog::hideDialog);
        connect(locationsExportDialog, &Ui::LocationsExportDialog::disappeared,
                locationsExportDialog, [this] {
                    locationsExportDialog->deleteLater();
                    locationsExportDialog = nullptr;
                });
    }

    locationsExportDialog->setModel(_model);
    locationsExportDialog->showDialog();
}


// ****


ExportManager::ExportManager(QObject* _parent, QWidget* _parentWidget)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget))
{
}

ExportManager::~ExportManager() = default;

bool ExportManager::canExportDocument(BusinessLayer::AbstractModel* _model) const
{
    if (_model == nullptr || _model->document() == nullptr) {
        return false;
    }

    switch (_model->document()->type()) {
    //
    case Domain::DocumentObjectType::Screenplay:
    case Domain::DocumentObjectType::ScreenplayTitlePage:
    case Domain::DocumentObjectType::ScreenplaySynopsis:
    case Domain::DocumentObjectType::ScreenplayTreatment:
    case Domain::DocumentObjectType::ScreenplayText:
    case Domain::DocumentObjectType::ScreenplayStatistics:
    //
    case Domain::DocumentObjectType::SimpleText:
    //
    case Domain::DocumentObjectType::Character:
    case Domain::DocumentObjectType::Characters:
    case Domain::DocumentObjectType::Location:
    case Domain::DocumentObjectType::Locations:
        return true;

    default:
        return false;
    }
}

void ExportManager::exportDocument(
    const QVector<QPair<QString, BusinessLayer::AbstractModel*>>& _models, int _currentModelIndex)
{
    if (_models.isEmpty()) {
        return;
    }

    auto firstModel = _models.constFirst().second;
    if (!canExportDocument(firstModel)) {
        return;
    }

    Log::info("Exporting started. Export document of type %1",
              Domain::mimeTypeFor(firstModel->document()->type()).constData());

    switch (firstModel->document()->type()) {
    case Domain::DocumentObjectType::ScreenplayText: {
        d->exportScreenplays(_models, _currentModelIndex);
        break;
    }

    case Domain::DocumentObjectType::SimpleText: {
        d->exportSimpleText(firstModel);
        break;
    }

    case Domain::DocumentObjectType::Character: {
        d->exportCharacter(firstModel);
        break;
    }

    case Domain::DocumentObjectType::Characters: {
        d->exportCharacters(firstModel);
        break;
    }

    case Domain::DocumentObjectType::Location: {
        d->exportLocation(firstModel);
        break;
    }

    case Domain::DocumentObjectType::Locations: {
        d->exportLocations(firstModel);
        break;
    }

    default:
        break;
    }
}

} // namespace ManagementLayer
