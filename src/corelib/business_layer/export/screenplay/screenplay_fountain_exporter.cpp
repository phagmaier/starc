#include "screenplay_fountain_exporter.h"

#include "screenplay_export_options.h"

#include <business_layer/document/text/text_block_data.h>
#include <business_layer/import/abstract_fountain_importer.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_scene_item.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <business_layer/templates/screenplay_template.h>
#include <business_layer/templates/templates_facade.h>
#include <utils/helpers/text_helper.h>

#include <QCoreApplication>


namespace BusinessLayer {

namespace {
/**
 * @brief Список мест, в которые умеет фонтан
 */
const QStringList sceneHeadingStart = {
    QCoreApplication::translate("BusinessLayer::FountainExporter", "INT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "EXT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "EST"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "INT./EXT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "INT/EXT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "EXT./INT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "EXT/INT"),
    QCoreApplication::translate("BusinessLayer::FountainExporter", "I/E"),
};

bool sameReviewMetadata(const QTextCharFormat& _first, const QTextCharFormat& _second)
{
    const int properties[] = {
        QTextFormat::ForegroundBrush,
        QTextFormat::BackgroundBrush,
        TextBlockStyle::PropertyIsDone,
        TextBlockStyle::PropertyComments,
        TextBlockStyle::PropertyCommentsAuthors,
        TextBlockStyle::PropertyCommentsAuthorsEmails,
        TextBlockStyle::PropertyCommentsDates,
        TextBlockStyle::PropertyCommentsIsRevision,
        TextBlockStyle::PropertyCommentsIsAddition,
        TextBlockStyle::PropertyCommentsIsRemoval,
        TextBlockStyle::PropertyCommentsIsEdited,
    };
    for (const auto property : properties) {
        if (_first.property(property) != _second.property(property)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Review marks of the block, with the runs of one review thread joined
 */
QVector<QTextLayout::FormatRange> reviewThreads(const QTextBlock& _block)
{
    QVector<QTextLayout::FormatRange> reviewMarks;
    for (const QTextLayout::FormatRange& format : _block.textFormats()) {
        if (format.format.boolProperty(TextBlockStyle::PropertyIsReviewMark)) {
            // Emphasis splits one review thread into several QTextBlock runs.
            // Join only adjacent runs carrying the complete same review data.
            if (!reviewMarks.isEmpty()
                && reviewMarks.last().start + reviewMarks.last().length == format.start
                && sameReviewMetadata(reviewMarks.last().format, format.format)) {
                reviewMarks.last().length += format.length;
            } else {
                reviewMarks.push_back(format);
            }
        }
    }
    return reviewMarks;
}

/**
 * @brief Whether a review note is written straight after the position in the block's text
 */
bool hasNoteAt(const QTextBlock& _block, int _position)
{
    for (const QTextLayout::FormatRange& reviewMark : reviewThreads(_block)) {
        if (reviewMark.start + reviewMark.length != _position) {
            continue;
        }
        const QStringList comments
            = reviewMark.format.property(TextBlockStyle::PropertyComments).toStringList();
        for (const QString& comment : comments) {
            if (!comment.simplified().isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Whether the character is one of several equal square brackets in a row, which is
 *        how a review note opens and closes
 */
bool isNoteDelimiter(const QString& _text, int _position)
{
    const QChar character = _text.at(_position);
    if (character != '[' && character != ']') {
        return false;
    }
    return (_position > 0 && _text.at(_position - 1) == character)
        || (_position + 1 < _text.size() && _text.at(_position + 1) == character);
}

/**
 * @brief Text of a comment as it is written inside a review note
 */
QString escapedNote(const QString& _comment)
{
    QString note;
    note.reserve(_comment.size());
    for (int position = 0; position < _comment.size(); ++position) {
        const QChar character = _comment.at(position);
        // The importer looks for emphasis in a note as well, and the note's closing brackets
        // would take a bracket at the end of the comment for one of their own.
        const bool closesNote = character == ']' && position + 1 == _comment.size();
        if (character == '\\' || character == '*' || character == '_'
            || isNoteDelimiter(_comment, position) || closesNote) {
            note.append('\\');
        }
        note.append(character);
    }
    return note;
}
} // namespace


class ScreenplayFountainExporter::Implementation
{
public:
    explicit Implementation();

    /**
     * @brief Глубина вложенности дирректории
     */
    mutable unsigned m_dirNesting = 0;
};

ScreenplayFountainExporter::Implementation::Implementation() = default;


// ****


ScreenplayFountainExporter::ScreenplayFountainExporter()
    : ScreenplayExporter()
    , AbstractMarkdownExporter({ '\\', '*', '_' })
    , d(new Implementation())
{
}

ScreenplayFountainExporter::~ScreenplayFountainExporter() = default;

bool ScreenplayFountainExporter::processBlock(QString& _paragraph, const QTextBlock& _block,
                                              const ExportOptions& _exportOptions) const
{
    const auto& exportOptions = static_cast<const ScreenplayExportOptions&>(_exportOptions);
    const auto paragraphType = TextBlockStyle::forBlock(_block);
    // Fountain has no elements for these screenplay paragraphs. Preserve their content as
    // forced Action rather than silently dropping it or letting it become a cue/heading.
    const bool convertedToAction = paragraphType == TextParagraphType::Shot
        || paragraphType == TextParagraphType::SceneCharacters
        || paragraphType == TextParagraphType::UnformattedText;

    // Every line of an Action or converted block is a Fountain paragraph of its own. Find
    // where the lines start before review notes bring line breaks of their own into the text.
    QVector<int> actionLineStarts;
    if (paragraphType == TextParagraphType::Action || convertedToAction) {
        actionLineStarts.append(0);
        for (int position = _paragraph.indexOf(QChar::LineFeed); position != -1;
             position = _paragraph.indexOf(QChar::LineFeed, position + 1)) {
            actionLineStarts.append(position + 1);
        }
    }

    //
    // Если нужны редакторские заметки, то вставляем их
    //
    if (exportOptions.includeReviewMarks) {
        const QVector<QTextLayout::FormatRange> reviewMarks = reviewThreads(_block);
        for (int markIndex = reviewMarks.size() - 1; markIndex >= 0; --markIndex) {
            //
            // Извлечем список редакторских заметок для данной области блока
            //
            const QStringList comments = reviewMarks[markIndex]
                                             .format.property(TextBlockStyle::PropertyComments)
                                             .toStringList();
            //
            // Вставлять редакторские заметки нужно с конца, чтобы не сбилась их
            // позиция вставки
            //
            for (int commentIndex = comments.size() - 1; commentIndex >= 0; --commentIndex) {
                if (!comments[commentIndex].simplified().isEmpty()) {
                    const int position = formattedPosition(reviewMarks[markIndex].start
                                                           + reviewMarks[markIndex].length);
                    const QString note = "[[" + escapedNote(comments[commentIndex]) + "]]";
                    _paragraph.insert(position, note);
                    for (int& lineStart : actionLineStarts) {
                        if (lineStart > position) {
                            lineStart += note.size();
                        }
                    }
                }
            }
        }
    }

    //
    // Добавить форматные символы перед заголовком
    //
    auto formatToHeading = [&_paragraph](int _level) {
        const QString prefix = "#";
        _paragraph.prepend(prefix.repeated(_level) + " ");
    };

    switch (paragraphType) {
    case TextParagraphType::SceneHeading: {
        //
        // Если заголовок сцены начинается с одного из ключевых слов, то все хорошо
        //
        bool startsWithHeading = false;
        for (const QString& heading : sceneHeadingStart) {
            if (_paragraph.startsWith(heading)) {
                startsWithHeading = true;
                break;
            }
        }

        //
        // Иначе, нужно сказать, что это заголовок сцены добавлением точки в начало
        //
        if (!startsWithHeading) {
            _paragraph.prepend('.');
        }

        //
        // А если печатаем номера сцен, то добавим в конец этот номер, окруженный #
        //
        if (exportOptions.showScenesNumbers) {
            const auto blockData = static_cast<TextBlockData*>(_block.userData());
            if (blockData != nullptr && blockData->item()->parent() != nullptr
                && blockData->item()->parent()->type() == TextModelItemType::Group
                && static_cast<TextGroupType>(blockData->item()->parent()->subtype())
                    == TextGroupType::Scene) {
                const auto sceneItem
                    = static_cast<ScreenplayTextModelSceneItem*>(blockData->item()->parent());
                if (const auto number = sceneItem->number(); number.has_value()) {
                    _paragraph += QString(" #%1#").arg(number->followNumber + number->value);
                }
            }
        }
        return true;
    }

    case TextParagraphType::Character: {
        if (!TextHelper::isUppercase(_paragraph)) {
            //
            // Если название персонажа не состоит из заглавных букв,
            // то необходимо добавить @ в начало
            //
            _paragraph.prepend('@');
        }

        //
        // Если двойной диалог, то к имени второго персонажа допишем "^"
        //
        const auto blockData = static_cast<TextBlockData*>(_block.userData());
        if (blockData != nullptr) {
            const auto textItem = static_cast<TextModelTextItem*>(blockData->item());
            if (textItem->isInFirstColumn().has_value() && textItem->isInFirstColumn() == false) {
                _paragraph.append(" ^");
            }
        }
        return true;
    }

    case TextParagraphType::Transition: {
        //
        // Если переход задан заглавными буквами и в конце есть TO:
        //
        if (TextHelper::isUppercase(_paragraph) && _paragraph.endsWith("TO:")) {
            //
            // Ничего делать не надо, всё распознается нормально
            //
        }
        //
        // А если переход задан как то иначе
        //
        else {
            //
            // То надо добавить в начало >
            //
            _paragraph.prepend("> ");
        }
        return true;
    }

    case TextParagraphType::InlineNote: {
        //
        // Обернем в /* и */
        //
        _paragraph = "/* " + _paragraph + " */";
        return true;
    }

    case TextParagraphType::Shot:
    case TextParagraphType::SceneCharacters:
    case TextParagraphType::UnformattedText:
    case TextParagraphType::Action: {
        // The importer reads every line as a paragraph of its own and decides its type from
        // the text, so put "!" before each line it would not read as Action.
        const QStringList sourceLines
            = _block.text().replace(QChar::LineSeparator, QChar::LineFeed).split(QChar::LineFeed);
        for (int line = actionLineStarts.size() - 1; line >= 0; --line) {
            // The importer trims a line before looking at it and skips an empty one.
            const QString sourceLine = TextHelper::simplified(sourceLines.value(line));
            if (sourceLine.isEmpty()) {
                continue;
            }

            // An uppercase line following an empty one is a cue or a transition. The importer
            // compares the line as written, including the first line of a review note.
            const int lineStart = actionLineStarts[line];
            const int lineEnd = _paragraph.indexOf(QChar::LineFeed, lineStart);
            const QString writtenLine
                = _paragraph.mid(lineStart, lineEnd == -1 ? -1 : lineEnd - lineStart);
            const bool followsEmptyLine
                = line == 0 || TextHelper::simplified(sourceLines.value(line - 1)).isEmpty();
            if (convertedToAction || AbstractFountainImporter::startsAnotherBlock(sourceLine)
                || (followsEmptyLine && TextHelper::isUppercase(writtenLine))) {
                _paragraph.insert(lineStart, '!');
            }
        }
        return true;
    }

    case TextParagraphType::BeatHeading: {
        //
        // Блоки описания сцены предворяются = и расставляются обособлено
        //
        _paragraph.prepend("= ");
        return true;
    }

    case TextParagraphType::Lyrics: {
        //
        // Добавим ~ вначало блока лирики
        //
        _paragraph.prepend("~ ");
        return true;
    }

    case TextParagraphType::ActHeading:
    case TextParagraphType::SequenceHeading: {
        //
        // Напечатаем в начале столько #, насколько глубоко мы в директории
        //
        ++d->m_dirNesting;
        formatToHeading(d->m_dirNesting);
        return true;
    }

    case TextParagraphType::ActFooter:
    case TextParagraphType::SequenceFooter: {
        --d->m_dirNesting;
        return false;
    }

    case TextParagraphType::Parenthetical: {
        _paragraph = "(" + _paragraph + ")";
        return true;
    }

    case TextParagraphType::Dialogue: {
        return true;
    }

    default: {
        //
        // Игнорируем неизвестные блоки
        //
        return false;
    }
    }
}

QString ScreenplayFountainExporter::formatSymbols(TextSelectionTypes _type) const
{
    switch (_type) {
    case TextSelectionTypes::Bold: {
        return "**";
    }
    case TextSelectionTypes::Italic: {
        return "*";
    }
    case TextSelectionTypes::Underline: {
        return "_";
    }
    default: {
        return "";
    }
    }
}

bool ScreenplayFountainExporter::needsEscaping(const QTextBlock& _block, const QString& _text,
                                               int _position, QChar _next,
                                               const ExportOptions& _exportOptions) const
{
    // Classification precedes reading escapes on import. Escape the first text character of
    // each speech line that would become another paragraph type, inside any emphasis markup.
    // Do this here so the formatted-position map also accounts for the extra backslash.
    if (TextBlockStyle::forBlock(_block) == TextParagraphType::Dialogue
        && !_text.at(_position).isSpace()) {
        int lineStart = _position;
        while (lineStart > 0 && _text.at(lineStart - 1).isSpace()
               && _text.at(lineStart - 1) != QChar::LineSeparator
               && _text.at(lineStart - 1) != QChar::LineFeed) {
            --lineStart;
        }
        if (lineStart == 0 || _text.at(lineStart - 1) == QChar::LineSeparator
            || _text.at(lineStart - 1) == QChar::LineFeed) {
            const QString line = _text.mid(_position)
                                     .section(QChar::LineSeparator, 0, 0)
                                     .section(QChar::LineFeed, 0, 0);
            if (AbstractFountainImporter::startsAnotherBlock(TextHelper::simplified(line))) {
                return true;
            }
        }
    }

    // The importer takes the character after a backslash as text, in every kind of paragraph.
    if (AbstractMarkdownExporter::needsEscaping(_block, _text, _position, _next, _exportOptions)
        || isNoteDelimiter(_text, _position)) {
        return true;
    }

    switch (_text.at(_position).unicode()) {
    case '[': {
        // A note written straight after a bracket would open one character early.
        return _exportOptions.includeReviewMarks && hasNoteAt(_block, _position + 1);
    }

    case '/': {
        // "/*" opens an inline note, also when the star is the start or the end of emphasis.
        // A star of the text itself is escaped, so it is not what follows here.
        return _next == '*';
    }

    default: {
        return false;
    }
    }
}

void ScreenplayFountainExporter::addIndentationAtBegin(QString& _paragraph,
                                                       TextParagraphType _previosBlockType,
                                                       TextParagraphType _currentBlockType) const
{
    //
    // Таблица количества переносов строк между различными типами блоков
    // столбцы - предыдущий тип, строки - текущий
    //
    const QVector<QVector<int>> countIndentation = {
        // Undefined, SceneCharacters, Character, Parenthetical, Dialogue/Lyrics, Footers,
        // ActHeading/SequenceHeading/SceneHeading, other
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // Undefined
        { 0, 1, 2, 2, 2, 3, 2, 2 }, // SceneCharacters
        { 0, 2, 2, 2, 2, 3, 2, 2 }, // Character
        { 0, 2, 1, 1, 1, 3, 2, 2 }, // Parenthetical
        { 0, 2, 1, 1, 1, 3, 2, 2 }, // Dialogue/Lyrics
        { 0, 2, 2, 2, 2, 2, 2, 2 }, // Footers
        { 0, 3, 3, 3, 3, 3, 2, 3 }, // ActHeading/SequenceHeading/SceneHeading
        { 0, 2, 2, 2, 2, 3, 2, 2 }, // other
        // other:
        // Action/Shot/Transition/BeatHeading/UnformattedText/InlineNote
    };

    auto positionInTable = [](TextParagraphType _type) {
        switch (_type) {
        case TextParagraphType::SceneCharacters: {
            return 1;
        }
        case TextParagraphType::Character: {
            return 2;
        }
        case TextParagraphType::Parenthetical: {
            return 3;
        }
        case TextParagraphType::Dialogue:
        case TextParagraphType::Lyrics: {
            return 4;
        }
        case TextParagraphType::ActFooter:
        case TextParagraphType::SequenceFooter: {
            return 5;
        }
        case TextParagraphType::ActHeading:
        case TextParagraphType::SequenceHeading:
        case TextParagraphType::SceneHeading: {
            return 6;
        }
        case TextParagraphType::Action:
        case TextParagraphType::Shot:
        case TextParagraphType::Transition:
        case TextParagraphType::BeatHeading:
        case TextParagraphType::UnformattedText:
        case TextParagraphType::InlineNote: {
            return 7;
        }
        default: {
            return 0;
        }
        }
    };
    int column = positionInTable(_previosBlockType);
    int row = positionInTable(_currentBlockType);
    _paragraph.prepend(QString("\n").repeated(countIndentation[row][column]));
}

} // namespace BusinessLayer
