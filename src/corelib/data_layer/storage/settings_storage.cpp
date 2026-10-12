#include "settings_storage.h"

#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/simple_text_template.h>
#include <data_layer/mapper/mapper_facade.h>
#include <data_layer/mapper/settings_mapper.h>
#include <ui/design_system/design_system.h>
#include <utils/logging.h>

#include <QDir>
#include <QKeySequence>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>

using DataMappingLayer::MapperFacade;


namespace DataStorageLayer {

namespace {
/**
 * @brief Имя пользователя из системы
 */
static QString systemUserName()
{
    QString name = qgetenv("USER");
    if (name.isEmpty()) {
        //
        // Windows
        //
        name = QString::fromLocal8Bit(qgetenv("USERNAME"));
        if (name.isEmpty()) {
            name = "user";
        }
    }
    return name;
}
} // namespace

class SettingsStorage::Implementation
{
public:
    Implementation();

    /**
     * @brief Загрузить параметр из кэша
     */
    QVariant cachedValue(const QString& _key, bool& _ok) const;

    /**
     * @brief Сохранить параметр в кэше
     */
    void cacheValue(const QString& _key, const QVariant& _value);


    /**
     * @brief Возможно ли сохранять параметры
     */
    bool isReadOnly = false;

    /**
     * @brief Кэшированные значения параметров
     */
    QVariantMap cachedAppSettings;

    /**
     * @brief Значения параметров текущей сессии
     */
    QVariantMap sessionSettings;

    /**
     * @brief Изменённые значения параметров приложения, ожидающие записи на диск
     */
    QVariantMap pendingAppSettings;

    /**
     * @brief Настройки приложения
     */
    QSettings appSettings;

