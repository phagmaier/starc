#pragma once

// Exercise the surviving explicit Export dialog, including format/options dispatch
// and its destination chooser. No source-save convenience API is used.
#include <management_layer/content/export/export_manager.h>
#include <ui/export/screenplay_export_dialog.h>
#include <ui/widgets/check_box/check_box.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/dialog/dialog.h>
#include <ui/widgets/label/label.h>

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>

inline bool explicitScreenplayExport(ManagementLayer::ExportManager& _manager,
                                     BusinessLayer::AbstractModel* _model,
                                     const QString& _destination, const QString& _format,
                                     bool _titlePage = true)
{
    _manager.exportDocument({ { QString(), _model } }, 0);
    Ui::ScreenplayExportDialog* options = nullptr;
    QElapsedTimer deadline;
    deadline.start();
    while (!options && deadline.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        for (auto candidate : QApplication::allWidgets()) {
            if (auto dialog = qobject_cast<Ui::ScreenplayExportDialog*>(candidate);
                dialog && dialog->isVisible()) {
                options = dialog;
            }
        }
    }
    if (!options) {
        return false;
    }
    bool controlsVisible = false;
    deadline.restart();
    while (!controlsVisible && deadline.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        for (auto box : options->findChildren<CheckBox*>()) {
            if (box->text() == "Text" && box->isVisibleTo(options)) {
                controlsVisible = true;
            }
        }
    }
    if (!controlsVisible) {
        return false;
    }
    bool formatSelected = false;
    for (auto combo : options->findChildren<ComboBox*>()) {
        for (int row = 0; row < combo->model()->rowCount(); ++row) {
            const auto index = combo->model()->index(row, 0);
            if (index.data().toString() == _format) {
                combo->setCurrentIndex(index);
                formatSelected = true;
                break;
            }
        }
    }
    if (!formatSelected) {
        return false;
    }
    for (auto box : options->findChildren<CheckBox*>()) {
        if (box->text() == "Open document after export" || box->text() == "Synopsis"
            || box->text() == "Treatment") {
            box->setChecked(false);
        } else if (box->text() == "Title page") {
            box->setChecked(_titlePage);
        } else if (box->text() == "Text" || box->text() == "Include inline notes"
                   || box->text() == "Include review marks / revisions"
                   || box->text() == "Include sequences headers and footers") {
            box->setChecked(true);
        }
    }
    bool chosen = false;
    QTimer chooser;
    chooser.setInterval(10);
    QObject::connect(&chooser, &QTimer::timeout, [&] {
        for (auto candidate : QApplication::allWidgets()) {
            if (auto confirmation = qobject_cast<QMessageBox*>(candidate);
                confirmation && confirmation->isVisible()) {
                if (auto yes = confirmation->button(QMessageBox::Yes)) {
                    yes->click();
                }
                return;
            }
            if (auto dialog = qobject_cast<QFileDialog*>(candidate);
                dialog && dialog->isVisible() && !chosen) {
                chosen = true;
                if (dialog->focusWidget()) {
                    dialog->focusWidget()->clearFocus();
                }
                dialog->selectFile(_destination);
                QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
                return;
            }
        }
    });
    chooser.start();
    options->exportRequested();
    chooser.stop();
    bool failed = false;
    QPointer<Ui::ScreenplayExportDialog> observed = options;
    QPointer<Dialog> error;
    deadline.restart();
    // Success hides the options dialog; failure queues a separate error dialog.
    while (observed && !error && deadline.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        for (auto candidate : QApplication::allWidgets()) {
            if (auto dialog = qobject_cast<Dialog*>(candidate); dialog && dialog->isVisible()) {
                for (auto label : dialog->findChildren<AbstractLabel*>()) {
                    if (label->text().contains("The document could not be written")) {
                        error = dialog;
                        break;
                    }
                }
            }
        }
    }
    if (error) {
        failed = true;
        error->finished({ 0, "OK", Dialog::RejectButton });
        if (observed) {
            observed->canceled();
        }
    }
    deadline.restart();
    while ((observed || error) && deadline.elapsed() < 15000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    return chosen && !failed && !observed && !error;
}
