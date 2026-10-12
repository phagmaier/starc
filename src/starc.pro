TEMPLATE = subdirs ordered

SUBDIRS += \
    3rd_party \
    app \
    corelib \
    core/management_layer/plugins \
    core \
    # testapp \
    # starcaiapp \
    # starcservices \
    # starctools \

CONFIG += ordered

HEADERS += \
    interfaces/management_layer/i_application_manager.h \
    interfaces/management_layer/i_document_manager.h \
    interfaces/ui/i_document_view.h

TRANSLATIONS += \
    core/translations/translation_en.ts

#
# Настраиваем вывод в консоль в Windows в нормальной кодировке
#
win32-msvc* {
    QMAKE_EXTRA_TARGETS += before_build makefilehook

    makefilehook.target = $(MAKEFILE)
    makefilehook.depends = .beforebuild

    PRE_TARGETDEPS += .beforebuild

    before_build.target = .beforebuild
    before_build.depends = FORCE
    before_build.commands = chcp 1251
}
