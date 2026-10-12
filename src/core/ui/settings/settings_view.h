#pragma once

#include <ui/design_system/design_system.h>
#include <ui/widgets/stack_widget/stack_widget.h>

class QAbstractItemModel;
class HierarchicalModel;


namespace Ui {

/**
 * @brief Представление настроек
 */
class SettingsView : public StackWidget
{
    Q_OBJECT

public:
    explicit SettingsView(QWidget* _parent = nullptr);
    ~SettingsView() override;

    /**
     * @brief Показать основную страницу
     */
    void showDefaultPage();

    /**
     * @brief По возможности сфокусировать на экране заданный виджет
     */
    void showApplication();
    void showApplicationUserInterface();
    void showApplicationSaveAndBackups();
    void showApplicationGitHubSync();
    void showApplicationTextEditing();

    void showComponents();
    void showComponentsSimpleText();
    void showComponentsScreenplay();
    void showComponentsNovel();
    void showShortcuts();
    void showAdvanced();

    //
    // Задание параметров приложения
    //
    void setApplicationLanguage(int _language);
    void setApplicationScaleFactor(qreal _scaleFactor);
    void setApplicationDensity(int _density);
    void setApplicationUseAutoSave(bool _use);
    void setApplicationSaveBackups(bool _save);
    void setApplicationBackupsFolder(const QString& _path);
    void setApplicationBackupsQty(int _qty);
    void setApplicationGitHubSyncRepository(const QString& _repository);
    void setApplicationGitHubSyncToken(const QString& _token);
    void setApplicationGitHubSyncUploadOnExit(bool _upload);
    void setApplicationShowDocumentsPages(bool _show);
    void setApplicationUseTypewriterSound(bool _use);
    void setApplicationUseSpellChecker(bool _use);
    void setApplicationSpellCheckerLanguage(const QString& _languageCode);
    void setApplicationHighlightCurrentLine(bool _highlight);
    void setApplicationFocusCurrentParagraph(bool _focus);
    void setApplicationUseTypewriterScrolling(bool _use);
    void setApplicationCorrectDoubleCapitals(bool _correct);
    void setApplicationCapitalizeSinglaILetter(bool _capitalize);
    void setApplicationReplaceThreeDotsWithEllipsis(bool _replace);
    void setApplicationUseSmartQuotes(bool _use);
    void setApplicationReplaceTwoDashesWithEmDash(bool _replace);
    void setApplicationAvoidMultipleSpaces(bool _avoid);

    //
    // Задание параметров редактора текста
    //
    void setSimpleTextAvailable(bool _available);
    //
    void setSimpleTextEditorDefaultTemplate(const QString& _templateId);
    //
    void setSimpleTextNavigatorShowSceneText(bool _show, int _lines);

    //
    // Задание параметров редактора сценария
    //
    void setScreenplayAvailable(bool _available);
    //
    void setScreenplayEditorDefaultTemplate(const QString& _templateId);
    void setScreenplayEditorShowSceneNumber(bool _show, bool _atLeft, bool _atRight);
    void setScreenplayEditorShowDialogueNumber(bool _show);
    void setScreenplayEditorContinueDialogue(bool _continue);
    void setScreenplayEditorCorrectTextOnPageBreaks(bool _correct);
    void setScreenplayEditorSaveItemsFromText(bool _save);
    void setScreenplayEditorShowHints(bool _all, bool _primary, bool _secondary, bool _tertiary);
    void setScreenplayEditorShowCharacterSuggestionsInEmptyBlock(bool _show);
    void setScreenplayEditorUseOpenParenthesisInDialogueForParenthetical(bool _use);
    //
    void setScreenplayNavigatorShowBeats(bool _show, bool _inTreatment, bool _inScreenplay);
    void setScreenplayNavigatorShowSceneNumber(bool _show);
    void setScreenplayNavigatorShowSceneText(bool _show, int _lines);
    //
    void setScreenplayDurationType(int _type);
    void setScreenplayDurationByPageDuration(int _duration);
    void setScreenplayDurationByCharactersCharacters(int _characters);
    void setScreenplayDurationByCharactersIncludeSpaces(bool _include);
    void setScreenplayDurationByCharactersDuration(int _duration);
    void setScreenplayDurationConfigurablePerParagraphForAction(qreal _duration);
    void setScreenplayDurationConfigurablePerEvery50ForAction(qreal _duration);
    void setScreenplayDurationConfigurablePerParagraphForDialogue(qreal _duration);
    void setScreenplayDurationConfigurablePerEvery50ForDialogue(qreal _duration);
    void setScreenplayDurationConfigurablePerParagraphForSceneHeading(qreal _duration);
    void setScreenplayDurationConfigurablePerEvery50ForSceneHeading(qreal _duration);
    void setScreenplayDurationUseEighths(bool _use);

