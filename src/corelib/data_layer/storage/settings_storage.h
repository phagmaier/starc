#pragma once

#include <QVariant>
#include <QVariantMap>

#include <corelib_global.h>


/**
 * Правила именования настроек
 *
 * Приложение
 * application/key
 *
 * Проект
 * project/key - общее для всех
 * project/concrete/path/ - для конкретного проекта
 *
 * Система
 * system/key
 *
 * Виджеты
 * widgets/widget-name/key
 */

namespace DataStorageLayer {

namespace {
//
// Устройтво
//
const QString kDeviceGroupKey = "device";
//
// уникальный индентификатор устройства
const QString kDeviceUuidKey = kDeviceGroupKey + "/uuid";
//
// Приложение
//
const QString kApplicationGroupKey = "application";
//
// первый запуск приложения (false) или оно уже сконфигурировано (true)
const QString kApplicationConfiguredKey = kApplicationGroupKey + "/configured";
// язык приложения
const QString kApplicationLanguagedKey = kApplicationGroupKey + "/language";
const QString kApplicationLanguageQtVersionKey = kApplicationGroupKey + "/language-qt-version";
const QString kApplicationLanguagedFileKey = kApplicationGroupKey + "/language-file";
// тема приложения
const QString kApplicationThemeKey = kApplicationGroupKey + "/theme";
// параметры кастомной тема приложения
const QString kApplicationCustomThemeColorsKey = kApplicationGroupKey + "/custom-theme";
// масштаб приложения
const QString kApplicationScaleFactorKey = kApplicationGroupKey + "/scale-factor";
// использование компактного режима
const QString kApplicationDensityKey = kApplicationGroupKey + "/density";
// состояние и геометрия основного окна приложения
const QString kApplicationViewStateKey = kApplicationGroupKey + "-view/";
// включено ли автосохранение
const QString kApplicationUseAutoSaveKey = kApplicationGroupKey + "/autosave";
// включено ли сохранение резервных копий
const QString kApplicationSaveBackupsKey = kApplicationGroupKey + "/save-backups";
// папка в которую будут сохраняться резервные копии
const QString kApplicationBackupsFolderKey = kApplicationGroupKey + "/backups-folder";
// максимальное кол-во бекапов для сохранения
const QString kApplicationBackupsQtyKey = kApplicationGroupKey + "/backups-qty";
// показывать ли страницы текстовых документов
const QString kApplicationShowDocumentsPagesKey = kApplicationGroupKey + "/show-documents-pages";
// включены ли звуки печатной машинки при наборе текста
const QString kApplicationUseTypewriterSoundKey = kApplicationGroupKey + "/typewriter-sound";
// включена ли проверка орфографии
const QString kApplicationUseSpellCheckerKey = kApplicationGroupKey + "/use-spell-checker";
// словарь для проверки орфографии
const QString kApplicationSpellCheckerLanguageKey
    = kApplicationGroupKey + "/spell-checker-language";
// включена ли подсветка текущей строки для текстовых редакторов
const QString kApplicationHighlightCurrentLineKey
    = kApplicationGroupKey + "/highlight-current-line";
// включён ли режим фокусировки на текущем параграфе для текстовых редакторов
const QString kApplicationFocusCurrentParagraphKey
    = kApplicationGroupKey + "/focus-current-paragraph";
// включён ли режим печатной машинки для текстовых редакторов
const QString kApplicationUseTypewriterScrollingKey
    = kApplicationGroupKey + "/use-typewriter-scrolling";
// активна ли автозамена двух заглавных на одну
const QString kApplicationCorrectDoubleCapitalsKey
    = kApplicationGroupKey + "/correct-double-capitals";
// активна ли автозамена i на I
const QString kApplicationCapitalizeSingleILetterKey
    = kApplicationGroupKey + "/capitalize-single-i-letter";
// активна ли автозамена трёх точек на символ многоточия
const QString kApplicationReplaceThreeDotsWithEllipsisKey
    = kApplicationGroupKey + "/replace-three-dots-with-ellipsis";
// активны ли замены обычных кавычек на умные для текущего языка
const QString kApplicationSmartQuotesKey = kApplicationGroupKey + "/smart-quotes";
// заменять два тире на длинное тире
const QString kApplicationReplaceTwoDashesWithEmDashKey
    = kApplicationGroupKey + "/replace-two-dashes-with-em-dash";
// запретить вводить несколько пробелов подряд
const QString kApplicationAvoidMultipleSpacesKey = kApplicationGroupKey + "/avoid-multiple-spaces";
// доступен ли ИИ помощник
// список недавних проектов
const QString kApplicationProjectsKey = kApplicationGroupKey + "/projects";
//
// горячие клавиши
const QString kApplicationShortcutsKey = kApplicationGroupKey + "/shortcuts";
//
const QString kApplicationShortcutsImportKey = kApplicationShortcutsKey + "/import";
const QString kApplicationShortcutsCurrentDocumentExportKey
    = kApplicationShortcutsKey + "/current-document-export";
//
// дополнительные
const QString kApplicationAdvancedKey = kApplicationGroupKey + "/advanced";
// способ преобразования пикселей в миллиметры
const QString kApplicationAdvancedAccurateMetricsHandlingKey
    = kApplicationAdvancedKey + "/accurate-metrics-handling";
//
// логгирование
const QString kApplicationLoggingKey = kApplicationGroupKey + "/logging";
// уровень логгирования
const QString kApplicationLoggingLevelKey = kApplicationLoggingKey + "/level";

//
// Аккаунт
//
const QString kAccountGroupKey = "account";
//
// имейл
const QString kAccountEmailKey = kAccountGroupKey + "/email";
// имя пользователя
const QString kAccountUserNameKey = kAccountGroupKey + "/user-name";
// описание пользователя
const QString kAccountDescriptionKey = kAccountGroupKey + "/description";
// подписка пользователя на новости
const QString kAccountNewsletterLanguageKey = kAccountGroupKey + "/newsletter-language";
const QString kAccountNewsletterSubscribedKey = kAccountGroupKey + "/newsletter-subscribed";
// аватарка
const QString kAccountAvatarKey = kAccountGroupKey + "/avatar";

//
// Sync of the stories with a GitHub repository
//
const QString kGitHubSyncGroupKey = "github-sync";
//
// repository in the form "owner/name"
const QString kGitHubSyncRepositoryKey = kGitHubSyncGroupKey + "/repository";
// personal access token
const QString kGitHubSyncTokenKey = kGitHubSyncGroupKey + "/token";
// upload changed stories when the application is closing
const QString kGitHubSyncUploadOnExitKey = kGitHubSyncGroupKey + "/upload-on-exit";
// what was synced on this device, see GitHubSyncManager
const QString kGitHubSyncProjectsKey = kGitHubSyncGroupKey + "/projects";
// files of the repository the user didn't want to download at startup
const QString kGitHubSyncSkippedFilesKey = kGitHubSyncGroupKey + "/skipped-files";


//
// Проект
//
const QString kProjectGroupKey = "project";
//
// тип нового проекта
const QString kProjectTypeKey = kProjectGroupKey + "/type";
// папка сохранения проектов
const QString kProjectSaveFolderKey = kProjectGroupKey + "/save-folder";
// папка открытия проектов
const QString kProjectOpenFolderKey = kProjectGroupKey + "/open-folder";
// папка импорта проектов
const QString kProjectImportFolderKey = kProjectGroupKey + "/import-folder";
// папка экспорта проектов
const QString kProjectExportFolderKey = kProjectGroupKey + "/export-folder";
// путь до свойств конкретного проекта
QString projectKey(const QString& _path)
{
    return kProjectGroupKey + "/concrete/" + _path;
}
// путь до структуры проекта
QString projectStructureKey(const QString& _path)
{
    return projectKey(_path) + "/structure";
}
// был ли октрыт ли навигатор по проекту
QString projectStructureVisibleKey(const QString& _path)
{
    return projectKey(_path) + "/structure-visible";
}

//
// Система
//
const QString kSystemGroupKey = "system";
//
// системное имя пользователя
const QString kSystemUsernameKey = kSystemGroupKey + "/username";

//
// Компоненты
//
const QString kComponentsGroupKey = QStringLiteral("components");
//
// текст
const QString kComponentsSimpleTextKey = kComponentsGroupKey + QStringLiteral("/simple-text");
const QString kComponentsSimpleTextAvailableKey
    = kComponentsSimpleTextKey + QStringLiteral("/available");
// ... редактор
const QString kComponentsSimpleTextEditorKey = kComponentsSimpleTextKey + QStringLiteral("/editor");
const QString kComponentsSimpleTextEditorDefaultTemplateKey
    = kComponentsSimpleTextEditorKey + QStringLiteral("/default-template");
const QString kComponentsSimpleTextEditorCorrectTextOnPageBreaksKey
    = kComponentsSimpleTextEditorKey + QStringLiteral("/correct-text-on-page-breaks");
const QString kComponentsSimpleTextEditorShortcutsKey
    = kComponentsSimpleTextEditorKey + QStringLiteral("/shortcuts");
// ... навигатор
const QString kComponentsSimpleTextNavigatorKey
    = kComponentsSimpleTextKey + QStringLiteral("/navigator");
const QString kComponentsSimpleTextNavigatorShowSceneTextKey
    = kComponentsSimpleTextNavigatorKey + QStringLiteral("/show-scene-text");
const QString kComponentsSimpleTextNavigatorSceneTextLinesKey
    = kComponentsSimpleTextNavigatorKey + QStringLiteral("/scene-text-lines");
//
// сценарий
const QString kComponentsScreenplayKey = kComponentsGroupKey + QStringLiteral("/screenplay");
const QString kComponentsScreenplayAvailableKey
    = kComponentsScreenplayKey + QStringLiteral("/available");
// ... редактор тритмента
const QString kComponentsScreenplayTreatmentEditorKey
    = kComponentsScreenplayKey + QStringLiteral("/treatment");
const QString kComponentsScreenplayTreatmentEditorShortcutsKey
    = kComponentsScreenplayTreatmentEditorKey + QStringLiteral("/shortcuts");
// ... редактор текста
const QString kComponentsScreenplayEditorKey = kComponentsScreenplayKey + QStringLiteral("/editor");
const QString kComponentsScreenplayEditorDefaultTemplateKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/default-template");
const QString kComponentsScreenplayEditorShowSceneNumbersKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-scene-numbers");
const QString kComponentsScreenplayEditorShowSceneNumbersOnRightKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-scene-numbers-on-left");
const QString kComponentsScreenplayEditorShowSceneNumbersOnLeftKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-scene-number-on-right");
const QString kComponentsScreenplayEditorShowDialogueNumbersKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-dialogue-number");
const QString kComponentsScreenplayEditorContinueDialogueKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/continue-dialogue");
const QString kComponentsScreenplayEditorCorrectTextOnPageBreaksKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/correct-text-on-page-breaks");
const QString kComponentsScreenplayEditorSaveItemsFromTextKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/save-items-from-text");
const QString kComponentsScreenplayEditorShowHintsForAllItemsKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-hints-for-all-items");
const QString kComponentsScreenplayEditorShowHintsForPrimaryItemsKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-hints-for-primary-items");
const QString kComponentsScreenplayEditorShowHintsForSecondaryItemsKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-hints-for-secondary-items");
const QString kComponentsScreenplayEditorShowHintsForTertiaryItemsKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-hints-for-tertiary-items");
const QString kComponentsScreenplayEditorShowCharacterSuggestionsInEmptyBlockKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/show-characters-hints-in-empty-blocks");
const QString kComponentsScreenplayEditorUseOpenParenthesisInDialogueForParentheticalKey
    = kComponentsScreenplayEditorKey
    // в какой-то момент нам подсказали, что brackets это квадратные скобки [ ], поэтому мы
    // переименовали параметр и методы, но в самой настройке оставим первоначальный вид, чтобы не
    // ломать для тех, у кого этот параметр уже задан
    + QStringLiteral("/use-open-bracket-in-dialogue-for-parenthetical");
const QString kComponentsScreenplayEditorShortcutsKey
    = kComponentsScreenplayEditorKey + QStringLiteral("/shortcuts");
// ... навигатор
const QString kComponentsScreenplayNavigatorKey
    = kComponentsScreenplayKey + QStringLiteral("/navigator");
const QString kComponentsScreenplayNavigatorShowBeatsKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/show-beats");
const QString kComponentsScreenplayNavigatorShowBeatsInTreatmentKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/show-beats-in-treatment");
const QString kComponentsScreenplayNavigatorShowBeatsInScreenplayKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/show-beats-in-screenplay");
const QString kComponentsScreenplayNavigatorShowSceneNumberKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/show-scene-number");
const QString kComponentsScreenplayNavigatorShowSceneTextKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/show-scene-text");
const QString kComponentsScreenplayNavigatorSceneTextLinesKey
    = kComponentsScreenplayNavigatorKey + QStringLiteral("/scene-text-lines");
// ... хронометраж
const QString kComponentsScreenplayDurationKey
    = kComponentsScreenplayKey + QStringLiteral("/duration");
const QString kComponentsScreenplayDurationUseEighthsKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/use-eighths");
const QString kComponentsScreenplayDurationTypeKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/type");
const QString kComponentsScreenplayDurationByPageDurationKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/by-page-duration");
const QString kComponentsScreenplayDurationByCharactersCharactersKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/by-characters-characters");
const QString kComponentsScreenplayDurationByCharactersIncludeSpacesKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/by-characters-include-spaces");
const QString kComponentsScreenplayDurationByCharactersDurationKey
    = kComponentsScreenplayDurationKey + QStringLiteral("/by-characters-duration");
const QString kComponentsScreenplayDurationConfigurableSecondsPerParagraphForActionKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-paragraph-for-action");
const QString kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForActionKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-every50-for-action");
const QString kComponentsScreenplayDurationConfigurableSecondsPerParagraphForDialogueKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-paragraph-for-dialogue");
const QString kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForDialogueKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-every50-for-dialogue");
const QString kComponentsScreenplayDurationConfigurableSecondsPerParagraphForSceneHeadingKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-paragraph-for-scene-heading");
const QString kComponentsScreenplayDurationConfigurableSecondsPerEvery50ForSceneHeadingKey
    = kComponentsScreenplayDurationKey
    + QStringLiteral("/configurable-secsonds-per-every50-for-scene-heading");
// ... редактор карточек
const QString kComponentsScreenplayCardsKey = kComponentsScreenplayKey + QStringLiteral("/cards");
const QString kComponentsScreenplayCardsSplitterStateKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/splitter-state");
const QString kComponentsScreenplayCardsLockCardsKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/lock-cards");
const QString kComponentsScreenplayCardsArrangeByRowsKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/arrange-by-rows");
const QString kComponentsScreenplayCardsCardsSizeKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/cards-size");
const QString kComponentsScreenplayCardsCardsRatioKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/cards-ratio");
const QString kComponentsScreenplayCardsCardsSpacingKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/cards-spacing");
const QString kComponentsScreenplayCardsCardsInRowKey
    = kComponentsScreenplayCardsKey + QStringLiteral("/cards-in-row");
// ... редактор таймлайна
const QString kComponentsScreenplayTimelineKey
    = kComponentsScreenplayKey + QStringLiteral("/timeline");
const QString kComponentsScreenplayTimelineSplitterStateKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/splitter-state");
const QString kComponentsScreenplayTimelineArrangeHorizontalKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/arrange-horiontal");
const QString kComponentsScreenplayTimelineCardsSizeKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/cards-size");
const QString kComponentsScreenplayTimelineCardsRatioKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/cards-ratio");
const QString kComponentsScreenplayTimelineCardsSpacingKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/cards-spacing");
const QString kComponentsScreenplayTimelineScaleKey
    = kComponentsScreenplayTimelineKey + QStringLiteral("/timeline-scale");
// ... статистика
const QString kComponentsScreenplayStatisticsKey
    = kComponentsScreenplayKey + QStringLiteral("/statistics");
const QString kComponentsScreenplayStatisticsSplitterStateKey
    = kComponentsScreenplayStatisticsKey + QStringLiteral("/splitter-state");
//
// редактор отношений персонажей
const QString kComponentsCharactersRelationsKey
    = kComponentsGroupKey + QStringLiteral("/characters-relations");
const QString kComponentsCharactersRelationsFocusCurrentCharacterKey
    = kComponentsCharactersRelationsKey + QStringLiteral("/focus-current-character");
const QString kComponentsCharactersRelationsSplitterStateKey
    = kComponentsCharactersRelationsKey + QStringLiteral("/splitter-state");
//
// редактор карты локаций
const QString kComponentsLocationsMapKey = kComponentsGroupKey + QStringLiteral("/locations-map");
const QString kComponentsLocationsMapFocusCurrentLocationKey
    = kComponentsLocationsMapKey + QStringLiteral("/focus-current-location");
const QString kComponentsLocationsMapSplitterStateKey
    = kComponentsLocationsMapKey + QStringLiteral("/splitter-state");
//
// редактор карты миров
const QString kComponentsWorldsMapKey = kComponentsGroupKey + QStringLiteral("/worlds-map");
const QString kComponentsWorldsMapFocusCurrentWorldKey
    = kComponentsWorldsMapKey + QStringLiteral("/focus-current-world");
const QString kComponentsWorldsMapSplitterStateKey
    = kComponentsWorldsMapKey + QStringLiteral("/splitter-state");
//
// редактор ментальных карт
const QString kComponentsMindMapKey = kComponentsGroupKey + QStringLiteral("/mind-map");
const QString kComponentsMindMapFocusCurrentNodeKey
    = kComponentsMindMapKey + QStringLiteral("/focus-current-node");
const QString kComponentsMindMapiAssistantEnabledKey
    = kComponentsMindMapKey + QStringLiteral("/ai-assistant-enabled");
const QString kComponentsMindMapSplitterStateKey
    = kComponentsMindMapKey + QStringLiteral("/splitter-state");

} // namespace

/**
 * @brief Хранилище настроек
 */
class CORE_LIBRARY_EXPORT SettingsStorage
{
public:
    /**
     * @brief Как хранить заданный параметр
     */
    enum class Type {
        Application, //!< Для всего приложения
        Session, //!< Для текущей сессии
    };

public:
    ~SettingsStorage();

