#pragma once

#include "abstract_exporter.h"

#include <QTextLayout>

namespace BusinessLayer {

enum class TextParagraphType;

class CORE_LIBRARY_EXPORT AbstractMarkdownExporter : virtual public AbstractExporter
{
public:
    AbstractMarkdownExporter(const QSet<QChar> _escapedCharacters);
    ~AbstractMarkdownExporter() override;

    /**
     * @brief Экспортировать текстовый документ
     */
    bool exportTo(AbstractModel* _model, ExportOptions& _exportOptions) const override;

    /**
     * @brief Экспортировать текстовый документ в заданном интервале текста
     */
    bool exportTo(AbstractModel* _model, int _fromPosition, int _toPosition,
                  ExportOptions& _exportOptions) const;

protected:
    /**
     * @brief Типы выделения текста, которые поддерживаются markdown и fountain
     */
    enum TextSelectionTypes {
        Bold,
        Italic,
        StrikeOut,
        Underline,
    };

    /**
     * @brief Добавить пустые строки перед абзацем
     */
    virtual void addIndentationAtBegin(QString& _paragraph, TextParagraphType _previosBlockType,
                                       TextParagraphType _currentBlockType) const
        = 0;

    /**
     * @brief Обработать блок в зависимости от его типа
     * @return Был ли блок обработан
     */
    virtual bool processBlock(QString& _paragraph, const QTextBlock& _block,
                              const ExportOptions& _exportOptions) const
        = 0;

    /**
     * @brief Получить символы типа выделения текста
     */
    virtual QString formatSymbols(TextSelectionTypes _type) const = 0;

    /**
     * @brief Whether a character of the block's text is written after an escaping backslash.
     *        _next is the character formatting has put after it, null at the end of the text.
     */
    virtual bool needsEscaping(const QTextBlock& _block, const QString& _text, int _position,
                               QChar _next, const ExportOptions& _exportOptions) const;

    /**
     * @brief Очистить начало параграфа от пробельных символов
     */
    void removeWhitespaceAtBegin(QString& _paragraph) const;

    /**
     * @brief Map a UTF-16 source boundary to the escaped/formatted paragraph in processBlock.
     */
    int formattedPosition(int _sourcePosition) const;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace BusinessLayer
