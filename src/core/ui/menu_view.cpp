#include "menu_view.h"

#include "about_application_dialog.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/drawer/drawer.h>
#include <ui/widgets/label/link_label.h>
#include <utils/3rd_party/WAF/Animation/Animation.h>
#include <utils/helpers/color_helper.h>
#include <utils/helpers/ui_helper.h>
#include <utils/logging.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QMenuBar>
#include <QScrollArea>
#include <QTimer>

namespace Ui {

class MenuView::Implementation
{
public:
    explicit Implementation(MenuView* _parent);

    /**
     * @brief Создание MenuBar
     */
    void createMenuBar();

    MenuView* q = nullptr;

    QScrollArea* menuPage = nullptr;

    Drawer* drawer = nullptr;
    QAction* projects = nullptr;
    QAction* createProject = nullptr;
    QAction* openProject = nullptr;
    QAction* project = nullptr;
    QAction* saveProject = nullptr;
    QAction* saveProjectAs = nullptr;
    QAction* exportCurrentDocument = nullptr;
    QAction* importProject = nullptr;
    QAction* fullScreen = nullptr;
    QAction* settings = nullptr;
    QAction* aboutApplicationAction = nullptr;

    QAction* writingStatistics = nullptr;
    QAction* writingSprint = nullptr;

