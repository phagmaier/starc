#include "application_manager.h"

#include "content/export/export_manager.h"
#include "content/github_sync/github_sync_manager.h"
#include "content/import/import_manager.h"
#include "content/notifications/notifications_manager.h"
#include "content/onboarding/onboarding_manager.h"
#include "content/project/project_manager.h"
#include "content/projects/projects_manager.h"
#include "content/projects/projects_model_project_item.h"
#include "content/settings/settings_manager.h"
#include "content/shortcuts/shortcuts_manager.h"
#include "content/writing_session/writing_session_manager.h"
#include "plugins_builder.h"

#ifdef CLOUD_SERVICE_MANAGER
#include <cloud/cloud_service_manager.h>
#endif

// #define PRINT_DOCUMENT_HISTORY
#ifdef PRINT_DOCUMENT_HISTORY
#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <data_layer/mapper/document_change_mapper.h>
#include <data_layer/mapper/mapper_facade.h>
#include <data_layer/storage/document_image_storage.h>
#endif

#include <business_layer/model/abstract_model.h>
#include <data_layer/database.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_change_object.h>
#include <domain/document_object.h>
#include <domain/objects_builder.h>
#include <domain/starcloud_api.h>
#include <include/custom_events.h>
#include <interfaces/management_layer/i_document_manager.h>
#include <ui/application_style.h>
#include <ui/application_view.h>
#include <ui/connection_status_tool_bar.h>
#include <ui/design_system/design_system.h>
#include <ui/menu_view.h>
#include <ui/modules/avatar_generator/avatar_generator.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/dialog/standard_dialog.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <ui/widgets/text_edit/scalable_wrapper/scalable_wrapper.h>
#include <ui/widgets/text_edit/spell_check/spell_check_text_edit.h>
#include <utils/3rd_party/WAF/Animation/Animation.h>
#include <utils/helpers/dialog_helper.h>
#include <utils/helpers/extension_helper.h>
#include <utils/helpers/image_helper.h>
#include <utils/helpers/measurement_helper.h>
#include <utils/helpers/platform_helper.h>
#include <utils/logging.h>
#include <utils/tools/backup_builder.h>
#include <utils/tools/once.h>
#include <utils/tools/run_once.h>
#include <utils/validators/email_validator.h>

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QLocale>
#include <QLockFile>
#include <QMenu>
#include <QProcess>
#include <QRawFont>
#include <QScopedPointer>
#include <QShortcut>
#include <QSoundEffect>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QUuid>
#include <QVariant>
#include <QWidget>
#include <QtConcurrentRun>

#include <NetworkRequest.h>

namespace ManagementLayer {

namespace {
/**
 * @brief Состояние приложения
 */
enum class ApplicationState {
    Initializing,
    ProjectLoading,
    ProjectClosing,
    Working,
    Importing,
};

/**
 * @brief Перезапустить приложение
 */
void restartApplication()
{
    QApplication::quit();

    const auto arguments = QApplication::arguments();
    if (!arguments.isEmpty()) {
        QProcess::startDetached(arguments.constFirst(), {});
    }
}

} // namespace

class ApplicationManager::Implementation
{
public:
    explicit Implementation(ApplicationManager* _q);
    ~Implementation();

    /**
     * @brief Настроить способ работы с конвертацией метрик
     */
    void initMeasurementMetrics();

    /**
     * @brief Настроить логгирование
     */
    void initLogging();

    /**
     * @brief Настроить док-меню
     */
    void initDockMenu();

    /**
     * @brief Предложить обновиться до последней версии
     */
    void askUpdateToLatestVersion();

    /**
     * @brief Настроить параметры автосохранения
     */
    void configureAutoSave();

    /**
     * @brief Показать контент приложения
     */
    void showContent();
    void openStartupProject();

    /**
     * @brief Показать меню приложения
     */
    void showMenu();

    /**
     * @brief Показать личный кабинет
     */

    /**
     * @brief Показать онбординг
     */
    void showOnboarding();

    /**
     * @brief Показать страницу проектов
     */
    void showProjects();

    /**
     * @brief Показать страницу проекта
     */
    void showProject();

    /**
     * @brief Показать страницу настроек
     */
    void showSettings();

    /**
     * @brief Показать страницу статистика работы с программой
     */
    void showSessionStatistics();

    /**
     * @brief Показать предыдущий контент
     */
    void showLastContent();

    /**
     * @brief Установить перевод
     */
    void setTranslation();

    /**
     * @brief Установить тему
     */
    void setDesignSystemTheme(Ui::ApplicationTheme _theme);

    /**
     * @brief Установить цвета кастомной темы
     */
    void setDesignSystemCustomThemeColors(const Ui::DesignSystem::Color& _color);

    /**
     * @brief Установить коэффициент масштабирования
     */
    void setDesignSystemScaleFactor(qreal _scaleFactor);

    /**
     * @brief Задать использование компакстного режима
     */
    void setDesignSystemDensity(int _density);

    //
    // Работа с проектом
    //

    /**
     * @brief Обновить заголовок приложения
     */
    void updateWindowTitle(const QString& _projectName = {});

    /**
     * @brief Настроить состояние приложения сохранены ли все изменения или нет
     */
    void markChangesSaved(bool _saved);

    /**
     * @brief Сохранить изменения проекта
     * @return Whether the story has no unsaved changes after this attempt
     */
    bool saveChanges();

    /**
     * @brief Если проект был изменён, но не сохранён предложить пользователю сохранить его
     * @param _callback - метод, который будет вызван, если пользователь хочет (Да),
     *        или не хочет (Нет) сохранять и не будет вызван, если пользователь передумал (Отмена)
     *        or the requested save fails
     */
    void saveIfNeeded(std::function<void()> _callback);

    /**
     * @brief Сохранить проект как другой файл
     */
    void saveAs();

    /**
     * @brief Создать проект
     */
    void createProject();
    bool createLocalProject(const QString& _projectName, const QString& _projectPath,
                            const QString& _importFilePath);

    /**
     * @brief Открыть проект по заданному пути
     */
    void openProject();
    bool openProject(const QString& _path);

    /**
     * @brief Попробовать захватить владение файлом, заблокировав его изменение другими копиями
     *        приложения
     */
    bool tryLockProject(const QString& _path, QScopedPointer<QLockFile>& _lockFile);

    /**
     * @brief Попробовать захватить владение файлом, заблокировав его изменение другими копиями
     *        приложения при открытии проекта
     * @note В данном методе есть возможность зафорсить открытие проекта
     */
    bool tryLockProjectOnOpen(const QString& _path);

    /**
     * @brief Перейти к редактированию текущего проекта
     */
    bool goToEditCurrentProject(bool _afterProjectCreation, const QString& _importFilePath);

    /**
     * @brief Закрыть текущий проект
     */
    void closeCurrentProject();

    /**
     * @brief Импортировать проект
     */
    void importProject();

    /**
     * @brief Экспортировать текущий документ
     */
    void exportCurrentDocument();

    /**
     * @brief Активировать полноэкранный режим
     */
    void toggleFullScreen();

    //

    /**
     * @brief Если необходимо имитировать звуки печатной машинки в зависимости от нажатой клавиши
     */
    void imitateTypewriterSound(QKeyEvent* _event) const;

    /**
     * @brief Выйти из приложения
     */
    void exit();

    //
    // Данные
    //

    ApplicationManager* q = nullptr;

    /**
     * @brief Используем для блокировки файла во время работы
     */
    QScopedPointer<QLockFile> lockFile;

    /**
     * @brief Док-меню
     */
    QMenu* dockMenu = nullptr;

    /**
     * @brief Интерфейс приложения
     */
    Ui::ApplicationView* applicationView = nullptr;
    Ui::MenuView* menuView = nullptr;
    QShortcut* importShortcut = nullptr;
    QShortcut* exportShortcut = nullptr;
    struct LastContent {
        QWidget* toolBar = nullptr;
        QWidget* navigator = nullptr;
        QWidget* view = nullptr;
        bool isHideNavigatorButtonAvailable = false;
    } lastContent;
    Ui::ConnectionStatusToolBar* connectionStatus = nullptr;
    QPointer<Dialog> saveChangesDialog;

    /**
     * @brief Построитель плагинов редакторов
     */
    PluginsBuilder pluginsBuilder;

    /**
     * @brief Менеджеры управляющие конкретными частями приложения
     */
    QScopedPointer<ShortcutsManager> shortcutsManager;
    QScopedPointer<OnboardingManager> onboardingManager;
    QScopedPointer<ProjectsManager> projectsManager;
    QScopedPointer<ProjectManager> projectManager;
    QScopedPointer<ImportManager> importManager;
    QScopedPointer<ExportManager> exportManager;
    QScopedPointer<SettingsManager> settingsManager;
    QScopedPointer<WritingSessionManager> writingSessionManager;
    QScopedPointer<NotificationsManager> notificationsManager;
    QScopedPointer<GitHubSyncManager> gitHubSyncManager;
#ifdef CLOUD_SERVICE_MANAGER
    QScopedPointer<CloudServiceManager> cloudServiceManager;
    bool isCloudAccountAuthorized = false;
#endif

    /**
     * @brief Таймер автосохранения проекта
     */
    QTimer autosaveTimer;

    /**
     * @brief Дата и время последней активности пользователя
     */
    QDateTime lastActivityDateTime;

    /**
     * @brief Состояние приложения в данный момент
     */
    ApplicationState state = ApplicationState::Initializing;

    bool startupScheduled = false;
    bool startupOpenPending = true;
    bool startupRestoreAllowed = false;
    QString startupFilePath;

private:
    template<typename Manager>
    void showContent(Manager* _manager);

