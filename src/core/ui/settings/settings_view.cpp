#include "settings_view.h"

#include "widgets/theme_preview.h"

#include <business_layer/chronometry/chronometer.h>
#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/simple_text_template.h>
#include <business_layer/templates/templates_facade.h>
#include <include/custom_events.h>
#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/card/card.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/context_menu/context_menu.h>
#include <ui/widgets/icon_button/icon_button.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/radio_button/radio_button.h>
#include <ui/widgets/radio_button/radio_button_group.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/slider/slider.h>
#include <ui/widgets/text_field/text_field.h>
#include <ui/widgets/toggle/toggle.h>
#include <ui/widgets/tree/tree.h>
#include <ui/widgets/tree/tree_delegate.h>
#include <ui/widgets/tree/tree_header_view.h>
#include <utils/helpers/ui_helper.h>

#include <QAction>
#include <QFileDialog>
#include <QGridLayout>
#include <QLocale>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QVariantAnimation>
#include <QtMath>


namespace Ui {

namespace {

/**
 * @brief Сформиовать компоновщик для строки настроек
 */
QHBoxLayout* makeLayout()
{
    auto layout = new QHBoxLayout;
    layout->setContentsMargins({});
    layout->setSpacing(0);
    return layout;
};

/**
 * @brief Карта соответствия названий языков для проверки орфографии с их кодами
 * @link https://github.com/wooorm/dictionaries/
 * @link https://cgit.freedesktop.org/libreoffice/dictionaries/tree/
 * @link https://www.softmaker.com/en/dictionaries
 */
const QVector<QString> kSpellCheckerLanguagesNameToCode = {
    "af",    "an",         "ar",          "ast",   "az",       "be",    "bg",    "bn",    "bo",
    "br",    "bs",         "ca-valencia", "ca",    "cs",       "cy",    "da",    "de-AT", "de-CH",
    "de",    "el-polyton", "el",          "en-AU", "en-CA",    "en-GB", "en-NZ", "en-ZA", "en",
    "eo",    "es-AR",      "es-BO",       "es-CL", "es-CO",    "es-CR", "es-CU", "es-DO", "es-EC",
    "es-GT", "es-HN",      "es-MX",       "es-NI", "es-PA",    "es-PE", "es-PH", "es-PR", "es-PY",
    "es-SV", "es-US",      "es-UY",       "es-VE", "es",       "et",    "eu",    "fa",    "fo",
    "fr",    "fur",        "fy",          "ga",    "gd",       "gl",    "gu",    "gug",   "he",
    "hi",    "hr",         "hu",          "hy",    "hyw",      "ia",    "id",    "is",    "it",
    "ka",    "kk",         "kmr",         "ko",    "la",       "lb",    "lo",    "lt",    "ltg",
    "lv",    "mk",         "mn",          "mt",    "nb",       "nds",   "ne",    "nl",    "nn",
    "oc",    "pl",         "pt-BR",       "pt",    "qu",       "ro",    "ru-yo", "ru",    "rw",
    "si",    "sk",         "sl",          "sq",    "sr-Latn",  "sr",    "sv-FI", "sv",    "sw",
    "ta",    "te",         "th",          "tk",    "tlh-Latn", "tlh",   "tr",    "uk",    "vi"
};

/**
 * @brief Индекс для сохранения в модели информации о коде языка
 */
const int kSpellCheckerLanguageCodeRole = Qt::UserRole + 1;

/**
 * @brief Построить модель для всех доступных справочников проверки орфографии
 */
QStandardItemModel* buildSpellCheckerLanguagesModel(QObject* _parent)
{
    auto model = new QStandardItemModel(_parent);
    for (const auto& language : kSpellCheckerLanguagesNameToCode) {
        auto item = new QStandardItem(language);
        item->setData(language, kSpellCheckerLanguageCodeRole);
        model->appendRow(item);
    }
    return model;
}

/**
 * @brief Построить модель типов абзацев для модуля
 */
QStringListModel* buildParagraphTypesModel(QObject* _parent, QStringListModel* _model,
                                           const QVector<BusinessLayer::TextParagraphType>& _types)
{
    using namespace BusinessLayer;
    QStringList paragraphTypes;
    for (const auto& type : _types) {
        paragraphTypes.append(toDisplayString(type));
    }

    if (!_model) {
        _model = new QStringListModel(_parent);
    }
    _model->setStringList(paragraphTypes);

    return _model;
}
QStringListModel* buildSimpleTextParagraphTypesModel(QObject* _parent,
                                                     QStringListModel* _model = nullptr)
{
    using namespace BusinessLayer;
    return buildParagraphTypesModel(_parent, _model,
                                    {
                                        TextParagraphType::ChapterHeading1,
                                        TextParagraphType::ChapterHeading2,
                                        TextParagraphType::ChapterHeading3,
                                        TextParagraphType::ChapterHeading4,
                                        TextParagraphType::ChapterHeading5,
                                        TextParagraphType::ChapterHeading6,
                                        TextParagraphType::Text,
                                        TextParagraphType::InlineNote,
                                    });
}
QStringListModel* buildScreenplayParagraphTypesModel(QObject* _parent,
                                                     QStringListModel* _model = nullptr)
{
    using namespace BusinessLayer;
    return buildParagraphTypesModel(_parent, _model,
                                    {
                                        TextParagraphType::SceneHeading,
                                        TextParagraphType::SceneCharacters,
                                        TextParagraphType::BeatHeading,
                                        TextParagraphType::Action,
                                        TextParagraphType::Character,
                                        TextParagraphType::Parenthetical,
                                        TextParagraphType::Dialogue,
                                        TextParagraphType::Lyrics,
                                        TextParagraphType::Transition,
                                        TextParagraphType::Shot,
                                        TextParagraphType::InlineNote,
                                        TextParagraphType::SequenceHeading,
                                        TextParagraphType::ActHeading,
                                        TextParagraphType::UnformattedText,
                                    });
}

/**
 * @brief Колонки таблицы горячих клавиш специализированного редактора текста
 */
enum TextEditorShortcuts {
    Type = 0,
    Shortcut,
    JumpByTab,
    JumpByEnter,
    ChangeByTab,
    ChangeByEnter,
};

} // namespace

class SettingsView::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    /**
     * @brief Настроить карточку параметров приложения
     */
    void initApplicationCard();

    /**
     * @brief Настроить карточку параметров компонентов
     */
    void initSimpleTextCard();
    void initScreenplayCard();

    /**
     * @brief Настроить карточку горячих клавиш
     */
    void initShortcutsCard();

    /**
     * @brief Настроить карточку экспериментальных опций
     */
    void initAdvancedCard();

    /**
     * @brief Проскролить представление до заданного виджета
     */
    void scrollToTitle(AbstractLabel* title);

    /**
     * @brief Обновить размеры колонок в таблицах
     */
    void updateTablesGeometry();


    QScrollArea* content = nullptr;
    QVariantAnimation scrollAnimation;
    ContextMenu* contextMenu = nullptr;

    QVariantAnimation colorAnimation;
    AbstractLabel* colorableTitle = nullptr;

    //
    // Application
    //
    Card* applicationCard = nullptr;
    QGridLayout* applicationCardLayout = nullptr;
    H5Label* applicationTitle = nullptr;
    //
    // ... Common
    //
    Body1Label* language = nullptr;
    Button* changeLanuage = nullptr;
    //
    // ... User interface
    //
    H6Label* applicationUserInterfaceTitle = nullptr;
    QHBoxLayout* applicationThemesLayout = nullptr;
    ThemePreview* lightTheme = nullptr;
    ThemePreview* darkAndLightTheme = nullptr;
    ThemePreview* darkTheme = nullptr;
    ThemePreview* customTheme = nullptr;
    Body1Label* scaleFactorTitle = nullptr;
    Slider* scaleFactor = nullptr;
    Body2Label* scaleFactorSmallInfo = nullptr;
    Body2Label* scaleFactorBigInfo = nullptr;
    CheckBox* isCompact = nullptr;
    //
    // ... Save changes & backups
    //
    H6Label* applicationSaveAndBackupTitle = nullptr;
    CheckBox* autoSave = nullptr;
    CheckBox* saveBackups = nullptr;
    TextField* backupsFolderPath = nullptr;
    TextField* backupsQty = nullptr;
    //
    // ... Sync with GitHub
    //
    H6Label* applicationGitHubSyncTitle = nullptr;
    TextField* gitHubSyncRepository = nullptr;
    TextField* gitHubSyncToken = nullptr;
    CheckBox* gitHubSyncUploadOnExit = nullptr;
    //
    // ... Text editing
    //
    H6Label* applicationTextEditingTitle = nullptr;
    CheckBox* showDocumentsPages = nullptr;
    CheckBox* useTypewriterSound = nullptr;
    CheckBox* useSpellChecker = nullptr;
    ComboBox* spellCheckerLanguage = nullptr;
    QStandardItemModel* spellCheckerLanguagesModel = nullptr;
    IconButton* spellCheckerUserDictionary = nullptr;
    CheckBox* highlightCurrentLine = nullptr;
    CheckBox* focusCurrentParagraph = nullptr;
    CheckBox* useTypewriterScrolling = nullptr;
    Subtitle1Label* applicationTextEditingSubtitle = nullptr;
    CheckBox* correctDoubleCapitals = nullptr;
    CheckBox* capitalizeSingleILetter = nullptr;
    CheckBox* replaceThreeDotsWithEllipsis = nullptr;
    CheckBox* useSmartQuotes = nullptr;
    CheckBox* replaceTwoDashesWithEmDash = nullptr;
    CheckBox* avoidMultipleSpaces = nullptr;
    //
    //
    //
    int applicationCardBottomSpacerIndex = 0;

    H5Label* componentsTitle = nullptr;

    //
    // Comonents
    //
    // Simple text
    //
    Card* simpleTextCard = nullptr;
    QGridLayout* simpleTextCardLayout = nullptr;
    H5Label* simpleTextTitle = nullptr;
    Toggle* simpleTextAvailable = nullptr;
    //
    // ... Simple text editor
    //
    H6Label* simpleTextEditorTitle = nullptr;
    ComboBox* simpleTextEditorDefaultTemplate = nullptr;
    IconButton* simpleTextEditorDefaultTemplateOptions = nullptr;
    //
    // ... Simple text navigator
    //
    H6Label* simpleTextNavigatorTitle = nullptr;
    CheckBox* simpleTextNavigatorShowSceneText = nullptr;
    RadioButton* simpleTextNavigatorSceneDescriptionLines1 = nullptr;
    RadioButton* simpleTextNavigatorSceneDescriptionLines2 = nullptr;
    RadioButton* simpleTextNavigatorSceneDescriptionLines3 = nullptr;
    RadioButton* simpleTextNavigatorSceneDescriptionLines4 = nullptr;
    RadioButton* simpleTextNavigatorSceneDescriptionLines5 = nullptr;
    //
    int simpleTextCardBottomSpacerIndex = 0;
    //
    // Screenplay
    //
    Card* screenplayCard = nullptr;
    QGridLayout* screenplayCardLayout = nullptr;
    H5Label* screenplayTitle = nullptr;
    Toggle* screenplayAvailable = nullptr;
    //
    // ... Screenplay editor
    //
    H6Label* screenplayEditorTitle = nullptr;
    ComboBox* screenplayEditorDefaultTemplate = nullptr;
    IconButton* screenplayEditorDefaultTemplateOptions = nullptr;
    CheckBox* screenplayEditorShowSceneNumber = nullptr;
    CheckBox* screenplayEditorShowSceneNumberOnLeft = nullptr;
    CheckBox* screenplayEditorShowSceneNumberOnRight = nullptr;
    CheckBox* screenplayEditorShowDialogueNumber = nullptr;
    CheckBox* screenplayEditorContinueDialogue = nullptr;
    CheckBox* screenplayEditorCorrectTextOnPageBreaks = nullptr;
    CheckBox* screenplayEditorSaveItemsFromText = nullptr;
    CheckBox* screenplayEditorShowHintsForAllItems = nullptr;
    CheckBox* screenplayEditorShowHintsForPrimaryItems = nullptr;
    CheckBox* screenplayEditorShowHintsForSecondaryItems = nullptr;
    CheckBox* screenplayEditorShowHintsForTertiaryItems = nullptr;
    CheckBox* screenplayEditorShowCharacterSuggestionsInEmptyBlock = nullptr;
    CheckBox* screenplayEditorUseOpenParenthesisInDialogueForParenthetical = nullptr;
    //
    // ... Screenplay navigator
    //
    H6Label* screenplayNavigatorTitle = nullptr;
    CheckBox* screenplayNavigatorShowBeats = nullptr;
    CheckBox* screenplayNavigatorShowBeatsInTreatment = nullptr;
    CheckBox* screenplayNavigatorShowBeatsInScreenplay = nullptr;
    CheckBox* screenplayNavigatorShowSceneNumber = nullptr;
    CheckBox* screenplayNavigatorShowSceneText = nullptr;
    RadioButton* screenplayNavigatorSceneDescriptionLines1 = nullptr;
    RadioButton* screenplayNavigatorSceneDescriptionLines2 = nullptr;
    RadioButton* screenplayNavigatorSceneDescriptionLines3 = nullptr;
    RadioButton* screenplayNavigatorSceneDescriptionLines4 = nullptr;
    RadioButton* screenplayNavigatorSceneDescriptionLines5 = nullptr;
    //
    // ... Screenplay duration
    //
    H6Label* screenplayDurationTitle = nullptr;
    //
    RadioButton* screenplayDurationByPage = nullptr;
    TextField* screenplayDurationByPagePage = nullptr;
    TextField* screenplayDurationByPageDuration = nullptr;
    //
    RadioButton* screenplayDurationByCharacters = nullptr;
    TextField* screenplayDurationByCharactersCharacters = nullptr;
    CheckBox* screenplayDurationByCharactersIncludingSpaces = nullptr;
    TextField* screenplayDurationByCharactersDuration = nullptr;
    //
    RadioButton* screenplayDurationConfigurable = nullptr;
    Body1Label* screenplayDurationConfigurableForActionLabel = nullptr;
    TextField* screenplayDurationConfigurablePerParagraphForAction = nullptr;
    Body1Label* screenplayDurationConfigurableForActionPlus = nullptr;
    TextField* screenplayDurationConfigurablePerEvery50ForAction = nullptr;
    Body1Label* screenplayDurationConfigurableForDialogueLabel = nullptr;
    TextField* screenplayDurationConfigurablePerParagraphForDialogue = nullptr;
    Body1Label* screenplayDurationConfigurableForDialoguePlus = nullptr;
    TextField* screenplayDurationConfigurablePerEvery50ForDialogue = nullptr;
    Body1Label* screenplayDurationConfigurableForSceneHeadingLabel = nullptr;
    TextField* screenplayDurationConfigurablePerParagraphForSceneHeading = nullptr;
    Body1Label* screenplayDurationConfigurableForSceneHeadingPlus = nullptr;
    TextField* screenplayDurationConfigurablePerEvery50ForSceneHeading = nullptr;
    //
    RadioButton* screenplayDurationUseEighths = nullptr;
    //
    int screenplayCardBottomSpacerIndex = 0;

    //
    // Shortcuts
    //
    Card* shortcutsCard = nullptr;
    QGridLayout* shortcutsCardLayout = nullptr;
    //
    H5Label* shortcutsTitle = nullptr;
    //
    // ... Appliction
    //
    H6Label* shortcutsForApplicationTitle = nullptr;
    Tree* shortcutsForApplication = nullptr;
    HierarchicalModel* shortcutsForApplicationModel = nullptr;
    //
    // ... Simple text
    //
    H6Label* shortcutsForSimpleTextTitle = nullptr;
    Tree* shortcutsForSimpleText = nullptr;
    HierarchicalModel* shortcutsForSimpleTextModel = nullptr;
    QStringListModel* simpleTextParagraphTypesModel = nullptr;
    ComboBoxItemDelegate* simpleTextParagraphAddTypeDelegate = nullptr;
    ComboBoxItemDelegate* simpleTextParagraphChangeTypeDelegate = nullptr;
    //
    // ... Screenplay
    //
    H6Label* shortcutsForScreenplayTitle = nullptr;
    Tree* shortcutsForScreenplay = nullptr;
    HierarchicalModel* shortcutsForScreenplayModel = nullptr;
    QStringListModel* screenplayParagraphTypesModel = nullptr;
    ComboBoxItemDelegate* screenplayParagraphAddTypeDelegate = nullptr;
    ComboBoxItemDelegate* screenplayParagraphChangeTypeDelegate = nullptr;
    //
    int shortcutsCardBottomSpacerIndex = 0;

