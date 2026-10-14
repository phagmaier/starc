TARGET = corelib
TEMPLATE = lib

CONFIG += c++1z
CONFIG += force_debug_info
CONFIG += separate_debug_info

# Keep the runtime module name identical to the symbol-file module name on
# every platform. Unlike the plugins, a regular qmake library is versioned by
# default (libcorelib.so.1 / libcorelib.1.dylib and, depending on the mkspec,
# corelib1.dll), while its debug artifact is based on the unversioned target.
# Matching names allow debuggers to resolve frames belonging to corelib.
win32 {
    CONFIG += skip_target_version_ext
} else {
    CONFIG += unversioned_libname
    CONFIG += unversioned_soname
}

QT += widgets widgets-private sql xml network concurrent

# Qt 6 native credential storage; QtKeychain's plaintext fallback is disabled in the adapter.
# Optional prefix supports a locally installed library without changing system packages.
isEmpty(QTKEYCHAIN_PREFIX): QTKEYCHAIN_PREFIX = $$(QTKEYCHAIN_PREFIX)
!isEmpty(QTKEYCHAIN_PREFIX) {
    INCLUDEPATH += $$QTKEYCHAIN_PREFIX/include
    LIBS += -L$$QTKEYCHAIN_PREFIX/lib
    unix:QMAKE_RPATHDIR += $$QTKEYCHAIN_PREFIX/lib
}
LIBS += -lqt6keychain
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}

DEFINES += CORE_LIBRARY
DEFINES += QT_DEPRECATED_WARNINGS

mac {
    DESTDIR = ../_build/starcapp.app/Contents/Frameworks

    # Qt's resolve_target feature ignores unversioned_libname on macOS, while
    # the makefile generator honours it. Correct the resolved path before
    # separate_debug_info uses it to build the dsymutil and strip commands.
    load(resolve_target)
    QMAKE_RESOLVED_TARGET = $$absolute_path($$DESTDIR, $$OUT_PWD)/lib$${TARGET}.$${QMAKE_EXTENSION_SHLIB}
} else {
    DESTDIR = ../_build
}
LIBSDIR = ../_build/libs

INCLUDEPATH += ..

#
# Запишем commit hash и дату сборки либы, для отладки
#
GIT_COMIMT_HASH = $$system(git rev-parse HEAD)
DEFINES += CORE_LIBRARY_GIT_COMMIT_HASH=\\\"$$GIT_COMIMT_HASH\\\"
win32 {
    BUILD_TIMESTAMP = $$system(powershell -Command "Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'")
} else {
    BUILD_TIMESTAMP = $$system(date +%Y-%m-%d_%H:%M:%S)
}
!isEmpty(BUILD_TIMESTAMP) {
    DEFINES += CORE_LIBRARY_BUILD_TIMESTAMP=\\\"$$BUILD_TIMESTAMP\\\"
} else {
    DEFINES += CORE_LIBRARY_BUILD_TIMESTAMP=\\\"unknown\\\"
}

#
# Настраиваем QCustomPlot
#
DEFINES += QCP_LIB_DECL QCP_NO_PRINTER

#
# Подключаем библиотеку fileformats
#
LIBS += -L$$LIBSDIR/ -lfileformats
win32-msvc*:QMAKE_LFLAGS += /WHOLEARCHIVE:$$LIBSDIR/fileformats.lib

INCLUDEPATH += $$PWD/../3rd_party/fileformats
DEPENDPATH += $$PWD/../3rd_party/fileformats
PRE_TARGETDEPS += $$PWD/../3rd_party/fileformats
#

#
# Подключаем библиотеку Zlib
#
mac:LIBS += -lz
#

#
# Подключаем библиотеку HUNSPELL
#
LIBS += -L$$LIBSDIR/ -lhunspell
win32-msvc*:QMAKE_LFLAGS += /WHOLEARCHIVE:$$LIBSDIR/hunspell.lib

INCLUDEPATH += $$PWD/../3rd_party/hunspell/src
DEPENDPATH += $$PWD/../3rd_party/hunspell
PRE_TARGETDEPS += $$PWD/../3rd_party/hunspell
#

#
# Подключаем библиотеку qgumboparser
#
LIBS += -L$$LIBSDIR/ -lqgumboparser
win32-msvc*:QMAKE_LFLAGS += /WHOLEARCHIVE:$$LIBSDIR/qgumboparser.lib

INCLUDEPATH += $$PWD/../3rd_party/qgumboparser
DEPENDPATH += $$PWD/../3rd_party/qgumboparser
PRE_TARGETDEPS += $$PWD/../3rd_party/qgumboparser
#

#
# Подключаем библиотеку Webloader
#
LIBS += -L$$LIBSDIR/ -lwebloader
win32-msvc*:QMAKE_LFLAGS += /WHOLEARCHIVE:$$LIBSDIR/webloader.lib

INCLUDEPATH += $$PWD/../3rd_party/webloader/src
DEPENDPATH += $$PWD/../3rd_party/webloader
PRE_TARGETDEPS += $$PWD/../3rd_party/webloader
#