    //
    // Задание параметров редактора текста
    //
    void setNovelAvailable(bool _available);
    //
    void setNovelEditorDefaultTemplate(const QString& _templateId);
    //
    void setNovelNavigatorShowSceneText(bool _show, int _lines);
    void setNovelNavigatorCounterType(int _type);

    //
    // Задание параметров горячих клавиш
    //
    void setShortcutsForApplicationModel(HierarchicalModel* _model);
    void setShortcutsForSimpleTextModel(HierarchicalModel* _model);
    void setShortcutsForScreenplayModel(HierarchicalModel* _model);
    void setShortcutsForNovelModel(HierarchicalModel* _model);

    //
    // Задание экспериментальных параметров
    //
    void setAdvancedAccurateMetricsHandling(bool _accurate);
    void setAdvancedUseExtendedLogging(bool _use);

signals:
    /**
     * @brief Пользователь нажал кнопку выборя языка
     */
    void applicationLanguagePressed();

    //
    // Уведомление об изменении параметров приложения
    //
    void applicationThemePressed(Ui::ApplicationTheme _theme);
    void customThemeHashPasted(const DesignSystem::Color& _color);
    void applicationScaleFactorChanged(qreal _scaleFactor);
    void applicationDensityChanged(int _density);
    void applicationUseAutoSaveChanged(bool _use);
    void applicationSaveBackupsChanged(bool _save);
    void applicationBackupsFolderChanged(const QString& _path);
    void applicationBackupsQtyChanged(int _qty);
    void applicationGitHubSyncRepositoryChanged(const QString& _repository);
    void applicationGitHubSyncTokenChanged(const QString& _token);
    void applicationGitHubSyncUploadOnExitChanged(bool _upload);
    void applicationShowDocumentsPagesChanged(bool _show);
    void applicationUseTypewriterSoundChanged(bool _use);
    void applicationUseSpellCheckerChanged(bool _use);
    void applicationSpellCheckerLanguageChanged(const QString& _languageCode);
    void applicationHighlightCurentLineChanged(bool _highlight);
    void applicationFocusCurrentParagraphChanged(bool _focus);
    void applicationUseTypewriterScrollingChanged(bool _use);
    void applicationCorrectDoubleCapitalsChanged(bool _correct);
    void applicationCapitalizeSingleILetterChanged(bool _capitalize);
    void applicationReplaceThreeDotsWithEllipsisChanged(bool _replace);
    void applicationUseSmartQuotesChanged(bool _use);
    void applicationReplaceTwoDashedWithEmDashChanged(bool _replace);
    void applicationAvoidMultipleSpacesChanged(bool _avoid);

    //
    // Уведомление об изменении параметров редактора текста
    //
    void simpleTextAvailableChanged(bool _available);
    //
    void simpleTextEditorDefaultTemplateChanged(const QString& _templateId);
    //
    void simpleTextNavigatorShowSceneTextChanged(bool _show, int _lines);