    /**
     * @brief Значения параметров по умолчанию
     */
    QVariantMap defaultSettings;
};

SettingsStorage::Implementation::Implementation()
{
    using namespace BusinessLayer;

    //
    // Настроим значения параметров по умолчанию
    //

    //
    // Для приложения
    //
    defaultSettings.insert(kApplicationConfiguredKey, false);
    defaultSettings.insert(kApplicationLanguagedKey, QLocale::AnyLanguage);
    defaultSettings.insert(kApplicationThemeKey, static_cast<int>(Ui::ApplicationTheme::Dark));
    defaultSettings.insert(
        kApplicationCustomThemeColorsKey,
        "323740ffffff2ab177f8f8f2272b34f8f8f222262ef8f8f2ec3740f8f8f2000000f8f8f2272b34f8f8f2");
    defaultSettings.insert(kApplicationScaleFactorKey, 1.0);
    defaultSettings.insert(kApplicationDensityKey, 0);
    defaultSettings.insert(kApplicationUseAutoSaveKey, true);
    defaultSettings.insert(kApplicationSaveBackupsKey, true);
    defaultSettings.insert(kApplicationBackupsFolderKey,
                           QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                               + "/starc/backups");
    defaultSettings.insert(kApplicationBackupsQtyKey, 7);
    defaultSettings.insert(kApplicationShowDocumentsPagesKey, true);
    defaultSettings.insert(kApplicationUseTypewriterSoundKey, false);
    defaultSettings.insert(kApplicationUseSpellCheckerKey, false);
    defaultSettings.insert(kApplicationHighlightCurrentLineKey, false);
    defaultSettings.insert(kApplicationFocusCurrentParagraphKey, false);
    defaultSettings.insert(kApplicationUseTypewriterScrollingKey, false);
    defaultSettings.insert(kApplicationCorrectDoubleCapitalsKey, true);
    defaultSettings.insert(kApplicationCapitalizeSingleILetterKey, true);
    defaultSettings.insert(kApplicationReplaceThreeDotsWithEllipsisKey, true);
    defaultSettings.insert(kApplicationSmartQuotesKey, false);
    defaultSettings.insert(kApplicationReplaceTwoDashesWithEmDashKey, false);
    defaultSettings.insert(kApplicationAvoidMultipleSpacesKey, false);
    defaultSettings.insert(kApplicationShortcutsImportKey, "Alt+I");
    defaultSettings.insert(kApplicationShortcutsCurrentDocumentExportKey, "Alt+E");
    defaultSettings.insert(kApplicationAdvancedAccurateMetricsHandlingKey, false);
    defaultSettings.insert(kApplicationLoggingLevelKey, static_cast<int>(Log::Level::Debug));

    defaultSettings.insert(kGitHubSyncUploadOnExitKey, true);

    defaultSettings.insert(kProjectTypeKey, 0);
    defaultSettings.insert(kProjectSaveFolderKey,
                           QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                               + "/starc/projects");
    defaultSettings.insert(kProjectImportFolderKey,
                           QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    defaultSettings.insert(kProjectExportFolderKey,
                           QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    //
    // Параметры редактора текста
    //
    {
        auto addSimpleTextEditorStylesAction
            = [this](const QString& _actionType, const QString& _actionKey, TextParagraphType _from,
                     TextParagraphType _to) {
                  defaultSettings.insert(QString("%1/styles-%2/from-%3-by-%4")
                                             .arg(kComponentsSimpleTextEditorKey, _actionType,
                                                  toString(_from), _actionKey),
                                         toString(_to));
              };
        auto addSimpleTextEditorStylesActionByTab
            = [addSimpleTextEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                                TextParagraphType _to) {
                  addSimpleTextEditorStylesAction(_actionType, "tab", _from, _to);
              };
        auto addSimpleTextEditorStylesActionByEnter
            = [addSimpleTextEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                                TextParagraphType _to) {
                  addSimpleTextEditorStylesAction(_actionType, "enter", _from, _to);
              };
        //
        auto addSimpleTextEditorStylesJumpByTab =
            [addSimpleTextEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addSimpleTextEditorStylesActionByTab("jumping", _from, _to);
            };
        auto addSimpleTextEditorStylesJumpByEnter
            = [addSimpleTextEditorStylesActionByEnter](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addSimpleTextEditorStylesActionByEnter("jumping", _from, _to);
              };
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading1,
                                           TextParagraphType::ChapterHeading2);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading1,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading2,
                                           TextParagraphType::ChapterHeading3);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading2,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading3,
                                           TextParagraphType::ChapterHeading4);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading3,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading4,
                                           TextParagraphType::ChapterHeading5);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading4,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading5,
                                           TextParagraphType::ChapterHeading6);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading5,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::ChapterHeading6,
                                           TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::ChapterHeading6,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::Text, TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::Text, TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByTab(TextParagraphType::InlineNote, TextParagraphType::Text);
        addSimpleTextEditorStylesJumpByEnter(TextParagraphType::InlineNote,
                                             TextParagraphType::Text);
        //
        auto addSimpleTextEditorStylesChangeByTab =
            [addSimpleTextEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addSimpleTextEditorStylesActionByTab("changing", _from, _to);
            };
        auto addSimpleTextEditorStylesChangeByEnter
            = [addSimpleTextEditorStylesActionByEnter](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addSimpleTextEditorStylesActionByEnter("changing", _from, _to);
              };
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading1,
                                             TextParagraphType::ChapterHeading2);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading1,
                                               TextParagraphType::ChapterHeading1);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading2,
                                             TextParagraphType::ChapterHeading3);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading2,
                                               TextParagraphType::ChapterHeading1);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading3,
                                             TextParagraphType::ChapterHeading4);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading3,
                                               TextParagraphType::ChapterHeading2);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading4,
                                             TextParagraphType::ChapterHeading5);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading4,
                                               TextParagraphType::ChapterHeading3);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading5,
                                             TextParagraphType::ChapterHeading6);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading5,
                                               TextParagraphType::ChapterHeading4);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::ChapterHeading6,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::ChapterHeading6,
                                               TextParagraphType::ChapterHeading5);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::Text,
                                             TextParagraphType::InlineNote);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::Text,
                                               TextParagraphType::ChapterHeading6);
        addSimpleTextEditorStylesChangeByTab(TextParagraphType::InlineNote,
                                             TextParagraphType::Text);
        addSimpleTextEditorStylesChangeByEnter(TextParagraphType::InlineNote,
                                               TextParagraphType::InlineNote);
        //
        auto addShortcut
            = [this](BusinessLayer::TextParagraphType _type, const QString& _shortcut) {
                  defaultSettings.insert(
                      QString("%1/shortcuts/%2")
                          .arg(kComponentsSimpleTextEditorKey, BusinessLayer::toString(_type)),
                      QKeySequence(_shortcut).toString(QKeySequence::NativeText));
              };
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading1, "Ctrl+1");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading2, "Ctrl+2");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading3, "Ctrl+3");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading4, "Ctrl+4");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading5, "Ctrl+5");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading6, "Ctrl+6");
        addShortcut(BusinessLayer::TextParagraphType::Text, "Ctrl+7");
        addShortcut(BusinessLayer::TextParagraphType::InlineNote, "Ctrl+Shift+0");
        //
        defaultSettings.insert(kComponentsSimpleTextAvailableKey, true);
        //
        defaultSettings.insert(kComponentsSimpleTextEditorDefaultTemplateKey, "mono_cp_a4");
        defaultSettings.insert(kComponentsSimpleTextEditorCorrectTextOnPageBreaksKey, false);
        //
        // Параметры навигатора простого текстового документа
        //
        defaultSettings.insert(kComponentsSimpleTextNavigatorShowSceneTextKey, true);
        defaultSettings.insert(kComponentsSimpleTextNavigatorSceneTextLinesKey, 1);
    }
    //
    // Параметры редактора тритмента сценария
    //
    {
        auto addTreatmentEditorStylesAction
            = [this](const QString& _actionType, const QString& _actionKey, TextParagraphType _from,
                     TextParagraphType _to) {
                  defaultSettings.insert(QString("%1/styles-%2/from-%3-by-%4")
                                             .arg(kComponentsScreenplayTreatmentEditorKey,
                                                  _actionType, toString(_from), _actionKey),
                                         toString(_to));
              };
        auto addTreatmentEditorStylesActionByTab
            = [addTreatmentEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                               TextParagraphType _to) {
                  addTreatmentEditorStylesAction(_actionType, "tab", _from, _to);
              };
        auto addTreatmentEditorStylesActionByEnter
            = [addTreatmentEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                               TextParagraphType _to) {
                  addTreatmentEditorStylesAction(_actionType, "enter", _from, _to);
              };
        //
        auto addTreatmentEditorStylesJumpByTab =
            [addTreatmentEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addTreatmentEditorStylesActionByTab("jumping", _from, _to);
            };
        auto addTreatmentEditorStylesJumpByEnter
            = [addTreatmentEditorStylesActionByEnter](TextParagraphType _from,
                                                      TextParagraphType _to) {
                  addTreatmentEditorStylesActionByEnter("jumping", _from, _to);
              };
        addTreatmentEditorStylesJumpByTab(TextParagraphType::SceneHeading,
                                          TextParagraphType::SceneCharacters);
        addTreatmentEditorStylesJumpByEnter(TextParagraphType::SceneHeading,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesJumpByTab(TextParagraphType::SceneCharacters,
                                          TextParagraphType::BeatHeading);
        addTreatmentEditorStylesJumpByEnter(TextParagraphType::SceneCharacters,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesJumpByTab(TextParagraphType::BeatHeading,
                                          TextParagraphType::BeatHeading);
        addTreatmentEditorStylesJumpByEnter(TextParagraphType::BeatHeading,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesJumpByTab(TextParagraphType::SequenceHeading,
                                          TextParagraphType::SceneHeading);
        addTreatmentEditorStylesJumpByEnter(TextParagraphType::SequenceHeading,
                                            TextParagraphType::SceneHeading);
        addTreatmentEditorStylesJumpByTab(TextParagraphType::ActHeading,
                                          TextParagraphType::SequenceHeading);
        addTreatmentEditorStylesJumpByEnter(TextParagraphType::ActHeading,
                                            TextParagraphType::SceneHeading);
        //
        auto addTreatmentEditorStylesChangeByTab =
            [addTreatmentEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addTreatmentEditorStylesActionByTab("changing", _from, _to);
            };
        auto addTreatmentEditorStylesChangeByEnter
            = [addTreatmentEditorStylesActionByEnter](TextParagraphType _from,
                                                      TextParagraphType _to) {
                  addTreatmentEditorStylesActionByEnter("changing", _from, _to);
              };
        addTreatmentEditorStylesChangeByTab(TextParagraphType::SceneHeading,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesChangeByEnter(TextParagraphType::SceneHeading,
                                              TextParagraphType::SceneHeading);
        addTreatmentEditorStylesChangeByTab(TextParagraphType::SceneCharacters,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesChangeByEnter(TextParagraphType::SceneCharacters,
                                              TextParagraphType::SceneCharacters);
        addTreatmentEditorStylesChangeByTab(TextParagraphType::BeatHeading,
                                            TextParagraphType::BeatHeading);
        addTreatmentEditorStylesChangeByEnter(TextParagraphType::BeatHeading,
                                              TextParagraphType::SceneHeading);
        addTreatmentEditorStylesChangeByTab(TextParagraphType::SequenceHeading,
                                            TextParagraphType::SequenceHeading);
        addTreatmentEditorStylesChangeByEnter(TextParagraphType::SequenceHeading,
                                              TextParagraphType::SequenceHeading);
        addTreatmentEditorStylesChangeByTab(TextParagraphType::ActHeading,
                                            TextParagraphType::ActHeading);
        addTreatmentEditorStylesChangeByEnter(TextParagraphType::ActHeading,
                                              TextParagraphType::ActHeading);
        //
        auto addShortcut = [this](BusinessLayer::TextParagraphType _type,
                                  const QString& _shortcut) {
            defaultSettings.insert(
                QString("%1/shortcuts/%2")
                    .arg(kComponentsScreenplayTreatmentEditorKey, BusinessLayer::toString(_type)),
                QKeySequence(_shortcut).toString(QKeySequence::NativeText));
        };
        addShortcut(BusinessLayer::TextParagraphType::SceneHeading, "Ctrl+1");
        addShortcut(BusinessLayer::TextParagraphType::BeatHeading, "Ctrl+Shift+1");
        addShortcut(BusinessLayer::TextParagraphType::SceneCharacters, "Ctrl+2");
        addShortcut(BusinessLayer::TextParagraphType::SequenceHeading, "Ctrl+Space");
        addShortcut(BusinessLayer::TextParagraphType::ActHeading, "Ctrl+Shift+Space");
    }
    //
    // Параметры редактора сценария
    //
    {
        auto addScreenplayEditorStylesAction
            = [this](const QString& _actionType, const QString& _actionKey, TextParagraphType _from,
                     TextParagraphType _to) {
                  defaultSettings.insert(QString("%1/styles-%2/from-%3-by-%4")
                                             .arg(kComponentsScreenplayEditorKey, _actionType,
                                                  toString(_from), _actionKey),
                                         toString(_to));
              };
        auto addScreenplayEditorStylesActionByTab
            = [addScreenplayEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                                TextParagraphType _to) {
                  addScreenplayEditorStylesAction(_actionType, "tab", _from, _to);
              };
        auto addScreenplayEditorStylesActionByEnter
            = [addScreenplayEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                                TextParagraphType _to) {
                  addScreenplayEditorStylesAction(_actionType, "enter", _from, _to);
              };
        //
        auto addScreenplayEditorStylesJumpByTab =
            [addScreenplayEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addScreenplayEditorStylesActionByTab("jumping", _from, _to);
            };
        auto addScreenplayEditorStylesJumpByEnter
            = [addScreenplayEditorStylesActionByEnter](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addScreenplayEditorStylesActionByEnter("jumping", _from, _to);
              };
        addScreenplayEditorStylesJumpByTab(TextParagraphType::UnformattedText,
                                           TextParagraphType::UnformattedText);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::UnformattedText,
                                             TextParagraphType::UnformattedText);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::SceneHeading,
                                           TextParagraphType::SceneCharacters);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::SceneHeading,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::SceneCharacters,
                                           TextParagraphType::Action);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::SceneCharacters,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::BeatHeading,
                                           TextParagraphType::Action);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::BeatHeading,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Action, TextParagraphType::Character);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Action, TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Character,
                                           TextParagraphType::Parenthetical);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Character,
                                             TextParagraphType::Dialogue);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Parenthetical,
                                           TextParagraphType::Dialogue);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Parenthetical,
                                             TextParagraphType::Dialogue);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Dialogue,
                                           TextParagraphType::Parenthetical);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Dialogue,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Lyrics,
                                           TextParagraphType::Parenthetical);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Lyrics, TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Transition,
                                           TextParagraphType::SceneHeading);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Transition,
                                             TextParagraphType::SceneHeading);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::Shot, TextParagraphType::Action);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::Shot, TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::InlineNote,
                                           TextParagraphType::Action);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::InlineNote,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::SequenceHeading,
                                           TextParagraphType::SceneHeading);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::SequenceHeading,
                                             TextParagraphType::SceneHeading);
        addScreenplayEditorStylesJumpByTab(TextParagraphType::ActHeading,
                                           TextParagraphType::SequenceHeading);
        addScreenplayEditorStylesJumpByEnter(TextParagraphType::ActHeading,
                                             TextParagraphType::SceneHeading);
        //
        auto addScreenplayEditorStylesChangeByTab =
            [addScreenplayEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                addScreenplayEditorStylesActionByTab("changing", _from, _to);
            };
        auto addScreenplayEditorStylesChangeByEnter
            = [addScreenplayEditorStylesActionByEnter](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addScreenplayEditorStylesActionByEnter("changing", _from, _to);
              };
        addScreenplayEditorStylesChangeByTab(TextParagraphType::UnformattedText,
                                             TextParagraphType::UnformattedText);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::UnformattedText,
                                               TextParagraphType::UnformattedText);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::SceneHeading,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::SceneHeading,
                                               TextParagraphType::SceneHeading);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::SceneCharacters,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::SceneCharacters,
                                               TextParagraphType::Action);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::BeatHeading,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::BeatHeading,
                                               TextParagraphType::Action);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Action,
                                             TextParagraphType::Character);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Action,
                                               TextParagraphType::SceneHeading);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Character,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Character,
                                               TextParagraphType::Action);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Parenthetical,
                                             TextParagraphType::Dialogue);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Parenthetical,
                                               TextParagraphType::Parenthetical);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Dialogue,
                                             TextParagraphType::Parenthetical);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Dialogue,
                                               TextParagraphType::Action);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Lyrics,
                                             TextParagraphType::Parenthetical);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Lyrics,
                                               TextParagraphType::Action);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Transition,
                                             TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Transition,
                                               TextParagraphType::Transition);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::Shot, TextParagraphType::Action);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::Shot, TextParagraphType::Shot);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::InlineNote,
                                             TextParagraphType::InlineNote);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::InlineNote,
                                               TextParagraphType::InlineNote);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::SequenceHeading,
                                             TextParagraphType::SceneHeading);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::SequenceHeading,
                                               TextParagraphType::ActHeading);
        addScreenplayEditorStylesChangeByTab(TextParagraphType::ActHeading,
                                             TextParagraphType::SequenceHeading);
        addScreenplayEditorStylesChangeByEnter(TextParagraphType::ActHeading,
                                               TextParagraphType::ActHeading);
        //
        auto addShortcut
            = [this](BusinessLayer::TextParagraphType _type, const QString& _shortcut) {
                  defaultSettings.insert(
                      QString("%1/shortcuts/%2")
                          .arg(kComponentsScreenplayEditorKey, BusinessLayer::toString(_type)),
                      QKeySequence(_shortcut).toString(QKeySequence::NativeText));
              };
        addShortcut(BusinessLayer::TextParagraphType::UnformattedText, "Ctrl+0");
        addShortcut(BusinessLayer::TextParagraphType::SceneHeading, "Ctrl+1");
        addShortcut(BusinessLayer::TextParagraphType::BeatHeading, "Ctrl+Shift+1");
        addShortcut(BusinessLayer::TextParagraphType::SceneCharacters, "Ctrl+2");
        addShortcut(BusinessLayer::TextParagraphType::Action, "Ctrl+3");
        addShortcut(BusinessLayer::TextParagraphType::Character, "Ctrl+4");
        addShortcut(BusinessLayer::TextParagraphType::Parenthetical, "Ctrl+5");
        addShortcut(BusinessLayer::TextParagraphType::Dialogue, "Ctrl+6");
        addShortcut(BusinessLayer::TextParagraphType::Lyrics, "Ctrl+7");
        addShortcut(BusinessLayer::TextParagraphType::Shot, "Ctrl+8");
        addShortcut(BusinessLayer::TextParagraphType::Transition, "Ctrl+9");
        addShortcut(BusinessLayer::TextParagraphType::InlineNote, "Ctrl+Shift+0");
        addShortcut(BusinessLayer::TextParagraphType::SequenceHeading, "Ctrl+Space");
        addShortcut(BusinessLayer::TextParagraphType::ActHeading, "Ctrl+Shift+Space");
        //
        defaultSettings.insert(kComponentsScreenplayAvailableKey, true);
        //
        defaultSettings.insert(kComponentsScreenplayEditorDefaultTemplateKey, "world_cp");
        defaultSettings.insert(kComponentsScreenplayEditorShowSceneNumbersKey, false);
        defaultSettings.insert(kComponentsScreenplayEditorShowSceneNumbersOnLeftKey, true);
        defaultSettings.insert(kComponentsScreenplayEditorShowSceneNumbersOnRightKey, true);
        defaultSettings.insert(kComponentsScreenplayEditorContinueDialogueKey, false);
        defaultSettings.insert(kComponentsScreenplayEditorCorrectTextOnPageBreaksKey, true);
        defaultSettings.insert(kComponentsScreenplayEditorSaveItemsFromTextKey, true);
        defaultSettings.insert(kComponentsScreenplayEditorShowHintsForAllItemsKey, true);
        defaultSettings.insert(kComponentsScreenplayEditorShowHintsForPrimaryItemsKey, false);
        defaultSettings.insert(kComponentsScreenplayEditorShowHintsForSecondaryItemsKey, false);
        defaultSettings.insert(kComponentsScreenplayEditorShowHintsForTertiaryItemsKey, false);
        defaultSettings.insert(kComponentsScreenplayEditorShowCharacterSuggestionsInEmptyBlockKey,
                               true);
        defaultSettings.insert(
            kComponentsScreenplayEditorUseOpenParenthesisInDialogueForParentheticalKey, true);
        //
        // Параметры навигатора сценария
        //
        defaultSettings.insert(kComponentsScreenplayNavigatorShowBeatsKey, true);
        defaultSettings.insert(kComponentsScreenplayNavigatorShowBeatsInTreatmentKey, true);
        defaultSettings.insert(kComponentsScreenplayNavigatorShowBeatsInScreenplayKey, false);
        defaultSettings.insert(kComponentsScreenplayNavigatorShowSceneNumberKey, true);
        defaultSettings.insert(kComponentsScreenplayNavigatorShowSceneTextKey, true);
        defaultSettings.insert(kComponentsScreenplayNavigatorSceneTextLinesKey, 1);
        //
        // Параметры хронометража сценария
        //
        defaultSettings.insert(kComponentsScreenplayDurationTypeKey, 0);
        defaultSettings.insert(kComponentsScreenplayDurationByPageDurationKey, 60);
        defaultSettings.insert(kComponentsScreenplayDurationByCharactersCharactersKey, 1350);
        defaultSettings.insert(kComponentsScreenplayDurationByCharactersIncludeSpacesKey, true);
        defaultSettings.insert(kComponentsScreenplayDurationByCharactersDurationKey, 60);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerParagraphForActionKey, 1.0);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForActionKey, 1.5);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerParagraphForDialogueKey, 2.0);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForDialogueKey, 2.4);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerParagraphForSceneHeadingKey, 2.0);
        defaultSettings.insert(
            kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForSceneHeadingKey, 0.0);
        defaultSettings.insert(kComponentsScreenplayDurationUseEighthsKey, false);
        //
        // Параметры карточек сценария
        //
        defaultSettings.insert(kComponentsScreenplayCardsLockCardsKey, true);
        defaultSettings.insert(kComponentsScreenplayCardsArrangeByRowsKey, true);
        defaultSettings.insert(kComponentsScreenplayCardsCardsSizeKey, 10);
        defaultSettings.insert(kComponentsScreenplayCardsCardsRatioKey, 2);
        defaultSettings.insert(kComponentsScreenplayCardsCardsSpacingKey, 20);
        defaultSettings.insert(kComponentsScreenplayCardsCardsInRowKey, -1);
        //
        // Параметры таймлайна сценария
        //
        defaultSettings.insert(kComponentsScreenplayTimelineArrangeHorizontalKey, true);
        defaultSettings.insert(kComponentsScreenplayTimelineCardsSizeKey, 10);
        defaultSettings.insert(kComponentsScreenplayTimelineCardsRatioKey, 2);
        defaultSettings.insert(kComponentsScreenplayTimelineCardsSpacingKey, 20);
        //
        // Параметры посерийного плана
        //
        defaultSettings.insert(kComponentsScreenplaySeriesPlanCardsSizeKey, 10);
        defaultSettings.insert(kComponentsScreenplaySeriesPlanCardsRatioKey, 2);
        defaultSettings.insert(kComponentsScreenplaySeriesPlanCardsSpacingKey, 20);
    }
    //
    // Параметры редактора плана романа
    //
    {
        auto addNovelOutlineEditorStylesAction
            = [this](const QString& _actionType, const QString& _actionKey, TextParagraphType _from,
                     TextParagraphType _to) {
                  defaultSettings.insert(QString("%1/styles-%2/from-%3-by-%4")
                                             .arg(kComponentsNovelOutlineEditorKey, _actionType,
                                                  toString(_from), _actionKey),
                                         toString(_to));
              };
        auto addNovelOutlineEditorStylesActionByTab
            = [addNovelOutlineEditorStylesAction](const QString& _actionType,
                                                  TextParagraphType _from, TextParagraphType _to) {
                  addNovelOutlineEditorStylesAction(_actionType, "tab", _from, _to);
              };
        auto addNovelOutlineEditorStylesActionByEnter
            = [addNovelOutlineEditorStylesAction](const QString& _actionType,
                                                  TextParagraphType _from, TextParagraphType _to) {
                  addNovelOutlineEditorStylesAction(_actionType, "enter", _from, _to);
              };
        //
        auto addNovelOutlineEditorStylesJumpByTab
            = [addNovelOutlineEditorStylesActionByTab](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addNovelOutlineEditorStylesActionByTab("jumping", _from, _to);
              };
        auto addNovelOutlineEditorStylesJumpByEnter
            = [addNovelOutlineEditorStylesActionByEnter](TextParagraphType _from,
                                                         TextParagraphType _to) {
                  addNovelOutlineEditorStylesActionByEnter("jumping", _from, _to);
              };
        addNovelOutlineEditorStylesJumpByTab(TextParagraphType::SceneHeading,
                                             TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByEnter(TextParagraphType::SceneHeading,
                                               TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByTab(TextParagraphType::BeatHeading,
                                             TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByEnter(TextParagraphType::BeatHeading,
                                               TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByTab(TextParagraphType::ChapterHeading,
                                             TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByEnter(TextParagraphType::ChapterHeading,
                                               TextParagraphType::SceneHeading);
        addNovelOutlineEditorStylesJumpByTab(TextParagraphType::PartHeading,
                                             TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesJumpByEnter(TextParagraphType::PartHeading,
                                               TextParagraphType::ChapterHeading);
        //
        auto addNovelOutlineEditorStylesChangeByTab
            = [addNovelOutlineEditorStylesActionByTab](TextParagraphType _from,
                                                       TextParagraphType _to) {
                  addNovelOutlineEditorStylesActionByTab("changing", _from, _to);
              };
        auto addNovelOutlineEditorStylesChangeByEnter
            = [addNovelOutlineEditorStylesActionByEnter](TextParagraphType _from,
                                                         TextParagraphType _to) {
                  addNovelOutlineEditorStylesActionByEnter("changing", _from, _to);
              };
        addNovelOutlineEditorStylesChangeByTab(TextParagraphType::SceneHeading,
                                               TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesChangeByEnter(TextParagraphType::SceneHeading,
                                                 TextParagraphType::ChapterHeading);
        addNovelOutlineEditorStylesChangeByTab(TextParagraphType::BeatHeading,
                                               TextParagraphType::BeatHeading);
        addNovelOutlineEditorStylesChangeByEnter(TextParagraphType::BeatHeading,
                                                 TextParagraphType::SceneHeading);
        addNovelOutlineEditorStylesChangeByTab(TextParagraphType::ChapterHeading,
                                               TextParagraphType::SceneHeading);
        addNovelOutlineEditorStylesChangeByEnter(TextParagraphType::ChapterHeading,
                                                 TextParagraphType::PartHeading);
        addNovelOutlineEditorStylesChangeByTab(TextParagraphType::PartHeading,
                                               TextParagraphType::ChapterHeading);
        addNovelOutlineEditorStylesChangeByEnter(TextParagraphType::PartHeading,
                                                 TextParagraphType::PartHeading);
        //
        auto addShortcut
            = [this](BusinessLayer::TextParagraphType _type, const QString& _shortcut) {
                  defaultSettings.insert(
                      QString("%1/shortcuts/%2")
                          .arg(kComponentsNovelOutlineEditorKey, BusinessLayer::toString(_type)),
                      QKeySequence(_shortcut).toString(QKeySequence::NativeText));
              };
        addShortcut(BusinessLayer::TextParagraphType::SceneHeading, "Ctrl+1");
        addShortcut(BusinessLayer::TextParagraphType::BeatHeading, "Ctrl+Shift+1");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading, "Ctrl+Space");
        addShortcut(BusinessLayer::TextParagraphType::PartHeading, "Ctrl+Shift+Space");
    }
    //
    // Параметры редактора романа
    //
    {
        auto addNovelEditorStylesAction
            = [this](const QString& _actionType, const QString& _actionKey, TextParagraphType _from,
                     TextParagraphType _to) {
                  defaultSettings.insert(
                      QString("%1/styles-%2/from-%3-by-%4")
                          .arg(kComponentsNovelEditorKey, _actionType, toString(_from), _actionKey),
                      toString(_to));
              };
        auto addNovelEditorStylesActionByTab
            = [addNovelEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                           TextParagraphType _to) {
                  addNovelEditorStylesAction(_actionType, "tab", _from, _to);
              };
        auto addNovelEditorStylesActionByEnter
            = [addNovelEditorStylesAction](const QString& _actionType, TextParagraphType _from,
                                           TextParagraphType _to) {
                  addNovelEditorStylesAction(_actionType, "enter", _from, _to);
              };
        //
        auto addNovelEditorStylesJumpByTab
            = [addNovelEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                  addNovelEditorStylesActionByTab("jumping", _from, _to);
              };
        auto addNovelEditorStylesJumpByEnter
            = [addNovelEditorStylesActionByEnter](TextParagraphType _from, TextParagraphType _to) {
                  addNovelEditorStylesActionByEnter("jumping", _from, _to);
              };
        addNovelEditorStylesJumpByTab(TextParagraphType::UnformattedText,
                                      TextParagraphType::UnformattedText);
        addNovelEditorStylesJumpByEnter(TextParagraphType::UnformattedText,
                                        TextParagraphType::UnformattedText);
        addNovelEditorStylesJumpByTab(TextParagraphType::SceneHeading, TextParagraphType::Text);
        addNovelEditorStylesJumpByEnter(TextParagraphType::SceneHeading, TextParagraphType::Text);
        addNovelEditorStylesJumpByTab(TextParagraphType::BeatHeading, TextParagraphType::Text);
        addNovelEditorStylesJumpByEnter(TextParagraphType::BeatHeading, TextParagraphType::Text);
        addNovelEditorStylesJumpByTab(TextParagraphType::Text, TextParagraphType::Text);
        addNovelEditorStylesJumpByEnter(TextParagraphType::Text, TextParagraphType::Text);
        addNovelEditorStylesJumpByTab(TextParagraphType::InlineNote, TextParagraphType::Text);
        addNovelEditorStylesJumpByEnter(TextParagraphType::InlineNote, TextParagraphType::Text);
        addNovelEditorStylesJumpByTab(TextParagraphType::ChapterHeading,
                                      TextParagraphType::SceneHeading);
        addNovelEditorStylesJumpByEnter(TextParagraphType::ChapterHeading,
                                        TextParagraphType::SceneHeading);
        addNovelEditorStylesJumpByTab(TextParagraphType::PartHeading,
                                      TextParagraphType::ChapterHeading);
        addNovelEditorStylesJumpByEnter(TextParagraphType::PartHeading,
                                        TextParagraphType::SceneHeading);
        //
        auto addNovelEditorStylesChangeByTab
            = [addNovelEditorStylesActionByTab](TextParagraphType _from, TextParagraphType _to) {
                  addNovelEditorStylesActionByTab("changing", _from, _to);
              };
        auto addNovelEditorStylesChangeByEnter
            = [addNovelEditorStylesActionByEnter](TextParagraphType _from, TextParagraphType _to) {
                  addNovelEditorStylesActionByEnter("changing", _from, _to);
              };
        addNovelEditorStylesChangeByTab(TextParagraphType::UnformattedText,
                                        TextParagraphType::UnformattedText);
        addNovelEditorStylesChangeByEnter(TextParagraphType::UnformattedText,
                                          TextParagraphType::UnformattedText);
        addNovelEditorStylesChangeByTab(TextParagraphType::SceneHeading, TextParagraphType::Text);
        addNovelEditorStylesChangeByEnter(TextParagraphType::SceneHeading,
                                          TextParagraphType::ChapterHeading);
        addNovelEditorStylesChangeByTab(TextParagraphType::BeatHeading, TextParagraphType::Text);
        addNovelEditorStylesChangeByEnter(TextParagraphType::BeatHeading, TextParagraphType::Text);
        addNovelEditorStylesChangeByTab(TextParagraphType::Text, TextParagraphType::Text);
        addNovelEditorStylesChangeByEnter(TextParagraphType::Text, TextParagraphType::SceneHeading);
        addNovelEditorStylesChangeByTab(TextParagraphType::InlineNote,
                                        TextParagraphType::InlineNote);
        addNovelEditorStylesChangeByEnter(TextParagraphType::InlineNote,
                                          TextParagraphType::InlineNote);
        addNovelEditorStylesChangeByTab(TextParagraphType::ChapterHeading,
                                        TextParagraphType::SceneHeading);
        addNovelEditorStylesChangeByEnter(TextParagraphType::ChapterHeading,
                                          TextParagraphType::PartHeading);
        addNovelEditorStylesChangeByTab(TextParagraphType::PartHeading,
                                        TextParagraphType::ChapterHeading);
        addNovelEditorStylesChangeByEnter(TextParagraphType::PartHeading,
                                          TextParagraphType::PartHeading);
        //
        auto addShortcut
            = [this](BusinessLayer::TextParagraphType _type, const QString& _shortcut) {
                  defaultSettings.insert(
                      QString("%1/shortcuts/%2")
                          .arg(kComponentsNovelEditorKey, BusinessLayer::toString(_type)),
                      QKeySequence(_shortcut).toString(QKeySequence::NativeText));
              };
        addShortcut(BusinessLayer::TextParagraphType::UnformattedText, "Ctrl+0");
        addShortcut(BusinessLayer::TextParagraphType::SceneHeading, "Ctrl+1");
        addShortcut(BusinessLayer::TextParagraphType::BeatHeading, "Ctrl+Shift+1");
        addShortcut(BusinessLayer::TextParagraphType::Text, "Ctrl+2");
        addShortcut(BusinessLayer::TextParagraphType::InlineNote, "Ctrl+Shift+0");
        addShortcut(BusinessLayer::TextParagraphType::ChapterHeading, "Ctrl+Space");
        addShortcut(BusinessLayer::TextParagraphType::PartHeading, "Ctrl+Shift+Space");
        //
        defaultSettings.insert(kComponentsNovelAvailableKey, true);
        //
        defaultSettings.insert(kComponentsNovelEditorDefaultTemplateKey, "manuscript_t_a4");
        defaultSettings.insert(kComponentsNovelEditorCorrectTextOnPageBreaksKey, false);
        //
        // Параметры навигатора
        //
        defaultSettings.insert(kComponentsNovelNavigatorShowBeatsKey, true);
        defaultSettings.insert(kComponentsNovelNavigatorShowBeatsInOutlineKey, true);
        defaultSettings.insert(kComponentsNovelNavigatorShowBeatsInTextKey, false);
        defaultSettings.insert(kComponentsNovelNavigatorShowSceneTextKey, true);
        defaultSettings.insert(kComponentsNovelNavigatorSceneTextLinesKey, 1);
        defaultSettings.insert(kComponentsNovelNavigatorCounterTypeKey, 0);
        //
        // Параметры карточек сценария
        //
        defaultSettings.insert(kComponentsNovelCardsLockCardsKey, true);
        defaultSettings.insert(kComponentsNovelCardsArrangeByRowsKey, true);
        defaultSettings.insert(kComponentsNovelCardsCardsSizeKey, 10);
        defaultSettings.insert(kComponentsNovelCardsCardsRatioKey, 2);
        defaultSettings.insert(kComponentsNovelCardsCardsSpacingKey, 20);
        defaultSettings.insert(kComponentsNovelCardsCardsInRowKey, -1);
    }


    defaultSettings.insert(kSystemUsernameKey, systemUserName());
}

QVariant SettingsStorage::Implementation::cachedValue(const QString& _key, bool& _ok) const
{
    _ok = cachedAppSettings.contains(_key);
    return cachedAppSettings.value(_key);
}

void SettingsStorage::Implementation::cacheValue(const QString& _key, const QVariant& _value)
{
    cachedAppSettings.insert(_key, _value);
}


// ****


SettingsStorage::~SettingsStorage() = default;

void SettingsStorage::setValue(const QString& _key, const QVariant& _value,
                               SettingsStorage::Type _type)
{
    if (d->isReadOnly) {
        return;
    }

    //
    // Проверяем, изменилось ли устанавливаемое значение
    //
    if ((_type == Type::Application
         && (d->pendingAppSettings.value(_key) == _value
             || d->appSettings.value(_key.toUtf8().toHex()) == _value))
        || (_type == Type::Session && d->sessionSettings.value(_key) == _value)) {
        return;
    }

    //
    // Если значение невалидно, то сбросим заданное значение
    //
    if (_value.isNull()) {
        d->cachedAppSettings.remove(_key);
        if (_type == Type::Application) {
            d->pendingAppSettings.remove(_key);
            d->appSettings.remove(_key.toUtf8().toHex());
        } else {
            d->sessionSettings.remove(_key);
        }
        return;
    }

    //
    // Кэшируем значение которое задано:
    // - для всего приложения, если сессия его не переопределяет
    // - для сессии, каждый раз
    //
    if ((_type == Type::Application && !d->sessionSettings.contains(_key))
        || (_type == Type::Session)) {
        d->cacheValue(_key, _value);
    }

    //
    // Сохраняем его в заданное хранилище
    //
    if (_type == Type::Application) {
        d->pendingAppSettings.insert(_key, _value);
    } else {
        d->sessionSettings[_key] = _value;
    }
}

void SettingsStorage::setValues(const QString& _valuesGroup, const QVariantMap& _values,
                                SettingsStorage::Type _type)
{
    if (d->isReadOnly) {
        return;
    }

    //
    // Кэшируем значение
    //
    d->cacheValue(_valuesGroup, _values);

    //
    // Сохраняем его в заданное хранилище
    //
    if (_type == Type::Application) {
        //
        // Очистим группу
        //
        {
            d->appSettings.beginGroup(_valuesGroup);
            d->appSettings.remove("");
            d->appSettings.endGroup();
        }

        //
        // Откроем группу
        //
        d->appSettings.beginGroup(_valuesGroup);

        //
        // Сохраним значения
        //
        for (const QString& key : _values.keys()) {
            d->cacheValue(key, _values.value(key));
            d->appSettings.setValue(key.toUtf8().toHex(), _values.value(key));
        }

        //
        // Закроем группу
        //
        d->appSettings.endGroup();

        //
        // Сохраняем изменения в файл
        //
        d->appSettings.sync();
    } else {
        Q_ASSERT_X(0, Q_FUNC_INFO, "Session settings can't save group of settings");
    }
}

QVariant SettingsStorage::value(const QString& _key, const QVariant& _defaultValue) const
{
    //
    // Пробуем получить значение из кэша
    //
    bool hasCachedValue = false;
    QVariant value = d->cachedValue(_key, hasCachedValue);

    //
    // Если в кэше нашлось нужное значение, вернём его
    //
    if (hasCachedValue) {
        return value;
    }

    //
    // Если в кэше нет, то смотрим в параметрах сессии
    //
    if (const auto iter = d->sessionSettings.find(_key); iter != d->sessionSettings.end()) {
        value = iter.value();
    }
    //
    // Если в сессии нет, то смотрим в настройках приложения
    //
    else {
        //
        // ... подготовленные к записи
        //
        if (const auto iter = d->pendingAppSettings.find(_key);
            iter != d->pendingAppSettings.end()) {
            value = iter.value();
        }
        //
        // ... сохранённые
        //
        else {
            value = d->appSettings.value(_key.toUtf8().toHex(), QVariant());
        }
    }

    //
    // Если удалось загрузить
    //
    if (!value.isNull()) {
        //
        // Сохраняем значение в кэш
        //
        d->cacheValue(_key, value);

        return value;
    }

    //
    // Если не удалось загрузить, то используем заданное значение по умолчанию
    //
    if (!_defaultValue.isNull()) {
        return _defaultValue;
    }

    //
    // В конце концов пробуем вытащить что-нибудь из предустановленных умолчальных значений
    //
    return d->defaultSettings.value(_key);
}

QVariantMap SettingsStorage::values(const QString& _valuesGroup)
{
    //
    // Пробуем получить значение из кэша
    //
    bool hasCachedValue = false;
    QVariantMap values = d->cachedValue(_valuesGroup, hasCachedValue).toMap();

    if (hasCachedValue) {
        return values;
    }

    //
    // NOTE: реализовать сохранение групп для сессии, тогда можно будет их тут извлекать
    //

    //
    // Если в кэше нет, то загружаем из параметров приложения
    //
    {
        //
        // Откроем группу для считывания
        //
        d->appSettings.beginGroup(_valuesGroup);

        //
        // Получим все ключи
        //
        QStringList keys = d->appSettings.childKeys();

        //
        // Получим все значения
        //
        for (const QString& key : keys) {
            values.insert(QByteArray::fromHex(key.toUtf8()), d->appSettings.value(key));
        }

        //
        // Закроем группу
        //
        d->appSettings.endGroup();
    }

    //
    // Сохраняем значение в кэш
    //
    d->cacheValue(_valuesGroup, values);

    return values;
}

void SettingsStorage::sync()
{
    Log::info("[SettingsStorage] Sync settings");

    //
    // Перенесём все параметры из временного хранилища в постоянное
    //
    for (auto iter = d->pendingAppSettings.cbegin(); iter != d->pendingAppSettings.cend(); ++iter) {
        d->appSettings.setValue(iter.key().toUtf8().toHex(), iter.value());
    }
    d->pendingAppSettings.clear();

    //
    // Пишем в файл
    //
    d->appSettings.sync();
}

void SettingsStorage::resetSession()
{
    //
    // Перед тем, как сбросить, нужно убрать из кэша все параметры, которые туда попали из сессии
    //
    for (auto iter = d->sessionSettings.begin(); iter != d->sessionSettings.end(); ++iter) {
        d->cachedAppSettings.remove(iter.key());
    }
    //
    // ... а только после этого очищаем параметры сессии
    //
    d->sessionSettings.clear();
}

QString SettingsStorage::accountName() const
{
    auto name = value(kAccountUserNameKey).toString();
    if (name.isEmpty()) {
        name = value(kSystemUsernameKey).toString();
    }
    return name;
}

QString SettingsStorage::accountEmail() const
{
    return value(kAccountEmailKey).toString();
}

QString SettingsStorage::documentFolderPath(const QString& _key) const
{
    QString folderPath = value(_key).toString();
    if (folderPath.isEmpty()) {
        folderPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    return folderPath;
}

QString SettingsStorage::documentFilePath(const QString& _key, const QString& _fileName) const
{
    const QString filePath = documentFolderPath(_key) + QDir::separator() + _fileName;
    return QDir::toNativeSeparators(filePath);
}

void SettingsStorage::setDocumentFolderPath(const QString& _key, const QString& _filePath)
{
    setValue(_key, QFileInfo(_filePath).absoluteDir().absolutePath(), Type::Application);
}

void SettingsStorage::resetToDefaults(bool _fullReset)
{
    //
    // Запрещаем писать настройки до перезагрузки приложения
    //
    d->isReadOnly = true;

    //
    // Стираем все параметры сессии
    //
    resetSession();

    //
    // Очищаем всё, что не успело записаться
    //
    d->pendingAppSettings.clear();

    //
    // Стираем все настройки
    //
    QHash<QString, QVariant> settings;
    if (!_fullReset) {
        //
        // ... если стирание не полное, то оставляем несколько параметров, которые по-сути не
        //     являются настройками приложения и пользователь не ожидает, что они будут удалены
        //
        for (const auto& key : d->appSettings.allKeys()) {
            const QString realKey = QByteArray::fromHex(key.toUtf8());
            if (realKey == kDeviceUuidKey || realKey == kApplicationProjectsKey
                || realKey.startsWith(kAccountGroupKey) || realKey.startsWith(kProjectGroupKey)
                || realKey.startsWith(kGitHubSyncGroupKey)
                || realKey.startsWith(QLatin1String("starc"))) {
                settings.insert(key, d->appSettings.value(key));
            }
        }
    }
    d->appSettings.clear();

    //
    // Восстанавливаем параметры, если есть необходимость
    //
    for (auto iter = settings.begin(); iter != settings.end(); ++iter) {
        d->appSettings.setValue(iter.key(), iter.value());
    }
    //
    // Форсим сохранение файла с настройками
    //
    d->appSettings.sync();
}

SettingsStorage::SettingsStorage()
    : d(new Implementation)
{
}

} // namespace DataStorageLayer