#
# Подключаем библиотеку QtXlsxWriter
#
LIBS += -L$$LIBSDIR/ -lqtxlsxwriter
win32-msvc*:QMAKE_LFLAGS += /WHOLEARCHIVE:$$LIBSDIR/qtxlsxwriter.lib

INCLUDEPATH += $$PWD/../3rd_party/qtxlsxwriter
DEPENDPATH += $$PWD/../3rd_party/qtxlsxwriter
PRE_TARGETDEPS += $$PWD/../3rd_party/qtxlsxwriter
#

#
# Подключаем библиотеку User32 для доступа к фичам настройки темы в Windows
#
win32-msvc*:LIBS += -lUser32
#

#
# Подключаем библиотеку AVOID
#
LIBS += -L$$LIBSDIR/ -lavoid
INCLUDEPATH += $$PWD/../3rd_party/libavoid/
DEPENDPATH += $$PWD/../3rd_party/libavoid
#

#
# Подключаем библиотеку TextExtraction
#
LIBS += -L$$LIBSDIR/ -lTextExtraction
INCLUDEPATH += $$PWD/../3rd_party/pdftextextraction/TextExtraction
DEPENDPATH += $$PWD/../3rd_party/pdftextextraction/TextExtraction
#

#
# Подключаем библиотеку PDFWriter
#
LIBS += -L$$LIBSDIR/ -lPDFWriter
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/PDFWriter
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/PDFWriter
#

#
# Подключаем библиотеку LibAesgm
#
LIBS += -L$$LIBSDIR/ -lLibAesgm
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/LibAesgm
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/LibAesgm
#

#
# Подключаем библиотеку FreeType
#
LIBS += -L$$LIBSDIR/ -lFreeType
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/FreeType/include
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/FreeType/include
#

#
# Подключаем библиотеку LibJpeg
#
LIBS += -L$$LIBSDIR/ -lLibJpeg
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/LibJpeg
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/LibJpeg
# Keep the PDF reader's bundled JPEG ABI private. Otherwise Qt's system JPEG
# plugin binds to these unversioned symbols and mixes incompatible codec state.
linux:QMAKE_LFLAGS += -Wl,--exclude-libs,libLibJpeg.a
#

#
# Подключаем библиотеку LibPng
#
LIBS += -L$$LIBSDIR/ -lLibPng
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/LibPng
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/LibPng
#

#
# Подключаем библиотеку LibTiff
#
LIBS += -L$$LIBSDIR/ -lLibTiff
INCLUDEPATH += $$PWD/../3rd_party/pdfhummus/LibTiff
DEPENDPATH += $$PWD/../3rd_party/pdfhummus/LibTiff
#

