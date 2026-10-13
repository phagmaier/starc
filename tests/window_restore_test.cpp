#include <ui/application_view.h>
#include <ui/widgets/splitter/splitter.h>

#include <QApplication>
#include <QDataStream>
#include <QFontDatabase>
#include <QIODevice>
#include <QLayout>
#include <QScreen>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>

namespace {
void require(bool _ok, const char* _label)
{
    if (!_ok) {
        std::fprintf(stderr, "FAIL: %s\n", _label);
        std::exit(1);
    }
}

const QString kSplitterState = "splitter/state";
const QString kViewGeometry = "view/geometry";

// The window layout of the profile that aborted startup: the 468x740 first-run window
// with the navigator at full width, saved on an 800 pixel wide screen.
const QByteArray kSavedProportions = QByteArray::fromHex("3ff00000000000000000000000000000");
const QByteArray kSavedGeometry
    = QByteArray::fromHex("01d9d0cb00030000000000a50000001d0000027c00000304000000a70000001f0000027a"
                          "0000030200000000000000000320000000a70000001f0000027a00000302");

// Splitter::saveState() layout.
QByteArray proportions(qreal _first, qreal _second)
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << _first << _second;
    return state;
}

// The saved geometry as if its screen had the given width. Qt refuses to restore a
// geometry whose screen width differs from the current one by more than about a fifth.
QByteArray savedGeometry(int _screenWidth)
{
    auto geometry = kSavedGeometry;
    QByteArray width;
    QDataStream stream(&width, QIODevice::WriteOnly);
    stream << qint32(_screenWidth);
    require(geometry.mid(46, 4) == QByteArray::fromHex("00000320"), "saved screen width field");
    geometry.replace(46, 4, width);
    return geometry;
}

int screenWidth()
{
    return QApplication::primaryScreen()->geometry().width();
}

Splitter* splitterOf(const Ui::ApplicationView& _view)
{
    const auto splitter = _view.findChild<Splitter*>(QString(), Qt::FindDirectChildrenOnly);
    require(splitter != nullptr, "application view owns its splitter");
    return splitter;
}

// Panels are laid out inside the splitter with usable, finite geometry.
void requireLayout(const Splitter* _splitter, const QWidget* _first, const QWidget* _second,
                   int _firstShare)
{
    const auto width = _splitter->width();
    require(width >= 100 && _splitter->height() >= 100, "splitter has a usable size");
    for (const auto panel : { _first, _second }) {
        const auto geometry = panel->geometry();
        require(geometry.left() >= 0 && geometry.top() == 0 && geometry.width() >= 0
                    && geometry.right() <= width + 1
                    && geometry.height() <= _splitter->height() + 1,
                "panel geometry stays inside the splitter");
    }
    require(std::abs(_first->width() - width * _firstShare / 1000) <= 2,
            "first panel has its share of the width");
    require(_firstShare == 1000 || std::abs(_first->width() + _second->width() - width) <= 2,
            "panels share the whole width");
}

void requireProportions(const Splitter* _splitter, const QVector<int>& _sizes)
{
    require(_splitter->sizes() == _sizes, "splitter keeps the expected proportions");
    const auto state = _splitter->saveState();
    QDataStream stream(state);
    qreal first = 0;
    qreal second = 0;
    stream >> first >> second;
    require(std::isfinite(first) && std::isfinite(second) && first + second > 0,
            "saved proportions are finite and usable");
}

struct Panels {
    QWidget host;
    Splitter* splitter = new Splitter(&host);
    QWidget* first = new QWidget;
    QWidget* second = new QWidget;

    Panels()
    {
        host.resize(1000, 600);
        splitter->setWidgets(first, second);
        splitter->setSizes({ 3, 7 });
        requireProportions(splitter, { 300, 700 });
    }

    void showWithWidth(int _width)
    {
        splitter->setGeometry(0, 0, _width, 600);
        host.show();
        QApplication::processEvents();
    }

    void setWidth(int _width)
    {
        splitter->setGeometry(0, 0, _width, 600);
        QApplication::processEvents();
    }
};

struct Case {
    const char* name;
    std::function<void()> run;
};

// Restore a saved layout through the production window, as startup does.
void restoreWindow(const QByteArray& _geometry, const QByteArray& _proportions, bool _geometryFits,
                   const QVector<int>& _expected)
{
    QWidget probe;
    require(probe.restoreGeometry(_geometry) == _geometryFits,
            "fixture geometry is accepted or refused by Qt as intended");
    Ui::ApplicationView view;
    view.restoreState(true, { { kViewGeometry, _geometry }, { kSplitterState, _proportions } });
    QApplication::processEvents();
    require(view.isVisible(), "restored window is shown");
    const auto available = QApplication::primaryScreen()->availableGeometry();
    if (_geometryFits && available.contains(QRect(165, 29, 472, 744))) {
        require(view.size() == QSize(468, 740), "saved window size is restored");
    } else {
        require(view.width() >= 100 && view.height() >= 100, "window falls back to a usable size");
    }
    const auto splitter = splitterOf(view);
    requireProportions(splitter, _expected);
    const auto panels = splitter->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    QWidget* navigator = nullptr;
    for (const auto panel : panels) {
        if (panel == view.view()) {
            continue;
        }
        // The navigation panel is the only other child with a layout.
        if (panel->layout() != nullptr) {
            navigator = panel;
        }
    }
    require(navigator != nullptr, "application view owns its navigation panel");
    requireLayout(splitter, navigator, view.view(), _expected.constFirst());
    require(view.saveState().value(kSplitterState).toByteArray()
                == proportions(_expected.constFirst() / 1000.0, _expected.constLast() / 1000.0),
            "the layout saved next is the restored one");
    view.hide();
}

