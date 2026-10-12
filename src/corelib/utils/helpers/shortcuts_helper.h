#pragma once

#include <corelib_global.h>

class QString;

namespace BusinessLayer {
enum class TextParagraphType;
}


/**
 * @brief Вспомогательный класс для работы с горячими клавишами
 */
class CORE_LIBRARY_EXPORT ShortcutsHelper
{
public:
    //
    // Простой текст
    //
    static QString simpleTextShortcut(BusinessLayer::TextParagraphType _type);
    static void setSimpleTextShortcut(BusinessLayer::TextParagraphType _type,
                                      const QString& _shortcut);
    //
    static QString simpleTextJumpByTab(BusinessLayer::TextParagraphType _type);
    static void setSimpleTextJumpByTab(BusinessLayer::TextParagraphType _fromType,
                                       BusinessLayer::TextParagraphType _toType);
    //
    static QString simpleTextJumpByEnter(BusinessLayer::TextParagraphType _type);
    static void setSimpleTextJumpByEnter(BusinessLayer::TextParagraphType _fromType,
                                         BusinessLayer::TextParagraphType _toType);
    //
    static QString simpleTextChangeByTab(BusinessLayer::TextParagraphType _type);
    static void setSimpleTextChangeByTab(BusinessLayer::TextParagraphType _fromType,
                                         BusinessLayer::TextParagraphType _toType);
    //
    static QString simpleTextChangeByEnter(BusinessLayer::TextParagraphType _type);
    static void setSimpleTextChangeByEnter(BusinessLayer::TextParagraphType _fromType,
                                           BusinessLayer::TextParagraphType _toType);

    //
    // Сценарий
    //
    // ... тритмент
    //
    static QString screenplayTreatmentShortcut(BusinessLayer::TextParagraphType _type);
    static void setScreenplayTreatmentShortcut(BusinessLayer::TextParagraphType _type,
                                               const QString& _shortcut);
    //
    static QString screenplayTreatmentJumpByTab(BusinessLayer::TextParagraphType _type);
    static void setScreenplayTreatmentJumpByTab(BusinessLayer::TextParagraphType _fromType,
                                                BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayTreatmentJumpByEnter(BusinessLayer::TextParagraphType _type);
    static void setScreenplayTreatmentJumpByEnter(BusinessLayer::TextParagraphType _fromType,
                                                  BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayTreatmentChangeByTab(BusinessLayer::TextParagraphType _type);
    static void setScreenplayTreatmentChangeByTab(BusinessLayer::TextParagraphType _fromType,
                                                  BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayTreatmentChangeByEnter(BusinessLayer::TextParagraphType _type);
    static void setScreenplayTreatmentChangeByEnter(BusinessLayer::TextParagraphType _fromType,
                                                    BusinessLayer::TextParagraphType _toType);
    //
    // ... текст
    //
    static QString screenplayShortcut(BusinessLayer::TextParagraphType _type);
    static void setScreenplayShortcut(BusinessLayer::TextParagraphType _type,
                                      const QString& _shortcut);
    //
    static QString screenplayJumpByTab(BusinessLayer::TextParagraphType _type);
    static void setScreenplayJumpByTab(BusinessLayer::TextParagraphType _fromType,
                                       BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayJumpByEnter(BusinessLayer::TextParagraphType _type);
    static void setScreenplayJumpByEnter(BusinessLayer::TextParagraphType _fromType,
                                         BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayChangeByTab(BusinessLayer::TextParagraphType _type);
    static void setScreenplayChangeByTab(BusinessLayer::TextParagraphType _fromType,
                                         BusinessLayer::TextParagraphType _toType);
    //
    static QString screenplayChangeByEnter(BusinessLayer::TextParagraphType _type);
    static void setScreenplayChangeByEnter(BusinessLayer::TextParagraphType _fromType,
                                           BusinessLayer::TextParagraphType _toType);
};