    Subtitle2LinkLabel* appName = nullptr;
    Body2LinkLabel* appVersion = nullptr;
    Body2Label* aboutAppSpacer = nullptr;
    Body2LinkLabel* aboutApp = nullptr;
    QGridLayout* appInfoLayout = nullptr;
};

MenuView::Implementation::Implementation(MenuView* _parent)
    : q(_parent)
    , menuPage(UiHelper::createScrollArea(_parent))
    , drawer(new Drawer(_parent))
    , projects(new QAction)
    , createProject(new QAction)
    , openProject(new QAction)
    , project(new QAction)
    , saveProject(new QAction)
    , saveProjectAs(new QAction)
    , exportCurrentDocument(new QAction)
    , importProject(new QAction)
    , fullScreen(new QAction)
    , settings(new QAction)
    , aboutApplicationAction(new QAction)
    , writingStatistics(new QAction)
    , writingSprint(new QAction)
    , appName(new Subtitle2LinkLabel(_parent))
    , appVersion(new Body2LinkLabel(_parent))
    , aboutAppSpacer(new Body2Label(_parent))
    , aboutApp(new Body2LinkLabel(_parent))
    , appInfoLayout(new QGridLayout)
{
    //
    // Настроим страницу меню
    //
    {
        drawer->addAction(projects);
        drawer->addAction(createProject);
        drawer->addAction(openProject);
        drawer->addAction(project);
        drawer->addAction(saveProject);
        drawer->addAction(saveProjectAs);
        drawer->addAction(importProject);
        drawer->addAction(exportCurrentDocument);
        drawer->addAction(fullScreen);
        drawer->addAction(settings);
        drawer->addAction(writingStatistics);
        drawer->addAction(writingSprint);

        projects->setIconText(u8"\U000f024b");
        projects->setCheckable(true);
        projects->setChecked(true);
        //
        createProject->setIconText(u8"\U000f0415");
        createProject->setCheckable(false);
        //
        openProject->setIconText(u8"\U000f0770");
        openProject->setCheckable(false);
        //
        project->setIconText(u8"\U000f00be");
        project->setCheckable(true);
        project->setVisible(false);
        project->setSeparator(true);
        //
        saveProject->setIconText(u8"\U000f0193");
        saveProject->setCheckable(false);
        saveProject->setEnabled(false);
        saveProject->setVisible(false);
        //
        saveProjectAs->setIconText(" ");
        saveProjectAs->setCheckable(false);
        saveProjectAs->setVisible(false);
        //
        exportCurrentDocument->setIconText(u8"\U000f0207");
        exportCurrentDocument->setCheckable(false);
        exportCurrentDocument->setEnabled(false);
        exportCurrentDocument->setVisible(false);
        //
        importProject->setIconText(u8"\U000f02fa");
        importProject->setCheckable(false);
        importProject->setVisible(false);
        //
        fullScreen->setIconText(u8"\U000F0293");
        fullScreen->setCheckable(false);
        fullScreen->setVisible(false);
        fullScreen->setSeparator(true);
        //
        settings->setIconText(u8"\U000f0493");
        settings->setCheckable(false);
        settings->setVisible(true);
        settings->setSeparator(true);

        QActionGroup* actions = new QActionGroup(_parent);
        actions->addAction(projects);
        actions->addAction(project);
        actions->addAction(settings);

        writingStatistics->setIconText(u8"\U000F085D");
        //
        writingSprint->setIconText(u8"\U000F13AB");

        appName->setText("Story Architect");
        appName->setLink(QUrl("https://starc.app"));
        appVersion->setLink(QUrl("https://starc.app/blog/"));
        aboutAppSpacer->setText(" - ");

        appInfoLayout->setContentsMargins({});
        appInfoLayout->setSpacing(0);
        appInfoLayout->addWidget(appName, 0, 0, 1, 4, Qt::AlignLeft);
        appInfoLayout->addWidget(appVersion, 1, 0);
        appInfoLayout->addWidget(aboutAppSpacer, 1, 1);
        appInfoLayout->addWidget(aboutApp, 1, 2);
        appInfoLayout->setColumnStretch(3, 1);

        auto layout = static_cast<QVBoxLayout*>(menuPage->widget()->layout());
        layout->addWidget(drawer);
        layout->addStretch();
        layout->addLayout(appInfoLayout);
    }
}

// ****

MenuView::MenuView(QWidget* _parent)
    : StackWidget(_parent)
    , d(new Implementation(this))
{
    setAnimationType(StackWidget::AnimationType::Slide);
    setCurrentWidget(d->menuPage);

    connect(d->projects, &QAction::triggered, this, &MenuView::projectsPressed);
    connect(d->createProject, &QAction::triggered, this, &MenuView::createProjectPressed);
    connect(d->openProject, &QAction::triggered, this, &MenuView::openProjectPressed);
    connect(d->project, &QAction::triggered, this, &MenuView::projectPressed);
    connect(d->saveProject, &QAction::triggered, this, &MenuView::saveProjectChangesPressed);
    connect(d->saveProjectAs, &QAction::triggered, this, &MenuView::saveProjectAsPressed);
    connect(d->importProject, &QAction::triggered, this, &MenuView::importPressed);
    connect(d->exportCurrentDocument, &QAction::triggered, this,
            &MenuView::exportCurrentDocumentPressed);
    connect(d->fullScreen, &QAction::triggered, this, &MenuView::fullscreenPressed);
    connect(d->settings, &QAction::triggered, this, &MenuView::settingsPressed);
    //
    connect(d->writingStatistics, &QAction::triggered, this, &MenuView::writingStatisticsPressed);
    connect(d->writingSprint, &QAction::triggered, this, &MenuView::writingSprintPressed);
    //

    connect(d->aboutApplicationAction, &QAction::triggered, this, [this] {
        auto dialog = new AboutApplicationDialog(parentWidget());
        connect(dialog, &Ui::AboutApplicationDialog::disappeared, dialog,
                &Ui::AboutApplicationDialog::deleteLater);
        dialog->showDialog();
    });
    connect(d->aboutApp, &Body2LinkLabel::clicked, d->aboutApplicationAction, &QAction::trigger);

    connect(this, &MenuView::projectsPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::createProjectPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::openProjectPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::projectPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::saveProjectAsPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::importPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::exportCurrentDocumentPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::fullscreenPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::settingsPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::helpPressed, this, &MenuView::closeMenu);
    //
    connect(this, &MenuView::writingStatisticsPressed, this, &MenuView::closeMenu);
    connect(this, &MenuView::writingSprintPressed, this, &MenuView::closeMenu);

    setVisible(false);

    //
    // Добавляем MenuBar
    //
    d->createMenuBar();
}

MenuView::~MenuView() = default;

void MenuView::checkProjects()
{
    QSignalBlocker signalBlocker(this);
    d->projects->setChecked(true);
}

void MenuView::setProjectActionsVisible(bool _visible)
{
    d->project->setVisible(_visible);
    d->saveProject->setVisible(_visible);
    d->saveProjectAs->setVisible(_visible);
    d->exportCurrentDocument->setVisible(_visible);
    d->importProject->setVisible(_visible);
    d->fullScreen->setVisible(_visible);
    d->drawer->updateGeometry();
}

void MenuView::checkProject()
{
    QSignalBlocker signalBlocker(this);
    d->project->setChecked(true);
}

void MenuView::setProjectTitle(const QString& _title)
{
    d->project->setText(_title);
}

void MenuView::checkSettings()
{
    QSignalBlocker signalBlocker(this);
    d->importProject->setChecked(true);
}

void MenuView::markChangesSaved(bool _saved)
{
    d->saveProject->setEnabled(!_saved);
    d->saveProject->setText(_saved ? tr("All changes saved") : tr("Save changes"));
    update();
}

void MenuView::setImportShortcut(const QKeySequence& _key)
{
    d->importProject->setWhatsThis(_key.toString(QKeySequence::NativeText));
}

void MenuView::setImportAvailable(bool _available)
{
    d->importProject->setEnabled(_available);
}

void MenuView::setCurrentDocumentExportShortcut(const QKeySequence& _key)
{
    d->exportCurrentDocument->setWhatsThis(_key.toString(QKeySequence::NativeText));
}

void MenuView::setCurrentDocumentExportAvailable(bool _available)
{
    d->exportCurrentDocument->setEnabled(_available);
}

void MenuView::openMenu()
{
    Log::info("Show menu");

    //
    // Скрываем, переключаем и показываем, чтобы виджет просто переключился, без анимации
    //
    hide();
    setCurrentWidget(d->menuPage);
    show();

    WAF::Animation::sideSlideIn(this);
}

void MenuView::closeMenu()
{
    Log::info("Hide menu");
    const auto duration = WAF::Animation::sideSlideOut(this);
    QTimer::singleShot(duration, this, &MenuView::hide);
}

QSize MenuView::sizeHint() const
{
    return d->drawer->sizeHint();
}

void MenuView::keyPressEvent(QKeyEvent* _event)
{
    if (_event->key() == Qt::Key_Escape) {
        closeMenu();
    }

    StackWidget::keyPressEvent(_event);
}

void MenuView::updateTranslations()
{
    d->projects->setText(tr("Stories"));
    d->createProject->setText(tr("Create story"));
    d->openProject->setText(tr("Open story"));
    d->saveProject->setText(d->saveProject->isEnabled() ? tr("Save story changes")
                                                        : tr("All changes saved"));
    d->saveProject->setWhatsThis(
        QKeySequence(QKeySequence::Save).toString(QKeySequence::NativeText));
    d->saveProjectAs->setText(tr("Save story as..."));
    d->importProject->setText(tr("Import documents..."));
    d->exportCurrentDocument->setText(tr("Export current document..."));
    d->fullScreen->setText(tr("Full screen"));
    d->fullScreen->setWhatsThis(
        QKeySequence(QKeySequence::FullScreen).toString(QKeySequence::NativeText));
    d->settings->setText(tr("Application settings"));

    d->writingStatistics->setToolTip(tr("Show writing statistics"));
    d->writingSprint->setToolTip(tr("Show writing sprint timer"));

    d->appVersion->setText(
        QString("%1 %2").arg(tr("Version"), QCoreApplication::applicationVersion()));
    d->aboutApp->setText(tr("About"));

    d->writingStatistics->setText(tr("Writing statistics"));
    d->writingSprint->setText(tr("Writing sprint"));
}

void MenuView::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    StackWidget::designSystemChangeEvent(_event);

