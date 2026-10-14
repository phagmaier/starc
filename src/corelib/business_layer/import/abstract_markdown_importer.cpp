#include "abstract_markdown_importer.h"

#include <business_layer/model/text/text_model_xml.h>

#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <QXmlStreamWriter>

namespace BusinessLayer {

namespace {

/**
 * @brief Информация о типах выделения в тексте
 */
struct SelectionTypeInText {
    int position = -1;
    QString formatSymbols;
};

/**
 * @brief Информация о форматах в тексте
 */
struct FormatInText {
    QSet<QLatin1String> attributes;
    int from = -1;
    int length = 0;
};

} // namespace

class AbstractMarkdownImporter::Implementation
{
public:
    Implementation(const QMap<QString, QLatin1String>& _selectionTypes,
                   const QRegularExpression& _selectionTypeChecker, const QString& _capturedGroup);


    /**
     * @brief Форматы в тексте
     */
    QVector<FormatInText> formatsInText;

    /**
     * @brief Возможные типы выделения текста в markdown или fountain
     */
    const QMap<QString, QLatin1String> markdownSelectionTypes;

    /**
     * @brief Регулярное выражение для поиска любого типа выделения текста
     */
    const QRegularExpression& selectionTypeChecker;

    /**
     * @brief Имя группы захвата форматных символов
     */
    const QString capturedGroup;
};

AbstractMarkdownImporter::Implementation::Implementation(
    const QMap<QString, QLatin1String>& _selectionTypes,
    const QRegularExpression& _selectionTypeChecker, const QString& _capturedGroup)
    : markdownSelectionTypes(_selectionTypes)
    , selectionTypeChecker(_selectionTypeChecker)
    , capturedGroup(_capturedGroup)
{
}


// ****


AbstractMarkdownImporter::AbstractMarkdownImporter(
    const QMap<QString, QLatin1String>& _selectionTypes,
    const QRegularExpression& _selectionTypeChecker, const QString& _capturedGroup)
    : d(new Implementation(_selectionTypes, _selectionTypeChecker, _capturedGroup))
{
}

AbstractMarkdownImporter::~AbstractMarkdownImporter() = default;

void AbstractMarkdownImporter::movePreviousTypes(int _position, int _offset) const
{
    for (auto& type : d->formatsInText) {
        if (_position < type.from) {
            type.from -= _offset;
        }
    }
}

void AbstractMarkdownImporter::removePreviousTypes(int _position, int _length) const
{
    const auto mapPosition = [_position, _length](int _value) {
        if (_value <= _position) {
            return _value;
        }
        return _value < _position + _length ? _position : _value - _length;
    };
    for (auto& format : d->formatsInText) {
        const auto end = mapPosition(format.from + format.length);
        format.from = mapPosition(format.from);
        format.length = end - format.from;
    }
    d->formatsInText.erase(std::remove_if(d->formatsInText.begin(), d->formatsInText.end(),
                                          [](const auto& _format) { return _format.length == 0; }),
                           d->formatsInText.end());
}

//
// Чтобы не возникало путаницы, обозначим термины, которые будут использоваться при описании того,
// что здесь происходит: ФОРМАТНЫЕ СИМВОЛЫ - символовы, которые делают текст жирным, подчеркнутым,
// зачеркнутым или курсивным, ТИП ВЫДЕЛЕНИЯ - жирный, курсив, подчеркнутый или зачеркнутый - что-то
// одно (задается XML-аттрибутом), а ФОРМАТОМ будем называть комбинацию из нескольких типов
// выделения
//
void AbstractMarkdownImporter::collectSelectionTypes(QString& _paragraphText) const
{
    //
    // Собираем типы выделения
    //
    // Escapes, review comments and inline-note delimiters are not emphasis markers.
    // Keep their source positions while masking them from the formatting search.
    //
    QString searchText = _paragraphText;
    bool inReview = false;
    for (int index = 0; index < searchText.size(); ++index) {
        const auto symbol = _paragraphText.at(index);
        const auto next = index + 1 < searchText.size() ? _paragraphText.at(index + 1) : QChar();
        if (symbol == '\\' && !next.isNull()) {
            searchText[index] = QChar();
            searchText[++index] = QChar();
        } else if ((!inReview && symbol == '[' && next == '[')
                   || (inReview && symbol == ']' && next == ']')) {
            inReview = !inReview;
            searchText[index] = QChar();
            searchText[++index] = QChar();
        } else if (inReview) {
            searchText[index] = QChar();
        } else if ((symbol == '/' && next == '*') || (symbol == '*' && next == '/')) {
            searchText[index] = QChar();
            searchText[++index] = QChar();
        }
    }
    QVector<SelectionTypeInText> selectionTypes;
    QRegularExpressionMatch match = d->selectionTypeChecker.match(searchText);
    while (match.hasMatch()) {
        const SelectionTypeInText type = { static_cast<int>(match.capturedStart(d->capturedGroup)),
                                           match.captured(d->capturedGroup) };
        selectionTypes.append(type);
        match = d->selectionTypeChecker.match(searchText, match.capturedEnd(d->capturedGroup));
    }

    //
    // Отбираем только те, у которых есть закрывающая пара
    //
    QVector<SelectionTypeInText> pairedTypes;
    while (!selectionTypes.isEmpty()) {
        const auto first = selectionTypes.takeFirst();
        const auto contentStart = first.position + first.formatSymbols.size();
        if (contentStart == _paragraphText.size() || _paragraphText.at(contentStart).isSpace()) {
            continue;
        }
        for (int i = 0; i != selectionTypes.size(); ++i) {
            if (first.formatSymbols == selectionTypes[i].formatSymbols) {
                //
                // ... дополнительно проверим, что между форматными символами есть расстояние,
                // иначе это не типы выделения, а просто набор символов (напр. ****)
                //
                SelectionTypeInText minType, maxType;
                if (first.position < selectionTypes[i].position) {
                    minType = first;
                    maxType = selectionTypes[i];
                } else {
                    minType = selectionTypes[i];
                    maxType = first;
                }
                int distance = maxType.position - (minType.position + minType.formatSymbols.size());
                // Legacy exports can switch emphasis immediately before a whitespace-only
                // tail. Recognize that adjacent, already paired style boundary without
                // accepting ordinary whitespace-padded markers as emphasis.
                auto content = _paragraphText.mid(contentStart, distance);
                bool hasAdjacentStyle = false;
                for (const auto& paired : pairedTypes) {
                    if (paired.position >= contentStart && paired.position < maxType.position) {
                        hasAdjacentStyle |= paired.position == contentStart;
                        content.replace(paired.position - contentStart, paired.formatSymbols.size(),
                                        QString(paired.formatSymbols.size(), QChar::Space));
                    }
                }
                const bool styledWhitespaceTail = hasAdjacentStyle && content.trimmed().isEmpty()
                    && maxType.position + maxType.formatSymbols.size() == _paragraphText.size();
                // A pair must touch its content on both sides. Arithmetic markers and
                // whitespace-padded markers remain literal, even if another marker follows.
                if (distance > 0
                    && (!_paragraphText.at(maxType.position - 1).isSpace()
                        || styledWhitespaceTail)) {
                    pairedTypes.append(first);
                    pairedTypes.append(selectionTypes[i]);
                    selectionTypes.remove(i);
                    break;
                }
            }
        }
    }

    //
    // Удаляем форматные символы и корректируем позиции типов выделения
    //
    for (int i = 0; i != pairedTypes.size(); ++i) {
        _paragraphText.remove(pairedTypes[i].position, pairedTypes[i].formatSymbols.size());
        for (int j = 0; j != pairedTypes.size(); ++j) {
            if (pairedTypes[j].position > pairedTypes[i].position) {
                pairedTypes[j].position -= pairedTypes[i].formatSymbols.size();
            }
        }
    }

    //
    // Сортируем по позиции в тексте
    //
    std::sort(pairedTypes.begin(), pairedTypes.end(),
              [](const SelectionTypeInText& _lhs, const SelectionTypeInText& _rhs) {
                  return _lhs.position < _rhs.position;
              });

    //
    // Парсим
    //
    FormatInText currentFormat;
    for (int i = 0; i != pairedTypes.size(); ++i) {
        //
        // Собираем все типы выделения в текущей позиции
        //
        QSet<QLatin1String> typesInPosition;
        int count = 0;
        while (i + count < pairedTypes.size()
               && pairedTypes[i].position == pairedTypes[i + count].position) {
            typesInPosition.insert(
                d->markdownSelectionTypes.value(pairedTypes[i + count].formatSymbols));
            ++count;
        }
        i += count - 1;

        //
        // Если ли уже есть открытый формат
        //
        if (!currentFormat.attributes.isEmpty()) {
            //
            // ... закрываем его (пишем его длину и добавляем в список форматов)
            //
            currentFormat.length = pairedTypes[i].position - currentFormat.from;
            d->formatsInText.append(currentFormat);

            //
            // ... заполняем новый формат в ту же переменную
            //
            currentFormat.from = pairedTypes[i].position;
            currentFormat.length = 0;

            //
            // ... смотрим, какие типы выделения закрываются в текущей позиции, а какие
            // открываются
            //
            for (const auto& type : typesInPosition) {
                if (currentFormat.attributes.contains(type)) {
                    //
                    // ... закрывающиеся удаляем из текущего формата
                    //
                    currentFormat.attributes.remove(type);
                } else {
                    //
                    // ... а открывающиеся - добавляем
                    //
                    currentFormat.attributes.insert(type);
                }
            }

        }
        //
        // Если открытого формата нет, то откроем
        //
        else {
            currentFormat.attributes = typesInPosition;
            currentFormat.from = pairedTypes[i].position;
        }
    }
}

void AbstractMarkdownImporter::writeSelectionTypes(QXmlStreamWriter& _writer) const
{
    if (!d->formatsInText.isEmpty()) {
        _writer.writeStartElement(xml::kFormatsTag);
        for (const auto& type : d->formatsInText) {
            _writer.writeStartElement(xml::kFormatTag);
            _writer.writeAttribute(xml::kFromAttribute, QString::number(type.from));
            _writer.writeAttribute(xml::kLengthAttribute, QString::number(type.length));
            for (const auto& attribute : type.attributes) {
                _writer.writeAttribute(attribute, "true");
            }
            _writer.writeEndElement(); // format
        }
        _writer.writeEndElement(); // formats
        d->formatsInText.clear();
    }
}

} // namespace BusinessLayer