    //
    // Advanced options
    //
    Card* advancedCard = nullptr;
    QGridLayout* advancedCardLayout = nullptr;
    H5Label* advancedTitle = nullptr;
    CheckBox* advancedAccurateMetricsHandling = nullptr;
    CheckBox* advancedUseExtendedLogging = nullptr;
};

SettingsView::Implementation::Implementation(QWidget* _parent)
    : content(UiHelper::createScrollArea(_parent, false, true))
    , contextMenu(new ContextMenu(_parent))
    //
    , applicationCard(new Card(content))
    , applicationCardLayout(new QGridLayout)
    , applicationTitle(new H5Label(applicationCard))
    , language(new Body1Label(applicationCard))
    , changeLanuage(new Button(applicationCard))
    , applicationUserInterfaceTitle(new H6Label(applicationCard))
    , applicationThemesLayout(new QHBoxLayout)
    , lightTheme(new ThemePreview(applicationCard))
    , darkAndLightTheme(new ThemePreview(applicationCard))
    , darkTheme(new ThemePreview(applicationCard))
    , customTheme(new ThemePreview(applicationCard))
    , scaleFactorTitle(new Body1Label(applicationCard))
    , scaleFactor(new Slider(applicationCard))
    , scaleFactorSmallInfo(new Body2Label(applicationCard))
    , scaleFactorBigInfo(new Body2Label(applicationCard))
    , isCompact(new CheckBox(applicationCard))
    , applicationSaveAndBackupTitle(new H6Label(applicationCard))
    , autoSave(new CheckBox(applicationCard))
    , saveBackups(new CheckBox(applicationCard))
    , backupsFolderPath(new TextField(applicationCard))
    , backupsQty(new TextField(applicationCard))
    , applicationGitHubSyncTitle(new H6Label(applicationCard))
    , gitHubSyncRepository(new TextField(applicationCard))
    , gitHubSyncToken(new TextField(applicationCard))
    , gitHubSyncUploadOnExit(new CheckBox(applicationCard))
    , applicationTextEditingTitle(new H6Label(applicationCard))
    , showDocumentsPages(new CheckBox(applicationCard))
    , useTypewriterSound(new CheckBox(applicationCard))
    , useSpellChecker(new CheckBox(applicationCard))
    , spellCheckerLanguage(new ComboBox(applicationCard))
    , spellCheckerLanguagesModel(buildSpellCheckerLanguagesModel(spellCheckerLanguage))
    , spellCheckerUserDictionary(new IconButton(applicationCard))
    , highlightCurrentLine(new CheckBox(applicationCard))
    , focusCurrentParagraph(new CheckBox(applicationCard))
    , useTypewriterScrolling(new CheckBox(applicationCard))
    , applicationTextEditingSubtitle(new Subtitle1Label(applicationCard))
    , correctDoubleCapitals(new CheckBox(applicationCard))
    , capitalizeSingleILetter(new CheckBox(applicationCard))
    , replaceThreeDotsWithEllipsis(new CheckBox(applicationCard))
    , useSmartQuotes(new CheckBox(applicationCard))
    , replaceTwoDashesWithEmDash(new CheckBox(applicationCard))
    , avoidMultipleSpaces(new CheckBox(applicationCard))
    //
    , componentsTitle(new H5Label(content))
    //
    , simpleTextCard(new Card(content))
    , simpleTextCardLayout(new QGridLayout)
    , simpleTextTitle(new H5Label(simpleTextCard))
    , simpleTextAvailable(new Toggle(simpleTextCard))
    , simpleTextEditorTitle(new H6Label(simpleTextCard))
    , simpleTextEditorDefaultTemplate(new ComboBox(simpleTextCard))
    , simpleTextEditorDefaultTemplateOptions(new IconButton(simpleTextCard))
    , simpleTextNavigatorTitle(new H6Label(simpleTextCard))
    , simpleTextNavigatorShowSceneText(new CheckBox(simpleTextCard))
    , simpleTextNavigatorSceneDescriptionLines1(new RadioButton(simpleTextCard))
    , simpleTextNavigatorSceneDescriptionLines2(new RadioButton(simpleTextCard))
    , simpleTextNavigatorSceneDescriptionLines3(new RadioButton(simpleTextCard))
    , simpleTextNavigatorSceneDescriptionLines4(new RadioButton(simpleTextCard))
    , simpleTextNavigatorSceneDescriptionLines5(new RadioButton(simpleTextCard))
    //
    , screenplayCard(new Card(content))
    , screenplayCardLayout(new QGridLayout)
    , screenplayTitle(new H5Label(screenplayCard))
    , screenplayAvailable(new Toggle(screenplayCard))
    , screenplayEditorTitle(new H6Label(screenplayCard))
    , screenplayEditorDefaultTemplate(new ComboBox(screenplayCard))
    , screenplayEditorDefaultTemplateOptions(new IconButton(screenplayCard))
    , screenplayEditorShowSceneNumber(new CheckBox(screenplayCard))
    , screenplayEditorShowSceneNumberOnLeft(new CheckBox(screenplayCard))
    , screenplayEditorShowSceneNumberOnRight(new CheckBox(screenplayCard))
    , screenplayEditorShowDialogueNumber(new CheckBox(screenplayCard))
    , screenplayEditorContinueDialogue(new CheckBox(screenplayCard))
    , screenplayEditorCorrectTextOnPageBreaks(new CheckBox(screenplayCard))
    , screenplayEditorSaveItemsFromText(new CheckBox(screenplayCard))
    , screenplayEditorShowHintsForAllItems(new CheckBox(screenplayCard))
    , screenplayEditorShowHintsForPrimaryItems(new CheckBox(screenplayCard))
    , screenplayEditorShowHintsForSecondaryItems(new CheckBox(screenplayCard))
    , screenplayEditorShowHintsForTertiaryItems(new CheckBox(screenplayCard))
    , screenplayEditorShowCharacterSuggestionsInEmptyBlock(new CheckBox(screenplayCard))
    , screenplayEditorUseOpenParenthesisInDialogueForParenthetical(new CheckBox(screenplayCard))
    , screenplayNavigatorTitle(new H6Label(screenplayCard))
    , screenplayNavigatorShowBeats(new CheckBox(screenplayCard))
    , screenplayNavigatorShowBeatsInTreatment(new CheckBox(screenplayCard))
    , screenplayNavigatorShowBeatsInScreenplay(new CheckBox(screenplayCard))
    , screenplayNavigatorShowSceneNumber(new CheckBox(screenplayCard))
    , screenplayNavigatorShowSceneText(new CheckBox(screenplayCard))
    , screenplayNavigatorSceneDescriptionLines1(new RadioButton(screenplayCard))
    , screenplayNavigatorSceneDescriptionLines2(new RadioButton(screenplayCard))
    , screenplayNavigatorSceneDescriptionLines3(new RadioButton(screenplayCard))
    , screenplayNavigatorSceneDescriptionLines4(new RadioButton(screenplayCard))
    , screenplayNavigatorSceneDescriptionLines5(new RadioButton(screenplayCard))
    , screenplayDurationTitle(new H6Label(screenplayCard))
    , screenplayDurationByPage(new RadioButton(screenplayCard))
    , screenplayDurationByPagePage(new TextField(screenplayCard))
    , screenplayDurationByPageDuration(new TextField(screenplayCard))
    , screenplayDurationByCharacters(new RadioButton(screenplayCard))
    , screenplayDurationByCharactersCharacters(new TextField(screenplayCard))
    , screenplayDurationByCharactersIncludingSpaces(new CheckBox(screenplayCard))
    , screenplayDurationByCharactersDuration(new TextField(screenplayCard))
    , screenplayDurationConfigurable(new RadioButton(screenplayCard))
    , screenplayDurationConfigurableForActionLabel(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerParagraphForAction(new TextField(screenplayCard))
    , screenplayDurationConfigurableForActionPlus(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerEvery50ForAction(new TextField(screenplayCard))
    , screenplayDurationConfigurableForDialogueLabel(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerParagraphForDialogue(new TextField(screenplayCard))
    , screenplayDurationConfigurableForDialoguePlus(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerEvery50ForDialogue(new TextField(screenplayCard))
    , screenplayDurationConfigurableForSceneHeadingLabel(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerParagraphForSceneHeading(new TextField(screenplayCard))
    , screenplayDurationConfigurableForSceneHeadingPlus(new Body1Label(screenplayCard))
    , screenplayDurationConfigurablePerEvery50ForSceneHeading(new TextField(screenplayCard))
    , screenplayDurationUseEighths(new RadioButton(screenplayCard))
    //
    , shortcutsCard(new Card(content))
    , shortcutsCardLayout(new QGridLayout)
    , shortcutsTitle(new H5Label(shortcutsCard))
    //
    , shortcutsForApplicationTitle(new H6Label(shortcutsCard))
    , shortcutsForApplication(new Tree(shortcutsCard))
    //
    , shortcutsForSimpleTextTitle(new H6Label(shortcutsCard))
    , shortcutsForSimpleText(new Tree(shortcutsCard))
    , simpleTextParagraphTypesModel(buildSimpleTextParagraphTypesModel(shortcutsForSimpleText))
    , simpleTextParagraphAddTypeDelegate(
          new ComboBoxItemDelegate(shortcutsForSimpleText, simpleTextParagraphTypesModel))
    , simpleTextParagraphChangeTypeDelegate(
          new ComboBoxItemDelegate(shortcutsForSimpleText, simpleTextParagraphTypesModel))
    //
    , shortcutsForScreenplayTitle(new H6Label(shortcutsCard))
    , shortcutsForScreenplay(new Tree(shortcutsCard))
    , screenplayParagraphTypesModel(buildScreenplayParagraphTypesModel(shortcutsForScreenplay))
    , screenplayParagraphAddTypeDelegate(
          new ComboBoxItemDelegate(shortcutsForScreenplay, screenplayParagraphTypesModel))
    , screenplayParagraphChangeTypeDelegate(
          new ComboBoxItemDelegate(shortcutsForScreenplay, screenplayParagraphTypesModel))
    //
    , advancedCard(new Card(content))
    , advancedCardLayout(new QGridLayout)
    , advancedTitle(new H5Label(advancedCard))
    , advancedAccurateMetricsHandling(new CheckBox(advancedCard))
    , advancedUseExtendedLogging(new CheckBox(advancedCard))
{
    scrollAnimation.setEasingCurve(QEasingCurve::OutQuad);
    scrollAnimation.setDuration(180);
    colorAnimation.setEasingCurve(QEasingCurve::InBack);
    colorAnimation.setDuration(1400);

    initApplicationCard();
    initSimpleTextCard();
    initScreenplayCard();
    initShortcutsCard();
    initAdvancedCard();

    auto layout = qobject_cast<QVBoxLayout*>(content->widget()->layout());
    layout->addWidget(applicationCard);
    layout->addWidget(componentsTitle);
    layout->addWidget(simpleTextCard);
    layout->addWidget(screenplayCard);
    layout->addWidget(shortcutsCard);
    layout->addWidget(advancedCard);
    layout->addStretch();
}

void SettingsView::Implementation::initApplicationCard()
{
    lightTheme->setTheme(ApplicationTheme::Light);
    darkAndLightTheme->setTheme(ApplicationTheme::DarkAndLight);
    darkTheme->setTheme(ApplicationTheme::Dark);
    customTheme->setTheme(ApplicationTheme::Custom);
    // 0 - 0.5, 500 - 1, 3500 - 4
    scaleFactor->setMaximumValue(3500);
    scaleFactor->setValue(500);
    scaleFactor->setDefaultValue(500);
    backupsFolderPath->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    backupsFolderPath->setEnabled(false);
    backupsFolderPath->setTrailingIcon(u8"\U000f0256");
    backupsQty->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    backupsQty->setEnabled(false);
    backupsQty->setSuffix("  ");
    for (auto textField : { gitHubSyncRepository, gitHubSyncToken }) {
        textField->setSpellCheckPolicy(SpellCheckPolicy::Manual);
        textField->setCapitalizeWords(false);
        textField->setCorrectDoubleCapitals(false);
        textField->setCapitalizeSingleILetter(false);
    }
    gitHubSyncToken->setPasswordModeEnabled(true);
    spellCheckerLanguage->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    spellCheckerLanguage->setEnabled(false);
    spellCheckerLanguage->setModel(spellCheckerLanguagesModel);
    spellCheckerUserDictionary->setIcon(u8"\U000F0900");
    spellCheckerUserDictionary->hide();

    //
    // Компоновка
    //
    applicationCardLayout->setContentsMargins({});
    applicationCardLayout->setSpacing(0);
    int itemIndex = 0;
    applicationCardLayout->addWidget(applicationTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(language, 0, Qt::AlignCenter);
        layout->addWidget(changeLanuage);
        layout->addStretch();
        applicationCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... интерфейс
    //
    applicationCardLayout->addWidget(applicationUserInterfaceTitle, itemIndex++, 0);
    {
        applicationThemesLayout->setContentsMargins({});
        applicationThemesLayout->setSpacing(0);
        applicationThemesLayout->addWidget(lightTheme);
        applicationThemesLayout->addWidget(darkAndLightTheme);
        applicationThemesLayout->addWidget(darkTheme);
        applicationThemesLayout->addWidget(customTheme);
        applicationThemesLayout->addStretch();
        applicationCardLayout->addLayout(applicationThemesLayout, itemIndex++, 0);
    }
    applicationCardLayout->addWidget(scaleFactorTitle, itemIndex++, 0);
    applicationCardLayout->addWidget(scaleFactor, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(scaleFactorSmallInfo);
        layout->addStretch();
        layout->addWidget(scaleFactorBigInfo);
        applicationCardLayout->addLayout(layout, itemIndex++, 0);
    }
    applicationCardLayout->addWidget(isCompact, itemIndex++, 0);
    //
    // ... сохранение и бэкапы
    //
    applicationCardLayout->addWidget(applicationSaveAndBackupTitle, itemIndex++, 0);
    applicationCardLayout->addWidget(autoSave, itemIndex++, 0);
    {
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        backupsFolderPath->setSizePolicy(policy);

        auto layout = makeLayout();
        layout->addWidget(saveBackups, 0, Qt::AlignCenter);
        layout->addWidget(backupsFolderPath);
        layout->addWidget(backupsQty);
        applicationCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... sync with GitHub
    //
    applicationCardLayout->addWidget(applicationGitHubSyncTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(gitHubSyncRepository, 1);
        layout->addWidget(gitHubSyncToken, 1);
        applicationCardLayout->addLayout(layout, itemIndex++, 0);
    }
    applicationCardLayout->addWidget(gitHubSyncUploadOnExit, itemIndex++, 0);
    //
    // ... параметры редакторов текста
    //
    applicationCardLayout->addWidget(applicationTextEditingTitle, itemIndex++, 0);
    {
        spellCheckerLanguage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto layout = makeLayout();
        layout->addWidget(useSpellChecker, 0, Qt::AlignCenter);
        layout->addWidget(spellCheckerLanguage);
        layout->addWidget(spellCheckerUserDictionary);
        applicationCardLayout->addLayout(layout, itemIndex++, 0);
    }
    applicationCardLayout->addWidget(showDocumentsPages, itemIndex++, 0);
    applicationCardLayout->addWidget(useTypewriterSound, itemIndex++, 0);
    applicationCardLayout->addWidget(useTypewriterScrolling, itemIndex++, 0);
    applicationCardLayout->addWidget(focusCurrentParagraph, itemIndex++, 0);
    applicationCardLayout->addWidget(highlightCurrentLine, itemIndex++, 0);
    applicationCardLayout->addWidget(applicationTextEditingSubtitle, itemIndex++, 0);
    applicationCardLayout->addWidget(correctDoubleCapitals, itemIndex++, 0);
    applicationCardLayout->addWidget(capitalizeSingleILetter, itemIndex++, 0);
    applicationCardLayout->addWidget(replaceThreeDotsWithEllipsis, itemIndex++, 0);
    applicationCardLayout->addWidget(useSmartQuotes, itemIndex++, 0);
    applicationCardLayout->addWidget(replaceTwoDashesWithEmDash, itemIndex++, 0);
    applicationCardLayout->addWidget(avoidMultipleSpaces, itemIndex++, 0);

    applicationCardBottomSpacerIndex = itemIndex;
    applicationCard->setContentLayout(applicationCardLayout);
}

void SettingsView::Implementation::initSimpleTextCard()
{
    simpleTextEditorDefaultTemplate->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    simpleTextEditorDefaultTemplate->setModel(
        BusinessLayer::TemplatesFacade::simpleTextTemplates());
    simpleTextEditorDefaultTemplateOptions->setIcon(u8"\U000F01D9");
    //
    auto linesGroup = new RadioButtonGroup(simpleTextCard);
    linesGroup->add(simpleTextNavigatorSceneDescriptionLines1);
    linesGroup->add(simpleTextNavigatorSceneDescriptionLines2);
    linesGroup->add(simpleTextNavigatorSceneDescriptionLines3);
    linesGroup->add(simpleTextNavigatorSceneDescriptionLines4);
    linesGroup->add(simpleTextNavigatorSceneDescriptionLines5);
    simpleTextNavigatorSceneDescriptionLines1->setEnabled(false);
    simpleTextNavigatorSceneDescriptionLines1->setChecked(true);
    simpleTextNavigatorSceneDescriptionLines2->setEnabled(false);
    simpleTextNavigatorSceneDescriptionLines3->setEnabled(false);
    simpleTextNavigatorSceneDescriptionLines4->setEnabled(false);
    simpleTextNavigatorSceneDescriptionLines5->setEnabled(false);


    //
    // Компоновка
    //
    simpleTextCardLayout->setContentsMargins({});
    simpleTextCardLayout->setSpacing(0);
    int itemIndex = 0;
    {
        auto layout = makeLayout();
        layout->addWidget(simpleTextTitle);
        layout->addStretch();
        layout->addWidget(simpleTextAvailable, 0, Qt::AlignVCenter);
        simpleTextCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... редактор текста
    //
    simpleTextCardLayout->addWidget(simpleTextEditorTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(simpleTextEditorDefaultTemplate, 1);
        layout->addWidget(simpleTextEditorDefaultTemplateOptions);
        simpleTextCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... навигатор текста
    //
    simpleTextCardLayout->addWidget(simpleTextNavigatorTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(simpleTextNavigatorShowSceneText);
        layout->addWidget(simpleTextNavigatorSceneDescriptionLines1);
        layout->addWidget(simpleTextNavigatorSceneDescriptionLines2);
        layout->addWidget(simpleTextNavigatorSceneDescriptionLines3);
        layout->addWidget(simpleTextNavigatorSceneDescriptionLines4);
        layout->addWidget(simpleTextNavigatorSceneDescriptionLines5);
        layout->addStretch();
        simpleTextCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    simpleTextCardBottomSpacerIndex = itemIndex;
    simpleTextCard->setContentLayout(simpleTextCardLayout);
}

void SettingsView::Implementation::initScreenplayCard()
{
    screenplayEditorDefaultTemplate->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    screenplayEditorDefaultTemplate->setModel(
        BusinessLayer::TemplatesFacade::screenplayTemplates());
    screenplayEditorDefaultTemplateOptions->setIcon(u8"\U000F01D9");
    screenplayEditorShowSceneNumberOnLeft->setEnabled(false);
    screenplayEditorShowSceneNumberOnLeft->setChecked(true);
    screenplayEditorShowSceneNumberOnRight->setEnabled(false);
    //
    screenplayNavigatorShowBeatsInTreatment->setEnabled(false);
    screenplayNavigatorShowBeatsInTreatment->setChecked(true);
    screenplayNavigatorShowBeatsInScreenplay->setEnabled(false);
    screenplayNavigatorShowBeatsInScreenplay->setChecked(false);
    auto linesGroup = new RadioButtonGroup(screenplayCard);
    linesGroup->add(screenplayNavigatorSceneDescriptionLines1);
    linesGroup->add(screenplayNavigatorSceneDescriptionLines2);
    linesGroup->add(screenplayNavigatorSceneDescriptionLines3);
    linesGroup->add(screenplayNavigatorSceneDescriptionLines4);
    linesGroup->add(screenplayNavigatorSceneDescriptionLines5);
    screenplayNavigatorSceneDescriptionLines1->setEnabled(false);
    screenplayNavigatorSceneDescriptionLines1->setChecked(true);
    screenplayNavigatorSceneDescriptionLines2->setEnabled(false);
    screenplayNavigatorSceneDescriptionLines3->setEnabled(false);
    screenplayNavigatorSceneDescriptionLines4->setEnabled(false);
    screenplayNavigatorSceneDescriptionLines5->setEnabled(false);
    //
    auto durationGroup = new RadioButtonGroup(screenplayCard);
    durationGroup->add(screenplayDurationByPage);
    durationGroup->add(screenplayDurationByCharacters);
    durationGroup->add(screenplayDurationConfigurable);
    durationGroup->add(screenplayDurationUseEighths);
    screenplayDurationByPage->setChecked(true);
    screenplayDurationByPagePage->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    screenplayDurationByPagePage->setText("1");
    screenplayDurationByPagePage->setReadOnly(true);
    screenplayDurationByPageDuration->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    screenplayDurationByCharactersCharacters->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    screenplayDurationByCharactersCharacters->setVisible(false);
    screenplayDurationByCharactersIncludingSpaces->setVisible(false);
    screenplayDurationByCharactersDuration->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    screenplayDurationByCharactersDuration->setVisible(false);
    screenplayDurationConfigurableForActionLabel->setVisible(false);
    screenplayDurationConfigurablePerParagraphForAction->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerParagraphForAction->setVisible(false);
    screenplayDurationConfigurableForActionPlus->setVisible(false);
    screenplayDurationConfigurablePerEvery50ForAction->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerEvery50ForAction->setVisible(false);
    screenplayDurationConfigurableForDialogueLabel->setVisible(false);
    screenplayDurationConfigurablePerParagraphForDialogue->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerParagraphForDialogue->setVisible(false);
    screenplayDurationConfigurablePerParagraphForDialogue->setLabelVisible(false);
    screenplayDurationConfigurableForDialoguePlus->setVisible(false);
    screenplayDurationConfigurablePerEvery50ForDialogue->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerEvery50ForDialogue->setVisible(false);
    screenplayDurationConfigurablePerEvery50ForDialogue->setLabelVisible(false);
    screenplayDurationConfigurableForSceneHeadingLabel->setVisible(false);
    screenplayDurationConfigurablePerParagraphForSceneHeading->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerParagraphForSceneHeading->setVisible(false);
    screenplayDurationConfigurablePerParagraphForSceneHeading->setLabelVisible(false);
    screenplayDurationConfigurableForSceneHeadingPlus->setVisible(false);
    screenplayDurationConfigurablePerEvery50ForSceneHeading->setSpellCheckPolicy(
        SpellCheckPolicy::Manual);
    screenplayDurationConfigurablePerEvery50ForSceneHeading->setVisible(false);
    screenplayDurationConfigurablePerEvery50ForSceneHeading->setLabelVisible(false);


    //
    // Компоновка
    //
    screenplayCardLayout->setContentsMargins({});
    screenplayCardLayout->setSpacing(0);
    int itemIndex = 0;
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayTitle);
        layout->addStretch();
        layout->addWidget(screenplayAvailable, 0, Qt::AlignVCenter);
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... редактор сценария
    //
    screenplayCardLayout->addWidget(screenplayEditorTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayEditorDefaultTemplate, 1);
        layout->addWidget(screenplayEditorDefaultTemplateOptions);
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayEditorShowSceneNumber);
        layout->addWidget(screenplayEditorShowSceneNumberOnLeft);
        layout->addWidget(screenplayEditorShowSceneNumberOnRight);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayEditorShowDialogueNumber, itemIndex++, 0);
    screenplayCardLayout->addWidget(screenplayEditorContinueDialogue, itemIndex++, 0);
    screenplayCardLayout->addWidget(screenplayEditorCorrectTextOnPageBreaks, itemIndex++, 0);
    screenplayCardLayout->addWidget(screenplayEditorSaveItemsFromText, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayEditorShowHintsForAllItems);
        layout->addWidget(screenplayEditorShowHintsForPrimaryItems);
        layout->addWidget(screenplayEditorShowHintsForSecondaryItems);
        layout->addWidget(screenplayEditorShowHintsForTertiaryItems);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayEditorShowCharacterSuggestionsInEmptyBlock,
                                    itemIndex++, 0);
    screenplayCardLayout->addWidget(screenplayEditorUseOpenParenthesisInDialogueForParenthetical,
                                    itemIndex++, 0);
    //
    // ... навигатор сценария
    //
    screenplayCardLayout->addWidget(screenplayNavigatorTitle, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayNavigatorShowBeats);
        layout->addWidget(screenplayNavigatorShowBeatsInTreatment);
        layout->addWidget(screenplayNavigatorShowBeatsInScreenplay);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayNavigatorShowSceneNumber, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayNavigatorShowSceneText);
        layout->addWidget(screenplayNavigatorSceneDescriptionLines1);
        layout->addWidget(screenplayNavigatorSceneDescriptionLines2);
        layout->addWidget(screenplayNavigatorSceneDescriptionLines3);
        layout->addWidget(screenplayNavigatorSceneDescriptionLines4);
        layout->addWidget(screenplayNavigatorSceneDescriptionLines5);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    //
    // ... счётчики хронометража
    //
    screenplayCardLayout->addWidget(screenplayDurationTitle, itemIndex++, 0);
    screenplayCardLayout->addWidget(screenplayDurationByPage, itemIndex++, 0);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayDurationByPagePage);
        layout->addWidget(screenplayDurationByPageDuration);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayDurationByCharacters, itemIndex++, 0,
                                    Qt::AlignBottom);
    {
        auto layout = makeLayout();
        layout->addWidget(screenplayDurationByCharactersCharacters);
        layout->addWidget(screenplayDurationByCharactersIncludingSpaces, 0, Qt::AlignCenter);
        layout->addWidget(screenplayDurationByCharactersDuration);
        layout->addStretch();
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayDurationConfigurable, itemIndex++, 0,
                                    Qt::AlignBottom);
    {
        auto layout = new QGridLayout;
        int row = 0;
        int column = 0;
        layout->addWidget(screenplayDurationConfigurableForActionLabel, row, column++,
                          Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(screenplayDurationConfigurablePerParagraphForAction, row, column++);
        layout->addWidget(screenplayDurationConfigurableForActionPlus, row, column++,
                          Qt::AlignCenter);
        layout->addWidget(screenplayDurationConfigurablePerEvery50ForAction, row, column++);
        ++row;
        column = 0;
        layout->addWidget(screenplayDurationConfigurableForDialogueLabel, row, column++,
                          Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(screenplayDurationConfigurablePerParagraphForDialogue, row, column++);
        layout->addWidget(screenplayDurationConfigurableForDialoguePlus, row, column++,
                          Qt::AlignCenter);
        layout->addWidget(screenplayDurationConfigurablePerEvery50ForDialogue, row, column++);
        ++row;
        column = 0;
        layout->addWidget(screenplayDurationConfigurableForSceneHeadingLabel, row, column++,
                          Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(screenplayDurationConfigurablePerParagraphForSceneHeading, row, column++);
        layout->addWidget(screenplayDurationConfigurableForSceneHeadingPlus, row, column++,
                          Qt::AlignCenter);
        layout->addWidget(screenplayDurationConfigurablePerEvery50ForSceneHeading, row, column++);
        layout->setColumnStretch(column, 1);
        screenplayCardLayout->addLayout(layout, itemIndex++, 0);
    }
    screenplayCardLayout->addWidget(screenplayDurationUseEighths, itemIndex++, 0);
    //
    screenplayCardBottomSpacerIndex = itemIndex;
    screenplayCard->setContentLayout(screenplayCardLayout);
}

void SettingsView::Implementation::initShortcutsCard()
{
    shortcutsForApplication->setHeader(new HierarchicalHeaderView(shortcutsForApplication));
    shortcutsForSimpleText->setHeader(new HierarchicalHeaderView(shortcutsForSimpleText));
    shortcutsForScreenplay->setHeader(new HierarchicalHeaderView(shortcutsForScreenplay));

    shortcutsCardLayout->setContentsMargins({});
    shortcutsCardLayout->setSpacing(0);
    int itemIndex = 0;
    shortcutsCardLayout->addWidget(shortcutsTitle, itemIndex++, 0);
    //
    shortcutsCardLayout->addWidget(shortcutsForApplicationTitle, itemIndex++, 0);
    shortcutsCardLayout->addWidget(shortcutsForApplication, itemIndex++, 0);
    shortcutsCardLayout->addWidget(shortcutsForSimpleTextTitle, itemIndex++, 0);
    shortcutsCardLayout->addWidget(shortcutsForSimpleText, itemIndex++, 0);
    shortcutsCardLayout->addWidget(shortcutsForScreenplayTitle, itemIndex++, 0);
    shortcutsCardLayout->addWidget(shortcutsForScreenplay, itemIndex++, 0);
    //
    shortcutsCardBottomSpacerIndex = itemIndex;
    shortcutsCard->setContentLayout(shortcutsCardLayout);
}

void SettingsView::Implementation::initAdvancedCard()
{
    //
    // Компоновка
    //
    advancedCardLayout->setContentsMargins({});
    advancedCardLayout->setSpacing(0);
    int itemIndex = 0;
    advancedCardLayout->addWidget(advancedTitle, itemIndex++, 0);
    advancedCardLayout->addWidget(advancedAccurateMetricsHandling, itemIndex++, 0);
    advancedCardLayout->addWidget(advancedUseExtendedLogging, itemIndex++, 0);
    //
    advancedCard->setContentLayout(advancedCardLayout);
}

void SettingsView::Implementation::scrollToTitle(AbstractLabel* title)
{
    const QRect microFocus = title->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    const QRect defaultMicroFocus
        = title->QWidget::inputMethodQuery(Qt::ImCursorRectangle).toRect();
    QRect focusRect = (microFocus != defaultMicroFocus)
        ? QRect(title->mapTo(content->widget(), microFocus.topLeft()), microFocus.size())
        : QRect(title->mapTo(content->widget(), QPoint(0, 0)), title->size());

    focusRect.adjust(-50, -50, 50, 50);

    scrollAnimation.setStartValue(content->verticalScrollBar()->value());
    scrollAnimation.setEndValue(focusRect.top());
    scrollAnimation.start();

    colorAnimation.stop();
    if (colorableTitle != nullptr) {
        colorableTitle->setTextColor(colorAnimation.endValue().value<QColor>());
    }
    colorableTitle = title;
    colorAnimation.setEndValue(colorableTitle->textColor());
    colorableTitle->setTextColor(colorAnimation.startValue().value<QColor>());
    colorAnimation.start();
}

void SettingsView::Implementation::updateTablesGeometry()
{
    auto updateTableGeometry = [](Tree* _table, qreal _firstColumnWidthDelta) {
        if (_table->model() != nullptr) {
            _table->setColumnWidth(0, _table->width() * _firstColumnWidthDelta);
            _table->setFixedHeight((_table->model()->rowCount() + 2)
                                   * qCeil(DesignSystem::treeOneLineItem().height()));
        }
    };
    updateTableGeometry(shortcutsForApplication, 0.25);
    updateTableGeometry(shortcutsForSimpleText, 0.25);
    updateTableGeometry(shortcutsForScreenplay, 0.25);
}


// ****


SettingsView::SettingsView(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{
    setAnimationType(StackWidget::AnimationType::FadeThrough);
    showDefaultPage();

    d->gitHubSyncRepository->installEventFilter(this);
    d->gitHubSyncToken->installEventFilter(this);

    connect(&d->scrollAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& _value) {
                d->content->verticalScrollBar()->setValue(_value.toInt());
            });
    connect(&d->colorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& _value) {
                if (d->colorableTitle != nullptr) {
                    d->colorableTitle->setTextColor(_value.value<QColor>());
                }
            });
    //
    // Приложение
    //
    connect(d->useSpellChecker, &CheckBox::checkedChanged, d->spellCheckerLanguage,
            &ComboBox::setEnabled);
    connect(d->saveBackups, &CheckBox::checkedChanged, d->backupsFolderPath,
            &TextField::setEnabled);
    connect(d->saveBackups, &CheckBox::checkedChanged, d->backupsQty, &TextField::setEnabled);
    connect(d->backupsFolderPath, &TextField::trailingIconPressed, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(
            this, tr("Choose the folder where backups will be saved"),
            d->backupsFolderPath->text());
        if (!path.isEmpty()) {
            d->backupsFolderPath->setText(path);
        }
    });
    //
    connect(d->changeLanuage, &Button::clicked, this, &SettingsView::applicationLanguagePressed);
    for (auto theme : {
             d->lightTheme,
             d->darkAndLightTheme,
             d->darkTheme,
             d->customTheme,
         }) {
        connect(theme, &ThemePreview::themePressed, this, &SettingsView::applicationThemePressed);
        connect(theme, &ThemePreview::customThemeHashPasted, this,
                &SettingsView::customThemeHashPasted);
    }
    connect(d->scaleFactor, &Slider::valueChanged, this, [this](int _value) {
        emit applicationScaleFactorChanged(0.5 + static_cast<qreal>(_value) / 1000.0);
    });
    connect(d->isCompact, &CheckBox::checkedChanged, this,
            [this](bool _checked) { emit applicationDensityChanged(_checked ? 2 : 0); });
    connect(d->autoSave, &CheckBox::checkedChanged, this,
            &SettingsView::applicationUseAutoSaveChanged);
    connect(d->saveBackups, &CheckBox::checkedChanged, this,
            &SettingsView::applicationSaveBackupsChanged);
    connect(d->backupsFolderPath, &TextField::textChanged, this,
            [this] { emit applicationBackupsFolderChanged(d->backupsFolderPath->text()); });
    connect(d->backupsQty, &TextField::textChanged, this, [this] {
        QSignalBlocker signalBlocker(d->backupsQty);

        bool isValidInt = false;
        auto backupsQty = d->backupsQty->text().toInt(&isValidInt);
        if (isValidInt) {
            if (backupsQty <= 2) {
                backupsQty = 2;
            } else if (backupsQty > 50) {
                backupsQty = 50;
            }
            setApplicationBackupsQty(backupsQty);
            emit applicationBackupsQtyChanged(backupsQty);
        } else {
            d->backupsQty->undo();
        }
    });
    connect(d->gitHubSyncRepository, &TextField::textChanged, this, [this] {
        emit applicationGitHubSyncRepositoryChanged(d->gitHubSyncRepository->text());
    });
    connect(d->gitHubSyncToken, &TextField::textChanged, this,
            [this] { emit applicationGitHubSyncTokenChanged(d->gitHubSyncToken->text()); });
    connect(d->gitHubSyncUploadOnExit, &CheckBox::checkedChanged, this,
            &SettingsView::applicationGitHubSyncUploadOnExitChanged);
    connect(d->showDocumentsPages, &CheckBox::checkedChanged, this,
            &SettingsView::applicationShowDocumentsPagesChanged);
    connect(d->useTypewriterSound, &CheckBox::checkedChanged, this,
            &SettingsView::applicationUseTypewriterSoundChanged);
    connect(d->useSpellChecker, &CheckBox::checkedChanged, this, [this](bool _checked) {
        emit applicationUseSpellCheckerChanged(_checked);
        if (_checked) {
            emit applicationSpellCheckerLanguageChanged(d->spellCheckerLanguage->currentIndex()
                                                            .data(kSpellCheckerLanguageCodeRole)
                                                            .toString());
        }
    });
    connect(d->spellCheckerLanguage, &ComboBox::currentIndexChanged, this,
            [this](const QModelIndex& _index) {
                emit applicationSpellCheckerLanguageChanged(
                    _index.data(kSpellCheckerLanguageCodeRole).toString());
            });
    connect(d->highlightCurrentLine, &CheckBox::checkedChanged, this,
            &SettingsView::applicationHighlightCurentLineChanged);
    connect(d->focusCurrentParagraph, &CheckBox::checkedChanged, this,
            &SettingsView::applicationFocusCurrentParagraphChanged);
    connect(d->useTypewriterScrolling, &CheckBox::checkedChanged, this,
            &SettingsView::applicationUseTypewriterScrollingChanged);
    connect(d->correctDoubleCapitals, &CheckBox::checkedChanged, this,
            &SettingsView::applicationCorrectDoubleCapitalsChanged);
    connect(d->capitalizeSingleILetter, &CheckBox::checkedChanged, this,
            &SettingsView::applicationCapitalizeSingleILetterChanged);
    connect(d->replaceThreeDotsWithEllipsis, &CheckBox::checkedChanged, this,
            &SettingsView::applicationReplaceThreeDotsWithEllipsisChanged);
    connect(d->useSmartQuotes, &CheckBox::checkedChanged, this,
            &SettingsView::applicationUseSmartQuotesChanged);
    connect(d->replaceTwoDashesWithEmDash, &CheckBox::checkedChanged, this,
            &SettingsView::applicationReplaceTwoDashedWithEmDashChanged);
    connect(d->avoidMultipleSpaces, &CheckBox::checkedChanged, this,
            &SettingsView::applicationAvoidMultipleSpacesChanged);

    //
    // Компоненты
    //
    // ... Редактор текста
    //
    connect(d->simpleTextAvailable, &Toggle::checkedChanged, this, [this](bool _available) {
        for (auto widget : std::vector<QWidget*>{
                 d->simpleTextEditorTitle,
                 d->simpleTextEditorDefaultTemplate,
                 d->simpleTextEditorDefaultTemplateOptions,
                 d->simpleTextNavigatorTitle,
                 d->simpleTextNavigatorShowSceneText,
                 d->simpleTextNavigatorSceneDescriptionLines1,
                 d->simpleTextNavigatorSceneDescriptionLines2,
                 d->simpleTextNavigatorSceneDescriptionLines3,
                 d->simpleTextNavigatorSceneDescriptionLines4,
                 d->simpleTextNavigatorSceneDescriptionLines5,
                 d->shortcutsForSimpleTextTitle,
                 d->shortcutsForSimpleText,
             }) {
            widget->setVisible(_available);
        }
        d->simpleTextCardLayout->setRowMinimumHeight(d->simpleTextCardBottomSpacerIndex,
                                                     _available ? DesignSystem::layout().px24()
                                                                : DesignSystem::layout().px12());
        d->updateTablesGeometry();

        emit simpleTextAvailableChanged(_available);
    });
    connect(d->simpleTextEditorDefaultTemplateOptions, &IconButton::clicked, this, [this] {
        QVector<QAction*> actions;
        const auto templateIndex = d->simpleTextEditorDefaultTemplate->currentIndex();
        const auto templateId
            = templateIndex.data(BusinessLayer::TemplatesFacade::kTemplateIdRole).toString();
        const auto isDefaultTemplate
            = BusinessLayer::TemplatesFacade::simpleTextTemplate(templateId).isDefault();
        //
        if (!isDefaultTemplate) {
            auto editAction = new QAction(tr("Edit"), d->contextMenu);
            connect(editAction, &QAction::triggered, this, [this, templateId] {
                emit editCurrentSimpleTextEditorTemplateRequested(templateId);
            });
            actions.append(editAction);
        }
        //
        auto duplicateAction
            = new QAction(tr("Create new template based on current"), d->contextMenu);
        connect(duplicateAction, &QAction::triggered, this, [this, templateId] {
            emit duplicateCurrentSimpleTextEditorTemplateRequested(templateId);
        });
        actions.append(duplicateAction);
        //
        if (!isDefaultTemplate) {
            auto saveToFileAction = new QAction(tr("Save to file"), d->contextMenu);
            connect(saveToFileAction, &QAction::triggered, this, [this, templateId] {
                emit saveToFileCurrentSimpleTextEditorTemplateRequested(templateId);
            });
            actions.append(saveToFileAction);
            //
            auto removeAction = new QAction(tr("Remove"), d->contextMenu);
            connect(removeAction, &QAction::triggered, this, [this, templateId] {
                emit removeCurrentSimpleTextEditorTemplateRequested(templateId);
            });
            actions.append(removeAction);
        }
        //
        auto loadFromFileAction = new QAction(tr("Load template from file"), d->contextMenu);
        loadFromFileAction->setSeparator(true);
        connect(loadFromFileAction, &QAction::triggered, this,
                &SettingsView::loadFromFileSimpleTextEditorTemplateRequested);
        actions.append(loadFromFileAction);
        //
        d->contextMenu->setActions(actions);
        d->contextMenu->showContextMenu(QCursor::pos());
    });
    connect(d->simpleTextEditorDefaultTemplate, &ComboBox::currentIndexChanged, this,
            [this](const QModelIndex& _index) {
                emit simpleTextEditorDefaultTemplateChanged(
                    _index.data(BusinessLayer::TemplatesFacade::kTemplateIdRole).toString());
            });
    //
    // ... навигатор текста
    //
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->simpleTextNavigatorSceneDescriptionLines1, &RadioButton::setEnabled);
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->simpleTextNavigatorSceneDescriptionLines2, &RadioButton::setEnabled);
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->simpleTextNavigatorSceneDescriptionLines3, &RadioButton::setEnabled);
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->simpleTextNavigatorSceneDescriptionLines4, &RadioButton::setEnabled);
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->simpleTextNavigatorSceneDescriptionLines5, &RadioButton::setEnabled);
    //
    auto notifysimpleTextNavigatorShowSceneTextChanged = [this] {
        int sceneTextLines = 1;
        if (d->simpleTextNavigatorSceneDescriptionLines2->isChecked()) {
            sceneTextLines = 2;
        } else if (d->simpleTextNavigatorSceneDescriptionLines3->isChecked()) {
            sceneTextLines = 3;
        } else if (d->simpleTextNavigatorSceneDescriptionLines4->isChecked()) {
            sceneTextLines = 4;
        } else if (d->simpleTextNavigatorSceneDescriptionLines5->isChecked()) {
            sceneTextLines = 5;
        }
        emit simpleTextNavigatorShowSceneTextChanged(
            d->simpleTextNavigatorShowSceneText->isChecked(), sceneTextLines);
    };
    connect(d->simpleTextNavigatorShowSceneText, &CheckBox::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    connect(d->simpleTextNavigatorSceneDescriptionLines1, &RadioButton::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    connect(d->simpleTextNavigatorSceneDescriptionLines2, &RadioButton::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    connect(d->simpleTextNavigatorSceneDescriptionLines3, &RadioButton::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    connect(d->simpleTextNavigatorSceneDescriptionLines4, &RadioButton::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    connect(d->simpleTextNavigatorSceneDescriptionLines5, &RadioButton::checkedChanged, this,
            notifysimpleTextNavigatorShowSceneTextChanged);
    //
    // ... Редактор сценария
    //
    connect(d->screenplayAvailable, &Toggle::checkedChanged, this, [this](bool _available) {
        for (auto widget : std::vector<QWidget*>{
                 d->screenplayEditorTitle,
                 d->screenplayEditorDefaultTemplate,
                 d->screenplayEditorDefaultTemplateOptions,
                 d->screenplayEditorShowSceneNumber,
                 d->screenplayEditorShowSceneNumberOnLeft,
                 d->screenplayEditorShowSceneNumberOnRight,
                 d->screenplayEditorShowDialogueNumber,
                 d->screenplayEditorContinueDialogue,
                 d->screenplayEditorCorrectTextOnPageBreaks,
                 d->screenplayEditorSaveItemsFromText,
                 d->screenplayEditorShowHintsForAllItems,
                 d->screenplayEditorShowHintsForPrimaryItems,
                 d->screenplayEditorShowHintsForSecondaryItems,
                 d->screenplayEditorShowHintsForTertiaryItems,
                 d->screenplayEditorShowCharacterSuggestionsInEmptyBlock,
                 d->screenplayEditorUseOpenParenthesisInDialogueForParenthetical,
                 d->screenplayNavigatorTitle,
                 d->screenplayNavigatorShowBeats,
                 d->screenplayNavigatorShowBeatsInTreatment,
                 d->screenplayNavigatorShowBeatsInScreenplay,
                 d->screenplayNavigatorShowSceneNumber,
                 d->screenplayNavigatorShowSceneText,
                 d->screenplayNavigatorSceneDescriptionLines1,
                 d->screenplayNavigatorSceneDescriptionLines2,
                 d->screenplayNavigatorSceneDescriptionLines3,
                 d->screenplayNavigatorSceneDescriptionLines4,
                 d->screenplayNavigatorSceneDescriptionLines5,
                 d->screenplayDurationTitle,
                 d->screenplayDurationByPage,
                 d->screenplayDurationByPagePage,
                 d->screenplayDurationByPageDuration,
                 d->screenplayDurationByCharacters,
                 d->screenplayDurationByCharactersCharacters,
                 d->screenplayDurationByCharactersIncludingSpaces,
                 d->screenplayDurationByCharactersDuration,
                 d->screenplayDurationConfigurable,
                 d->screenplayDurationConfigurableForActionLabel,
                 d->screenplayDurationConfigurablePerParagraphForAction,
                 d->screenplayDurationConfigurableForActionPlus,
                 d->screenplayDurationConfigurablePerEvery50ForAction,
                 d->screenplayDurationConfigurableForDialogueLabel,
                 d->screenplayDurationConfigurablePerParagraphForDialogue,
                 d->screenplayDurationConfigurableForDialoguePlus,
                 d->screenplayDurationConfigurablePerEvery50ForDialogue,
                 d->screenplayDurationConfigurableForSceneHeadingLabel,
                 d->screenplayDurationConfigurablePerParagraphForSceneHeading,
                 d->screenplayDurationConfigurableForSceneHeadingPlus,
                 d->screenplayDurationConfigurablePerEvery50ForSceneHeading,
                 d->screenplayDurationUseEighths,
                 d->shortcutsForScreenplayTitle,
                 d->shortcutsForScreenplay,
             }) {
            widget->setVisible(_available);
        }
        d->screenplayCardLayout->setRowMinimumHeight(d->screenplayCardBottomSpacerIndex,
                                                     _available ? DesignSystem::layout().px24()
                                                                : DesignSystem::layout().px12());
        const auto screenplayDurationByCharactersRow
            = d->screenplayCardLayout->indexOf(d->screenplayDurationByCharacters);
        d->screenplayCardLayout->setRowMinimumHeight(
            screenplayDurationByCharactersRow, _available ? DesignSystem::layout().px62() : 0.0);
        const auto screenplayDurationConfigurableRow
            = d->screenplayCardLayout->indexOf(d->screenplayDurationConfigurable);
        d->screenplayCardLayout->setRowMinimumHeight(
            screenplayDurationConfigurableRow, _available ? DesignSystem::layout().px62() : 0.0);
        d->updateTablesGeometry();

        emit screenplayAvailableChanged(_available);
    });
    connect(d->screenplayEditorDefaultTemplateOptions, &IconButton::clicked, this, [this] {
        QVector<QAction*> actions;
        const auto templateIndex = d->screenplayEditorDefaultTemplate->currentIndex();
        const auto templateId
            = templateIndex.data(BusinessLayer::TemplatesFacade::kTemplateIdRole).toString();
        const auto isDefaultTemplate
            = BusinessLayer::TemplatesFacade::screenplayTemplate(templateId).isDefault();
        //
        if (!isDefaultTemplate) {
            auto editAction = new QAction(tr("Edit"), d->contextMenu);
            connect(editAction, &QAction::triggered, this, [this, templateId] {
                emit editCurrentScreenplayEditorTemplateRequested(templateId);
            });
            actions.append(editAction);
        }
        //
        auto duplicateAction
            = new QAction(tr("Create new template based on current"), d->contextMenu);
        connect(duplicateAction, &QAction::triggered, this, [this, templateId] {
            emit duplicateCurrentScreenplayEditorTemplateRequested(templateId);
        });
        actions.append(duplicateAction);
        //
        if (!isDefaultTemplate) {
            auto saveToFileAction = new QAction(tr("Save to file"), d->contextMenu);
            connect(saveToFileAction, &QAction::triggered, this, [this, templateId] {
                emit saveToFileCurrentScreenplayEditorTemplateRequested(templateId);
            });
            actions.append(saveToFileAction);
            //
            auto removeAction = new QAction(tr("Remove"), d->contextMenu);
            connect(removeAction, &QAction::triggered, this, [this, templateId] {
                emit removeCurrentScreenplayEditorTemplateRequested(templateId);
            });
            actions.append(removeAction);
        }
        //
        auto loadFromFileAction = new QAction(tr("Load template from file"), d->contextMenu);
        loadFromFileAction->setSeparator(true);
        connect(loadFromFileAction, &QAction::triggered, this,
                &SettingsView::loadFromFileScreenplayEditorTemplateRequested);
        actions.append(loadFromFileAction);
        //
        d->contextMenu->setActions(actions);
        d->contextMenu->showContextMenu(QCursor::pos());
    });
    connect(d->screenplayEditorShowSceneNumber, &CheckBox::checkedChanged,
            d->screenplayEditorShowSceneNumberOnLeft, &CheckBox::setEnabled);
    connect(d->screenplayEditorShowSceneNumber, &CheckBox::checkedChanged,
            d->screenplayEditorShowSceneNumberOnRight, &CheckBox::setEnabled);
    auto screenplayEditorCorrectShownSceneNumber = [this] {
        if (!d->screenplayEditorShowSceneNumberOnLeft->isChecked()
            && !d->screenplayEditorShowSceneNumberOnRight->isChecked()) {
            d->screenplayEditorShowSceneNumberOnLeft->setChecked(true);
        }
    };
    connect(d->screenplayEditorShowSceneNumberOnLeft, &CheckBox::checkedChanged, this,
            screenplayEditorCorrectShownSceneNumber);
    connect(d->screenplayEditorShowSceneNumberOnRight, &CheckBox::checkedChanged, this,
            screenplayEditorCorrectShownSceneNumber);
    connect(d->screenplayEditorShowHintsForAllItems, &CheckBox::checkedChanged, this,
            [this](bool _checked) {
                if (_checked) {
                    d->screenplayEditorShowHintsForPrimaryItems->setChecked(false);
                    d->screenplayEditorShowHintsForSecondaryItems->setChecked(false);
                    d->screenplayEditorShowHintsForTertiaryItems->setChecked(false);
                }
            });
    auto screenplayEditorCorrectShowHintsForAllItems = [this](bool _checked) {
        if (_checked) {
            d->screenplayEditorShowHintsForAllItems->setChecked(false);
        }
    };
    connect(d->screenplayEditorShowHintsForPrimaryItems, &CheckBox::checkedChanged, this,
            screenplayEditorCorrectShowHintsForAllItems);
    connect(d->screenplayEditorShowHintsForSecondaryItems, &CheckBox::checkedChanged, this,
            screenplayEditorCorrectShowHintsForAllItems);
    connect(d->screenplayEditorShowHintsForTertiaryItems, &CheckBox::checkedChanged, this,
            screenplayEditorCorrectShowHintsForAllItems);
    //
    connect(d->screenplayEditorDefaultTemplate, &ComboBox::currentIndexChanged, this,
            [this](const QModelIndex& _index) {
                emit screenplayEditorDefaultTemplateChanged(
                    _index.data(BusinessLayer::TemplatesFacade::kTemplateIdRole).toString());
            });
    auto notifyScreenplayEditorShowSceneNumbersChanged = [this] {
        emit screenplayEditorShowSceneNumberChanged(
            d->screenplayEditorShowSceneNumber->isChecked(),
            d->screenplayEditorShowSceneNumberOnLeft->isChecked(),
            d->screenplayEditorShowSceneNumberOnRight->isChecked());
    };
    connect(d->screenplayEditorShowSceneNumber, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowSceneNumbersChanged);
    connect(d->screenplayEditorShowSceneNumberOnLeft, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowSceneNumbersChanged);
    connect(d->screenplayEditorShowSceneNumberOnRight, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowSceneNumbersChanged);
    connect(d->screenplayEditorShowDialogueNumber, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayEditorShowDialogueNumberChanged);
    connect(d->screenplayEditorContinueDialogue, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayEditorContinueDialogueChanged);
    connect(d->screenplayEditorCorrectTextOnPageBreaks, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayEditorCorrectTextOnPageBreaksChanged);
    connect(d->screenplayEditorSaveItemsFromText, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayEditorSaveItemsFromTextChanged);
    auto notifyScreenplayEditorShowHintsChanged = [this] {
        emit screenplayEditorShowHintsChanged(
            d->screenplayEditorShowHintsForAllItems->isChecked(),
            d->screenplayEditorShowHintsForPrimaryItems->isChecked(),
            d->screenplayEditorShowHintsForSecondaryItems->isChecked(),
            d->screenplayEditorShowHintsForTertiaryItems->isChecked());
    };
    connect(d->screenplayEditorShowHintsForAllItems, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowHintsChanged);
    connect(d->screenplayEditorShowHintsForPrimaryItems, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowHintsChanged);
    connect(d->screenplayEditorShowHintsForSecondaryItems, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowHintsChanged);
    connect(d->screenplayEditorShowHintsForTertiaryItems, &CheckBox::checkedChanged, this,
            notifyScreenplayEditorShowHintsChanged);
    connect(d->screenplayEditorShowCharacterSuggestionsInEmptyBlock, &CheckBox::checkedChanged,
            this, &SettingsView::screenplayEditorShowCharacterSuggestionsInEmptyBlockChanged);
    connect(d->screenplayEditorUseOpenParenthesisInDialogueForParenthetical,
            &CheckBox::checkedChanged, this,
            &SettingsView::screenplayEditorUseOpenBracketInDialogueForParentheticalChanged);
    //
    // ... навигатор сценария
    //
    connect(d->screenplayNavigatorShowBeats, &CheckBox::checkedChanged,
            d->screenplayNavigatorShowBeatsInTreatment, &CheckBox::setEnabled);
    connect(d->screenplayNavigatorShowBeats, &CheckBox::checkedChanged,
            d->screenplayNavigatorShowBeatsInScreenplay, &CheckBox::setEnabled);
    auto screenplayNavigatorCorrectShownBeats = [this] {
        if (!d->screenplayNavigatorShowBeatsInTreatment->isChecked()
            && !d->screenplayNavigatorShowBeatsInScreenplay->isChecked()) {
            d->screenplayNavigatorShowBeatsInTreatment->setChecked(true);
        }
    };
    connect(d->screenplayNavigatorShowBeatsInTreatment, &CheckBox::checkedChanged, this,
            screenplayNavigatorCorrectShownBeats);
    connect(d->screenplayNavigatorShowBeatsInScreenplay, &CheckBox::checkedChanged, this,
            screenplayNavigatorCorrectShownBeats);
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->screenplayNavigatorSceneDescriptionLines1, &RadioButton::setEnabled);
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->screenplayNavigatorSceneDescriptionLines2, &RadioButton::setEnabled);
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->screenplayNavigatorSceneDescriptionLines3, &RadioButton::setEnabled);
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->screenplayNavigatorSceneDescriptionLines4, &RadioButton::setEnabled);
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged,
            d->screenplayNavigatorSceneDescriptionLines5, &RadioButton::setEnabled);
    //
    auto notifyScreenplayNavigatorShowBeatsChanged = [this] {
        emit screenplayNavigatorShowBeatsChanged(
            d->screenplayNavigatorShowBeats->isChecked(),
            d->screenplayNavigatorShowBeatsInTreatment->isChecked(),
            d->screenplayNavigatorShowBeatsInScreenplay->isChecked());
    };
    connect(d->screenplayNavigatorShowBeats, &CheckBox::checkedChanged, this,
            notifyScreenplayNavigatorShowBeatsChanged);
    connect(d->screenplayNavigatorShowBeatsInTreatment, &CheckBox::checkedChanged, this,
            notifyScreenplayNavigatorShowBeatsChanged);
    connect(d->screenplayNavigatorShowBeatsInScreenplay, &CheckBox::checkedChanged, this,
            notifyScreenplayNavigatorShowBeatsChanged);
    connect(d->screenplayNavigatorShowSceneNumber, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayNavigatorShowSceneNumberChanged);
    auto notifyScreenplayNavigatorShowSceneTextChanged = [this] {
        int sceneTextLines = 1;
        if (d->screenplayNavigatorSceneDescriptionLines2->isChecked()) {
            sceneTextLines = 2;
        } else if (d->screenplayNavigatorSceneDescriptionLines3->isChecked()) {
            sceneTextLines = 3;
        } else if (d->screenplayNavigatorSceneDescriptionLines4->isChecked()) {
            sceneTextLines = 4;
        } else if (d->screenplayNavigatorSceneDescriptionLines5->isChecked()) {
            sceneTextLines = 5;
        }
        emit screenplayNavigatorShowSceneTextChanged(
            d->screenplayNavigatorShowSceneText->isChecked(), sceneTextLines);
    };
    connect(d->screenplayNavigatorShowSceneText, &CheckBox::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    connect(d->screenplayNavigatorSceneDescriptionLines1, &RadioButton::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    connect(d->screenplayNavigatorSceneDescriptionLines2, &RadioButton::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    connect(d->screenplayNavigatorSceneDescriptionLines3, &RadioButton::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    connect(d->screenplayNavigatorSceneDescriptionLines4, &RadioButton::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    connect(d->screenplayNavigatorSceneDescriptionLines5, &RadioButton::checkedChanged, this,
            notifyScreenplayNavigatorShowSceneTextChanged);
    //
    // ... хронометраж
    //
    connect(d->screenplayDurationByPage, &RadioButton::checkedChanged, this, [this](bool _checked) {
        d->screenplayDurationByPagePage->setVisible(d->screenplayAvailable->isChecked()
                                                    && _checked);
        d->screenplayDurationByPageDuration->setVisible(d->screenplayAvailable->isChecked()
                                                        && _checked);
    });
    connect(d->screenplayDurationByCharacters, &RadioButton::checkedChanged, this,
            [this](bool _checked) {
                d->screenplayDurationByCharactersCharacters->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationByCharactersIncludingSpaces->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationByCharactersDuration->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
            });
    connect(d->screenplayDurationConfigurable, &RadioButton::checkedChanged, this,
            [this](bool _checked) {
                d->screenplayDurationConfigurableForActionLabel->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerParagraphForAction->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurableForActionPlus->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerEvery50ForAction->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurableForDialogueLabel->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerParagraphForDialogue->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurableForDialoguePlus->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerEvery50ForDialogue->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurableForSceneHeadingLabel->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerParagraphForSceneHeading->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurableForSceneHeadingPlus->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
                d->screenplayDurationConfigurablePerEvery50ForSceneHeading->setVisible(
                    d->screenplayAvailable->isChecked() && _checked);
            });
    //
    auto notifyScreenplayDurationTypeChanged = [this] {
        using namespace BusinessLayer;
        if (d->screenplayDurationByPage->isChecked()) {
            emit screenplayDurationTypeChanged(static_cast<int>(ChronometerType::Page));
        } else if (d->screenplayDurationByCharacters->isChecked()) {
            emit screenplayDurationTypeChanged(static_cast<int>(ChronometerType::Characters));
        } else if (d->screenplayDurationConfigurable->isChecked()) {
            emit screenplayDurationTypeChanged(static_cast<int>(ChronometerType::Sophocles));
        }
    };
    connect(d->screenplayDurationByPage, &RadioButton::checkedChanged, this,
            notifyScreenplayDurationTypeChanged);
    connect(d->screenplayDurationByCharacters, &RadioButton::checkedChanged, this,
            notifyScreenplayDurationTypeChanged);
    connect(d->screenplayDurationConfigurable, &RadioButton::checkedChanged, this,
            notifyScreenplayDurationTypeChanged);
    connect(d->screenplayDurationByPageDuration, &TextField::textChanged, this, [this] {
        emit screenplayDurationByPageDurationChanged(
            d->screenplayDurationByPageDuration->text().toInt());
    });
    connect(d->screenplayDurationByCharactersCharacters, &TextField::textChanged, this, [this] {
        emit screenplayDurationByCharactersCharactersChanged(
            d->screenplayDurationByCharactersCharacters->text().toInt());
    });
    connect(d->screenplayDurationByCharactersIncludingSpaces, &CheckBox::checkedChanged, this,
            &SettingsView::screenplayDurationByCharactersIncludeSpacesChanged);
    connect(d->screenplayDurationByCharactersDuration, &TextField::textChanged, this, [this] {
        emit screenplayDurationByCharactersDurationChanged(
            d->screenplayDurationByCharactersDuration->text().toInt());
    });
    connect(d->screenplayDurationConfigurablePerParagraphForAction, &TextField::textChanged, this,
            [this] {
                emit screenplayDurationConfigurablePerParagraphForActionChanged(
                    d->screenplayDurationConfigurablePerParagraphForAction->text().toDouble());
            });
    connect(d->screenplayDurationConfigurablePerEvery50ForAction, &TextField::textChanged, this,
            [this] {
                emit screenplayDurationConfigurablePerEvery50ForActionChanged(
                    d->screenplayDurationConfigurablePerEvery50ForAction->text().toDouble());
            });
    connect(d->screenplayDurationConfigurablePerParagraphForDialogue, &TextField::textChanged, this,
            [this] {
                emit screenplayDurationConfigurablePerParagraphForDialogueChanged(
                    d->screenplayDurationConfigurablePerParagraphForDialogue->text().toDouble());
            });
    connect(d->screenplayDurationConfigurablePerEvery50ForDialogue, &TextField::textChanged, this,
            [this] {
                emit screenplayDurationConfigurablePerEvery50ForDialogueChanged(
                    d->screenplayDurationConfigurablePerEvery50ForDialogue->text().toDouble());
            });
    connect(
        d->screenplayDurationConfigurablePerParagraphForSceneHeading, &TextField::textChanged, this,
        [this] {
            emit screenplayDurationConfigurablePerParagraphForSceneHeadingChanged(
                d->screenplayDurationConfigurablePerParagraphForSceneHeading->text().toDouble());
        });
    connect(d->screenplayDurationConfigurablePerEvery50ForSceneHeading, &TextField::textChanged,
            this, [this] {
                emit screenplayDurationConfigurablePerEvery50ForSceneHeadingChanged(
                    d->screenplayDurationConfigurablePerEvery50ForSceneHeading->text().toDouble());
            });
    connect(d->screenplayDurationUseEighths, &RadioButton::checkedChanged, this,
            &SettingsView::screenplayDurationUseEighthsChanged);

    //
    // Соединения шорткатов настраиваются в момент установки моделей с данными о них в представление
    //

    //
    // Соединения дополнительных параметров
    //
    connect(d->advancedAccurateMetricsHandling, &CheckBox::checkedChanged, this,
            &SettingsView::advancedAccurateMetricsHandlingChanged);
    connect(d->advancedUseExtendedLogging, &CheckBox::checkedChanged, this,
            &SettingsView::advancedUseExtendedLoggingChanged);
}

void SettingsView::showDefaultPage()
{
    setCurrentWidget(d->content);
}

void SettingsView::showApplication()
{
    d->scrollToTitle(d->applicationTitle);
}

void SettingsView::showApplicationUserInterface()
{
    d->scrollToTitle(d->applicationUserInterfaceTitle);
}

void SettingsView::showApplicationSaveAndBackups()
{
    d->scrollToTitle(d->applicationSaveAndBackupTitle);
}

void SettingsView::showApplicationGitHubSync()
{
    d->scrollToTitle(d->applicationGitHubSyncTitle);
}

void SettingsView::showApplicationTextEditing()
{
    d->scrollToTitle(d->applicationTextEditingTitle);
}

void SettingsView::showComponents()
{
    d->scrollToTitle(d->componentsTitle);
}

void SettingsView::showComponentsSimpleText()
{
    d->scrollToTitle(d->simpleTextTitle);
}

void SettingsView::showComponentsScreenplay()
{
    d->scrollToTitle(d->screenplayTitle);
}

void SettingsView::showShortcuts()
{
    d->scrollToTitle(d->shortcutsTitle);
}

void SettingsView::showAdvanced()
{
    d->scrollToTitle(d->advancedTitle);
}

void SettingsView::setApplicationLanguage(int _language)
{
    auto languageString = [_language]() -> QString {
        switch (_language) {
        case QLocale::Arabic: {
            return "اَلْعَرَبِيَّةُ";
        }
        case QLocale::Armenian: {
            return "Հայերեն";
        }
        case QLocale::Azerbaijani: {
            return "Azərbaycan";
        }
        case QLocale::Belarusian: {
            return "Беларуский";
        }
        case QLocale::Catalan: {
            return "Català";
        }
        case QLocale::Chinese: {
            return "汉语";
        }
        case QLocale::Croatian: {
            return "Hrvatski";
        }
        case QLocale::Danish: {
            return "Dansk";
        }
        case QLocale::English: {
            return "English";
        }
        case QLocale::Esperanto: {
            return "Esperanto";
        }
        case QLocale::French: {
            return "Français";
        }
        case QLocale::Galician: {
            return "Galego";
        }
        case QLocale::German: {
            return "Deutsch";
        }
        case QLocale::Greek: {
            return "ελληνικά";
        }
        case QLocale::Hebrew: {
            return "עִבְרִית";
        }
        case QLocale::Hindi: {
            return "हिन्दी";
        }
        case QLocale::Hungarian: {
            return "Magyar";
        }
        case QLocale::Indonesian: {
            return "Indonesian";
        }
        case QLocale::Italian: {
            return "Italiano";
        }
        case QLocale::Korean: {
            return "한국어";
        }
        case QLocale::Persian: {
            return "فارسی";
        }
        case QLocale::Polish: {
            return "Polski";
        }
        case QLocale::LastLanguage + 1: {
            return "Português";
        }
        case QLocale::Portuguese: {
            return "Português Brasileiro";
        }
        case QLocale::Romanian: {
            return "Română";
        }
        case QLocale::Russian: {
            return "Русский";
        }
        case QLocale::Slovenian: {
            return "Slovenski";
        }
        case QLocale::Spanish: {
            return "Español";
        }
        case QLocale::Swedish: {
            return "Svenska";
        }
        case QLocale::Filipino: {
            return "Tagalog";
        }
        case QLocale::Tamil: {
            return "தமிழ்";
        }
        case QLocale::Telugu: {
            return "తెలుగు";
        }
        case QLocale::Turkish: {
            return "Türkçe";
        }
        case QLocale::Ukrainian: {
            return "Українська";
        }
        default: {
            return QLocale::languageToString(static_cast<QLocale::Language>(_language));
        }
        }
    };
    d->changeLanuage->setText(languageString());
}

void SettingsView::setApplicationScaleFactor(qreal _scaleFactor)
{
    d->scaleFactor->setValue((_scaleFactor - 0.5) * 1000.0);
}

void SettingsView::setApplicationDensity(int _density)
{
    d->isCompact->setChecked(_density > 0);
}

void SettingsView::setApplicationUseAutoSave(bool _use)
{
    d->autoSave->setChecked(_use);
}

void SettingsView::setApplicationSaveBackups(bool _save)
{
    d->saveBackups->setChecked(_save);
}

void SettingsView::setApplicationBackupsFolder(const QString& _path)
{
    d->backupsFolderPath->setText(_path);
}

void SettingsView::setApplicationBackupsQty(int _qty)
{
    d->backupsQty->setText(QString::number(_qty));
}

void SettingsView::setApplicationGitHubSyncRepository(const QString& _repository)
{
    d->gitHubSyncRepository->setText(_repository);
}

void SettingsView::setApplicationGitHubSyncToken(const QString& _token)
{
    d->gitHubSyncToken->setText(_token);
}

void SettingsView::setApplicationGitHubSyncUploadOnExit(bool _upload)
{
    d->gitHubSyncUploadOnExit->setChecked(_upload);
}

void SettingsView::setApplicationShowDocumentsPages(bool _show)
{
    d->showDocumentsPages->setChecked(_show);
}

void SettingsView::setApplicationUseTypewriterSound(bool _use)
{
    d->useTypewriterSound->setChecked(_use);
}

void SettingsView::setApplicationUseSpellChecker(bool _use)
{
    d->useSpellChecker->setChecked(_use);
}

void SettingsView::setApplicationSpellCheckerLanguage(const QString& _languageCode)
{
    for (int row = 0; row < d->spellCheckerLanguagesModel->rowCount(); ++row) {
        auto item = d->spellCheckerLanguagesModel->item(row);
        if (item->data(kSpellCheckerLanguageCodeRole).toString() != _languageCode) {
            continue;
        }

        d->spellCheckerLanguage->setCurrentIndex(item->index());
        break;
    }
}

void SettingsView::setApplicationHighlightCurrentLine(bool _highlight)
{
    d->highlightCurrentLine->setChecked(_highlight);
}

void SettingsView::setApplicationFocusCurrentParagraph(bool _focus)
{
    d->focusCurrentParagraph->setChecked(_focus);
}

void SettingsView::setApplicationUseTypewriterScrolling(bool _use)
{
    d->useTypewriterScrolling->setChecked(_use);
}

void SettingsView::setApplicationCorrectDoubleCapitals(bool _correct)
{
    d->correctDoubleCapitals->setChecked(_correct);
}

void SettingsView::setApplicationCapitalizeSinglaILetter(bool _capitalize)
{
    d->capitalizeSingleILetter->setChecked(_capitalize);
}

void SettingsView::setApplicationReplaceThreeDotsWithEllipsis(bool _replace)
{
    d->replaceThreeDotsWithEllipsis->setChecked(_replace);
}

void SettingsView::setApplicationUseSmartQuotes(bool _use)
{
    d->useSmartQuotes->setChecked(_use);
}

void SettingsView::setApplicationReplaceTwoDashesWithEmDash(bool _replace)
{
    d->replaceTwoDashesWithEmDash->setChecked(_replace);
}

void SettingsView::setApplicationAvoidMultipleSpaces(bool _avoid)
{
    d->avoidMultipleSpaces->setChecked(_avoid);
}

void SettingsView::setSimpleTextAvailable(bool _available)
{
    d->simpleTextAvailable->setChecked(_available);
}

void SettingsView::setSimpleTextEditorDefaultTemplate(const QString& _templateId)
{
    using namespace BusinessLayer;
    for (int row = 0; row < TemplatesFacade::simpleTextTemplates()->rowCount(); ++row) {
        auto item = TemplatesFacade::simpleTextTemplates()->item(row);
        if (item->data(TemplatesFacade::kTemplateIdRole).toString() != _templateId) {
            continue;
        }

        d->simpleTextEditorDefaultTemplate->setCurrentIndex(item->index());
        break;
    }
}

void SettingsView::setSimpleTextNavigatorShowSceneText(bool _show, int _lines)
{
    d->simpleTextNavigatorShowSceneText->setChecked(_show);
    if (_show) {
        const QHash<int, RadioButton*> buttons
            = { { 1, d->simpleTextNavigatorSceneDescriptionLines1 },
                { 2, d->simpleTextNavigatorSceneDescriptionLines2 },
                { 3, d->simpleTextNavigatorSceneDescriptionLines3 },
                { 4, d->simpleTextNavigatorSceneDescriptionLines4 },
                { 5, d->simpleTextNavigatorSceneDescriptionLines5 } };
        buttons[_lines]->setChecked(true);
    }
}

void SettingsView::setScreenplayAvailable(bool _available)
{
    d->screenplayAvailable->setChecked(_available);
}

void SettingsView::setScreenplayEditorDefaultTemplate(const QString& _templateId)
{
    using namespace BusinessLayer;
    for (int row = 0; row < TemplatesFacade::screenplayTemplates()->rowCount(); ++row) {
        auto item = TemplatesFacade::screenplayTemplates()->item(row);
        if (item->data(TemplatesFacade::kTemplateIdRole).toString() != _templateId) {
            continue;
        }

        d->screenplayEditorDefaultTemplate->setCurrentIndex(item->index());
        break;
    }
}

void SettingsView::setScreenplayEditorShowSceneNumber(bool _show, bool _atLeft, bool _atRight)
{
    QSignalBlocker blocker(d->screenplayEditorShowSceneNumberOnLeft);

    d->screenplayEditorShowSceneNumber->setChecked(_show);
    d->screenplayEditorShowSceneNumberOnLeft->setChecked(_atLeft);
    d->screenplayEditorShowSceneNumberOnRight->setChecked(_atRight);
}

void SettingsView::setScreenplayEditorShowDialogueNumber(bool _show)
{
    d->screenplayEditorShowDialogueNumber->setChecked(_show);
}

void SettingsView::setScreenplayEditorContinueDialogue(bool _continue)
{
    d->screenplayEditorContinueDialogue->setChecked(_continue);
}

void SettingsView::setScreenplayEditorCorrectTextOnPageBreaks(bool _correct)
{
    d->screenplayEditorCorrectTextOnPageBreaks->setChecked(_correct);
}

void SettingsView::setScreenplayEditorSaveItemsFromText(bool _save)
{
    d->screenplayEditorSaveItemsFromText->setChecked(_save);
}

void SettingsView::setScreenplayEditorShowHints(bool _all, bool _primary, bool _secondary,
                                                bool _tertiary)
{
    d->screenplayEditorShowHintsForAllItems->setChecked(_all);
    d->screenplayEditorShowHintsForPrimaryItems->setChecked(_primary);
    d->screenplayEditorShowHintsForSecondaryItems->setChecked(_secondary);
    d->screenplayEditorShowHintsForTertiaryItems->setChecked(_tertiary);
}

void SettingsView::setScreenplayEditorShowCharacterSuggestionsInEmptyBlock(bool _show)
{
    d->screenplayEditorShowCharacterSuggestionsInEmptyBlock->setChecked(_show);
}

void SettingsView::setScreenplayEditorUseOpenParenthesisInDialogueForParenthetical(bool _use)
{
    d->screenplayEditorUseOpenParenthesisInDialogueForParenthetical->setChecked(_use);
}

void SettingsView::setScreenplayNavigatorShowBeats(bool _show, bool _inTreatment,
                                                   bool _inScreenplay)
{
    QSignalBlocker blocker(d->screenplayNavigatorShowBeatsInTreatment);

    d->screenplayNavigatorShowBeats->setChecked(_show);
    d->screenplayNavigatorShowBeatsInTreatment->setChecked(_inTreatment);
    d->screenplayNavigatorShowBeatsInScreenplay->setChecked(_inScreenplay);
}

void SettingsView::setScreenplayNavigatorShowSceneNumber(bool _show)
{
    d->screenplayNavigatorShowSceneNumber->setChecked(_show);
}

void SettingsView::setScreenplayNavigatorShowSceneText(bool _show, int _lines)
{
    d->screenplayNavigatorShowSceneText->setChecked(_show);
    if (_show) {
        const QHash<int, RadioButton*> buttons
            = { { 1, d->screenplayNavigatorSceneDescriptionLines1 },
                { 2, d->screenplayNavigatorSceneDescriptionLines2 },
                { 3, d->screenplayNavigatorSceneDescriptionLines3 },
                { 4, d->screenplayNavigatorSceneDescriptionLines4 },
                { 5, d->screenplayNavigatorSceneDescriptionLines5 } };
        buttons[_lines]->setChecked(true);
    }
}

void SettingsView::setScreenplayDurationType(int _type)
{
    using namespace BusinessLayer;
    switch (static_cast<ChronometerType>(_type)) {
    case ChronometerType::Page: {
        d->screenplayDurationByPage->setChecked(true);
        break;
    }

    case ChronometerType::Characters: {
        d->screenplayDurationByCharacters->setChecked(true);
        break;
    }

    case ChronometerType::Sophocles: {
        d->screenplayDurationConfigurable->setChecked(true);
        break;
    }
    }
}

void SettingsView::setScreenplayDurationByPageDuration(int _duration)
{
    d->screenplayDurationByPageDuration->setText(QString::number(_duration));
}

void SettingsView::setScreenplayDurationByCharactersCharacters(int _characters)
{
    d->screenplayDurationByCharactersCharacters->setText(QString::number(_characters));
}

void SettingsView::setScreenplayDurationByCharactersIncludeSpaces(bool _include)
{
    d->screenplayDurationByCharactersIncludingSpaces->setChecked(_include);
}

void SettingsView::setScreenplayDurationByCharactersDuration(int _duration)
{
    d->screenplayDurationByCharactersDuration->setText(QString::number(_duration));
}

void SettingsView::setScreenplayDurationConfigurablePerParagraphForAction(qreal _duration)
{
    d->screenplayDurationConfigurablePerParagraphForAction->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationConfigurablePerEvery50ForAction(qreal _duration)
{
    d->screenplayDurationConfigurablePerEvery50ForAction->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationConfigurablePerParagraphForDialogue(qreal _duration)
{
    d->screenplayDurationConfigurablePerParagraphForDialogue->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationConfigurablePerEvery50ForDialogue(qreal _duration)
{
    d->screenplayDurationConfigurablePerEvery50ForDialogue->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationConfigurablePerParagraphForSceneHeading(qreal _duration)
{
    d->screenplayDurationConfigurablePerParagraphForSceneHeading->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationConfigurablePerEvery50ForSceneHeading(qreal _duration)
{
    d->screenplayDurationConfigurablePerEvery50ForSceneHeading->setText(
        QString::number(_duration, 'f', 1));
}

void SettingsView::setScreenplayDurationUseEighths(bool _use)
{
    d->screenplayDurationUseEighths->setChecked(_use);
}

void SettingsView::setShortcutsForApplicationModel(HierarchicalModel* _model)
{
    if (d->shortcutsForApplicationModel) {
        d->shortcutsForApplicationModel->disconnect(this);
        d->shortcutsForApplicationModel->deleteLater();
    }

    d->shortcutsForApplicationModel = _model;

    d->shortcutsForApplication->setModel(d->shortcutsForApplicationModel);
    d->shortcutsForApplication->setItemDelegateForColumn(
        1, new KeySequenceDelegate(d->shortcutsForApplication));
    d->updateTablesGeometry();

    connect(d->shortcutsForApplicationModel, &HierarchicalModel::dataChanged, this,
            [this](const QModelIndex& _index) {
                if (!_index.isValid()) {
                    return;
                }

                const auto shortcutName
                    = _index.sibling(_index.row(), TextEditorShortcuts::Type).data().toString();
                const auto shortcut
                    = _index.sibling(_index.row(), TextEditorShortcuts::Shortcut).data().toString();

                emit shortcutForApplicationChanged(shortcutName, shortcut);
            });
}

void SettingsView::setShortcutsForSimpleTextModel(HierarchicalModel* _model)
{
    if (d->shortcutsForSimpleTextModel) {
        d->shortcutsForSimpleTextModel->disconnect(this);
        d->shortcutsForSimpleTextModel->deleteLater();
    }

    d->shortcutsForSimpleTextModel = _model;

    d->shortcutsForSimpleText->setModel(d->shortcutsForSimpleTextModel);
    d->shortcutsForSimpleText->setItemDelegateForColumn(
        1, new KeySequenceDelegate(d->shortcutsForSimpleText));
    d->shortcutsForSimpleText->setItemDelegateForColumn(2, d->simpleTextParagraphAddTypeDelegate);
    d->shortcutsForSimpleText->setItemDelegateForColumn(3, d->simpleTextParagraphAddTypeDelegate);
    d->shortcutsForSimpleText->setItemDelegateForColumn(4,
                                                        d->simpleTextParagraphChangeTypeDelegate);
    d->shortcutsForSimpleText->setItemDelegateForColumn(5,
                                                        d->simpleTextParagraphChangeTypeDelegate);
    d->updateTablesGeometry();

    connect(
        d->shortcutsForSimpleTextModel, &HierarchicalModel::dataChanged, this,
        [this](const QModelIndex& _index) {
            if (!_index.isValid()) {
                return;
            }

            const auto blockType
                = _index.sibling(_index.row(), TextEditorShortcuts::Type).data().toString();
            const auto shortcut
                = _index.sibling(_index.row(), TextEditorShortcuts::Shortcut).data().toString();
            const auto jumpByTab
                = _index.sibling(_index.row(), TextEditorShortcuts::JumpByTab).data().toString();
            const auto jumpByEnter
                = _index.sibling(_index.row(), TextEditorShortcuts::JumpByEnter).data().toString();
            const auto changeByTab
                = _index.sibling(_index.row(), TextEditorShortcuts::ChangeByTab).data().toString();
            const auto changeByEnter
                = _index.sibling(_index.row(), TextEditorShortcuts::ChangeByEnter)
                      .data()
                      .toString();

            emit shortcutsForSimpleTextEditorChanged(blockType, shortcut, jumpByTab, jumpByEnter,
                                                     changeByTab, changeByEnter);
        });
}

void SettingsView::setShortcutsForScreenplayModel(HierarchicalModel* _model)
{
    if (d->shortcutsForScreenplayModel) {
        d->shortcutsForScreenplayModel->disconnect(this);
        d->shortcutsForScreenplayModel->deleteLater();
    }

    d->shortcutsForScreenplayModel = _model;

    d->shortcutsForScreenplay->setModel(d->shortcutsForScreenplayModel);
    d->shortcutsForScreenplay->setItemDelegateForColumn(
        1, new KeySequenceDelegate(d->shortcutsForScreenplay));
    d->shortcutsForScreenplay->setItemDelegateForColumn(2, d->screenplayParagraphAddTypeDelegate);
    d->shortcutsForScreenplay->setItemDelegateForColumn(3, d->screenplayParagraphAddTypeDelegate);
    d->shortcutsForScreenplay->setItemDelegateForColumn(4,
                                                        d->screenplayParagraphChangeTypeDelegate);
    d->shortcutsForScreenplay->setItemDelegateForColumn(5,
                                                        d->screenplayParagraphChangeTypeDelegate);
    d->updateTablesGeometry();

    connect(
        d->shortcutsForScreenplayModel, &HierarchicalModel::dataChanged, this,
        [this](const QModelIndex& _index) {
            if (!_index.isValid()) {
                return;
            }

            const auto blockType
                = _index.sibling(_index.row(), TextEditorShortcuts::Type).data().toString();
            const auto shortcut
                = _index.sibling(_index.row(), TextEditorShortcuts::Shortcut).data().toString();
            const auto jumpByTab
                = _index.sibling(_index.row(), TextEditorShortcuts::JumpByTab).data().toString();
            const auto jumpByEnter
                = _index.sibling(_index.row(), TextEditorShortcuts::JumpByEnter).data().toString();
            const auto changeByTab
                = _index.sibling(_index.row(), TextEditorShortcuts::ChangeByTab).data().toString();
            const auto changeByEnter
                = _index.sibling(_index.row(), TextEditorShortcuts::ChangeByEnter)
                      .data()
                      .toString();

            emit shortcutsForScreenplayEditorChanged(blockType, shortcut, jumpByTab, jumpByEnter,
                                                     changeByTab, changeByEnter);
        });
}

void SettingsView::setAdvancedAccurateMetricsHandling(bool _accurate)
{
    d->advancedAccurateMetricsHandling->setChecked(_accurate);
}

void SettingsView::setAdvancedUseExtendedLogging(bool _use)
{
    d->advancedUseExtendedLogging->setChecked(_use);
}

bool SettingsView::eventFilter(QObject* _watched, QEvent* _event)
{
    //
    // Text corrections turned on in the settings are applied to all text fields of the
    // application, but they must not change the name of the repository and the access token
    //
    if ((_watched == d->gitHubSyncRepository || _watched == d->gitHubSyncToken)
        && _event->type() == static_cast<QEvent::Type>(EventType::TextEditingOptionsChangeEvent)) {
        return true;
    }

    return StackWidget::eventFilter(_watched, _event);
}

void SettingsView::resizeEvent(QResizeEvent* _event)
{
    StackWidget::resizeEvent(_event);

    d->updateTablesGeometry();
}

void SettingsView::updateTranslations()
{
    d->applicationTitle->setText(tr("Application settings"));
    d->language->setText(tr("Language"));
    d->scaleFactorTitle->setText(tr("Size of the user interface elements:"));
    d->scaleFactorSmallInfo->setText(tr("small"));
    d->scaleFactorBigInfo->setText(tr("big"));
    d->isCompact->setText(tr("Make application interface compact"));
    d->applicationSaveAndBackupTitle->setText(tr("Save changes & backups"));
    d->autoSave->setText(tr("Automatically save changes as soon as possible"));
    d->autoSave->setToolTip(
        tr("Autosave works very accurately.\n"
           "It saves the project every 3 seconds if you do not use your mouse or keyboard.\n"
           "If you work with no interruptions it saves the project every 3 minutes."));
    d->saveBackups->setText(tr("Save backups"));
    d->backupsFolderPath->setLabel(tr("Backups folder path"));
    d->backupsQty->setLabel(tr("Qty"));
    d->applicationGitHubSyncTitle->setText(tr("Sync with GitHub"));
    d->gitHubSyncRepository->setLabel(tr("Private repository"));
    d->gitHubSyncRepository->setHelper(tr("In the form owner/name"));
    d->gitHubSyncToken->setLabel(tr("Access token"));
    d->gitHubSyncToken->setHelper(
        tr("With read and write access to the contents of the repository"));
    d->gitHubSyncUploadOnExit->setText(tr("Upload changed stories when closing the application"));
    d->applicationTextEditingTitle->setText(tr("Text editing"));
    d->showDocumentsPages->setText(tr("Show documents pages"));
    d->useTypewriterSound->setText(tr("Use typewriter sound for keys pressing"));
    d->useSpellChecker->setText(tr("Spell check"));
    d->spellCheckerLanguage->setLabel(tr("Spelling dictionary"));
    {
        const QVector<QString> languages = { tr("Afrikaans"),
                                             tr("Aragonese"),
                                             tr("Arabic"),
                                             tr("Asturian"),
                                             tr("Azerbaijani"),
                                             tr("Belarusian"),
                                             tr("Bulgarian"),
                                             tr("Bengali"),
                                             tr("Tibetan"),
                                             tr("Breton"),
                                             tr("Bosnian"),
                                             tr("Catalan (Valencian)"),
                                             tr("Catalan"),
                                             tr("Czech"),
                                             tr("Welsh"),
                                             tr("Danish"),
                                             tr("German (Austria)"),
                                             tr("German (Switzerland)"),
                                             tr("German"),
                                             tr("Greek (Polytonic)"),
                                             tr("Greek"),
                                             tr("English (Australia)"),
                                             tr("English (Canada)"),
                                             tr("English (United Kingdom)"),
                                             tr("English (New Zealand)"),
                                             tr("English (South Africa)"),
                                             tr("English (United States)"),
                                             tr("Esperanto"),
                                             tr("Spanish (Argentina)"),
                                             tr("Spanish (Bolivia)"),
                                             tr("Spanish (Chile)"),
                                             tr("Spanish (Colombia)"),
                                             tr("Spanish (Costa Rica)"),
                                             tr("Spanish (Cuba)"),
                                             tr("Spanish (Dominican Republic)"),
                                             tr("Spanish (Ecuador)"),
                                             tr("Spanish (Guatemala)"),
                                             tr("Spanish (Honduras)"),
                                             tr("Spanish (Mexico)"),
                                             tr("Spanish (Nicaragua)"),
                                             tr("Spanish (Panama)"),
                                             tr("Spanish (Peru)"),
                                             tr("Spanish (Philippines)"),
                                             tr("Spanish (Puerto Rico)"),
                                             tr("Spanish (Paraguay)"),
                                             tr("Spanish (El Salvador)"),
                                             tr("Spanish (United States)"),
                                             tr("Spanish (Uruguay)"),
                                             tr("Spanish (Venezuela)"),
                                             tr("Spanish"),
                                             tr("Estonian"),
                                             tr("Basque"),
                                             tr("Persian"),
                                             tr("Faroese"),
                                             tr("French"),
                                             tr("Friulian"),
                                             tr("Western Frisian"),
                                             tr("Irish"),
                                             tr("Gaelic"),
                                             tr("Galician"),
                                             tr("Gujarati"),
                                             tr("Guarani"),
                                             tr("Hebrew"),
                                             tr("Hindi"),
                                             tr("Croatian"),
                                             tr("Hungarian"),
                                             tr("Armenian"),
                                             tr("Armenian (Western)"),
                                             tr("Interlingua"),
                                             tr("Indonesian"),
                                             tr("Icelandic"),
                                             tr("Italian"),
                                             tr("Georgian"),
                                             tr("Kazakh"),
                                             tr("Kurdish"),
                                             tr("Korean"),
                                             tr("Latin"),
                                             tr("Luxembourgish"),
                                             tr("Lao"),
                                             tr("Lithuanian"),
                                             tr("Latgalian"),
                                             tr("Latvian"),
                                             tr("Macedonian"),
                                             tr("Mongolian"),
                                             tr("Maltese"),
                                             tr("Norwegian"),
                                             tr("Low German"),
                                             tr("Nepali"),
                                             tr("Dutch"),
                                             tr("Norwegian"),
                                             tr("Occitan"),
                                             tr("Polish"),
                                             tr("Portuguese (Brazilian)"),
                                             tr("Portuguese"),
                                             tr("Quechua"),
                                             tr("Romanian"),
                                             tr("Russian (with Yo)"),
                                             tr("Russian"),
                                             tr("Kinyarwanda"),
                                             tr("Sinhala"),
                                             tr("Slovak"),
                                             tr("Slovenian"),
                                             tr("Albanian"),
                                             tr("Serbian (Latin)"),
                                             tr("Serbian"),
                                             tr("Swedish (Finland)"),
                                             tr("Swedish"),
                                             tr("Swahili"),
                                             tr("Tamil"),
                                             tr("Telugu"),
                                             tr("Thai"),
                                             tr("Turkmen"),
                                             tr("Klingon (Latin)"),
                                             tr("Klingon"),
                                             tr("Turkish"),
                                             tr("Ukrainian"),
                                             tr("Vietnamese") };
        for (int index = 0; index < d->spellCheckerLanguagesModel->rowCount(); ++index) {
            const auto languageCode = d->spellCheckerLanguagesModel->item(index)
                                          ->data(kSpellCheckerLanguageCodeRole)
                                          .toString();
            const auto languageIndex = kSpellCheckerLanguagesNameToCode.indexOf(languageCode);
            d->spellCheckerLanguagesModel->item(index)->setText(languages.at(languageIndex));
        }
        d->spellCheckerLanguagesModel->sort(0);
    }
    d->applicationUserInterfaceTitle->setText(tr("User interface"));
    d->spellCheckerUserDictionary->setToolTip(tr("Manage user dictionary"));
    d->highlightCurrentLine->setText(tr("Highlight current line"));
    d->focusCurrentParagraph->setText(tr("Focus current paragraph"));
    d->useTypewriterScrolling->setText(
        tr("Use typewriter scrolling (keeps line with the cursor on the screen center)"));
    d->applicationTextEditingSubtitle->setText(tr("Automatic text corrections"));
    d->correctDoubleCapitals->setText(tr("Correct DOuble CApitals"));
    d->capitalizeSingleILetter->setText(tr("Capitalize single \"i\" letter"));
    d->replaceThreeDotsWithEllipsis->setText(tr("Replace three single dots with ellipsis"));
    d->useSmartQuotes->setText(tr("Replace vertical quotation marks to curly ones"));
    d->replaceTwoDashesWithEmDash->setText(tr("Replace two dashes with em dash"));
    d->avoidMultipleSpaces->setText(tr("Avoid multiple spaces"));

    d->componentsTitle->setText(tr("Components"));
    //
    BusinessLayer::TemplatesFacade::updateTranslations();
    //
    d->simpleTextTitle->setText(tr("Simple text module"));
    d->simpleTextAvailable->setToolTip(tr("Turn on/off text and folders module"));
    d->simpleTextEditorTitle->setText(tr("Text editor"));
    d->simpleTextEditorDefaultTemplate->setLabel(tr("Default template"));
    d->simpleTextEditorDefaultTemplateOptions->setToolTip(
        tr("Available actions for the selected template"));
    d->simpleTextNavigatorTitle->setText(tr("Navigator"));
    d->simpleTextNavigatorShowSceneText->setText(tr("Show chapter text, lines"));
    d->simpleTextNavigatorSceneDescriptionLines1->setText("1");
    d->simpleTextNavigatorSceneDescriptionLines2->setText("2");
    d->simpleTextNavigatorSceneDescriptionLines3->setText("3");
    d->simpleTextNavigatorSceneDescriptionLines4->setText("4");
    d->simpleTextNavigatorSceneDescriptionLines5->setText("5");
    //
    d->screenplayTitle->setText(tr("Screenplay module"));
    d->screenplayAvailable->setToolTip(tr("Turn on/off screenplay module"));
    d->screenplayEditorTitle->setText(tr("Text editor"));
    d->screenplayEditorDefaultTemplate->setLabel(tr("Default template"));
    d->screenplayEditorDefaultTemplateOptions->setToolTip(
        tr("Available actions for the selected template"));
    d->screenplayEditorShowSceneNumber->setText(tr("Show scene number"));
    d->screenplayEditorShowSceneNumberOnLeft->setText(tr("on the left"));
    d->screenplayEditorShowSceneNumberOnRight->setText(tr("on the right"));
    d->screenplayEditorShowDialogueNumber->setText(tr("Show dialogue number"));
    d->screenplayEditorContinueDialogue->setText(
        tr("Add a continuation (CONT'D) to dialogues separated by an action"));
    d->screenplayEditorCorrectTextOnPageBreaks->setText(
        tr("Automatically correct screenplay text on page breaks"));
    d->screenplayEditorSaveItemsFromText->setText(tr("Save characters & locations on typing"));
    d->screenplayEditorShowHintsForAllItems->setText(
        tr("Show hints for all characters & locations, or only for"));
    d->screenplayEditorShowHintsForPrimaryItems->setText(tr("primary"));
    d->screenplayEditorShowHintsForSecondaryItems->setText(tr("secondary"));
    d->screenplayEditorShowHintsForTertiaryItems->setText(tr("tertiary"));
    d->screenplayEditorShowCharacterSuggestionsInEmptyBlock->setText(
        tr("Show characters suggestions in empty paragraph"));
    d->screenplayEditorUseOpenParenthesisInDialogueForParenthetical->setText(
        tr("Use opening parenthesis in dialogue to switch to parenthetical"));
    d->screenplayNavigatorTitle->setText(tr("Screenplay navigator"));
    d->screenplayNavigatorShowBeats->setText(tr("Show beats"));
    d->screenplayNavigatorShowBeatsInTreatment->setText(tr("in treatment"));
    d->screenplayNavigatorShowBeatsInScreenplay->setText(tr("in screenplay"));
    d->screenplayNavigatorShowSceneNumber->setText(tr("Show scene number"));
    d->screenplayNavigatorShowSceneText->setText(tr("Show scene text, lines"));
    d->screenplayNavigatorSceneDescriptionLines1->setText("1");
    d->screenplayNavigatorSceneDescriptionLines2->setText("2");
    d->screenplayNavigatorSceneDescriptionLines3->setText("3");
    d->screenplayNavigatorSceneDescriptionLines4->setText("4");
    d->screenplayNavigatorSceneDescriptionLines5->setText("5");
    d->screenplayDurationTitle->setText(tr("Duration"));
    d->screenplayDurationByPage->setText(tr("Calculate duration based on the count of pages"));
    d->screenplayDurationByPagePage->setLabel(tr("at the rate of"));
    d->screenplayDurationByPagePage->setSuffix(tr("pages"));
    d->screenplayDurationByPageDuration->setLabel(tr("has duration"));
    d->screenplayDurationByPageDuration->setSuffix(tr("seconds"));
    d->screenplayDurationByCharacters->setText(
        tr("Calculate duration based on the count of letters"));
    d->screenplayDurationByCharactersCharacters->setLabel(tr("at the rate of"));
    d->screenplayDurationByCharactersCharacters->setSuffix(tr("letters"));
    d->screenplayDurationByCharactersIncludingSpaces->setText(tr("including spaces"));
    d->screenplayDurationByCharactersDuration->setLabel(tr("has duration"));
    d->screenplayDurationByCharactersDuration->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurable->setText(tr("Calculate duration based on the custom rules"));
    d->screenplayDurationConfigurableForActionLabel->setText(tr("For action"));
    d->screenplayDurationConfigurablePerParagraphForAction->setLabel(tr("Per entry"));
    d->screenplayDurationConfigurablePerParagraphForAction->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurableForActionPlus->setText(tr("+"));
    d->screenplayDurationConfigurablePerEvery50ForAction->setLabel(tr("Each 50 characters"));
    d->screenplayDurationConfigurablePerEvery50ForAction->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurableForDialogueLabel->setText(tr("For dialogue"));
    d->screenplayDurationConfigurablePerParagraphForDialogue->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurableForDialoguePlus->setText(tr("+"));
    d->screenplayDurationConfigurablePerEvery50ForDialogue->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurableForSceneHeadingLabel->setText(tr("For scene heading"));
    d->screenplayDurationConfigurablePerParagraphForSceneHeading->setSuffix(tr("seconds"));
    d->screenplayDurationConfigurableForSceneHeadingPlus->setText(tr("+"));
    d->screenplayDurationConfigurablePerEvery50ForSceneHeading->setSuffix(tr("seconds"));
    d->screenplayDurationUseEighths->setText(tr("Use page eighths as a duration metric"));

    d->shortcutsTitle->setText(tr("Shortcuts"));
    //
    d->shortcutsForApplicationTitle->setText(tr("Application"));
    {
        auto model = d->shortcutsForApplicationModel->headerModel();
        model->setData(model->index(0, 0), tr("Action"), Qt::DisplayRole);
        model->setData(model->index(0, 1), tr("Shortcut"), Qt::DisplayRole);
    }
    //
    d->shortcutsForSimpleTextTitle->setText(tr("Simple text editor"));
    {
        auto model = d->shortcutsForSimpleTextModel->headerModel();
        model->setData(model->index(0, 0), tr("Block name"), Qt::DisplayRole);
        model->setData(model->index(0, 1), tr("Shortcut"), Qt::DisplayRole);
        model->setData(model->index(0, 2), tr("If you press in paragraphs end"), Qt::DisplayRole);
        model->setData(model->index(0, 3), tr("If you press in empty paragraph"), Qt::DisplayRole);
    }
    buildSimpleTextParagraphTypesModel(d->shortcutsForSimpleText, d->simpleTextParagraphTypesModel);
    d->simpleTextParagraphAddTypeDelegate->setLabel(tr("Add paragraph"));
    d->simpleTextParagraphChangeTypeDelegate->setLabel(tr("Change to"));
    //
    d->shortcutsForScreenplayTitle->setText(tr("Screenplay editor"));
    {
        auto model = d->shortcutsForScreenplayModel->headerModel();
        model->setData(model->index(0, 0), tr("Block name"), Qt::DisplayRole);
        model->setData(model->index(0, 1), tr("Shortcut"), Qt::DisplayRole);
        model->setData(model->index(0, 2), tr("If you press in paragraphs end"), Qt::DisplayRole);
        model->setData(model->index(0, 3), tr("If you press in empty paragraph"), Qt::DisplayRole);
    }
    buildScreenplayParagraphTypesModel(d->shortcutsForScreenplay, d->screenplayParagraphTypesModel);
    d->screenplayParagraphAddTypeDelegate->setLabel(tr("Add paragraph"));
    d->screenplayParagraphChangeTypeDelegate->setLabel(tr("Change to"));
    //
    d->advancedTitle->setText(tr("Advanced settings"));
    d->advancedAccurateMetricsHandling->setText(
        tr("Use advanced handling to convert pixels to millimeters"));
    d->advancedUseExtendedLogging->setText(tr("Collect extended logs about application operation"));
    d->advancedCardLayout->setContentsMargins(0, 0, 0, DesignSystem::layout().px8());
}

SettingsView::~SettingsView() = default;

void SettingsView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    StackWidget::designSystemChangeEvent(_event);

    setBackgroundColor(DesignSystem::color().surface());
    d->content->widget()->layout()->setContentsMargins(
        QMarginsF(DesignSystem::layout().px24()
                      + (isLeftToRight() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  DesignSystem::compactLayout().topContentMargin(),
                  DesignSystem::layout().px24()
                      + (isRightToLeft() ? 0.0 : DesignSystem::scrollBar().minimumSize()),
                  DesignSystem::compactLayout().px24())
            .toMargins());
    d->contextMenu->setBackgroundColor(DesignSystem::color().background());
    d->contextMenu->setTextColor(DesignSystem::color().onBackground());

    d->colorAnimation.setStartValue(DesignSystem::color().accent());

    for (auto card : {
             d->applicationCard,
             d->simpleTextCard,
             d->screenplayCard,
             d->shortcutsCard,
             d->advancedCard,
         }) {
        card->setBackgroundColor(DesignSystem::color().background());
    }

    auto titleColor = DesignSystem::color().onBackground();
    titleColor.setAlphaF(DesignSystem::inactiveTextOpacity());
    auto titleMargins = DesignSystem::label().margins().toMargins();
    titleMargins.setBottom(DesignSystem::compactLayout().px12());
    for (auto cardTitle : std::vector<Widget*>{
             d->applicationTitle,
             d->applicationUserInterfaceTitle,
             d->applicationSaveAndBackupTitle,
             d->applicationGitHubSyncTitle,
             d->applicationTextEditingTitle,
             d->applicationTextEditingSubtitle,
             d->simpleTextTitle,
             d->simpleTextEditorTitle,
             d->simpleTextNavigatorTitle,
             d->screenplayTitle,
             d->screenplayEditorTitle,
             d->screenplayNavigatorTitle,
             d->screenplayDurationTitle,
             d->shortcutsTitle,
             d->shortcutsForApplicationTitle,
             d->shortcutsForSimpleTextTitle,
             d->shortcutsForScreenplayTitle,
             d->advancedTitle,
         }) {
        cardTitle->setBackgroundColor(DesignSystem::color().background());
        cardTitle->setTextColor(titleColor);
        cardTitle->setContentsMargins(titleMargins);
    }
    titleMargins.setTop(DesignSystem::compactLayout().px(32));
    for (auto cardTitle : std::vector<Widget*>{
             d->applicationUserInterfaceTitle,
             d->applicationSaveAndBackupTitle,
             d->applicationGitHubSyncTitle,
             d->applicationTextEditingTitle,
             d->simpleTextEditorTitle,
             d->simpleTextNavigatorTitle,
             d->screenplayEditorTitle,
             d->screenplayNavigatorTitle,
             d->screenplayDurationTitle,
         }) {
        cardTitle->setContentsMargins(titleMargins);
    }
    titleMargins = DesignSystem::label().margins().toMargins();
    titleMargins.setBottom(0);
    for (auto title : std::vector<Widget*>{ d->componentsTitle }) {
        title->setBackgroundColor(DesignSystem::color().surface());
        title->setTextColor(titleColor);
        title->setContentsMargins(titleMargins);
    }
    for (auto toggle : std::vector<Widget*>{
             d->simpleTextAvailable,
             d->screenplayAvailable,
         }) {
        toggle->setBackgroundColor(DesignSystem::color().background());
        toggle->setTextColor(DesignSystem::color().onBackground());
        toggle->setContentsMargins(DesignSystem::layout().px16(), DesignSystem::layout().px16(),
                                   DesignSystem::layout().px16(), 0);
    }

    auto labelMargins = DesignSystem::label().margins().toMargins();
    labelMargins.setTop(static_cast<int>(DesignSystem::button().shadowMargins().top()));
    labelMargins.setBottom(static_cast<int>(DesignSystem::button().shadowMargins().bottom()));
    for (auto label : std::vector<Widget*>{
             d->language,
             d->scaleFactorTitle,
             d->scaleFactorSmallInfo,
             d->scaleFactorBigInfo,
             d->screenplayDurationConfigurableForActionLabel,
             d->screenplayDurationConfigurableForActionPlus,
             d->screenplayDurationConfigurableForDialogueLabel,
             d->screenplayDurationConfigurableForDialoguePlus,
             d->screenplayDurationConfigurableForSceneHeadingLabel,
             d->screenplayDurationConfigurableForSceneHeadingPlus,
         }) {
        label->setBackgroundColor(DesignSystem::color().background());
        label->setTextColor(DesignSystem::color().onBackground());
        label->setContentsMargins(labelMargins);
    }

    auto iconLabelMargins = labelMargins;
    iconLabelMargins.setLeft(0);
    for (auto iconLabel : std::vector<Widget*>{
             d->spellCheckerUserDictionary,
             d->simpleTextEditorDefaultTemplateOptions,
             d->screenplayEditorDefaultTemplateOptions,
         }) {
        iconLabel->setBackgroundColor(DesignSystem::color().background());
        iconLabel->setTextColor(DesignSystem::color().onBackground());
        iconLabel->setContentsMargins(iconLabelMargins);
    }

    for (auto checkBox : std::vector<Widget*>{
             d->isCompact,
             d->autoSave,
             d->saveBackups,
             d->gitHubSyncUploadOnExit,
             //
             d->showDocumentsPages,
             d->useTypewriterSound,
             d->useSpellChecker,
             d->highlightCurrentLine,
             d->focusCurrentParagraph,
             d->useTypewriterScrolling,
             d->correctDoubleCapitals,
             d->capitalizeSingleILetter,
             d->replaceThreeDotsWithEllipsis,
             d->useSmartQuotes,
             d->replaceTwoDashesWithEmDash,
             d->avoidMultipleSpaces,
             //
             d->simpleTextNavigatorShowSceneText,
             //
             d->screenplayEditorShowSceneNumber,
             d->screenplayEditorShowSceneNumberOnLeft,
             d->screenplayEditorShowSceneNumberOnRight,
             d->screenplayEditorShowDialogueNumber,
             d->screenplayEditorContinueDialogue,
             d->screenplayEditorCorrectTextOnPageBreaks,
             d->screenplayEditorSaveItemsFromText,
             d->screenplayEditorShowHintsForAllItems,
             d->screenplayEditorShowHintsForPrimaryItems,
             d->screenplayEditorShowHintsForSecondaryItems,
             d->screenplayEditorShowHintsForTertiaryItems,
             d->screenplayEditorShowCharacterSuggestionsInEmptyBlock,
             d->screenplayEditorUseOpenParenthesisInDialogueForParenthetical,
             d->screenplayNavigatorShowBeats,
             d->screenplayNavigatorShowBeatsInTreatment,
             d->screenplayNavigatorShowBeatsInScreenplay,
             d->screenplayNavigatorShowSceneNumber,
             d->screenplayNavigatorShowSceneText,
             d->screenplayDurationByCharactersIncludingSpaces,
             //
             d->advancedAccurateMetricsHandling,
             d->advancedUseExtendedLogging,
         }) {
        checkBox->setBackgroundColor(DesignSystem::color().background());
        checkBox->setTextColor(DesignSystem::color().onBackground());
    }

    for (auto radioButton : {
             d->simpleTextNavigatorSceneDescriptionLines1,
             d->simpleTextNavigatorSceneDescriptionLines2,
             d->simpleTextNavigatorSceneDescriptionLines3,
             d->simpleTextNavigatorSceneDescriptionLines4,
             d->simpleTextNavigatorSceneDescriptionLines5,
             //
             d->screenplayNavigatorSceneDescriptionLines1,
             d->screenplayNavigatorSceneDescriptionLines2,
             d->screenplayNavigatorSceneDescriptionLines3,
             d->screenplayNavigatorSceneDescriptionLines4,
             d->screenplayNavigatorSceneDescriptionLines5,
             d->screenplayDurationByPage,
             d->screenplayDurationByCharacters,
             d->screenplayDurationConfigurable,
             d->screenplayDurationUseEighths,
         }) {
        radioButton->setBackgroundColor(DesignSystem::color().background());
        radioButton->setTextColor(DesignSystem::color().onBackground());
    }

    for (auto textField : std::vector<TextField*>{
             d->backupsFolderPath,
             d->backupsQty,
             d->gitHubSyncRepository,
             d->gitHubSyncToken,
             d->spellCheckerLanguage,
             //
             d->simpleTextEditorDefaultTemplate,
             //
             d->screenplayEditorDefaultTemplate,
             d->screenplayDurationByPagePage,
             d->screenplayDurationByPageDuration,
             d->screenplayDurationByCharactersCharacters,
             d->screenplayDurationByCharactersDuration,
             d->screenplayDurationConfigurablePerParagraphForAction,
             d->screenplayDurationConfigurablePerEvery50ForAction,
             d->screenplayDurationConfigurablePerParagraphForDialogue,
             d->screenplayDurationConfigurablePerEvery50ForDialogue,
             d->screenplayDurationConfigurablePerParagraphForSceneHeading,
             d->screenplayDurationConfigurablePerEvery50ForSceneHeading,
         }) {
        textField->setBackgroundColor(DesignSystem::color().onBackground());
        textField->setTextColor(DesignSystem::color().onBackground());
    }
    d->backupsQty->setCustomMargins({ isLeftToRight() ? 0.0 : DesignSystem::layout().px24(), 0.0,
                                      isLeftToRight() ? DesignSystem::layout().px24() : 0.0, 0.0 });
    d->gitHubSyncToken->setCustomMargins(
        { isLeftToRight() ? 0.0 : DesignSystem::layout().px24(), 0.0,
          isLeftToRight() ? DesignSystem::layout().px24() : 0.0, 0.0 });
    for (auto textField : {
             d->simpleTextEditorDefaultTemplate,
             d->screenplayEditorDefaultTemplate,
         }) {
        textField->setCustomMargins({ isLeftToRight() ? DesignSystem::layout().px24() : 0, 0,
                                      isLeftToRight() ? 0 : DesignSystem::layout().px24(), 0 });
    }
    for (auto combobox : {
             d->spellCheckerLanguage,
             d->simpleTextEditorDefaultTemplate,
             d->screenplayEditorDefaultTemplate,
         }) {
        combobox->setPopupBackgroundColor(DesignSystem::color().background());
    }
    for (auto icon : {
             d->simpleTextEditorDefaultTemplateOptions,
             d->screenplayEditorDefaultTemplateOptions,
         }) {
        icon->setContentsMargins(isLeftToRight() ? 0 : DesignSystem::layout().px16(), 0,
                                 isLeftToRight() ? DesignSystem::layout().px16() : 0, 0);
    }

    for (auto button : { d->changeLanuage }) {
        button->setBackgroundColor(DesignSystem::color().accent());
        button->setTextColor(DesignSystem::color().accent());
    }

    for (auto theme : {
             d->lightTheme,
             d->darkAndLightTheme,
             d->darkTheme,
             d->customTheme,
         }) {
        theme->setBackgroundColor(DesignSystem::color().background());
        theme->setTextColor(DesignSystem::color().onBackground());
    }

    d->scaleFactor->setBackgroundColor(DesignSystem::color().background());
    d->scaleFactor->setContentsMargins({ static_cast<int>(DesignSystem::layout().px24()), 0,
                                         static_cast<int>(DesignSystem::layout().px24()), 0 });

    d->applicationCardLayout->setRowMinimumHeight(
        d->applicationCardBottomSpacerIndex,
        static_cast<int>(DesignSystem::compactLayout().px24()));
    d->applicationThemesLayout->setSpacing(DesignSystem::compactLayout().px24());
    d->applicationThemesLayout->setContentsMargins(
        QMargins(DesignSystem::layout().px24(), DesignSystem::compactLayout().px12(),
                 DesignSystem::layout().px24(), DesignSystem::compactLayout().px16()));
    //
    d->simpleTextCardLayout->setRowMinimumHeight(d->simpleTextCardBottomSpacerIndex,
                                                 d->simpleTextAvailable->isChecked()
                                                     ? DesignSystem::compactLayout().px24()
                                                     : DesignSystem::compactLayout().px12());
    //
    d->screenplayCardLayout->setRowMinimumHeight(d->screenplayCardBottomSpacerIndex,
                                                 d->screenplayAvailable->isChecked()
                                                     ? DesignSystem::compactLayout().px24()
                                                     : DesignSystem::compactLayout().px12());
    const auto screenplayDurationByCharactersRow
        = d->screenplayCardLayout->indexOf(d->screenplayDurationByCharacters);
    d->screenplayCardLayout->setRowMinimumHeight(
        screenplayDurationByCharactersRow,
        d->screenplayAvailable->isChecked() ? DesignSystem::compactLayout().px62() : 0.0);
    const auto screenplayDurationConfigurableRow
        = d->screenplayCardLayout->indexOf(d->screenplayDurationConfigurable);
    d->screenplayCardLayout->setRowMinimumHeight(
        screenplayDurationConfigurableRow,
        d->screenplayAvailable->isChecked() ? DesignSystem::compactLayout().px62() : 0.0);
    const auto screenplayDurationUseEighthsRow
        = d->screenplayCardLayout->indexOf(d->screenplayDurationUseEighths);
    d->screenplayCardLayout->setRowMinimumHeight(screenplayDurationUseEighthsRow,
                                                 DesignSystem::compactLayout().px62());
    //
    d->shortcutsCardLayout->setRowMinimumHeight(
        d->shortcutsCardBottomSpacerIndex, static_cast<int>(DesignSystem::compactLayout().px24()));

    for (auto table : {
             d->shortcutsForApplication,
             d->shortcutsForSimpleText,
             d->shortcutsForScreenplay,
         }) {
        table->setBackgroundColor(DesignSystem::color().background());
        table->setTextColor(DesignSystem::color().onBackground());
    }
    d->updateTablesGeometry();
}

} // namespace Ui