SOURCES += \
    data_layer/storage/github_credential_store.cpp \
    business_layer/chronometry/chronometer.cpp \
    business_layer/compliance/compliance_checker.cpp \
    business_layer/compliance/compliance_checker_impl.cpp \
    business_layer/document/screenplay/text/screenplay_text_corrector.cpp \
    business_layer/document/screenplay/text/screenplay_text_document.cpp \
    business_layer/document/simple_text/simple_text_corrector.cpp \
    business_layer/document/simple_text/simple_text_document.cpp \
    business_layer/document/text/abstract_text_corrector.cpp \
    business_layer/document/text/text_block_data.cpp \
    business_layer/document/text/text_cursor.cpp \
    business_layer/document/text/text_document.cpp \
    business_layer/export/abstract_docx_exporter.cpp \
    business_layer/export/abstract_exporter.cpp \
    business_layer/export/abstract_markdown_exporter.cpp \
    business_layer/export/abstract_pdf_exporter.cpp \
    business_layer/export/characters/character_docx_exporter.cpp \
    business_layer/export/characters/character_exporter.cpp \
    business_layer/export/characters/character_pdf_exporter.cpp \
    business_layer/export/characters/characters_docx_exporter.cpp \
    business_layer/export/characters/characters_exporter.cpp \
    business_layer/export/characters/characters_pdf_exporter.cpp \
    business_layer/export/export_options.cpp \
    business_layer/export/locations/location_docx_exporter.cpp \
    business_layer/export/locations/location_exporter.cpp \
    business_layer/export/locations/location_pdf_exporter.cpp \
    business_layer/export/locations/locations_docx_exporter.cpp \
    business_layer/export/locations/locations_exporter.cpp \
    business_layer/export/locations/locations_pdf_exporter.cpp \
    business_layer/export/screenplay/screenplay_docx_exporter.cpp \
    business_layer/export/screenplay/screenplay_exporter.cpp \
    business_layer/export/screenplay/screenplay_fdx_exporter.cpp \
    business_layer/export/screenplay/screenplay_fountain_exporter.cpp \
    business_layer/export/screenplay/screenplay_pdf_exporter.cpp \
    business_layer/export/simple_text/simple_text_docx_exporter.cpp \
    business_layer/export/simple_text/simple_text_exporter.cpp \
    business_layer/export/simple_text/simple_text_markdown_exporter.cpp \
    business_layer/export/simple_text/simple_text_pdf_exporter.cpp \
    business_layer/import/abstract_document_importer.cpp \
    business_layer/import/abstract_fountain_importer.cpp \
    business_layer/import/abstract_markdown_importer.cpp \
    business_layer/import/screenplay/screenplay_celtx_importer.cpp \
    business_layer/import/screenplay/screenplay_docx_importer.cpp \
    business_layer/import/screenplay/screenplay_fdx_importer.cpp \
    business_layer/import/screenplay/screenplay_fountain_importer.cpp \
    business_layer/import/screenplay/screenplay_kit_scenarist_importer.cpp \
    business_layer/import/screenplay/screenplay_pdf_importer.cpp \
    business_layer/import/screenplay/screenplay_trelby_importer.cpp \
    business_layer/import/text/simple_text_docx_importer.cpp \
    business_layer/import/text/simple_text_markdown_importer.cpp \
    business_layer/import/text/simple_text_pdf_importer.cpp \
    business_layer/model/abstract_model.cpp \
    business_layer/model/abstract_model_item.cpp \
    business_layer/model/abstract_model_xml.cpp \
    business_layer/model/characters/character_model.cpp \
    business_layer/model/characters/characters_model.cpp \
    business_layer/model/images/images_gallery_model.cpp \
    business_layer/model/locations/location_model.cpp \
    business_layer/model/locations/locations_model.cpp \
    business_layer/model/mind_map/mind_map_model.cpp \
    business_layer/model/presentation/presentation_model.cpp \
    business_layer/model/project/project_information_model.cpp \
    business_layer/model/recycle_bin/recycle_bin_model.cpp \
    business_layer/model/screenplay/screenplay_dictionaries_model.cpp \
    business_layer/model/screenplay/screenplay_information_model.cpp \
    business_layer/model/screenplay/screenplay_statistics_model.cpp \
    business_layer/model/screenplay/screenplay_synopsis_model.cpp \
    business_layer/model/screenplay/text/screenplay_text_block_parser.cpp \
    business_layer/model/screenplay/text/screenplay_text_mime_handler.cpp \
    business_layer/model/screenplay/text/screenplay_text_model.cpp \
    business_layer/model/screenplay/text/screenplay_text_model_beat_item.cpp \
    business_layer/model/screenplay/text/screenplay_text_model_folder_item.cpp \
    business_layer/model/screenplay/text/screenplay_text_model_scene_item.cpp \
    business_layer/model/screenplay/text/screenplay_text_model_text_item.cpp \
    business_layer/model/screenplay/screenplay_title_page_model.cpp \
    business_layer/model/simple_text/simple_text_model.cpp \
    business_layer/model/simple_text/simple_text_model_chapter_item.cpp \
    business_layer/model/simple_text/simple_text_model_folder_item.cpp \
    business_layer/model/structure/structure_model.cpp \
    business_layer/model/structure/structure_model_item.cpp \
    business_layer/model/structure/structure_proxy_model.cpp \
    business_layer/model/text/text_model.cpp \
    business_layer/model/text/text_model_folder_item.cpp \
    business_layer/model/text/text_model_group_item.cpp \
    business_layer/model/text/text_model_item.cpp \
    business_layer/model/text/text_model_splitter_item.cpp \
    business_layer/model/text/text_model_text_item.cpp \
    business_layer/model/text/text_model_xml_writer.cpp \
    business_layer/model/base/script_text_model.cpp \
    business_layer/model/base/title_page_model.cpp \
    business_layer/plots/screenplay/screenplay_characters_activity_plot.cpp \
    business_layer/plots/screenplay/screenplay_structure_analysis_plot.cpp \
    business_layer/reports/abstract_report.cpp \
    business_layer/reports/screenplay/screenplay_cast_report.cpp \
    business_layer/reports/screenplay/screenplay_dialogues_report.cpp \
    business_layer/reports/screenplay/screenplay_gender_report.cpp \
    business_layer/reports/screenplay/screenplay_location_report.cpp \
    business_layer/reports/screenplay/screenplay_scene_report.cpp \
    business_layer/reports/screenplay/screenplay_summary_report.cpp \
    business_layer/templates/screenplay_template.cpp \
    business_layer/templates/simple_text_template.cpp \
    business_layer/templates/templates_facade.cpp \
    business_layer/templates/text_template.cpp \
    data_layer/database.cpp \
    data_layer/mapper/abstract_mapper.cpp \
    data_layer/mapper/document_change_mapper.cpp \
    data_layer/mapper/document_mapper.cpp \
    data_layer/mapper/mapper_facade.cpp \
    data_layer/mapper/settings_mapper.cpp \
    data_layer/storage/document_change_storage.cpp \
    data_layer/storage/document_image_storage.cpp \
    data_layer/storage/document_raw_data_storage.cpp \
    data_layer/storage/document_storage.cpp \
    data_layer/storage/settings_storage.cpp \
    data_layer/storage/storage_facade.cpp \
    domain/document_change_object.cpp \
    domain/document_object.cpp \
    domain/domain_object.cpp \
    domain/identifier.cpp \
    domain/objects_builder.cpp \
    ui/design_system/design_system.cpp \
    ui/layouts/flow_layout/flow_layout.cpp \
    ui/modules/avatar_generator/avatar_generator.cpp \
    ui/modules/bookmarks/add_bookmark_view.cpp \
    ui/modules/bookmarks/bookmark_delegate.cpp \
    ui/modules/bookmarks/bookmark_dialog.cpp \
    ui/modules/bookmarks/bookmarks_model.cpp \
    ui/modules/bookmarks/bookmarks_view.cpp \
    ui/modules/cards/abstract_card_item.cpp \
    ui/modules/cards/cards_graphics_scene.cpp \
    ui/modules/cards/cards_graphics_view.cpp \
    ui/modules/comments/add_comment_view.cpp \
    ui/modules/comments/comment_delegate.cpp \
    ui/modules/comments/comment_replies_view.cpp \
    ui/modules/comments/comment_view.cpp \
    ui/modules/comments/comments_model.cpp \
    ui/modules/comments/comments_toolbar.cpp \
    ui/modules/comments/comments_view.cpp \
    ui/modules/counters_info/counters_info_widget.cpp \
    ui/modules/fast_format_widget/fast_format_widget.cpp \
    ui/modules/images_list/images_list.cpp \
    ui/modules/images_list/images_list_preview.cpp \
    ui/modules/logline_generator/logline_generator_dialog.cpp \
    ui/modules/cards/card_item_parameters_view.cpp \
    ui/modules/promo_widget/module_promo_toolbar.cpp \
    ui/modules/promo_widget/module_promo_widget.cpp \
    ui/modules/script_text_edit/script_text_edit.cpp \
    ui/modules/search_toolbar/search_manager.cpp \
    ui/modules/search_toolbar/search_toolbar.cpp \
    ui/modules/text_scrollbar_manager/screenplay_text_scrollbar_manager.cpp \
    ui/widgets/animations/click_animation.cpp \
    ui/widgets/app_bar/app_bar.cpp \
    ui/widgets/button/button.cpp \
    ui/widgets/card/card.cpp \
    ui/widgets/card/card_popup.cpp \
    ui/widgets/card/card_popup_with_tree.cpp \
    ui/widgets/chat/chat_message.cpp \
    ui/widgets/chat/chat_messages_view.cpp \
    ui/widgets/chat/user.cpp \
    ui/widgets/check_box/check_box.cpp \
    ui/widgets/circular_progress_bar/circular_progress_bar.cpp \
    ui/widgets/color_picker/color_2d_slider.cpp \
    ui/widgets/color_picker/color_hue_slider.cpp \
    ui/widgets/color_picker/color_palette.cpp \
    ui/widgets/color_picker/color_picker.cpp \
    ui/widgets/color_picker/color_picker_popup.cpp \
    ui/widgets/combo_box/combo_box.cpp \
    ui/widgets/context_menu/context_menu.cpp \
    ui/widgets/date_picker/date_picker.cpp \
    ui/widgets/date_picker/date_picker_popup.cpp \
    ui/widgets/date_picker/month_days_widget.cpp \
    ui/widgets/dialog/abstract_dialog.cpp \
    ui/widgets/dialog/dialog.cpp \
    ui/widgets/dialog/standard_dialog.cpp \
    ui/widgets/drawer/drawer.cpp \
    ui/widgets/floating_tool_bar/floating_tool_bar.cpp \
    ui/widgets/floating_tool_bar/floating_toolbar_animator.cpp \
    ui/widgets/icon_button/icon_button.cpp \
    ui/widgets/image/image_card.cpp \
    ui/widgets/image/image_cropping_dialog.cpp \
    ui/widgets/image_cropper/image_cropper.cpp \
    ui/widgets/key_sequence_edit/key_sequence_edit.cpp \
    ui/widgets/label/label.cpp \
    ui/widgets/label/link_label.cpp \
    ui/widgets/pie/pie.cpp \
    ui/widgets/plot/plot.cpp \
    ui/widgets/plot/qcustomplot.cpp \
    ui/widgets/progress_bar/progress_bar.cpp \
    ui/widgets/radio_button/radio_button.cpp \
    ui/widgets/radio_button/radio_button_group.cpp \
    ui/widgets/resizable_widget/resizable_widget.cpp \
    ui/widgets/scalable_graphics_view/scalable_graphics_view.cpp \
    ui/widgets/scroll_bar/scroll_bar.cpp \
    ui/widgets/shadow/shadow.cpp \
    ui/widgets/slider/slider.cpp \
    ui/widgets/splitter/splitter.cpp \
    ui/widgets/stack_widget/stack_widget.cpp \
    ui/widgets/stepper/stepper.cpp \
    ui/widgets/toggle/toggle.cpp \
    ui/widgets/tab_bar/tab_bar.cpp \
    ui/widgets/task_bar/task_bar.cpp \
    ui/widgets/text_edit/base/base_text_edit.cpp \
    ui/widgets/text_edit/completer/completer.cpp \
    ui/widgets/text_edit/completer/completer_text_edit.cpp \
    ui/widgets/text_edit/page/page_metrics.cpp \
    ui/widgets/text_edit/page/page_text_edit.cpp \
    ui/widgets/text_edit/scalable_wrapper/scalable_wrapper.cpp \
    ui/widgets/text_edit/spell_check/spell_check_highlighter.cpp \
    ui/widgets/text_edit/spell_check/spell_check_text_edit.cpp \
    ui/widgets/text_edit/spell_check/spell_checker.cpp \
    ui/widgets/text_field/text_field.cpp \
    ui/widgets/tree/tree.cpp \
    ui/widgets/tree/tree_delegate.cpp \
    ui/widgets/tree/tree_header_view.cpp \
    ui/widgets/tree/tree_view.cpp \
    ui/widgets/widget/widget.cpp \
    utils/3rd_party/WAF/Animation/Animation.cpp \
    utils/3rd_party/WAF/Animation/CircleFill/CircleFillAnimator.cpp \
    utils/3rd_party/WAF/Animation/CircleFill/CircleFillDecorator.cpp \
    utils/3rd_party/WAF/Animation/CircleTransparent/CircleTransparentAnimator.cpp \
    utils/3rd_party/WAF/Animation/CircleTransparent/CircleTransparentDecorator.cpp \
    utils/3rd_party/WAF/Animation/Expand/ExpandAnimator.cpp \
    utils/3rd_party/WAF/Animation/Expand/ExpandDecorator.cpp \
    utils/3rd_party/WAF/Animation/SideSlide/SideSlideAnimator.cpp \
    utils/3rd_party/WAF/Animation/SideSlide/SideSlideDecorator.cpp \
    utils/3rd_party/WAF/Animation/Slide/SlideAnimator.cpp \
    utils/3rd_party/WAF/Animation/Slide/SlideForegroundDecorator.cpp \
    utils/diff_match_patch/diff_match_patch.cpp \
    utils/diff_match_patch/diff_match_patch_controller.cpp \
    utils/helpers/color_helper.cpp \
    utils/helpers/dialog_helper.cpp \
    utils/helpers/eighths_helper.cpp \
    utils/helpers/extension_helper.cpp \
    utils/helpers/hunspell_helper.cpp \
    utils/helpers/icon_helper.cpp \
    utils/helpers/image_helper.cpp \
    utils/helpers/libavoid_helper.cpp \
    utils/helpers/measurement_helper.cpp \
    utils/helpers/model_helper.cpp \
    utils/helpers/names_generator.cpp \
    utils/helpers/platform_helper.cpp \
    utils/helpers/price_helper.cpp \
    utils/helpers/quotes_generator.cpp \
    utils/helpers/scroller_helper.cpp \
    utils/helpers/shortcuts_helper.cpp \
    utils/helpers/string_helper.cpp \
    utils/helpers/text_helper.cpp \
    utils/helpers/time_helper.cpp \
    utils/helpers/ui_helper.cpp \
    utils/logging.cpp \
    utils/tools/alphanum_comparer.cpp \
    utils/tools/backup_builder.cpp \
    utils/tools/debouncer.cpp \
    utils/tools/github_files_client.cpp \
    utils/tools/model_index_path.cpp \
    utils/tools/performance_monitor.cpp \
    utils/tools/project_sync.cpp \
    utils/tools/run_once.cpp \
    utils/validators/email_validator.cpp