    setBackgroundColor(Ui::DesignSystem::color().primary());

    for (auto label : std::vector<Widget*>{
             d->appName,
             d->appVersion,
             d->aboutAppSpacer,
             d->aboutApp,
         }) {
        label->setBackgroundColor(Ui::DesignSystem::color().primary());
        label->setTextColor(ColorHelper::transparent(Ui::DesignSystem::color().onPrimary(),
                                                     Ui::DesignSystem::disabledTextOpacity()));
    }
    d->appInfoLayout->setContentsMargins(
        Ui::DesignSystem::layout().px16(), Ui::DesignSystem::layout().px24(),
        Ui::DesignSystem::layout().px16(), Ui::DesignSystem::layout().px16());
    d->appInfoLayout->setVerticalSpacing(Ui::DesignSystem::layout().px8());
}

void MenuView::Implementation::createMenuBar()
{
#ifdef Q_OS_MAC

    QMenuBar* menuBar = new QMenuBar;
    menuBar->setNativeMenuBar(true);

    //
    // Создаем пункты меню
    //

    // Основной пункт меню с нeзвазванием "Story Architect"
    QMenu* appMenu = menuBar->addMenu("Story Architect");
    aboutApplicationAction->setText(tr("About Story Architect"));
    aboutApplicationAction->setMenuRole(QAction::ApplicationSpecificRole);
    appMenu->addAction(aboutApplicationAction);
    appMenu->addSeparator();
    QAction* settingsAction = new QAction();
    settingsAction->setText(tr("Preferences"));
    connect(settingsAction, &QAction::triggered, q, &MenuView::settingsPressed);
    settingsAction->setMenuRole(QAction::ApplicationSpecificRole);
    appMenu->addAction(settingsAction);

#ifdef CLOUD_SERVICE_MANAGER
    appMenu->addSeparator();

    writingStatistics->setMenuRole(QAction::ApplicationSpecificRole);
    appMenu->addAction(writingStatistics);

    writingSprint->setMenuRole(QAction::ApplicationSpecificRole);
    appMenu->addAction(writingSprint);
#endif

    // Меню "File"
    QMenu* fileMenu = menuBar->addMenu(tr("File"));
    fileMenu->addAction(projects);
    fileMenu->addAction(createProject);
    fileMenu->addAction(openProject);
    saveProject->setShortcut(QKeySequence("⌘S"));
    fileMenu->addAction(saveProject);
    fileMenu->addAction(saveProjectAs);
    fileMenu->addSeparator();
    importProject->setShortcut(QKeySequence("⌥I"));
    fileMenu->addAction(importProject);
    exportCurrentDocument->setShortcut(QKeySequence("⌥E"));
    fileMenu->addAction(exportCurrentDocument);

    // Меню View
    QMenu* viewMenu = menuBar->addMenu(tr("View"));
    viewMenu->addAction(fullScreen);
#endif
}

} // namespace Ui