const QVector<Case> kCases = {
    // The production startup path.
    { "window-saved-on-wider-screen",
      [] {
          restoreWindow(savedGeometry(screenWidth() * 5 / 2), kSavedProportions, false,
                        { 1000, 0 });
      } },
    { "window-saved-on-narrower-screen",
      [] {
          restoreWindow(savedGeometry(screenWidth() * 2 / 5), kSavedProportions, false,
                        { 1000, 0 });
      } },
    { "window-saved-with-unusable-proportions",
      [] {
          restoreWindow(savedGeometry(screenWidth() * 5 / 2), proportions(0, 0), false,
                        { 300, 700 });
      } },
    { "window-saved-with-non-finite-proportions",
      [] {
          const auto nan = std::numeric_limits<qreal>::quiet_NaN();
          restoreWindow(savedGeometry(screenWidth() * 5 / 2), proportions(nan, nan), false,
                        { 300, 700 });
      } },
    { "window-saved-with-split-panels",
      [] {
          restoreWindow(savedGeometry(screenWidth() * 5 / 2), proportions(0.25, 0.75), false,
                        { 250, 750 });
      } },
    { "window-saved-on-same-screen-control",
      [] { restoreWindow(savedGeometry(screenWidth()), kSavedProportions, true, { 1000, 0 }); } },
    { "first-run-window-control",
      [] {
          Ui::ApplicationView view;
          view.restoreState(false, {});
          view.show();
          QApplication::processEvents();
          require(view.size() == QSize(468, 740), "first-run window size");
          requireProportions(splitterOf(view), { 1000, 0 });
          view.hide();
      } },
    // The splitter itself.
    { "splitter-shown-without-width",
      [] {
          Panels panels;
          panels.showWithWidth(0);
          requireProportions(panels.splitter, { 300, 700 });
          panels.setWidth(1000);
          requireProportions(panels.splitter, { 300, 700 });
          requireLayout(panels.splitter, panels.first, panels.second, 300);
      } },
    { "splitter-zero-total-sizes",
      [] {
          Panels panels;
          panels.showWithWidth(1000);
          const auto first = panels.first->geometry();
          const auto second = panels.second->geometry();
          for (const auto& sizes : QVector<QVector<int>>{ { 0, 0 }, { 300, -300 }, { -5, 2 } }) {
              panels.splitter->setSizes(sizes);
              requireProportions(panels.splitter, { 300, 700 });
              require(panels.first->geometry() == first && panels.second->geometry() == second,
                      "refused sizes leave the panels where they are");
          }
          panels.setWidth(0);
          for (const auto& sizes : QVector<QVector<int>>{ { 0, 0 }, { 300, -300 }, { -5, 2 } }) {
              panels.splitter->setSizes(sizes);
              requireProportions(panels.splitter, { 300, 700 });
          }
          panels.setWidth(1000);
          requireLayout(panels.splitter, panels.first, panels.second, 300);
      } },
    { "splitter-collapsed-panel-shown-without-width",
      [] {
          Panels panels;
          panels.showWithWidth(1000);
          panels.first->hide();
          QApplication::processEvents();
          requireProportions(panels.splitter, { 0, 1000 });
          panels.setWidth(0);
          panels.first->show();
          QApplication::processEvents();
          requireProportions(panels.splitter, { 0, 1000 });
          panels.setWidth(1000);
          requireProportions(panels.splitter, { 0, 1000 });
          requireLayout(panels.splitter, panels.first, panels.second, 0);
      } },
    { "splitter-restore-state-validation",
      [] {
          Panels panels;
          panels.showWithWidth(1000);
          const auto nan = std::numeric_limits<qreal>::quiet_NaN();
          for (const auto& state :
               { proportions(0, 0), proportions(nan, nan), proportions(-1, -5), proportions(nan, 0),
                 QByteArray(), proportions(0.5, 0.5).left(8),
                 proportions(0.5, 0.5) + proportions(0.5, 0.5).left(8) }) {
              panels.splitter->restoreState(state);
              requireProportions(panels.splitter, { 300, 700 });
              requireLayout(panels.splitter, panels.first, panels.second, 300);
          }
          panels.splitter->restoreState(proportions(0.25, 0.75));
          requireProportions(panels.splitter, { 250, 750 });
          requireLayout(panels.splitter, panels.first, panels.second, 250);
          panels.splitter->restoreState(kSavedProportions);
          requireProportions(panels.splitter, { 1000, 0 });
          requireLayout(panels.splitter, panels.first, panels.second, 1000);
      } },
    { "splitter-normal-resize-control",
      [] {
          Panels panels;
          panels.showWithWidth(1000);
          requireLayout(panels.splitter, panels.first, panels.second, 300);
          panels.setWidth(600);
          requireProportions(panels.splitter, { 300, 700 });
          requireLayout(panels.splitter, panels.first, panels.second, 300);
          // A panel narrower than 24 pixels is still hidden automatically.
          panels.splitter->setSizes({ 10, 590 });
          requireProportions(panels.splitter, { 0, 1000 });
          requireLayout(panels.splitter, panels.first, panels.second, 0);
      } },
};
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Window restore regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    require(argc == 2, "case name or --list supplied");
    const auto requested = QString::fromLocal8Bit(argv[1]);
    if (requested == "--list") {
        for (const auto& test : kCases) {
            std::puts(test.name);
        }
        return 0;
    }
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" }) {
        require(QFontDatabase::addApplicationFont(font) >= 0, "load embedded fonts");
    }
    for (const auto& test : kCases) {
        if (requested == test.name) {
            std::printf("case: %s, screen width %d\n", test.name, screenWidth());
            test.run();
            std::printf("ok: %s\n", test.name);
            return 0;
        }
    }
    require(false, "known case name");
    return 1;
}