    template<typename Manager>
    void saveLastContent(Manager* _manager);
};

ApplicationManager::Implementation::Implementation(ApplicationManager* _q)
    : q(_q)
    , applicationView(new Ui::ApplicationView)
    , menuView(new Ui::MenuView(applicationView))
    , connectionStatus(new Ui::ConnectionStatusToolBar(applicationView))
    , shortcutsManager(new ShortcutsManager(nullptr))
    , onboardingManager(new OnboardingManager(nullptr, applicationView))
    , projectsManager(new ProjectsManager(nullptr, applicationView))
    , projectManager(new ProjectManager(nullptr, applicationView, pluginsBuilder))
    , importManager(new ImportManager(nullptr, applicationView, pluginsBuilder))
    , exportManager(new ExportManager(nullptr, applicationView))
    , settingsManager(
          new SettingsManager(nullptr, applicationView, pluginsBuilder, shortcutsManager.data()))
    , writingSessionManager(new WritingSessionManager(nullptr, applicationView->view()))
    , notificationsManager(new NotificationsManager)
    , gitHubSyncManager(new GitHubSyncManager(nullptr, applicationView, projectsManager.data()))
#ifdef CLOUD_SERVICE_MANAGER
    , cloudServiceManager(new CloudServiceManager)
#endif
{
    initDockMenu();

    menuView->setImportShortcut(shortcutsManager->importShortcut());
    menuView->setCurrentDocumentExportShortcut(shortcutsManager->currentDocumentExportShortcut());

#ifdef CLOUD_SERVICE_MANAGER
    projectManager->setBlockedDocumentTypes(cloudServiceManager->blockedDocumentTypes());
#endif

    settingsManager->setThemeSetupView(applicationView->themeSetupView());

#ifdef Q_OS_MACOS
    new QShortcut(
        QKeySequence("Ctrl+M"), applicationView, [this] { applicationView->showMinimized(); },
        Qt::ApplicationShortcut);
#endif
}

ApplicationManager::Implementation::~Implementation()
{
    for (auto object : std::vector<QObject*>{
             applicationView,
             menuView,
             connectionStatus,
             onboardingManager.data(),
             projectsManager.data(),
             projectManager.data(),
             importManager.data(),
             exportManager.data(),
             writingSessionManager.data(),
             gitHubSyncManager.data(),
#ifdef CLOUD_SERVICE_MANAGER
             cloudServiceManager.data(),
#endif
         }) {
        object->disconnect();
    }
}

void ApplicationManager::Implementation::initMeasurementMetrics()
{
    MeasurementHelper::setAccurateMetricsHandling(
        settingsValue(DataStorageLayer::kApplicationAdvancedAccurateMetricsHandlingKey).toBool());
}

void ApplicationManager::Implementation::initLogging()
{
    //
    // Удаляем старые логи
    //
    const auto logFileFolder
        = QString("%1/starc/logs")
              .arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    auto logFiles = QDir(logFileFolder).entryInfoList(QDir::Files, QDir::Time);
    constexpr int maxLogFilesSize = 11;
    while (logFiles.size() > maxLogFilesSize) {
        QFile::remove(logFiles.takeLast().absoluteFilePath());
    }

    //
    // Настраиваем текущий лог файл
    //
    const auto logFilePath
        = QString("%1/%2.log")
              .arg(logFileFolder,
                   PlatformHelper::systemSavebleFileName(
                       QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    const auto loggingLevel =
#if defined(QT_DEBUG) || (defined(DEV_BUILD) && DEV_BUILD > 0)
        Log::Level::Trace;
#else
        static_cast<Log::Level>(
            settingsValue(DataStorageLayer::kApplicationLoggingLevelKey).toInt());
#endif
    Log::init(loggingLevel, logFilePath);

    QString applicationVersion = "0.8.4";
#if defined(DEV_BUILD) && DEV_BUILD > 0
    applicationVersion += QString(" dev %1").arg(DEV_BUILD);
#endif
    QApplication::setApplicationVersion(applicationVersion);

    Log::info("%1 version %2, %3, %4", QApplication::applicationName(),
              QApplication::applicationVersion(), QSysInfo().prettyProductName(),
              QSysInfo().currentCpuArchitecture());
}

void ApplicationManager::Implementation::initDockMenu()
{
#ifdef Q_OS_MAC
    Log::info("Init dock menu");
    //
    // Добавляем в маке возможность открытия ещё одного окна приложения
    //

    //
    // Если меню, ещё не было создано, то конфигурируем его
    //
    if (dockMenu == nullptr) {
        dockMenu = new QMenu(applicationView);
        auto openNewWindow = dockMenu->addAction(tr("Open new window"));
        connect(openNewWindow, &QAction::triggered, applicationView, [] {
            QString appPath = QApplication::applicationFilePath();
            appPath = appPath.split(".app").first();
            appPath += ".app";
            QProcess::startDetached("open", { "-na", appPath });
        });
        dockMenu->setAsDockMenu();
    }
    //
    // А если оно уже создано, то обновим переводы
    //
    else {
        constexpr int openNewWindowIndex = 0;
        dockMenu->actions().at(openNewWindowIndex)->setText(tr("Open new window"));
    }

#endif
}

void ApplicationManager::Implementation::askUpdateToLatestVersion()
{
    //
    // Добавим флаг сигнализирующий о том, что диалог уже показан, чтобы избежать множественного
    // отображения диалогов, если приложение продолжить стучаться на сервер
    //
    static bool isUpdateAlreadyOffered = false;
    if (isUpdateAlreadyOffered) {
        return;
    }
    isUpdateAlreadyOffered = true;

    auto dialog = new Dialog(applicationView);
    const int kCancelButtonId = 0;
    const int kYesButtonId = 1;
    dialog->showDialog(
        {}, tr("Please update to the latest version if you want to use the Cloud."),
        { { kCancelButtonId, tr("Nope, I’m fine without Cloud"), Dialog::RejectButton },
          { kYesButtonId, tr("Update"), Dialog::AcceptButton } });
    QObject::connect(
        dialog, &Dialog::finished, dialog,
        [this, dialog, kCancelButtonId](const Dialog::ButtonInfo& _buttonInfo) {
            //
            // Если текущий проект был в облаке, то вне зависимости от решения пользователя, закроем
            // текущий проект и вернёмся к списку проектов
            //
            const auto currentProject = projectsManager->currentProject();
            if (currentProject != nullptr && currentProject->isRemote()) {
                closeCurrentProject();
                showProjects();
            }

            dialog->hideDialog();
            isUpdateAlreadyOffered = false;

            //
            // Пользователь не хочет обновляться
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Загружаем апдейт и после загрузки атоматически стартуем его
            //
            const auto taskId = Q_FUNC_INFO;
            TaskBar::addTask(taskId);
            TaskBar::setTaskTitle(taskId, tr("The last version is downloading"));
            //
            auto downloader = new NetworkRequest;
            connect(downloader, &NetworkRequest::downloadProgress, q,
                    [taskId](int _progress) { TaskBar::setTaskProgress(taskId, _progress); });
            connect(downloader, &NetworkRequest::downloadComplete, q,
                    [this, taskId](const QByteArray& _data, const QUrl _url) {
                        TaskBar::finishTask(taskId);

                        const QString tempDirPath
                            = QDir::toNativeSeparators(QStandardPaths::writableLocation(
#ifdef Q_OS_LINUX
                                QStandardPaths::DownloadLocation
#else
                                QStandardPaths::TempLocation
#endif
                                ));
                        auto downloadedFilePath = tempDirPath + QDir::separator() + _url.fileName();
                        QFileInfo tempFileInfo(downloadedFilePath);
                        int copyIndex = 1;
                        while (tempFileInfo.exists()) {
                            auto fileName = _url.fileName();
                            fileName.replace(".", QString(".%1.").arg(copyIndex++));
                            downloadedFilePath = tempDirPath + QDir::separator() + fileName;
                            tempFileInfo.setFile(downloadedFilePath);
                        }
                        QFile tempFile(downloadedFilePath);
                        if (tempFile.open(QIODevice::WriteOnly)) {
                            tempFile.write(_data);
                            tempFile.close();
                        }

                        const bool updateStarted =
#ifdef Q_OS_LINUX
                            //
                            // Т.к. не все линуксы умеют устанавливать AppImage, то просто открываем
                            // папку с файлом
                            //
                            PlatformHelper::showInGraphicalShell(downloadedFilePath);
#else
                            //
                            // Для остальных операционок запускаем процесс установки обновления
                            //
                            QDesktopServices::openUrl(QUrl::fromLocalFile(downloadedFilePath));
#endif
                        if (updateStarted) {
                            gitHubSyncManager->settleCredentialsBeforeExit(
                                [] { QCoreApplication::quit(); });
                        }
                    });

            const auto url = notificationsManager->lastVersionDownloadLink();
            if (!url.isEmpty()) {
                downloader->loadAsync(url);
            } else {
                TaskBar::finishTask(taskId);
                StandardDialog::information(
                    applicationView, {},
                    tr("Can't load url for the last version update.\n\nPlease visit our official "
                       "website \"starc.app\" and download the last version manually."));
            }
        });
    QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
}

void ApplicationManager::Implementation::configureAutoSave()
{
    Log::info("Configure autosaving");
    autosaveTimer.stop();
    autosaveTimer.disconnect();

    if (settingsValue(DataStorageLayer::kApplicationUseAutoSaveKey).toBool()) {
        connect(&autosaveTimer, &QTimer::timeout, q, [this] { saveChanges(); });
        autosaveTimer.start(std::chrono::minutes{ 3 });
    }
}

void ApplicationManager::Implementation::showContent()
{
    //
    // Загружаем локальные проекты (облачные автоматом загружаются в момент подключения к серверу)
    // NOTE: загрузку проектов необходимо делать даже до запуска онбординга, т.к. при сбросе
    //       параметров приложения, мы сохраняем список проектов и ключ сессии, поэтому, если не
    //       загрузить проекты, то список проекта полученный из облака задублируется
    //
    projectsManager->loadProjects();

    //
    // Если это первый запуск приложения, то покажем онбординг
    //
    if (settingsValue(DataStorageLayer::kApplicationConfiguredKey).toBool() == false) {
        showOnboarding();
    }
    //
    // В противном случае показываем недавние проекты
    //
    else {
        //
        // ... а затем уже отобразить
        //
        showProjects();
    }
}

void ApplicationManager::Implementation::openStartupProject()
{
    // Keep onboarding visible. Its completion retries the pending explicit request.
    if (!startupOpenPending
        || !settingsValue(DataStorageLayer::kApplicationConfiguredKey).toBool()) {
        return;
    }

    // Consume before opening: failures, repeated callbacks and file events cannot trigger
    // another automatic attempt. An explicit request never falls back to the remembered story.
    startupOpenPending = false;
    if (!startupFilePath.isEmpty()) {
        openProject(startupFilePath);
    } else if (startupRestoreAllowed
               && settingsValue(DataStorageLayer::kApplicationOpenLastStoryKey).toBool()) {
        const auto path = settingsValue(DataStorageLayer::kApplicationLastStoryPathKey).toString();
        if (!path.isEmpty() && !openProject(path)) {
            showProjects();
        }
    }
}

void ApplicationManager::Implementation::showMenu()
{
    menuView->setFocus();
    menuView->setFixedWidth(std::max(menuView->sizeHint().width(),
                                     static_cast<int>(Ui::DesignSystem::drawer().width())));
    menuView->openMenu();
}

void ApplicationManager::Implementation::showOnboarding()
{
    Log::info("Show onboarding screen");
    onboardingManager->showWelcomePage();
    showContent(onboardingManager.data());
}

void ApplicationManager::Implementation::showProjects()
{
    Log::info("Show projects screen");
    menuView->checkProjects();
    showContent(projectsManager.data());
    saveLastContent(projectsManager.data());

    projectsManager->view()->setFocus();
}

void ApplicationManager::Implementation::showProject()
{
    Log::info("Show project screen");
    menuView->checkProject();
    showContent(projectManager.data());
    saveLastContent(projectManager.data());
}

void ApplicationManager::Implementation::showSettings()
{
    Log::info("Show settings screen");
    showContent(settingsManager.data());
}

void ApplicationManager::Implementation::showSessionStatistics()
{
    Log::info("Show session statistics screen");
    showContent(writingSessionManager.data());
}

void ApplicationManager::Implementation::showLastContent()
{
    if (lastContent.toolBar == nullptr || lastContent.navigator == nullptr
        || lastContent.view == nullptr) {
        return;
    }

    Log::info("Show last content");
    applicationView->showContent(lastContent.toolBar, lastContent.navigator, lastContent.view);
    applicationView->setHideNavigationButtonAvailable(lastContent.isHideNavigatorButtonAvailable);
}

void ApplicationManager::Implementation::setTranslation()
{
    //
    // The interface is English only. The system language and a language stored by an earlier
    // version are not consulted; the English catalog supplies the plural forms
    //
    QLocale::setDefault(QLocale(QLocale::English));

    //
    // Подключим файл переводов программы
    //
    Log::info("Setup translation for en");
    static QTranslator* appTranslator = [] {
        //
        // ... небольшой workaround для того, чтобы при запуске приложения кинуть событие о смене
        // языка
        //
        QTranslator* translator = new QTranslator;
        QApplication::installTranslator(translator);
        return translator;
    }();
    //
    // ... применяем файл с переводом из ресурсов приложения
    //
    QApplication::removeTranslator(appTranslator);
    appTranslator->load(":/translations/translation_en.qm");
    QApplication::installTranslator(appTranslator);

    QApplication::setLayoutDirection(Qt::LeftToRight);

    //
    // Настроим дизайн систему так, чтобы она использовала шрифт подходящий для используемого языка
    //
    Ui::DesignSystem::updateLanguage();
    QApplication::postEvent(q, new DesignSystemChangeEvent);

    //
    // Настроим/обновим переводы для док-меню
    //
    initDockMenu();

    //
    // Настроим/обновим переводы для конкретных менеджеров
    //
    shortcutsManager->updateTranslations();
}

void ApplicationManager::Implementation::setDesignSystemTheme(Ui::ApplicationTheme _theme)
{
    Log::info("Setup design system theme");
    if (state == ApplicationState::Working) {
        WAF::Animation::circleTransparentIn(applicationView, QCursor::pos(),
                                            applicationView->grab());
    }
    Ui::DesignSystem::setTheme(_theme);
    QApplication::postEvent(q, new DesignSystemChangeEvent);
}

void ApplicationManager::Implementation::setDesignSystemCustomThemeColors(
    const Ui::DesignSystem::Color& _color)
{
    if (Ui::DesignSystem::theme() != Ui::ApplicationTheme::Custom) {
        return;
    }

    Log::info("Setup design system theme custom colors");
    if (state == ApplicationState::Working) {
        WAF::Animation::circleTransparentIn(applicationView, QCursor::pos(),
                                            applicationView->grab());
    }
    Ui::DesignSystem::setColor(_color);
    QApplication::postEvent(q, new DesignSystemChangeEvent);
}

void ApplicationManager::Implementation::setDesignSystemScaleFactor(qreal _scaleFactor)
{
    Log::info("Setup design system scale factor");
    Ui::DesignSystem::setScaleFactor(_scaleFactor);
    QApplication::postEvent(q, new DesignSystemChangeEvent);
}

void ApplicationManager::Implementation::setDesignSystemDensity(int _density)
{
    Log::info("Setup design system density");
    Ui::DesignSystem::setDensity(_density);
    QApplication::postEvent(q, new DesignSystemChangeEvent);
}

void ApplicationManager::Implementation::updateWindowTitle(const QString& _projectName)
{
    if (projectsManager->currentProject() == nullptr) {
        applicationView->setWindowTitle("Story Architect");
        return;
    }

    const auto currentProject = projectsManager->currentProject();
    applicationView->setWindowTitle(
        QString("%1%2 (%3) - Story Architect%4")
            .arg(
#ifndef Q_OS_MAC
                "[*]"
#else
                ""
#endif
                ,
                _projectName.isEmpty() ? currentProject->name() : _projectName,
                (currentProject->isLocal() ? currentProject->path() : tr("in cloud")),
                (currentProject->isReadOnly() ? QString(" - %1").arg(tr("Read only")) : "")));

    if (applicationView->isWindowModified()) {
        markChangesSaved(false);
    }
}

void ApplicationManager::Implementation::markChangesSaved(bool _saved)
{
    const QString suffix
        = QApplication::translate("ManagementLayer::ApplicationManager", " - changed");
    if (_saved && applicationView->windowTitle().endsWith(suffix)) {
        applicationView->setWindowTitle(applicationView->windowTitle().remove(suffix));
    } else if (!_saved && !applicationView->windowTitle().endsWith(suffix)) {
        applicationView->setWindowTitle(applicationView->windowTitle() + suffix);
    }

    applicationView->setWindowModified(!_saved);
    menuView->markChangesSaved(_saved);
}

bool ApplicationManager::Implementation::saveChanges()
{
    //
    // Избегаем рекурсии
    //
    const auto canRun = RunOnce::tryRun(Q_FUNC_INFO);
    if (!canRun) {
        return false;
    }

    //
    // Сохраняем только, если приложение находится в рабочем состоянии
    //
    if (state != ApplicationState::Working) {
        return false;
    }

    //
    // Сохраняем только, если есть какие-либо изменения
    //
    if (!applicationView->isWindowModified()) {
        return true;
    }

    Log::info("Save changes triggered");

    constexpr int maximumSaveAttempts = 3;
    bool saved = false;
    for (int attempt = 1; attempt <= maximumSaveAttempts; ++attempt) {
        //
        // Очищаем ошибку предыдущей транзакции
        //
        DatabaseLayer::Database::clearLastError();

        //
        // Управляющие должны сохранить все изменения
        //
        // Retry a failed commit on its original connection. Reopening it would roll back
        // documents whose in-memory objects have already been marked as stored.
        const auto previousCommitSaved = !DatabaseLayer::Database::hasPendingCommit()
            || DatabaseLayer::Database::commit();
        if (previousCommitSaved && DatabaseLayer::Database::transaction()) {
            projectsManager->saveChanges();
            projectManager->saveChanges();
            saved = DatabaseLayer::Database::commit() && !DatabaseLayer::Database::hasError();
        }

        //
        // Если всё сохранилось без ошибок, продолжаем выполнение в обычном режиме
        //
        if (saved) {
            break;
        }

        //
        // Если сохранение не удалось, то логгируем ошибку и пробуем переподключиться к проекту
        //
        Log::warning(QStringLiteral("Saving attempt %1 of %2 failed: %3")
                         .arg(attempt)
                         .arg(maximumSaveAttempts)
                         .arg(DatabaseLayer::Database::lastError()));

        //
        // Ошибка SQLite 14 может быть вызвана временной недоступностью каталога проекта (например,
        // на внешнем или сетевом диске, флешке). Перед повторной попыткой откроем соединение
        // заново, так как существующий дескриптор SQLite может оставаться недоступным даже после
        // восстановления доступа к каталогу
        //
        if (const auto currentProjectFile = DatabaseLayer::Database::currentFile();
            QFile::exists(currentProjectFile) && attempt < maximumSaveAttempts
            && !DatabaseLayer::Database::hasPendingCommit()) {
            DatabaseLayer::Database::setCurrentFile(currentProjectFile);
        }
    }

    //
    // Если все попытки сохранения завершились с ошибкой, то делаем дополнительные проверки и
    // работаем с пользователем
    //
    if (!saved) {
        //
        // Если файл, в который мы пробуем сохранять изменения существует
        //
        if (QFile::exists(DatabaseLayer::Database::currentFile())) {
            //
            // ... то у нас случилась какая-то внутренняя ошибка базы данных
            //
            auto dialog = new Dialog(applicationView->topLevelWidget());
            dialog->setContentMaximumWidth(Ui::DesignSystem::dialog().normalWidth());
            dialog->showDialog(
                tr("Saving error"),
                tr("Changes can't be written. There is an internal database error: \"%1\" "
                   "Please check, if your file exists and if you have permission to write.")
                    .arg(DatabaseLayer::Database::lastError()),
                { { 0, StandardDialog::generateOkTerm(), Dialog::RejectButton },
                  { 1, tr("Retry"), Dialog::AcceptButton } });
            connect(dialog, &Dialog::finished, q,
                    [this, dialog](const Dialog::ButtonInfo& _presedButton) {
                        dialog->hideDialog();
                        if (_presedButton.type == Dialog::AcceptButton) {
                            if (!DatabaseLayer::Database::hasPendingCommit()) {
                                DatabaseLayer::Database::setCurrentFile(
                                    DatabaseLayer::Database::currentFile());
                            }
                            saveChanges();
                        }
                    });
            QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);

            //
            // TODO: пока хер знает, как реагировать на данную проблему...
            //       нужны реальные кейсы и пробовать что-то предпринимать
            // NOTE: я добавил обработку с попыткой переоткрытия файла и повторным
            //       сохранением, но как воспроизвести проблему так и не понял
            //
        }
        //
        // Файла с базой данных не найдено
        //
        else {
            //
            // ... возможно файл был на флешке, а она отошла, или файл был переименован во время
            // работы программы
            //
            StandardDialog::information(
                applicationView, tr("Saving error"),
                tr("Changes can't be written because the story located at \"%1\" doesn't exist. "
                   "Please move the file back and retry saving.")
                    .arg(DatabaseLayer::Database::currentFile()));
        }
        return false;
    }

    //
    // Если работает с теневым проектом, то экспортируем его при сохранении
    //
    if (projectsManager->currentProject()->projectType()
        == BusinessLayer::ProjectType::LocalShadow) {
        exportManager->exportDocument(projectManager->firstScriptModel(),
                                      projectsManager->currentProject()->path());
    }

    //
    // Если изменения сохранились без ошибок, то изменим статус окна на сохранение изменений
    //
    markChangesSaved(true);

    //
    // Обновляем информацию в списке проектов
    //
    projectsManager->updateCurrentProject(projectManager->projectName(),
                                          projectManager->projectLogline(),
                                          projectManager->projectCover());

    //
    // И, если необходимо создадим резервную копию закрываемого файла
    //
    if (settingsValue(DataStorageLayer::kApplicationSaveBackupsKey).toBool()) {
        QString baseBackupName;
        const auto currentProject = projectsManager->currentProject();
        if (currentProject->isRemote()) {
            //
            // Для удаленных проектов имя бекапа - имя проекта + id проекта
            // В случае, если имя удаленного проекта изменилось, то бэкапы со старым именем
            // останутся навсегда
            //
            baseBackupName
                = QString("%1 [%2]").arg(currentProject->name()).arg(currentProject->id());
        }
        auto backupWatcher = new QFutureWatcher<BackupBuilder::BackupResult>(q);
        QObject::connect(
            backupWatcher, &QFutureWatcher<BackupBuilder::BackupResult>::finished, q,
            [this, backupWatcher] {
                const auto result = backupWatcher->result();
                backupWatcher->deleteLater();
                if (result.success) {
                    Log::info(QStringLiteral("[ApplicationManager] Backup creation succeded: %1")
                                  .arg(result.status));
                    return;
                }

                Log::warning(QStringLiteral("[ApplicationManager] Backup creation failed: %1")
                                 .arg(result.status));
                const auto taskId = QUuid::createUuid().toString();
                TaskBar::addTask(taskId);
                TaskBar::setTaskTitle(taskId, result.status);
                TaskBar::setTaskHasProgress(taskId, false);
                QTimer::singleShot(15000, q, [taskId] { TaskBar::finishTask(taskId); });
            });
        backupWatcher->setFuture(QtConcurrent::run(
            BackupBuilder::save, projectsManager->currentProject()->path(),
            settingsValue(DataStorageLayer::kApplicationBackupsFolderKey).toString(),
            baseBackupName, settingsValue(DataStorageLayer::kApplicationBackupsQtyKey).toInt()));
    }

    //
    // The story differs from its copy on GitHub now
    //
    if (projectsManager->currentProject()->projectType() == BusinessLayer::ProjectType::Local) {
        gitHubSyncManager->updateProjectState(projectsManager->currentProject()->path());
    }

    Log::info("All changes saved");
    return true;
}

void ApplicationManager::Implementation::saveIfNeeded(std::function<void()> _callback)
{
    //
    // Избегаем зацикливания, проверяя, что диалог уже показан
    //
    if (!saveChangesDialog.isNull()) {
        return;
    }

    //
    // Если нет изменений, то просто переходим к следующему действию
    //
    if (!applicationView->isWindowModified()) {
        _callback();
        return;
    }

    //
    // Если проект облачный, то сохраняем без вопросов
    //
    if (projectsManager->currentProject()->isRemote()) {
        if (saveChanges()) {
            _callback();
        }
        return;
    }

    const int kCancelButtonId = 0;
    const int kNoButtonId = 1;
    const int kYesButtonId = 2;
    saveChangesDialog = new Dialog(applicationView);
    saveChangesDialog->showDialog({}, tr("Project was modified. Save changes?"),
                                  { { kCancelButtonId, tr("Cancel"), Dialog::RejectButton },
                                    { kNoButtonId, tr("Don't save"), Dialog::NormalButton },
                                    { kYesButtonId, tr("Save"), Dialog::AcceptButton } });
    QObject::connect(
        saveChangesDialog, &Dialog::finished, saveChangesDialog,
        [this, _callback, kCancelButtonId, kNoButtonId](const Dialog::ButtonInfo& _buttonInfo) {
            saveChangesDialog->hideDialog();

            //
            // Пользователь передумал сохранять
            //
            if (_buttonInfo.id == kCancelButtonId) {
                return;
            }

            //
            // Пользователь не хочет сохранять изменения
            //
            if (_buttonInfo.id == kNoButtonId) {
                markChangesSaved(true);
            }
            //
            // ... пользователь хочет сохранить изменения перед следующим действием
            //
            else {
                if (!saveChanges()) {
                    return;
                }
            }

            _callback();
        });
    QObject::connect(saveChangesDialog, &Dialog::disappeared, saveChangesDialog,
                     &Dialog::deleteLater);

    QApplication::alert(applicationView);
}

void ApplicationManager::Implementation::saveAs()
{
    if (projectsManager->currentProject() == nullptr) {
        Log::info("The current project is not defined and saving cannot be performed.");
        return;
    }

    //
    // Изначально высвечивается текущее имя проекта
    //
    const auto currentProject = projectsManager->currentProject();
    QString projectPath = currentProject->path();

    switch (currentProject->projectType()) {
    //
    // Для теневых проектов добавляем расширение старка, чтобы пользователя не пугал вопрос о
    // перезаписи основного файла
    //
    case BusinessLayer::ProjectType::LocalShadow: {
        projectPath += "." + ExtensionHelper::starc();
        break;
    }

    //
    // Для удаленных проектов используем имя проекта + id проекта
    // и сохраняем в папку вновь создаваемых проектов
    //
    case BusinessLayer::ProjectType::Cloud: {
        const auto projectsFolderPath
            = settingsValue(DataStorageLayer::kProjectSaveFolderKey).toString();
        projectPath = projectsFolderPath + QDir::separator()
            + QString("%1 [%2]%3")
                  .arg(currentProject->name())
                  .arg(currentProject->id())
                  .arg(BusinessLayer::ProjectsModelProjectItem::extension());
        break;
    }

    default: {
        break;
    }
    }

    //
    // Получим имя файла для сохранения
    //
    QString saveAsProjectFilePath
        = QFileDialog::getSaveFileName(applicationView, tr("Choose file to save story"),
                                       projectPath, DialogHelper::starcProjectFilter());
    if (saveAsProjectFilePath.isEmpty()) {
        return;
    }

    //
    // Если файл выбран
    //

    //
    // Установим расширение, если не задано
    //
    if (!saveAsProjectFilePath.endsWith(BusinessLayer::ProjectsModelProjectItem::extension())) {
        saveAsProjectFilePath.append(BusinessLayer::ProjectsModelProjectItem::extension());
    }

    //
    // Если пользователь указал тот же путь, ничего не делаем
    // NOTE: проверяем именно через QFileInfo, т.к. пути могут выглядеть по разному и сравнивать
    //       строки тут некорректно
    //
    if (QFileInfo(saveAsProjectFilePath) == QFileInfo(currentProject->path())) {
        return;
    }

    //
    // Cохраняем в новый файл
    //
    // ... если файл существовал, удалим его для удаления данных в нём
    //
    if (QFile::exists(saveAsProjectFilePath)) {
        QFile::remove(saveAsProjectFilePath);
    }
    //
    // ... скопируем текущую базу в указанный файл
    //
    const auto isCopied = QFile::copy(currentProject->realPath(), saveAsProjectFilePath);
    if (!isCopied) {
        StandardDialog::information(
            applicationView, tr("Saving error"),
            tr("Can't save the story to the file %1. Please check permissions and retry.")
                .arg(saveAsProjectFilePath));
        return;
    }

    //
    // Откроем копию текущего проекта
    //
    openProject(saveAsProjectFilePath);
}

void ApplicationManager::Implementation::createProject()
{
    auto callback = [this] { projectsManager->createProject(); };
    saveIfNeeded(callback);
}

bool ApplicationManager::Implementation::createLocalProject(const QString& _projectName,
                                                            const QString& _projectPath,
                                                            const QString& _importFilePath)
{
    if (_projectPath.isEmpty()) {
        return false;
    }

    const QFileInfo destination(_projectPath);
    const auto reportCreationError = [this, &destination] {
        const auto currentDestination = QFileInfo(destination.absoluteFilePath());
        const auto message = currentDestination.exists() || currentDestination.isSymLink()
            ? tr("A file already exists at %1. It has been left unchanged. Please choose another "
                 "name or location for the new story.")
                  .arg(destination.absoluteFilePath())
            : tr("The new story could not be created at %1. Please check permissions or choose "
                 "another location.")
                  .arg(destination.absoluteFilePath());
        StandardDialog::information(applicationView, tr("Create project error"), message);
    };
    if (destination.exists() || destination.isSymLink()) {
        reportCreationError();
        return false;
    }

    //
    // Папки, в которую пользователь хочет писать может и не быть,
    // создадим на всякий случай, чтобы его не мучать
    //
    if (!QDir::root().mkpath(destination.absolutePath())) {
        reportCreationError();
        return false;
    }

    // Keep the current story and its lock until the new destination is ready.
    QScopedPointer<QLockFile> destinationLock;
    if (!tryLockProject(_projectPath, destinationLock)) {
        return false;
    }

    // Publish only our own complete temporary file. rename refuses an existing target,
    // including one created after the initial existence check, without overwriting it.
    QTemporaryFile file(destination.absoluteFilePath() + ".XXXXXX");
    if (!file.open()) {
        reportCreationError();
        return false;
    }

    //
    // Если возможна запись в файл
    //
    // ... если в новый проект должны быть импортированы данные другого проекта,
    //     то просто создаём новый, как копию импортируемого файла
    //
    if (_importFilePath.endsWith(ExtensionHelper::starc(), Qt::CaseInsensitive)) {
        QFile source(_importFilePath);
        bool copied = source.open(QIODevice::ReadOnly);
        while (copied && !source.atEnd()) {
            const auto chunk = source.read(1024 * 1024);
            copied = source.error() == QFileDevice::NoError && file.write(chunk) == chunk.size();
        }
        if (!copied || !file.setPermissions(source.permissions())) {
            StandardDialog::information(applicationView, tr("Create project error"),
                                        tr("The source story could not be copied."));
            return false;
        }
    }
    if (!file.flush() || !file.rename(destination.absoluteFilePath())) {
        reportCreationError();
        return false;
    }
    file.setAutoRemove(false);

    closeCurrentProject();
    lockFile.reset(destinationLock.take());
    //
    // ... создаём новую базу данных в файле и делаем её текущим проектом
    //
    projectsManager->setCurrentProject(_projectPath);
    projectsManager->setCurrentProjectName(_projectName);
    //
    // ... сохраняем новый проект в списке недавних
    //
    projectsManager->saveProjects();
    //
    // ... перейдём к редактированию
    //
    constexpr auto afterProjectCreation = true;
    if (!goToEditCurrentProject(afterProjectCreation, _importFilePath)) {
        closeCurrentProject();
        showProjects();
        return false;
    }
    return true;
}

void ApplicationManager::Implementation::openProject()
{
    auto callback = [this] { projectsManager->openProject(); };
    saveIfNeeded(callback);
}

bool ApplicationManager::Implementation::openProject(const QString& _path)
{
    if (_path.isEmpty()) {
        return false;
    }

    if (projectsManager->project(_path) != nullptr && projectsManager->project(_path)->isLocal()
        && !QFileInfo::exists(_path)) {
        projectsManager->hideProject(_path);
        return false;
    }

    // Opening an unavailable source must not create an empty project or shadow story.
    const QFileInfo source(_path);
    QFile sourceFile(_path);
    if (!source.isFile() || !sourceFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    sourceFile.close();

    if (projectsManager->currentProject() != nullptr
        && QFileInfo(projectsManager->currentProject()->path()) == source) {
        showProject();
        return false;
    }

    if (_path.endsWith(ExtensionHelper::starc(), Qt::CaseInsensitive)
        && !DatabaseLayer::Database::canOpenFile(_path)) {
        StandardDialog::information(applicationView, { }, DatabaseLayer::Database::openFileError());
        return false;
    }

    //
    // ... закроем текущий проект
    //
    closeCurrentProject();

    //
    // ... проверяем открыт ли файл в другом приложении
    //
    if (!tryLockProjectOnOpen(_path)) {
        return false;
    }

    //
    // Если это не проект старка, то создаём временный файл проекта и в последующем импортируем
    // данные в него
    //
    QString projectFilePath = _path;
    QString importFilePath;
    if (!projectFilePath.endsWith(ExtensionHelper::starc(), Qt::CaseInsensitive)) {
        QTemporaryFile tempProject;
        tempProject.setAutoRemove(false);
        if (!tempProject.open()) {
            return false;
        }

        projectFilePath = tempProject.fileName();
        importFilePath = _path;
    }

    //
    // ... переключаемся на работу с выбранным файлом
    //
    projectsManager->setCurrentProject(_path, projectFilePath);

    //
    // ... сохраняем открытый проект в списке недавних
    //
    projectsManager->saveProjects();

    //
    // ... перейдём к редактированию
    //
    constexpr auto afterProjectCreation = false;
    if (!goToEditCurrentProject(afterProjectCreation, importFilePath)) {
        closeCurrentProject();
        showProjects();
        return false;
    }

    return true;
}

bool ApplicationManager::Implementation::tryLockProject(const QString& _path,
                                                        QScopedPointer<QLockFile>& _lockFile)
{
    const QFileInfo projectFileInfo(_path);
    _lockFile.reset(new QLockFile(
        QString("%1/%2.lock").arg(projectFileInfo.absolutePath(), projectFileInfo.fileName())));
    _lockFile->setStaleLockTime(0);
    if (!_lockFile->tryLock()) {
        StandardDialog::information(applicationView, {},
                                    tr("This file can't be open at this moment, because it is "
                                       "already open in another copy of the application."));
        return false;
    }

    return true;
}

bool ApplicationManager::Implementation::tryLockProjectOnOpen(const QString& _path)
{
    const QFileInfo projectFileInfo(_path);
    lockFile.reset(new QLockFile(
        QString("%1/%2.lock").arg(projectFileInfo.absolutePath(), projectFileInfo.fileName())));
    if (!lockFile->tryLock()) {
        //
        // В некоторых случаях, после падения приложения (особенно в маке), файл блокировки не
        // освобождается, т.к. названия процессов совпадают, поэтому для кейса с открытием документа
        // даём возможность форсировать открытие путём удаления протухшего файла
        //
        auto dialog = new Dialog(applicationView->topLevelWidget());
        dialog->setContentMaximumWidth(Ui::DesignSystem::dialog().normalWidth());
        dialog->showDialog({},
                           tr("This file can't be open at this moment, because it is already open "
                              "in another copy of the application."),
                           { { 0, StandardDialog::generateOkTerm(), Dialog::RejectButton },
                             { 1, tr("Ignore and open"), Dialog::AcceptButton } });
        connect(dialog, &Dialog::finished, q,
                [this, dialog, _path](const Dialog::ButtonInfo& _presedButton) {
                    dialog->hideDialog();
                    if (_presedButton.type == Dialog::AcceptButton) {
                        lockFile->removeStaleLockFile();
                        lockFile.reset();
                        openProject(_path);
                    }
                });
        QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
        return false;
    }

    lockFile->setStaleLockTime(0);
    return true;
}

bool ApplicationManager::Implementation::goToEditCurrentProject(bool _afterProjectCreation,
                                                                const QString& _importFilePath)
{
    state = ApplicationState::ProjectLoading;

    // Verify the database before requesting models. Failed opens must not be remembered.
    {
        auto query = DatabaseLayer::Database::query();
        if (!query.exec("SELECT count(*) FROM documents") || !query.next()) {
            StandardDialog::information(applicationView, tr("Open story error"),
                                        tr("The story could not be loaded. Please check the file "
                                           "and its permissions."));
            return false;
        }
    }

    //
    // Установим заголовок
    //
    updateWindowTitle();

    //
    // Настроим меню
    //
    const auto currentProject = projectsManager->currentProject();
    menuView->setProjectTitle(currentProject->name());
    menuView->setProjectActionsVisible(true);

    //
    // Сохраняем тип проекта по-умолчанию
    //
    const auto projectType = static_cast<Domain::DocumentObjectType>(
        settingsValue(DataStorageLayer::kProjectTypeKey).toInt());

    //
    // Импортировать будем всё, кроме старковских файлов, т.к. данные из них копируются на
    // предыдущем шаге
    //
    const auto isImportedFromStarc = _importFilePath.endsWith(ExtensionHelper::starc());
    const auto shouldPerformImport = !_importFilePath.isEmpty() && !isImportedFromStarc;

    //
    // Если будет импорт, то сбросим умолчальный тип проекта, чтобы не создавать лишних документов
    //
    if (shouldPerformImport || !_afterProjectCreation) {
        setSettingsValue(DataStorageLayer::kProjectTypeKey,
                         static_cast<int>(Domain::DocumentObjectType::Undefined));
    }

    //
    // Загрузим данные текущего проекта
    //
    projectManager->loadCurrentProject(currentProject);
    menuView->setImportAvailable(currentProject->editingMode() == DocumentEditingMode::Edit);

    //
    // Восстанавливаем тип проекта по-умолчанию для будущих свершений
    //
    if (shouldPerformImport || !_afterProjectCreation) {
        setSettingsValue(DataStorageLayer::kProjectTypeKey, static_cast<int>(projectType));
    }

    //
    // При необходимости импортируем данные из заданного файла
    //
    if (shouldPerformImport) {
        bool imported = false;
        const auto connection = connect(importManager.data(), &ImportManager::screenplayImported, q,
                                        [&imported] { imported = true; });
        importManager->importScreenplay(_importFilePath);
        QObject::disconnect(connection);
        if (!imported) {
            StandardDialog::information(applicationView, tr("Open story error"),
                                        tr("The source screenplay could not be imported."));
            return false;
        }

        //
        // ... а после импорта пробуем повторно восстановить состояние,
        //     особенно актуально для кейса с теневым проектом
        //
        projectManager->restoreCurrentProjectState(currentProject->path());
    }

    //
    // Отобразить страницу самого проекта
    //
    showProject();

    state = ApplicationState::Working;

    //
    // Для локальных проектов доступных только для чтения, покажем соответствующее уведомление
    //
    if (currentProject->projectType() != BusinessLayer::ProjectType::Cloud
        && currentProject->isReadOnly()) {
        StandardDialog::information(
            applicationView, {},
            tr("A file you are trying to open does not have write permissions. Check out file "
               "properties and allow it to be edited. Since it isn't editable, it will be opened "
               "in a read-only mode."));
    }

    if (currentProject->projectType() == BusinessLayer::ProjectType::LocalShadow) {
        //
        // Покажем диалог с предупреждением о том, что не все функции могут работать и предложим
        // сохранить в формате старка
        //
        if (currentProject->canAskAboutSwitch()) {
            const int kNeverAskAgainButtonId = 0;
            const int kKeepButtonId = 1;
            const int kYesButtonId = 2;
            Dialog* informationDialog = new Dialog(applicationView);
            const auto projectFileSuffix = QFileInfo(currentProject->path()).suffix().toUpper();
            informationDialog->showDialog(
                tr("Do you want continue to use .%1 file format?").arg(projectFileSuffix),
                tr("Some project data cannot be saved in .%1 format. We recommend you to use Story "
                   "Architect .%2 format so all the project data will be saved properly.")
                    .arg(projectFileSuffix.toUpper(), ExtensionHelper::starc().toUpper()),
                { { kNeverAskAgainButtonId, tr("Never ask again"), Dialog::NormalButton },
                  { kKeepButtonId, tr("Keep .%1").arg(projectFileSuffix), Dialog::RejectButton },
                  { kYesButtonId, tr("Switch to .STARC"), Dialog::AcceptButton } });
            QObject::connect(informationDialog, &Dialog::finished, informationDialog,
                             [this, informationDialog, kNeverAskAgainButtonId,
                              kKeepButtonId](const Dialog::ButtonInfo& _buttonInfo) {
                                 informationDialog->hideDialog();

                                 //
                                 // Пользователь хочет использовать оригинальный формат и просит
                                 // больше его не беспокоить
                                 //
                                 if (_buttonInfo.id == kNeverAskAgainButtonId) {
                                     projectsManager->setCurrentProjectNeverAskAboutSwitch();
                                     return;
                                 }

                                 //
                                 // Пользователь хочет использовать оригинальный формат в текущей
                                 // сессии
                                 //
                                 if (_buttonInfo.id == kKeepButtonId) {
                                     return;
                                 }

                                 //
                                 // Пользователь хочет перейти на формат старка
                                 //
                                 // ... создаём новый документ и туда импортируем данные текущего
                                 //
                                 const auto project = projectsManager->currentProject();
                                 const auto sourcePath = project->path();
                                 const QString projectPath
                                     = sourcePath + "." + ExtensionHelper::starc();
                                 //
                                 // ... скрываем текущий из списка недавних
                                 //
                                 if (createLocalProject(project->name(), projectPath, sourcePath)) {
                                     projectsManager->hideProject(sourcePath);
                                 }
                             });
            QObject::connect(informationDialog, &Dialog::disappeared, informationDialog,
                             &Dialog::deleteLater);

            QApplication::alert(applicationView);
        }
    }

    //
    // При импорте из проекта старка, очищаем всю историю изменений, т.к. она может быть неполной,
    // например если это проект из облака
    //
    if (isImportedFromStarc) {
        //
        // TODO: менять юид документа проекта, чтобы при копировании проектов не было одинаковых
        //       юидов у разных проектов
        //
        projectManager->clearChangesHistory();
    }

#ifdef CLOUD_SERVICE_MANAGER
    //
    // Для облачных проектов делаем синхронизацию офлайн изменений
    //
    if (currentProject->isRemote()) {
        projectsManager->setCurrentProjectCanBeSynced(true);
        const auto unsyncedDocuments = projectManager->unsyncedDocuments();
        for (const auto document : unsyncedDocuments) {
            cloudServiceManager->openDocument(projectsManager->currentProject()->id(), document,
                                              projectManager->draftSource(document->uuid()));
        }
    }
#endif

    //
    // Запускаем писательскую сессию
    //
    writingSessionManager->startSession(currentProject->uuid(), currentProject->name());

#ifdef PRINT_DOCUMENT_HISTORY
    const QUuid uuid("{951996cf-1432-4254-a675-5c7f7f28b771}");
    //
    // Character
    //
    // BusinessLayer::CharacterModel model;
    // DataStorageLayer::DocumentImageStorage documentImageStorage;
    // model.setImageWrapper(&documentImageStorage);
    // auto document = Domain::ObjectsBuilder::createDocument(
    //     {}, {}, Domain::DocumentObjectType::Character, {}, {});

    //
    // Screenplay text
    //
    BusinessLayer::ScreenplayTextModel model;
    BusinessLayer::ScreenplayInformationModel informationModel;
    model.setInformationModel(&informationModel);
    auto document = Domain::ObjectsBuilder::createDocument(
        {}, {}, Domain::DocumentObjectType::ScreenplayText, {}, {});

    model.setDocument(document);
    const auto changes = DataMappingLayer::MapperFacade::documentChangeMapper()->findAll(uuid);
    for (int index = 0; index < changes.size(); ++index) {
        const auto change = changes[index];
        qDebug() << "Applying change number" << index << change->uuid();
        qDebug() << "Redo patch is\n\n"
                 << QByteArray::fromPercentEncoding(change->redoPatch()).constData();
        model.applyDocumentChanges({ change->redoPatch() });
    }
    qDebug() << qUtf8Printable(model.document()->content());
#endif

    if (!DatabaseLayer::Database::hasError() || currentProject->isReadOnly()) {
        if (currentProject->isLocal()) {
            setSettingsValue(DataStorageLayer::kApplicationLastStoryPathKey,
                             QFileInfo(currentProject->path()).absoluteFilePath());
        }
        return true;
    }
    return false;
}

void ApplicationManager::Implementation::closeCurrentProject()
{
    Log::info("Closing current project");

    if (projectsManager->currentProject() == nullptr) {
        Log::warning("Current project is not valid. Skip closing.");
        return;
    }

    Q_ASSERT(!lockFile.isNull());
    Q_ASSERT(lockFile->isLocked());

    lockFile->unlock();
    lockFile.reset();

    state = ApplicationState::ProjectClosing;

    //
    // Порядок важен
    //
#ifdef CLOUD_SERVICE_MANAGER
    if (projectsManager->currentProject()->isRemote()) {
        cloudServiceManager->closeProject(projectsManager->currentProject()->id());
    }
#endif
    projectManager->closeCurrentProject(projectsManager->currentProject()->path());
    projectsManager->closeCurrentProject();

    menuView->setProjectActionsVisible(false);

    writingSessionManager->finishSession();

    DataStorageLayer::StorageFacade::settingsStorage()->resetSession();

    markChangesSaved(true);
    updateWindowTitle();

    state = ApplicationState::Working;
}

void ApplicationManager::Implementation::importProject()
{
    importManager->import();
}

void ApplicationManager::Implementation::exportCurrentDocument()
{
    if (projectsManager->currentProject() == nullptr) {
        Log::info("The current project is not defined and export cannot be performed.");
        return;
    }

    const auto models = projectManager->currentModelsForExport();
    if (models.isEmpty()) {
        return;
    }

    //
    // Если у нас хоть одной из моделей нет, то значит они сейчас загружаются с сервера
    //
    const char* modelsLoadingTaskId = "loading-models-for-export";
    if (std::find_if(models.begin(), models.end(),
                     [](const QPair<QString, BusinessLayer::AbstractModel*>& _item) {
                         return _item.second == nullptr;
                     })
        != models.end()) {
        //
        // ... так что покажем уведомление, мол мы подготавливаемся к экспорту
        //
        if (TaskBar::isTaskFinished(modelsLoadingTaskId)) {
            TaskBar::addTask(modelsLoadingTaskId);
            TaskBar::setTaskTitle(modelsLoadingTaskId, tr("Prepare document for exporting"));
            TaskBar::setTaskIndeterminate(modelsLoadingTaskId, true);
        }
        //
        // ... и запланируем выполнение экспорта чуть позже
        //
        QTimer::singleShot(300, q, [this] { exportCurrentDocument(); });
        return;
    }
    TaskBar::finishTask(modelsLoadingTaskId);

    //
    // Если экпортируем напрямую из редактора сценария, то предустановим выбранным драфтом тот,
    // с которым идёт работа в данный момент
    //
    int currentModelIndex = -1;
    if (const auto currentDocument = projectManager->currentDocument();
        models.constFirst().second->document()->type() == currentDocument->type()) {
        for (int index = 0; index < models.size(); ++index) {
            if (models[index].second->document()->uuid() == currentDocument->uuid()) {
                currentModelIndex = index;
                break;
            }
        }
    }
    exportManager->exportDocument(models, currentModelIndex);
}

void ApplicationManager::Implementation::toggleFullScreen()
{
    const bool isFullScreen = !applicationView->isFullScreen();

    //
    // Настраиваем видимость панели навигации
    //
    applicationView->toggleFullScreen(!isFullScreen);

    //
    // Конфигурируем редактор для отображения в полноэкранном режиме
    //
    projectManager->toggleFullScreen(isFullScreen);

    //
    // Собственно настраиваем состояние окна
    //
    const char* isMaximizedKey = "is-window-maximized";
    if (applicationView->isFullScreen()) {
        if (applicationView->property(isMaximizedKey).toBool()) {
#ifdef Q_OS_LINUX
            //
            // В Linux почему-то не возвращается нормально в режим максимизировано,
            // поэтому сперва отобразив в нормальном состоянии, а потом масимизируем
            //
            applicationView->showNormal();
#endif
            applicationView->showMaximized();
        } else {
            applicationView->showNormal();
        }
    } else {
        //
        // Сохраним состояние окна перед переходом в полноэкранный режим
        //
        applicationView->setProperty(isMaximizedKey,
                                     applicationView->windowState().testFlag(Qt::WindowMaximized));
        applicationView->showFullScreen();
    }
}

void ApplicationManager::Implementation::imitateTypewriterSound(QKeyEvent* _event) const
{
    //
    // Обрабатываем событие только в случае, если в виджет в фокусе можно вводить текст
    //
    if (QApplication::focusWidget()
        && !QApplication::focusWidget()->testAttribute(Qt::WA_InputMethodEnabled)) {
        return;
    }
    //
    // ... и если опция озвучивания печати включена
    //
    const auto keyboardSoundEnabled
        = settingsValue(DataStorageLayer::kApplicationUseTypewriterSoundKey).toBool();
    if (!keyboardSoundEnabled) {
        return;
    }

    auto makeSound = [this](const QString& path) {
        QSoundEffect* sound = new QSoundEffect(applicationView);
        sound->setSource(QUrl::fromLocalFile(path));
        return sound;
    };
    static auto s_returnSound = makeSound(":/audio/return");
    static auto s_spaceSound = makeSound(":/audio/space");
    static auto s_deleteSound = makeSound(":/audio/backspace");
    static QVector<QSoundEffect*> s_keySounds
        = { makeSound(":/audio/key-01"), makeSound(":/audio/key-02"), makeSound(":/audio/key-03"),
            makeSound(":/audio/key-04") };
    switch (_event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        s_returnSound->play();
        break;
    }

    case Qt::Key_Space: {
        s_spaceSound->play();
        break;
    }

    case Qt::Key_Backspace:
    case Qt::Key_Delete: {
        s_deleteSound->play();
        break;
    }

    default: {
        if (_event->text().isEmpty()) {
            break;
        }

        const int firstSoundId = 0;
        const int maxSoundId = 3;
        static int lastSoundId = firstSoundId;
        if (lastSoundId > maxSoundId) {
            lastSoundId = firstSoundId;
        }
        s_keySounds[lastSoundId++]->play();
        break;
    }
    }
}

void ApplicationManager::Implementation::exit()
{
    Log::info("Closing application");

    //
    // Скрываем интерфейс, чтобы для пользователя всё было максимально быстро
    //
    applicationView->hide();

    //
    // Закрываем текущий открытый проект
    //
    closeCurrentProject();

    //
    // Сохраняем состояние приложения
    //
    setSettingsValues(DataStorageLayer::kApplicationViewStateKey, applicationView->saveState());

    //
    // Сохраним расположение проектов
    //
    projectsManager->saveProjects();

    //
    // Выходим
    //
    QApplication::processEvents();
    QApplication::quit();
}

template<typename Manager>
void ApplicationManager::Implementation::showContent(Manager* _manager)
{
    applicationView->showContent(_manager->toolBar(), _manager->navigator(), _manager->view());
    applicationView->setHideNavigationButtonAvailable((void*)_manager
                                                      == (void*)projectManager.data());
}

template<typename Manager>
void ApplicationManager::Implementation::saveLastContent(Manager* _manager)
{
    lastContent.toolBar = _manager->toolBar();
    lastContent.navigator = _manager->navigator();
    lastContent.view = _manager->view();
    lastContent.isHideNavigatorButtonAvailable = (void*)_manager == (void*)projectManager.data();
}

// ****

ApplicationManager::ApplicationManager(QObject* _parent)
    : QObject(_parent)
    , IApplicationManager()
{
    //
    // Настроим вывод лога в консоль на всех платформах
    //
    PlatformHelper::initConsoleOutput();

    //
    // Настраиваем способ конвертации пикселей в миллиметры
    //
    d->initMeasurementMetrics();

    //
    // Первым делом настраиваем сбор логов
    //
    d->initLogging();

    QApplication::setStyle(new ApplicationStyle(QStyleFactory::create("Fusion")));

    //
    // Загрузим шрифты в базу шрифтов программы, если их там ещё нет
    //
    Log::info("Loading fonts [embedded]");
    //
    // ... встроенные в бинарник
    //
    QFontDatabase::addApplicationFont(":/fonts/materialdesignicons");
    QFontDatabase::addApplicationFont(":/fonts/font-awesome-brands");
    QFontDatabase::addApplicationFont(":/fonts/roboto-bold");
    QFontDatabase::addApplicationFont(":/fonts/roboto-light");
    QFontDatabase::addApplicationFont(":/fonts/roboto-medium");
    QFontDatabase::addApplicationFont(":/fonts/roboto-regular");
    QFontDatabase::addApplicationFont(":/fonts/noto-sans");
    QFontDatabase::addApplicationFont(":/fonts/noto-sans-light");
    QFontDatabase::addApplicationFont(":/fonts/noto-sans-medium");
    //
    QFontDatabase::addApplicationFont(":/fonts/arial");
    QFontDatabase::addApplicationFont(":/fonts/arial-bold");
    QFontDatabase::addApplicationFont(":/fonts/arial-italic");
    QFontDatabase::addApplicationFont(":/fonts/arial-bold-italic");
    QFontDatabase::addApplicationFont(":/fonts/courier-new");
    QFontDatabase::addApplicationFont(":/fonts/courier-new-bold");
    QFontDatabase::addApplicationFont(":/fonts/courier-new-italic");
    QFontDatabase::addApplicationFont(":/fonts/courier-new-bold-italic");
    QFontDatabase::addApplicationFont(":/fonts/courier-prime");
    QFontDatabase::addApplicationFont(":/fonts/courier-prime-bold");
    QFontDatabase::addApplicationFont(":/fonts/courier-prime-italic");
    QFontDatabase::addApplicationFont(":/fonts/courier-prime-bold-italic");
    QFontDatabase::addApplicationFont(":/fonts/mallanna-regular");
    QFontDatabase::addApplicationFont(":/fonts/muktamalar-bold");
    QFontDatabase::addApplicationFont(":/fonts/muktamalar-regular");
    QFontDatabase::addApplicationFont(":/fonts/times-new-roman");
    QFontDatabase::addApplicationFont(":/fonts/times-new-roman-bold");
    QFontDatabase::addApplicationFont(":/fonts/times-new-roman-italic");
    QFontDatabase::addApplicationFont(":/fonts/times-new-roman-bold-italic");
    //
    QFontDatabase::addApplicationFont(":/fonts/montserrat-regular");
    QFontDatabase::addApplicationFont(":/fonts/montserrat-bold");
    QFontDatabase::addApplicationFont(":/fonts/montserrat-italic");
    QFontDatabase::addApplicationFont(":/fonts/montserrat-bold-italic");
    QFontDatabase::addApplicationFont(":/fonts/sf-movie-poster");
    //
    // ... скаченные
    //
    Log::info("Loading fonts [downloaded]");
    const auto fontsFolderPath
        = QString("%1/fonts")
              .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto fonts = QDir(fontsFolderPath).entryInfoList(QDir::Files);
    for (const auto& font : fonts) {
        const auto rawFont = QRawFont(font.absoluteFilePath(), 12);
        if (!rawFont.isValid()) {
            Log::warning("Font %1 is invalid and will be removed", font.fileName());
            QFile::remove(font.absoluteFilePath());
            continue;
        }

        QFontDatabase::addApplicationFont(font.absoluteFilePath());
    }

    //
    // Инициилизируем данные после подгрузки шрифтов, чтобы они сразу подхватились системой
    //
    Log::info("Init application managers");
    d.reset(new Implementation(this));

    //
    // Настроим соединения с менеджерами и представлением приложения
    //
    Log::info("Init business logic between managers");
    initConnections();
}

ApplicationManager::~ApplicationManager()
{
    //
    // Сохраняем настройки приложения
    //
    DataStorageLayer::StorageFacade::settingsStorage()->sync();

    //
    // Удаляем старые логи
    //
    const auto logsDirPath
        = QString("%1/logs").arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto logsFiles = QDir(logsDirPath).entryInfoList();
    //
    // ... храним логи недельной давности
    //
    const auto dateBorder = QDateTime::currentDateTime().addDays(-7);
    for (const auto& logFile : logsFiles) {
        if (logFile.lastModified() >= dateBorder) {
            continue;
        }

        QFile::remove(logFile.absoluteFilePath());
    }
}

QString ApplicationManager::logFilePath() const
{
    return Log::logFilePath();
}

void ApplicationManager::exec(const QString& _fileToOpenPath)
{
    if (d->startupScheduled) {
        return;
    }
    d->startupScheduled = true;
    d->startupRestoreAllowed = settingsValue(DataStorageLayer::kApplicationConfiguredKey).toBool();
    if (d->startupFilePath.isEmpty()) {
        d->startupFilePath = _fileToOpenPath;
    }

    Log::info("Starting the application");

    //
    // Самое главное - настроить заголовок!
    //
    d->updateWindowTitle();

    //
    // Сразу регистрируем панель уведомлений
    //
    TaskBar::registerTaskBar(d->applicationView, {}, {}, {});

    //
    // Настраиваем язык приложения
    //
    d->setTranslation();


    //
    // Пробуем загрузить геометрию и состояние приложения
    //
    d->setDesignSystemTheme(static_cast<Ui::ApplicationTheme>(
        settingsValue(DataStorageLayer::kApplicationThemeKey).toInt()));
    d->setDesignSystemCustomThemeColors(Ui::DesignSystem::Color(
        settingsValue(DataStorageLayer::kApplicationCustomThemeColorsKey).toString()));
    d->setDesignSystemScaleFactor(
        settingsValue(DataStorageLayer::kApplicationScaleFactorKey).toReal());
    d->setDesignSystemDensity(settingsValue(DataStorageLayer::kApplicationDensityKey).toInt());
    d->applicationView->restoreState(
        settingsValue(DataStorageLayer::kApplicationConfiguredKey).toBool(),
        settingsValues(DataStorageLayer::kApplicationViewStateKey));

    //
    // Покажем интерфейс
    //
    Log::info("Show application window");
    d->applicationView->show();

    //
    // Осуществляем остальную настройку и показываем содержимое, после того, как на экране
    // отобразится приложение, чтобы у пользователя возник эффект моментального запуска
    //
    QMetaObject::invokeMethod(
        this,
        [this] {
            Log::info("Make startup checks");

#ifdef CLOUD_SERVICE_MANAGER
            //
            // Запуск облачного сервиса
            //
            d->cloudServiceManager->start();
#endif

            //
            // Настройка
            //
            d->configureAutoSave();

            //
            // Отображение
            //
            d->showContent();

            //
            // Проверяем словари проверки орфографии
            //
            d->settingsManager->validateSpellCheckerDictionaries();

            //
            // Переводим состояние приложение в рабочий режим
            //
            d->state = ApplicationState::Working;

            //
            // Check if the stories were changed on another computer. During the initial setup
            // the stories are checked when the user confirms the repository there
            //
            if (settingsValue(DataStorageLayer::kApplicationConfiguredKey).toBool()) {
                d->gitHubSyncManager->checkOnStart();
            }

            // Allow startup file-open events to arrive after initialization and before
            // selecting the automatic target. All opening uses the same one-shot route.
            QMetaObject::invokeMethod(
                this, [this] { d->openStartupProject(); }, Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

bool ApplicationManager::openProject(const QString& _path)
{
    if (d->startupOpenPending) {
        if (!_path.isEmpty()) {
            d->startupFilePath = _path;
        }
        return false;
    }
    return d->openProject(_path);
}

bool ApplicationManager::event(QEvent* _event)
{
    switch (static_cast<int>(_event->type())) {
    case static_cast<QEvent::Type>(EventType::IdleEvent): {
        //
        // Сохраняем только если пользователь желает делать это автоматически
        //
        if (d->autosaveTimer.isActive()) {
            d->saveChanges();
        }

        //
        // Уведомляем заинтересованные менеджеры о том, что перешли в состояние простоя
        //
        for (auto manager : { d->projectManager.data() }) {
            QApplication::sendEvent(manager, _event);
        }

        //
        // Если был долгий простой, то завершаем текущую сессию, пользователь ушёл
        //
        if (auto event = static_cast<IdleEvent*>(_event); event->isLongIdle) {
            //
            // ... текущая сессия завершилась во время последней активности в приложении
            //
            d->writingSessionManager->splitSession(d->lastActivityDateTime);
        }
        //
        // А если был маленький простой после активности персонажа, запоминаем этот момент,
        // как дату и время последней активности пользователя
        //
        else {
            d->lastActivityDateTime = QDateTime::currentDateTimeUtc();
        }

        //
        // Сохраняем настройки приложения
        //
        DataStorageLayer::StorageFacade::settingsStorage()->sync();

        _event->accept();
        return true;
    }

    case static_cast<QEvent::Type>(EventType::DesignSystemChangeEvent): {
        //
        // Уведомляем все виджеты о том, что сменилась дизайн система
        //
        const auto widgets = d->applicationView->findChildren<QWidget*>();
        for (auto widget : widgets) {
            QApplication::sendEvent(widget, _event);
        }
        QApplication::sendEvent(d->applicationView, _event);

        _event->accept();
        return true;
    }

    case static_cast<QEvent::Type>(EventType::TextEditingOptionsChangeEvent): {
        //
        // Уведомляем все редакторы текста о том, что сменились опции проверки орфографии
        //
        const auto textEdits = d->applicationView->findChildren<SpellCheckTextEdit*>();
        for (auto textEdit : textEdits) {
            QApplication::sendEvent(textEdit, _event);
        }
        const auto scalableWrappers = d->applicationView->findChildren<ScalableWrapper*>();
        for (auto scalableWrapper : scalableWrappers) {
            QApplication::sendEvent(scalableWrapper, _event);
        }
        QApplication::sendEvent(d->applicationView, _event);

        _event->accept();
        return true;
    }

    case QEvent::KeyPress: {
        const auto keyEvent = static_cast<QKeyEvent*>(_event);

        //
        // Музицируем
        //
        d->imitateTypewriterSound(keyEvent);

        //
        // Сохраняем стату
        //
        d->writingSessionManager->addKeyPressEvent(keyEvent);

        return false;
    }

    case static_cast<QEvent::Type>(EventType::FocusChangeEvent): {
        //
        // Посылаем событие о смене фокуса в менеджер проекта, чтобы определить текущее активное
        // представление
        //
        QApplication::sendEvent(d->projectManager.data(), _event);

        return false;
    }

    default: {
        return QObject::event(_event);
    }
    }
}

void ApplicationManager::initConnections()
{
    //
    // Горячие клавиши
    //
    QShortcut* saveShortcut = new QShortcut(QKeySequence::Save, d->applicationView);
    saveShortcut->setContext(Qt::ApplicationShortcut);
    connect(saveShortcut, &QShortcut::activated, this, [this] { d->saveChanges(); });
    //
    d->importShortcut = new QShortcut(d->shortcutsManager->importShortcut(), d->applicationView);
    d->importShortcut->setContext(Qt::ApplicationShortcut);
    connect(d->importShortcut, &QShortcut::activated, this, [this] { d->importProject(); });
    //
    d->exportShortcut
        = new QShortcut(d->shortcutsManager->currentDocumentExportShortcut(), d->applicationView);
    d->exportShortcut->setContext(Qt::ApplicationShortcut);
    connect(d->exportShortcut, &QShortcut::activated, this, [this] { d->exportCurrentDocument(); });
    //
    QShortcut* fullScreenShortcut = new QShortcut(QKeySequence::FullScreen, d->applicationView);
    fullScreenShortcut->setContext(Qt::ApplicationShortcut);
    connect(fullScreenShortcut, &QShortcut::activated, this, [this] { d->toggleFullScreen(); });

    //
    // Представление приложения
    //
    connect(d->applicationView, &Ui::ApplicationView::turnOffFullScreenRequested, this,
            [this] { d->toggleFullScreen(); });
    connect(d->applicationView, &Ui::ApplicationView::closeRequested, this, [this] {
        auto callback = [this] { d->gitHubSyncManager->uploadBeforeExit([this] { d->exit(); }); };
        d->saveIfNeeded(callback);
    });

    //
    // Представление меню
    //
    connect(d->menuView, &Ui::MenuView::projectsPressed, this, [this] { d->showProjects(); });
    connect(d->menuView, &Ui::MenuView::createProjectPressed, this, [this] { d->createProject(); });
    connect(d->menuView, &Ui::MenuView::openProjectPressed, this, [this] { d->openProject(); });
    connect(d->menuView, &Ui::MenuView::projectPressed, this, [this] { d->showProject(); });
    connect(d->menuView, &Ui::MenuView::saveProjectChangesPressed, this,
            [this] { d->saveChanges(); });
    connect(d->menuView, &Ui::MenuView::saveProjectAsPressed, this, [this] {
        auto callback = [this] { d->saveAs(); };
        d->saveIfNeeded(callback);
    });
    connect(d->menuView, &Ui::MenuView::importPressed, this, [this] { d->importProject(); });
    connect(d->menuView, &Ui::MenuView::exportCurrentDocumentPressed, this,
            [this] { d->exportCurrentDocument(); });
    connect(d->menuView, &Ui::MenuView::fullscreenPressed, this, [this] { d->toggleFullScreen(); });
    connect(d->menuView, &Ui::MenuView::settingsPressed, this, [this] { d->showSettings(); });
    //
    connect(d->menuView, &Ui::MenuView::writingStatisticsPressed, this, [this] {
#ifdef CLOUD_SERVICE_MANAGER
        d->cloudServiceManager->askSessionStatistics(
            d->writingSessionManager->sessionStatisticsLastSyncDateTime());
#endif
        d->showSessionStatistics();
    });
    connect(d->menuView, &Ui::MenuView::writingSprintPressed, this,
            [this] { d->writingSessionManager->showSprintPanel(); });
    //
    // Менеджер посадки
    //
    connect(d->onboardingManager.data(), &OnboardingManager::finished, this, [this](bool _sync) {
        setSettingsValue(DataStorageLayer::kApplicationConfiguredKey, true);
        setSettingsValue(DataStorageLayer::kApplicationThemeKey,
                         static_cast<int>(Ui::DesignSystem::theme()));
        if (Ui::DesignSystem::theme() == Ui::ApplicationTheme::Custom) {
            setSettingsValue(DataStorageLayer::kApplicationCustomThemeColorsKey,
                             Ui::DesignSystem::color().toString());
        }
        setSettingsValue(DataStorageLayer::kApplicationScaleFactorKey,
                         Ui::DesignSystem::scaleFactor());

        //
        // Перечитаем настройки после завершения онбординга, чтобы учесть настройки заданные
        // пользователем во время работы с ним
        //
        d->settingsManager->reloadSettings();

        //
        // И затем отображаем контент самого приложения
        //
        d->showContent();
        const int duration = d->applicationView->slideViewOut();
        //
        // Ждем, чтобы геометрия успела загрузиться, и сохраняем её.
        // Без этого после аварийного завершения при первом запуске (если вдруг такое случится)
        // левая панель будет занимать весь экран
        //
        QTimer::singleShot(duration, this, [this, _sync] {
            d->openStartupProject();
            setSettingsValues(DataStorageLayer::kApplicationViewStateKey,
                              d->applicationView->saveState());

            //
            // Check the repository the user has just set and offer the stories it keeps
            //
            if (_sync) {
                d->gitHubSyncManager->sync();
            }
        });
    });

    //
    // Менеджер горячих клавиш
    //
    connect(d->shortcutsManager.data(), &ShortcutsManager::importShortcutChanged,
            [this](const QKeySequence& _shortcut) {
                d->importShortcut->setKey(_shortcut);
                d->menuView->setImportShortcut(_shortcut);
            });
    connect(d->shortcutsManager.data(), &ShortcutsManager::exportShortcutChanged,
            [this](const QKeySequence& _shortcut) {
                d->exportShortcut->setKey(_shortcut);
                d->menuView->setCurrentDocumentExportShortcut(_shortcut);
            });

    //
    // Менеджер проектов
    //
    connect(d->projectsManager.data(), &ProjectsManager::menuRequested, this,
            [this] { d->showMenu(); });
    connect(d->projectsManager.data(), &ProjectsManager::createProjectRequested, this,
            [this] { d->createProject(); });
    connect(d->projectsManager.data(), &ProjectsManager::createLocalProjectRequested, this,
            [this](const QString& _projectName, const QString& _projectPath,
                   const QString& _importFilePath) {
                d->createLocalProject(_projectName, _projectPath, _importFilePath);
            });
    connect(d->projectsManager.data(), &ProjectsManager::openProjectRequested, this,
            [this] { d->openProject(); });
    connect(d->projectsManager.data(), &ProjectsManager::openLocalProjectRequested, this,
            [this](const QString& _path) {
                if (d->projectsManager->currentProject() != nullptr
                    && d->projectsManager->currentProject()->path() == _path) {
                    d->showProject();
                    return;
                }

                auto callback = [this, _path] { openProject(_path); };
                d->saveIfNeeded(callback);
            });
    connect(d->projectsManager.data(), &ProjectsManager::closeCurrentProjectRequested, this,
            [this] { d->closeCurrentProject(); });

    //
    // Sync of the stories with GitHub
    //
    d->projectsManager->setProjectActionsProvider(
        [this](BusinessLayer::ProjectsModelProjectItem* _project) {
            return d->gitHubSyncManager->projectActions(_project);
        });
    connect(d->projectsManager.data(), &ProjectsManager::syncProjectsRequested,
            d->gitHubSyncManager.data(), &GitHubSyncManager::sync);
    connect(d->gitHubSyncManager.data(), &GitHubSyncManager::saveCurrentProjectRequested, this,
            [this] { d->saveChanges(); });
    d->gitHubSyncManager->setProjectReplacementHandler([this](const QString& _path) {
        if (d->projectsManager->currentProject() != nullptr
            && d->projectsManager->currentProject()->path() == _path) {
            if (!d->saveChanges()) {
                return false;
            }
            d->closeCurrentProject();
            d->showProjects();
        }
        return true;
    });
    connect(d->gitHubSyncManager.data(), &GitHubSyncManager::setupRequested, this, [this] {
        d->showSettings();
        d->settingsManager->showGitHubSync();
    });

    //
    // Менеджер проекта
    //
    connect(d->projectManager.data(), &ProjectManager::menuRequested, this,
            [this] { d->showMenu(); });
    connect(d->projectManager.data(), &ProjectManager::contentsChanged, this,
            [this] { d->markChangesSaved(false); });
    connect(d->projectManager.data(), &ProjectManager::projectUuidChanged,
            d->projectsManager.data(), &ProjectsManager::setCurrentProjectUuid);
    connect(d->projectManager.data(), &ProjectManager::projectNameChanged, this,
            [this](const QString& _name) {
                d->menuView->setProjectTitle(_name);
                d->updateWindowTitle(_name);
            });
    connect(d->projectManager.data(), &ProjectManager::projectLoglineChanged,
            d->projectsManager.data(), &ProjectsManager::setCurrentProjectLogline);
    connect(d->projectManager.data(), &ProjectManager::projectCoverChanged,
            d->projectsManager.data(), &ProjectsManager::setCurrentProjectCover);
    connect(d->projectManager.data(), &ProjectManager::currentModelChanged, this,
            [this](BusinessLayer::AbstractModel* _model) {
                const bool _available = d->exportManager->canExportDocument(_model);
                d->menuView->setCurrentDocumentExportAvailable(_available);
                d->projectManager->setCurrentDocumentExportAvailable(_available);
            });
    connect(d->projectManager.data(), &ProjectManager::exportCurrentDocumentRequested, this,
            [this] { d->exportCurrentDocument(); });
    connect(d->projectManager.data(), &ProjectManager::importRequested, d->importManager.data(),
            &ImportManager::import);
    connect(d->projectManager.data(), &ProjectManager::importFileRequested, d->importManager.data(),
            &ImportManager::importToDocument);

    //
    // Менеджер импорта
    //
    connect(d->importManager.data(), &ImportManager::characterImported, d->projectManager.data(),
            &ProjectManager::storeCharacter);
    connect(d->importManager.data(), &ImportManager::locationImported, d->projectManager.data(),
            &ProjectManager::storeLocation);
    connect(d->importManager.data(), &ImportManager::documentImported, d->projectManager.data(),
            [this](const BusinessLayer::AbstractImporter::Document& _document) {
                d->projectManager->storeDocument(_document);
            });
    connect(d->importManager.data(), &ImportManager::simpleTextImported, d->projectManager.data(),
            &ProjectManager::storeSimpleText);
    connect(d->importManager.data(), &ImportManager::screenplayImported, d->projectManager.data(),
            &ProjectManager::storeScreenplay);
    connect(d->importManager.data(), &ImportManager::presentationImported, d->projectManager.data(),
            &ProjectManager::storePresentation);

    //
    // Менеджер настроек
    //
    connect(d->settingsManager.data(), &SettingsManager::closeSettingsRequested, this,
            [this] { d->showLastContent(); });
    //
    auto postSpellingChangeEvent = [this] {
        auto event = new TextEditingOptionsChangeEvent;
        event->spelling
            = { settingsValue(DataStorageLayer::kApplicationUseSpellCheckerKey).toBool(),
                settingsValue(DataStorageLayer::kApplicationSpellCheckerLanguageKey).toString() };
        QApplication::postEvent(this, event);
    };
    connect(d->settingsManager.data(), &SettingsManager::applicationUseSpellCheckerChanged, this,
            postSpellingChangeEvent);
    connect(d->settingsManager.data(), &SettingsManager::applicationSpellCheckerLanguageChanged,
            this, postSpellingChangeEvent);
    //
    connect(
        d->settingsManager.data(), &SettingsManager::applicationThemeChanged, this,
        [this](Ui::ApplicationTheme _theme) {
            d->setDesignSystemTheme(_theme);
            //
            // ... если применяется кастомная тема, то нужно загрузить её цвета
            //
            if (_theme == Ui::ApplicationTheme::Custom) {
                d->setDesignSystemCustomThemeColors(Ui::DesignSystem::Color(
                    settingsValue(DataStorageLayer::kApplicationCustomThemeColorsKey).toString()));
            }
        });
    connect(d->settingsManager.data(), &SettingsManager::applicationCustomThemeColorsChanged, this,
            [this](const Ui::DesignSystem::Color& _color) {
                d->setDesignSystemCustomThemeColors(_color);
            });
    connect(d->settingsManager.data(), &SettingsManager::applicationScaleFactorChanged, this,
            [this](qreal _scaleFactor) { d->setDesignSystemScaleFactor(_scaleFactor); });
    connect(d->settingsManager.data(), &SettingsManager::applicationDensityChanged, this,
            [this](int _density) { d->setDesignSystemDensity(_density); });
    connect(d->settingsManager.data(), &SettingsManager::applicationUseAutoSaveChanged, this,
            [this] { d->configureAutoSave(); });

    //
    connect(d->settingsManager.data(), &SettingsManager::simpleTextEditorChanged, this,
            [this](const QStringList& _changedSettingsKeys) {
                using namespace DataStorageLayer;
                //
                // Отправим событие для текстовых полей
                //
                auto event = new TextEditingOptionsChangeEvent;
                if (_changedSettingsKeys.contains(kApplicationCorrectDoubleCapitalsKey)) {
                    event->correctDoubleCapitals
                        = settingsValue(kApplicationCorrectDoubleCapitalsKey).toBool();
                }
                if (_changedSettingsKeys.contains(kApplicationCapitalizeSingleILetterKey)) {
                    event->capitalizeSingleILetter
                        = settingsValue(kApplicationCapitalizeSingleILetterKey).toBool();
                }
                if (_changedSettingsKeys.contains(kApplicationReplaceThreeDotsWithEllipsisKey)) {
                    event->replaceThreeDots
                        = settingsValue(kApplicationReplaceThreeDotsWithEllipsisKey).toBool();
                }
                if (_changedSettingsKeys.contains(kApplicationSmartQuotesKey)) {
                    event->useSmartQuotes = settingsValue(kApplicationSmartQuotesKey).toBool();
                }
                if (_changedSettingsKeys.contains(kApplicationReplaceTwoDashesWithEmDashKey)) {
                    event->replaceTwoDashes
                        = settingsValue(kApplicationReplaceTwoDashesWithEmDashKey).toBool();
                }
                if (_changedSettingsKeys.contains(kApplicationAvoidMultipleSpacesKey)) {
                    event->avoidMultipleSpaces
                        = settingsValue(kApplicationAvoidMultipleSpacesKey).toBool();
                }
                QApplication::postEvent(this, event);

                //
                // Настроим плагины
                //
                d->projectManager->reconfigureSimpleTextEditor(_changedSettingsKeys);
            });
    connect(d->settingsManager.data(), &SettingsManager::simpleTextNavigatorChanged, this,
            [this] { d->projectManager->reconfigureSimpleTextNavigator(); });
    //
    connect(d->settingsManager.data(), &SettingsManager::screenplayEditorChanged, this,
            [this](const QStringList& _changedSettingsKeys) {
                d->projectManager->reconfigureScreenplayEditor(_changedSettingsKeys);
            });
    connect(d->settingsManager.data(), &SettingsManager::screenplayNavigatorChanged, this,
            [this] { d->projectManager->reconfigureScreenplayNavigator(); });
    connect(d->settingsManager.data(), &SettingsManager::screenplayDurationChanged, this, [this] {
        d->projectManager->reconfigureScreenplayEditor({});
        d->projectManager->reconfigureScreenplayNavigator();
        d->projectManager->reconfigureScreenplayDuration();
    });
    //
    connect(d->settingsManager.data(), &SettingsManager::advancedAccurateMetricsHandlingChanged,
            this, [this] {
                //
                // Метрики нельзя переключить на лету, так что если пользователь сюда пришёл,
                // перезапускаем прилжоение
                //
                // ... закроем текущий проект
                //
                d->gitHubSyncManager->settleCredentialsBeforeExit([this] {
                    d->closeCurrentProject();
                    //
                    // ... сохраним настройки, чтобы они успешно считались следующей копией
                    // приложения
                    //
                    DataStorageLayer::StorageFacade::settingsStorage()->sync();
                    //
                    // ... перезапустим приложение
                    //
                    restartApplication();
                });
            });
    connect(d->settingsManager.data(), &SettingsManager::advancedUseExtendingLoggingChanged, this,
            [this] { d->initLogging(); });
    //
    connect(d->settingsManager.data(), &SettingsManager::resetToDefaultsRequested, this,
            [this](bool _fullReset) {
                //
                // Если пользователь хочет сбросить все настройки, закроем текущий проект
                //
                d->gitHubSyncManager->settleCredentialsBeforeExit([this, _fullReset] {
                    d->closeCurrentProject();
                    //
                    // ... сбросим настройки
                    //
                    DataStorageLayer::StorageFacade::settingsStorage()->resetToDefaults(_fullReset);
                    //
                    // ... перезапустим приложение
                    //
                    restartApplication();
                });
            });

    //
    // Менеджер статистики по сессиям работы с программой
    //
    connect(d->writingSessionManager.data(),
            &WritingSessionManager::closeSessionStatisticsRequested, this,
            [this] { d->showLastContent(); });

#ifdef CLOUD_SERVICE_MANAGER
    //
    // Менеджер облака
    //

    auto configureConnectionStatus = [this](bool _connected) {
        Log::trace("[ApplicationManager] Connection status changed. %1.",
                   _connected ? "Connected" : "Disconnected");
        d->connectionStatus->setConnectionAvailable(_connected);

        //
        // Сбросим список курсоров соавторов при смене состояния соединения
        //
        d->projectManager->clearCursors();

        //
        // Синхронизация офлайн правок будет сделана, после проверки ключа сессии на сервере
        //
    };
    connect(d->cloudServiceManager.data(), &CloudServiceManager::connected, d->connectionStatus,
            [configureConnectionStatus] { configureConnectionStatus(true); });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::disconnected, d->connectionStatus,
            [configureConnectionStatus] { configureConnectionStatus(false); });
    connect(d->connectionStatus, &Ui::ConnectionStatusToolBar::checkConnectionPressed,
            d->cloudServiceManager.data(), &CloudServiceManager::start);
    //
    // Stable release feed retained for askUpdateToLatestVersion(), without notification UI.
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::notificationsReceived,
            d->notificationsManager.data(), &NotificationsManager::processNotifications);
    //
    // Нужно обновить версию приложения для работы с облаком
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::appVersionUpgradeRequired, this,
            [this] { d->askUpdateToLatestVersion(); });
    //
    // Проверка регистрация или вход
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::loginCompleted, this,
            [this] {
                d->cloudServiceManager->askAccountInfo();
                d->cloudServiceManager->askNotifications();
                d->cloudServiceManager->askTeams();
                d->cloudServiceManager->askProjects();
                d->cloudServiceManager->askSessionStatistics(
                    d->writingSessionManager->sessionStatisticsLastSyncDateTime());

                //
                // Если поймали подключение и сейчас работаем с облачным проектом
                //
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject != nullptr && currentProject->isRemote()) {
                    const auto currentProjectId = d->projectsManager->currentProject()->id();
                    //
                    // ... то синхронизируем все документы, у которых есть офлайн правки
                    //
                    const auto unsyncedDocuments = d->projectManager->unsyncedDocuments();
                    for (const auto document : unsyncedDocuments) {
                        d->projectManager->notifyDownloadDocumentRequested(document->uuid());
                    }
                    //
                    // ... текущий открытый документ
                    //
                    if (const auto currentDocument = d->projectManager->currentDocument();
                        currentDocument != nullptr) {
                        d->projectManager->notifyDownloadDocumentRequested(currentDocument->uuid());
                    }
                    //
                    // ... а также структуру, данные проекта и словари
                    //
                    d->cloudServiceManager->openStructure(currentProjectId);
                    d->cloudServiceManager->openProjectInfo(currentProjectId);
                    d->cloudServiceManager->openScreenplayDictionaries(currentProjectId);
                }
            });

    //
    // Параметры аккаунта
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::accountInfoReceived, this,
            [this](const Domain::AccountInfo& _accountInfo) {
                d->isCloudAccountAuthorized = _accountInfo.isValid();
                d->projectManager->checkAvailabilityToEdit();
                d->projectManager->setBlockedDocumentTypes(
                    d->cloudServiceManager->blockedDocumentTypes());
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::logoutRequired, this, [this] {
        const auto currentProject = d->projectsManager->currentProject();
        if (currentProject != nullptr && currentProject->isRemote()) {
            d->saveChanges();
            d->closeCurrentProject();
            d->showProjects();
        } else {
            d->showLastContent();
        }

        d->isCloudAccountAuthorized = false;
        d->cloudServiceManager->logout();
        d->projectManager->checkAvailabilityToEdit();
    });
    //
    connect(Ui::AvatarGenerator::instance(), &Ui::AvatarGenerator::avatarRequested,
            d->cloudServiceManager.data(), &CloudServiceManager::askAvatar);
    connect(d->cloudServiceManager.data(), &CloudServiceManager::avatarRecieved,
            Ui::AvatarGenerator::instance(), &Ui::AvatarGenerator::setAvatar);

    //
    // Команды
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::teamsReceived, this,
            [this](const QVector<Domain::TeamInfo>& _teams) {
                d->projectsManager->setTeams(_teams);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::teamCreated, this,
            [this](const Domain::TeamInfo& _team) {
                d->projectsManager->addOrUpdateTeam(_team);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::teamUpdated, this,
            [this](const Domain::TeamInfo& _team) {
                d->projectsManager->addOrUpdateTeam(_team);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::teamRemoved, this,
            [this](int _id) {
                d->projectsManager->hideTeam(_id);
            });
    //
    // Проекты
    //
    connect(d->cloudServiceManager.data(), &CloudServiceManager::projectsReceived,
            d->projectsManager.data(), [this](const QVector<Domain::ProjectInfo>& _projects) {
                d->projectsManager->setCloudProjects(_projects);

                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject != nullptr && currentProject->isRemote()) {
                    d->projectManager->updateCurrentProject(currentProject);
                }
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::projectUpdated, this,
            [this](const Domain::ProjectInfo& _projectInfo) {
                d->projectsManager->addOrUpdateCloudProject(_projectInfo);

                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject != nullptr && _projectInfo.id == currentProject->id()) {
                    d->projectManager->updateCurrentProject(d->projectsManager->currentProject());
                }
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::projectRemoved,
            d->projectsManager.data(), [this](int _projectId) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject != nullptr && currentProject->id() == _projectId) {
                    d->closeCurrentProject();
                    d->showProjects();
                }
                d->projectsManager->removeProject(_projectId);
            });
    connect(d->projectsManager.data(), &ProjectsManager::openCloudProjectRequested, this,
            [this](int _id, const QString& _path) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject != nullptr && currentProject->id() == _id) {
                    d->showProject();
                    return;
                }

                auto callback = [this, _id, _path] {
                    const auto isProjectOpened = openProject(_path);
                    if (!isProjectOpened) {
                        return;
                    }

                    d->cloudServiceManager->openStructure(_id);
                    d->cloudServiceManager->openProjectInfo(_id);
                    d->cloudServiceManager->openScreenplayDictionaries(_id);
                };
                d->saveIfNeeded(callback);
            });
    connect(d->projectsManager.data(), &ProjectsManager::updateCloudProjectRequested,
            d->cloudServiceManager.data(), &CloudServiceManager::updateProject);
    connect(d->projectManager.data(), &ProjectManager::projectCollaboratorInviteRequested,
            d->projectsManager.data(),
            [this](const QString& _email, const QColor& _color, int _role,
                   const QHash<QUuid, int>& _permissions) {
                Q_UNUSED(_color)
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                const auto accountRole = _role;
                d->cloudServiceManager->addProjectCollaborator(currentProject->id(), _email,
                                                               accountRole, _permissions);
            });
    connect(d->projectManager.data(), &ProjectManager::projectCollaboratorUpdateRequested,
            d->projectsManager.data(),
            [this](const QString& _email, const QColor& _color, int _role,
                   const QHash<QUuid, int>& _permissions) {
                Q_UNUSED(_color)
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                const auto accountRole = _role;
                d->cloudServiceManager->updateProjectCollaborator(currentProject->id(), _email,
                                                                  accountRole, _permissions);
            });
    connect(d->projectManager.data(), &ProjectManager::projectCollaboratorRemoveRequested,
            d->projectsManager.data(), [this](const QString& _email) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->cloudServiceManager->removeProjectCollaborator(currentProject->id(), _email);
            });
    connect(d->projectsManager.data(), &ProjectsManager::removeCloudProjectRequested, this,
            [this](int _id) { d->cloudServiceManager->removeProject(_id); });
    connect(d->projectsManager.data(), &ProjectsManager::unsubscribeFromCloudProjectRequested, this,
            [this](int _id) { d->cloudServiceManager->unsubscribeFromProject(_id); });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::projectCantBeSynced, this, [this] {
        const auto currentProject = d->projectsManager->currentProject();

        //
        // Если проект уже в состоянии проблем синхронизации, то не показываем диалог вновь,
        // чтобы не поймать эффект множественного наложения диалогов
        //
        if (currentProject == nullptr || currentProject->isLocal()
            || !currentProject->canBeSynced()) {
            return;
        }

        d->projectsManager->setCurrentProjectCanBeSynced(false);

        auto dialog = new Dialog(d->applicationView);
        dialog->showDialog({}, tr("Cloud sync is unavailable. You can continue working offline."),
                           { { 0, tr("Continue offline"), Dialog::RejectButton } });
        QObject::connect(dialog, &Dialog::finished, dialog, &Dialog::hideDialog);
        QObject::connect(dialog, &Dialog::disappeared, dialog, &Dialog::deleteLater);
    });

    //
    // Документы
    //
    connect(d->projectManager.data(), &ProjectManager::structureModelChanged, this,
            [this](BusinessLayer::AbstractModel* _model) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->cloudServiceManager->openDocument(currentProject->id(), _model->document());
            });
    connect(d->projectManager.data(), &ProjectManager::downloadDocumentRequested, this,
            [this](const QUuid& _documentUuid) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced() || _documentUuid.isNull()) {
                    return;
                }

                //
                // Для сложных типов документов, нужна подгрузка документов, которые с ним связаны,
                // а сама загрузка документов должна происходить в строго определённом порядке
                //
                auto documentsToSync = d->projectManager->documentBundle(_documentUuid);
                if (documentsToSync.isEmpty()) {
                    return;
                }
                QVector<Domain::DocumentObject*> documentsToUpdate;
                QVector<Domain::DocumentObject*> temporaryDocuments;
                for (const auto& documentToSync : std::as_const(documentsToSync)) {
                    const auto document = d->projectManager->documentToSync(documentToSync);
                    //
                    // Если документ не удалось вытащить из базы
                    //
                    if (document == nullptr) {
                        //
                        // ... значит он ещё не был синхронизирован и для него нужна болванка,
                        //     чтобы запросить его с сервера в вместе с остальными документами
                        //
                        auto temporaryDocument = Domain::ObjectsBuilder::createDocument(
                            {}, documentToSync, Domain::DocumentObjectType::Undefined, {}, {});
                        documentsToUpdate.append(temporaryDocument);
                        temporaryDocuments.append(temporaryDocument);
                    }
                    //
                    // А если есть, то значит его инстанс уже есть в базе и его надо обновить
                    //
                    else {
                        documentsToUpdate.append(document);
                    }
                }
                d->cloudServiceManager->openDocuments(currentProject->id(), documentsToUpdate);
                //
                // ... очищаем все временные объекты
                //
                qDeleteAll(temporaryDocuments);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::documentReceived, this,
            [this](const Domain::DocumentInfo& _documentInfo) {
                //
                // Мержим изменения только в том случае, если пользователь работает с облачным
                // проектом, если сохранить проект локально, то гуиды документов будут совпадать и
                // поэтому изменения будут пытаться смержиться с локальным проектом
                //
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->projectManager->mergeDocumentInfo(_documentInfo);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::documentChanged,
            d->projectManager.data(), &ProjectManager::applyDocumentChanges);
    connect(d->projectManager.data(), &ProjectManager::contentsChanged, this,
            [this](BusinessLayer::AbstractModel* _model) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                //
                // Запросить отправку изменений
                //
                d->cloudServiceManager->startDocumentChange(currentProject->id(),
                                                            _model->document()->uuid());

                //
                // Запустить отсчёт до следующей отправки документа целиком
                //
                d->projectManager->planDocumentSyncing(_model->document()->uuid());
            });
    connect(d->projectManager.data(), &ProjectManager::uploadDocumentRequested, this,
            [this](const QUuid& _documentUuid, bool _isNewDocument) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                //
                // Если был создан новый документ, то кидаем его в облако и подписываемся
                //
                if (_isNewDocument) {
                    d->cloudServiceManager->openDocument(
                        currentProject->id(), d->projectManager->documentToSync(_documentUuid),
                        d->projectManager->draftSource(_documentUuid));
                }
                //
                // В противном случае, запросим отправку изменений
                //
                else {
                    d->cloudServiceManager->startDocumentChange(currentProject->id(),
                                                                _documentUuid);
                }
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::documentChangeAllowed, this,
            [this](const QUuid& _documentUuid) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                //
                // Если есть изменения, то отправляем их
                //
                const auto changes = d->projectManager->unsyncedChanges(_documentUuid);
                if (!changes.isEmpty()) {
                    d->cloudServiceManager->pushDocumentChange(currentProject->id(), _documentUuid,
                                                               changes);
                }
                //
                // В противном случае, отправляем документ целиком
                //
                else if (auto document = d->projectManager->documentToSync(_documentUuid);
                         document != nullptr) {
                    d->cloudServiceManager->pushDocumentChange(currentProject->id(), _documentUuid,
                                                               document->content());
                }
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::documentChangesPushed,
            d->projectManager.data(), &ProjectManager::markChangesSynced);
    connect(d->projectManager.data(), &ProjectManager::closeDocumentRequested, this,
            [this](const QUuid& _documentUuid) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->cloudServiceManager->closeDocument(currentProject->id(), _documentUuid);
            });
    connect(d->projectManager.data(), &ProjectManager::documentRemoved, this,
            [this](const QUuid& _documentUuid) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->cloudServiceManager->removeDocument(currentProject->id(), _documentUuid);
            });

    //
    // Курсоры
    //
    connect(d->projectManager.data(), &ProjectManager::cursorChanged, this,
            [this](const QUuid& _documentUuid, const QByteArray& _cursorData) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || !currentProject->canBeSynced()) {
                    return;
                }

                d->cloudServiceManager->updateCursor(currentProject->id(), _documentUuid,
                                                     _cursorData);
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::cursorsChanged, this,
            [this](int _projectId, const QVector<Domain::CursorInfo>& _cursors) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()
                    || currentProject->id() != _projectId) {
                    return;
                }

                d->projectManager->setCursors(_cursors);
            });

    //
    // Статистика по сессии
    //
    connect(d->writingSessionManager.data(),
            &WritingSessionManager::sessionStatisticsPublishRequested, this,
            [this](const QVector<Domain::SessionStatistics>& _sessionStatistics) {
                //
                // ... если пользователь авторизован, то отправляем статистику в сервис
                //
                if (d->isCloudAccountAuthorized) {
                    d->cloudServiceManager->pushSessionStatistics(_sessionStatistics);
                }
            });
    connect(d->cloudServiceManager.data(), &CloudServiceManager::sessionStatisticsReceived,
            d->writingSessionManager.data(), &WritingSessionManager::setSessionStatistics);

    //
    // Генерация текста
    //

    //
    // Тестовый метод, для отправки документа на проверку
    //
    connect(d->projectManager.data(), &ProjectManager::sendDocumentToReviewRequested,
            d->cloudServiceManager.data(),
            [this](const QUuid& _documentUuid, const QString& _draftName, const QString& _comment) {
                const auto currentProject = d->projectsManager->currentProject();
                if (currentProject == nullptr || currentProject->isLocal()) {
                    return;
                }

                d->cloudServiceManager->sendDocumentToReview(currentProject->id(), _documentUuid,
                                                             _draftName, _comment);
            });
    //

#endif
}

} // namespace ManagementLayer