    //
    // Уведомление об изменении параметров редактора сценария
    //
    void screenplayAvailableChanged(bool _available);
    //
    void screenplayEditorDefaultTemplateChanged(const QString& _templateId);
    void screenplayEditorShowSceneNumberChanged(bool _show, bool _atLeft, bool _atRight);
    void screenplayEditorShowDialogueNumberChanged(bool _show);
    void screenplayEditorContinueDialogueChanged(bool _continue);
    void screenplayEditorCorrectTextOnPageBreaksChanged(bool _correct);
    void screenplayEditorSaveItemsFromTextChanged(bool _save);
    void screenplayEditorShowHintsChanged(bool _all, bool _primary, bool _secondary,
                                          bool _tertiary);
    void screenplayEditorShowCharacterSuggestionsInEmptyBlockChanged(bool _show);
    void screenplayEditorUseOpenBracketInDialogueForParentheticalChanged(bool _use);
    //
    void screenplayNavigatorShowBeatsChanged(bool _show, bool _inTreatment, bool _inScreenplay);
    void screenplayNavigatorShowSceneNumberChanged(bool _show);
    void screenplayNavigatorShowSceneTextChanged(bool _show, int _lines);
    //
    void screenplayDurationUseEighthsChanged(bool _use);
    void screenplayDurationTypeChanged(int _type);
    void screenplayDurationByPageDurationChanged(int _duration);
    void screenplayDurationByCharactersCharactersChanged(int _characters);
    void screenplayDurationByCharactersIncludeSpacesChanged(bool _include);
    void screenplayDurationByCharactersDurationChanged(int _duration);
    void screenplayDurationConfigurablePerParagraphForActionChanged(qreal _duration);
    void screenplayDurationConfigurablePerEvery50ForActionChanged(qreal _duration);
    void screenplayDurationConfigurablePerParagraphForDialogueChanged(qreal _duration);
    void screenplayDurationConfigurablePerEvery50ForDialogueChanged(qreal _duration);
    void screenplayDurationConfigurablePerParagraphForSceneHeadingChanged(qreal _duration);
    void screenplayDurationConfigurablePerEvery50ForSceneHeadingChanged(qreal _duration);

    //
    // Уведомление об изменении параметров редактора романов
    //
    void novelAvailableChanged(bool _available);
    //
    void novelEditorDefaultTemplateChanged(const QString& _templateId);
    //
    void novelNavigatorShowSceneTextChanged(bool _show, int _lines);
    void novelNavigatorCounterTypeChanged(int _type);

    //
    // Редактирование шаблонов
    //
    // ... простой текстовый документ
    //
    void editCurrentSimpleTextEditorTemplateRequested(const QString& _templateId);
    void duplicateCurrentSimpleTextEditorTemplateRequested(const QString& _templateId);
    void saveToFileCurrentSimpleTextEditorTemplateRequested(const QString& _templateId);
    void removeCurrentSimpleTextEditorTemplateRequested(const QString& _templateId);
    void loadFromFileSimpleTextEditorTemplateRequested();
    //
    // ... сценарий
    //
    void editCurrentScreenplayEditorTemplateRequested(const QString& _templateId);
    void duplicateCurrentScreenplayEditorTemplateRequested(const QString& _templateId);
    void saveToFileCurrentScreenplayEditorTemplateRequested(const QString& _templateId);
    void removeCurrentScreenplayEditorTemplateRequested(const QString& _templateId);
    void loadFromFileScreenplayEditorTemplateRequested();
    //
    // ... роман
    //
    void editCurrentNovelEditorTemplateRequested(const QString& _templateId);
    void duplicateCurrentNovelEditorTemplateRequested(const QString& _templateId);
    void saveToFileCurrentNovelEditorTemplateRequested(const QString& _templateId);
    void removeCurrentNovelEditorTemplateRequested(const QString& _templateId);
    void loadFromFileNovelEditorTemplateRequested();

    //
    // Параметры горячих клавиш
    //
    void shortcutForApplicationChanged(const QString& _blockType, const QString& _shortcut);
    void shortcutsForSimpleTextEditorChanged(const QString& _blockType, const QString& _shortcut,
                                             const QString& _jumpByTab, const QString& _jumpByEnter,
                                             const QString& _changeByTab,
                                             const QString& _changeByEnter);
    void shortcutsForScreenplayEditorChanged(const QString& _blockType, const QString& _shortcut,
                                             const QString& _jumpByTab, const QString& _jumpByEnter,
                                             const QString& _changeByTab,
                                             const QString& _changeByEnter);
    void shortcutsForNovelEditorChanged(const QString& _blockType, const QString& _shortcut,
                                        const QString& _jumpByTab, const QString& _jumpByEnter,
                                        const QString& _changeByTab, const QString& _changeByEnter);

    //
    // Уведомления об изменении экспериментальных параметров
    //
    void advancedAccurateMetricsHandlingChanged(bool _use);
    void advancedUseExtendedLoggingChanged(bool _use);

protected:
    /**
     * @brief Keep the GitHub repository and the access token exactly as they were entered
     */
    bool eventFilter(QObject* _watched, QEvent* _event) override;

    /**
     * @brief Переопределяем для обновления размеров таблиц
     */
    void resizeEvent(QResizeEvent* _event) override;

    /**
     * @brief Обновить переводы
     */
    void updateTranslations() override;

    /**
     * @brief Обновляем виджет при изменении дизайн системы
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