    /**
     * @brief Сохранить значение с заданным ключём
     */
    void setValue(const QString& _key, const QVariant& _value, Type _type);

    /**
     * @brief Сохранить карту параметров
     */
    void setValues(const QString& _valuesGroup, const QVariantMap& _values, Type _type);

    /**
     * @brief Получить значение по ключу
     */
    QVariant value(const QString& _key, const QVariant& _defaultValue = {}) const;

    /**
     * @brief Получить группу значений
     */
    QVariantMap values(const QString& _valuesGroup);

    /**
     * @brief Сохранить настройки
     */
    void sync();

    /**
     * @brief Сбросить настройки сессии
     */
    void resetSession();


    //
    // Вспомогательные функции для работы с данными о пользователе
    //

    /**
     * @brief Получить имя пользователя (если авторизован - имя пользователя в облаке,
     *        если не авторизован - имя пользователя в системе)
     */
    QString accountName() const;

    /**
     * @brief Получить имейл пользователя (если не авторизован возвращает пустое значение)
     */
    QString accountEmail() const;


    //
    // Вспомогательные функции для работы с путями к специальным папкам и файлам в них
    //

    /**
     * @brief Получить путь к папке с документами для сохранения по заданному ключу
     */
    QString documentFolderPath(const QString& _key) const;

    /**
     * @brief Получить путь к файлу в папке с документами для заданного ключа и имени файла
     */
    QString documentFilePath(const QString& _key, const QString& _fileName) const;

    /**
     * @brief Сохранить путь к папке с документами по заданному ключу и пути файла из этой папки
     */
    void setDocumentFolderPath(const QString& _key, const QString& _filePath);


    /**
     * @brief Сбросить все заданные пользователем настройки (по-сути удаляем файл с настройками)
     */
    void resetToDefaults(bool _fullReset);

private:
    SettingsStorage();
    friend class StorageFacade;
    //
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace DataStorageLayer