HEADERS += \
    data_layer/storage/github_credential_store.h \
    business_layer/chronometry/chronometer.h \
    business_layer/compliance/compliance_checker.h \
    business_layer/compliance/compliance_checker_impl.h \
    business_layer/document/screenplay/text/screenplay_text_corrector.h \
    business_layer/document/screenplay/text/screenplay_text_document.h \
    business_layer/document/simple_text/simple_text_corrector.h \
    business_layer/document/simple_text/simple_text_document.h \
    business_layer/document/text/abstract_text_corrector.h \
    business_layer/document/text/text_block_data.h \
    business_layer/document/text/text_cursor.h \
    business_layer/document/text/text_document.h \
    business_layer/export/abstract_docx_exporter.h \
    business_layer/export/abstract_exporter.h \
    business_layer/export/abstract_markdown_exporter.h \
    business_layer/export/abstract_pdf_exporter.h \
    business_layer/export/characters/character_docx_exporter.h \
    business_layer/export/characters/character_export_options.h \
    business_layer/export/characters/character_exporter.h \
    business_layer/export/characters/character_pdf_exporter.h \
    business_layer/export/characters/characters_docx_exporter.h \
    business_layer/export/characters/characters_export_options.h \
    business_layer/export/characters/characters_exporter.h \
    business_layer/export/characters/characters_pdf_exporter.h \
    business_layer/export/export_options.h \
    business_layer/export/locations/location_docx_exporter.h \
    business_layer/export/locations/location_export_options.h \
    business_layer/export/locations/location_exporter.h \
    business_layer/export/locations/location_pdf_exporter.h \
    business_layer/export/locations/locations_docx_exporter.h \
    business_layer/export/locations/locations_exporter.h \
    business_layer/export/locations/locations_pdf_exporter.h \
    business_layer/export/screenplay/screenplay_docx_exporter.h \
    business_layer/export/screenplay/screenplay_export_options.h \
    business_layer/export/screenplay/screenplay_exporter.h \
    business_layer/export/screenplay/screenplay_fdx_exporter.h \
    business_layer/export/screenplay/screenplay_fountain_exporter.h \
    business_layer/export/screenplay/screenplay_pdf_exporter.h \
    business_layer/export/simple_text/simple_text_docx_exporter.h \
    business_layer/export/simple_text/simple_text_exporter.h \
    business_layer/export/simple_text/simple_text_markdown_exporter.h \
    business_layer/export/simple_text/simple_text_pdf_exporter.h \
    business_layer/import/abstract_document_importer.h \
    business_layer/import/abstract_fountain_importer.h \
    business_layer/import/abstract_importer.h \
    business_layer/import/abstract_markdown_importer.h \
    business_layer/import/import_options.h \
    business_layer/import/screenplay/abstract_screenplay_importer.h \
    business_layer/import/screenplay/screenplay_celtx_importer.h \
    business_layer/import/screenplay/screenplay_docx_importer.h \
    business_layer/import/screenplay/screenplay_fdx_importer.h \
    business_layer/import/screenplay/screenplay_fountain_importer.h \
    business_layer/import/screenplay/screenplay_kit_scenarist_importer.h \
    business_layer/import/screenplay/screenplay_pdf_importer.h \
    business_layer/import/screenplay/screenplay_trelby_importer.h \
    business_layer/import/text/abstract_simple_text_importer.h \
    business_layer/import/text/simple_text_docx_importer.h \
    business_layer/import/text/simple_text_markdown_importer.h \
    business_layer/import/text/simple_text_pdf_importer.h \
    business_layer/model/abstract_image_wrapper.h \
    business_layer/model/abstract_model.h \
    business_layer/model/abstract_model_item.h \
    business_layer/model/abstract_model_xml.h \
    business_layer/model/abstract_raw_data_wrapper.h \
    business_layer/model/characters/character_model.h \
    business_layer/model/characters/characters_model.h \
    business_layer/model/images/images_gallery_model.h \
    business_layer/model/locations/location_model.h \
    business_layer/model/locations/locations_model.h \
    business_layer/model/mind_map/mind_map_model.h \
    business_layer/model/presentation/presentation_model.h \
    business_layer/model/project/project_information_model.h \
    business_layer/model/recycle_bin/recycle_bin_model.h \
    business_layer/model/screenplay/screenplay_dictionaries_model.h \
    business_layer/model/screenplay/screenplay_information_model.h \
    business_layer/model/screenplay/screenplay_statistics_model.h \
    business_layer/model/screenplay/screenplay_synopsis_model.h \
    business_layer/model/screenplay/text/screenplay_text_block_parser.h \
    business_layer/model/screenplay/text/screenplay_text_mime_handler.h \
    business_layer/model/screenplay/text/screenplay_text_model.h \
    business_layer/model/screenplay/text/screenplay_text_model_beat_item.h \
    business_layer/model/screenplay/text/screenplay_text_model_folder_item.h \
    business_layer/model/screenplay/text/screenplay_text_model_scene_item.h \
    business_layer/model/screenplay/text/screenplay_text_model_text_item.h \
    business_layer/model/screenplay/screenplay_title_page_model.h \
    business_layer/model/simple_text/simple_text_model.h \
    business_layer/model/simple_text/simple_text_model_chapter_item.h \
    business_layer/model/simple_text/simple_text_model_folder_item.h \
    business_layer/model/structure/structure_model.h \
    business_layer/model/structure/structure_model_item.h \
    business_layer/model/structure/structure_proxy_model.h \
    business_layer/model/text/text_model.h \
    business_layer/model/text/text_model_folder_item.h \
    business_layer/model/text/text_model_group_item.h \
    business_layer/model/text/text_model_item.h \
    business_layer/model/text/text_model_splitter_item.h \
    business_layer/model/text/text_model_text_item.h \
    business_layer/model/text/text_model_xml.h \
    business_layer/model/text/text_model_xml_writer.h \
    business_layer/model/base/script_text_model.h \
    business_layer/model/base/title_page_model.h \
    business_layer/plots/abstract_plot.h \
    business_layer/plots/screenplay/screenplay_characters_activity_plot.h \
    business_layer/plots/screenplay/screenplay_structure_analysis_plot.h \
    business_layer/reports/abstract_report.h \
    business_layer/reports/screenplay/screenplay_cast_report.h \
    business_layer/reports/screenplay/screenplay_dialogues_report.h \
    business_layer/reports/screenplay/screenplay_gender_report.h \
    business_layer/reports/screenplay/screenplay_location_report.h \
    business_layer/reports/screenplay/screenplay_scene_report.h \
    business_layer/reports/screenplay/screenplay_summary_report.h \
    business_layer/templates/screenplay_template.h \
    business_layer/templates/simple_text_template.h \
    business_layer/templates/templates_facade.h \
    business_layer/templates/text_template.h \
    corelib_global.h \
    data_layer/database.h \
    data_layer/mapper/abstract_mapper.h \
    data_layer/mapper/document_change_mapper.h \
    data_layer/mapper/document_mapper.h \
    data_layer/mapper/mapper_facade.h \
    data_layer/mapper/settings_mapper.h \
    data_layer/storage/document_change_storage.h \
    data_layer/storage/document_image_storage.h \
    data_layer/storage/document_raw_data_storage.h \
    data_layer/storage/document_storage.h \
    data_layer/storage/settings_storage.h \
    data_layer/storage/storage_facade.h \
    domain/document_change_object.h \
    domain/document_object.h \
    domain/domain_object.h \
    domain/identifier.h \
    domain/objects_builder.h \
    domain/starcloud_api.h \
    ui/abstract_navigator.h \
    ui/design_system/design_system.h \
    ui/layouts/flow_layout/flow_layout.h \
    ui/modules/avatar_generator/avatar_generator.h \
    ui/modules/bookmarks/add_bookmark_view.h \
    ui/modules/bookmarks/bookmark_delegate.h \
    ui/modules/bookmarks/bookmark_dialog.h \
    ui/modules/bookmarks/bookmarks_model.h \
    ui/modules/bookmarks/bookmarks_view.h \
    ui/modules/cards/abstract_card_item.h \
    ui/modules/cards/cards_graphics_scene.h \
    ui/modules/cards/cards_graphics_view.h \
    ui/modules/comments/add_comment_view.h \
    ui/modules/comments/comment_delegate.h \
    ui/modules/comments/comment_replies_view.h \
    ui/modules/comments/comment_view.h \
    ui/modules/comments/comments_model.h \
    ui/modules/comments/comments_toolbar.h \
    ui/modules/comments/comments_view.h \
    ui/modules/counters_info/counters_info_widget.h \
    ui/modules/fast_format_widget/fast_format_widget.h \
    ui/modules/images_list/images_list.h \
    ui/modules/images_list/images_list_preview.h \
    ui/modules/logline_generator/logline_generator_dialog.h \
    ui/modules/cards/card_item_parameters_view.h \
    ui/modules/promo_widget/module_promo_toolbar.h \
    ui/modules/promo_widget/module_promo_widget.h \
    ui/modules/script_text_edit/script_text_edit.h \
    ui/modules/search_toolbar/search_manager.h \
    ui/modules/search_toolbar/search_toolbar.h \
    ui/modules/text_scrollbar_manager/screenplay_text_scrollbar_manager.h \
    ui/widgets/animations/click_animation.h \
    ui/widgets/app_bar/app_bar.h \
    ui/widgets/button/button.h \
    ui/widgets/card/card.h \
    ui/widgets/card/card_popup.h \
    ui/widgets/card/card_popup_with_tree.h \
    ui/widgets/chat/chat_message.h \
    ui/widgets/chat/chat_messages_view.h \
    ui/widgets/chat/user.h \
    ui/widgets/check_box/check_box.h \
    ui/widgets/circular_progress_bar/circular_progress_bar.h \
    ui/widgets/color_picker/color_2d_slider.h \
    ui/widgets/color_picker/color_hue_slider.h \
    ui/widgets/color_picker/color_palette.h \
    ui/widgets/color_picker/color_picker.h \
    ui/widgets/color_picker/color_picker_popup.h \
    ui/widgets/combo_box/combo_box.h \
    ui/widgets/context_menu/context_menu.h \
    ui/widgets/date_picker/date_picker.h \
    ui/widgets/date_picker/date_picker_popup.h \
    ui/widgets/date_picker/month_days_widget.h \
    ui/widgets/dialog/abstract_dialog.h \
    ui/widgets/dialog/dialog.h \
    ui/widgets/dialog/standard_dialog.h \
    ui/widgets/drawer/drawer.h \
    ui/widgets/floating_tool_bar/floating_tool_bar.h \
    ui/widgets/floating_tool_bar/floating_toolbar_animator.h \
    ui/widgets/icon_button/icon_button.h \
    ui/widgets/image/image_card.h \
    ui/widgets/image/image_cropping_dialog.h \
    ui/widgets/image_cropper/image_cropper.h \
    ui/widgets/key_sequence_edit/key_sequence_edit.h \
    ui/widgets/label/label.h \
    ui/widgets/label/link_label.h \
    ui/widgets/pie/pie.h \
    ui/widgets/plot/plot.h \
    ui/widgets/plot/qcustomplot.h \
    ui/widgets/progress_bar/progress_bar.h \
    ui/widgets/radio_button/radio_button.h \
    ui/widgets/radio_button/radio_button_group.h \
    ui/widgets/resizable_widget/resizable_widget.h \
    ui/widgets/scalable_graphics_view/scalable_graphics_view.h \
    ui/widgets/scroll_bar/scroll_bar.h \
    ui/widgets/shadow/shadow.h \
    ui/widgets/slider/slider.h \
    ui/widgets/splitter/splitter.h \
    ui/widgets/stack_widget/stack_widget.h \
    ui/widgets/stepper/stepper.h \
    ui/widgets/text_edit/page/abstract_scroll_area.h \
    ui/widgets/text_edit/page/abstract_scroll_area_p.h \
    ui/widgets/text_edit/page/page_text_edit_scroll_bar.h \
    ui/widgets/toggle/toggle.h \
    ui/widgets/tab_bar/tab_bar.h \
    ui/widgets/task_bar/task_bar.h \
    ui/widgets/text_edit/base/base_text_edit.h \
    ui/widgets/text_edit/completer/completer.h \
    ui/widgets/text_edit/completer/completer_text_edit.h \
    ui/widgets/text_edit/page/page_metrics.h \
    ui/widgets/text_edit/page/page_text_edit.h \
    ui/widgets/text_edit/page/page_text_edit_p.h \
    ui/widgets/text_edit/scalable_wrapper/scalable_wrapper.h \
    ui/widgets/text_edit/spell_check/spell_check_highlighter.h \
    ui/widgets/text_edit/spell_check/spell_check_text_edit.h \
    ui/widgets/text_edit/spell_check/spell_checker.h \
    ui/widgets/text_field/text_field.h \
    ui/widgets/tree/tree.h \
    ui/widgets/tree/tree_delegate.h \
    ui/widgets/tree/tree_header_view.h \
    ui/widgets/tree/tree_view.h \
    ui/widgets/widget/widget.h \
    utils/3rd_party/WAF/AbstractAnimator.h \
    utils/3rd_party/WAF/Animation/Animation.h \
    utils/3rd_party/WAF/Animation/AnimationPrivate.h \
    utils/3rd_party/WAF/Animation/CircleFill/CircleFillAnimator.h \
    utils/3rd_party/WAF/Animation/CircleFill/CircleFillDecorator.h \
    utils/3rd_party/WAF/Animation/CircleTransparent/CircleTransparentAnimator.h \
    utils/3rd_party/WAF/Animation/CircleTransparent/CircleTransparentDecorator.h \
    utils/3rd_party/WAF/Animation/Expand/ExpandAnimator.h \
    utils/3rd_party/WAF/Animation/Expand/ExpandDecorator.h \
    utils/3rd_party/WAF/Animation/SideSlide/SideSlideAnimator.h \
    utils/3rd_party/WAF/Animation/SideSlide/SideSlideDecorator.h \
    utils/3rd_party/WAF/Animation/Slide/SlideAnimator.h \
    utils/3rd_party/WAF/Animation/Slide/SlideForegroundDecorator.h \
    utils/3rd_party/WAF/WAF.h \
    utils/diff_match_patch/diff_match_patch.h \
    utils/diff_match_patch/diff_match_patch_controller.h \
    utils/helpers/color_helper.h \
    utils/helpers/dialog_helper.h \
    utils/helpers/eighths_helper.h \
    utils/helpers/extension_helper.h \
    utils/helpers/hunspell_helper.h \
    utils/helpers/icon_helper.h \
    utils/helpers/image_helper.h \
    utils/helpers/libavoid_helper.h \
    utils/helpers/measurement_helper.h \
    utils/helpers/model_helper.h \
    utils/helpers/names_generator.h \
    utils/helpers/platform_helper.h \
    utils/helpers/price_helper.h \
    utils/helpers/quotes_generator.h \
    utils/helpers/scroller_helper.h \
    utils/helpers/shortcuts_helper.h \
    utils/helpers/string_helper.h \
    utils/helpers/text_helper.h \
    utils/helpers/time_helper.h \
    utils/helpers/ui_helper.h \
    utils/logging.h \
    utils/shugar.h \
    utils/tools/alphanum_comparer.h \
    utils/tools/backup_builder.h \
    utils/tools/debouncer.h \
    utils/tools/github_files_client.h \
    utils/tools/model_index_path.h \
    utils/tools/once.h \
    utils/tools/performance_monitor.h \
    utils/tools/project_sync.h \
    utils/tools/run_once.h \
    utils/validators/email_validator.h


greaterThan(QT_MAJOR_VERSION, 5) {
    SOURCES += \
        ui/widgets/text_edit/page/qt6/abstract_scroll_area.cpp \
        ui/widgets/text_edit/page/qt6/abstract_slider.cpp \
        ui/widgets/text_edit/page/qt6/frame.cpp \
        ui/widgets/text_edit/page/qt6/page_text_edit_scroll_bar.cpp

    HEADERS += \
        ui/widgets/text_edit/page/qt6/abstract_scroll_area.h \
        ui/widgets/text_edit/page/qt6/abstract_scroll_area_p.h \
        ui/widgets/text_edit/page/qt6/abstract_slider.h \
        ui/widgets/text_edit/page/qt6/abstract_slider_p.h \
        ui/widgets/text_edit/page/qt6/frame.h \
        ui/widgets/text_edit/page/qt6/frame_p.h \
        ui/widgets/text_edit/page/qt6/page_text_edit_scroll_bar.h \
        ui/widgets/text_edit/page/qt6/page_text_edit_scroll_bar_p.h
} else {
    HEADERS += \
        ui/widgets/text_edit/page/qt5/abstract_scroll_area.h \
        ui/widgets/text_edit/page/qt5/abstract_scroll_area_p.h \
        ui/widgets/text_edit/page/qt5/page_text_edit_scroll_bar.h
}

RESOURCES += \
    fonts.qrc \
    images.qrc \
    templates.qrc
