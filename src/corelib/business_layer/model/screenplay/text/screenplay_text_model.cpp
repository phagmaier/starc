#include "screenplay_text_model.h"

#include "screenplay_text_block_parser.h"
#include "screenplay_text_model_beat_item.h"
#include "screenplay_text_model_folder_item.h"
#include "screenplay_text_model_scene_item.h"
#include "screenplay_text_model_text_item.h"

#include <business_layer/model/characters/character_model.h>
#include <business_layer/model/characters/characters_model.h>
#include <business_layer/model/locations/location_model.h>
#include <business_layer/model/locations/locations_model.h>
#include <business_layer/model/screenplay/screenplay_information_model.h>
#include <business_layer/templates/screenplay_template.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <utils/helpers/text_helper.h>
#include <utils/logging.h>
#include <utils/shugar.h>

#include <QRegularExpression>
#include <QStringListModel>
#include <QXmlStreamReader>


namespace BusinessLayer {

namespace {
const char* kMimeType = "application/x-starc/screenplay/text/item";

/**
 * @brief Whether paragraphs of the type are running text, where a character can be mentioned.
 *        Scene headings, character cues and scene characters are not: the locations and the
 *        characters of the project are collected from them, so a name inside another name
 *        (YOUNG WILL, WILL'S BAR) has to stay as it is there. Act and sequence headings are
 *        titles and aren't searched either
 */
bool isRunningText(TextParagraphType _type)
{
    switch (_type) {
    case TextParagraphType::Action:
    case TextParagraphType::Dialogue:
    case TextParagraphType::Parenthetical:
    case TextParagraphType::Lyrics:
    case TextParagraphType::Shot:
    case TextParagraphType::Transition:
    case TextParagraphType::BeatHeading:
    case TextParagraphType::InlineNote:
    case TextParagraphType::UnformattedText:
        return true;

    default:
        return false;
    }
}

/**
 * @brief Literal whole-name mentions whose first letter isn't lowercase, including names that
 *        start with punctuation or digits. Unicode letters and combining marks form words.
 *        The mentions are counted and renamed from this one list, so the two always agree
 * @note The name should be in capitals
 */
QVector<QRegularExpressionMatch> characterMentions(const QString& _text, const QString& _name)
{
    QVector<QRegularExpressionMatch> mentions;
    if (_name.isEmpty() || !_text.contains(_name, Qt::CaseInsensitive)) {
        return mentions;
    }

    const QRegularExpression nameMatcher(
        QString("(?<![\\p{L}\\p{M}\\p{N}_])(%1)(?![\\p{L}\\p{M}\\p{N}_])")
            .arg(QRegularExpression::escape(_name)),
        QRegularExpression::CaseInsensitiveOption);
    auto match = nameMatcher.globalMatch(_text);
    while (match.hasNext()) {
        const auto mention = match.next();
        bool startsLowercase = false;
        for (const auto codePoint : mention.captured().toUcs4()) {
            if (QChar::isLetter(codePoint)) {
                startsLowercase = QChar::isLower(codePoint);
                break;
            }
        }
        if (!startsLowercase) {
            mentions.append(mention);
        }
    }
    return mentions;
}

/**
 * @brief Exact names in a cue or comma-separated scene-character list, with source positions.
 *        Match the same simplified names as the parser, preserving whitespace and extensions
 */
QVector<QPair<int, int>> characterCueNames(const QString& _text, const QString& _name, bool _isList)
{
    QVector<QPair<int, int>> names;
    int from = 0;
    while (from < _text.length()) {
        int end = _isList ? _text.indexOf(',', from) : -1;
        if (end == -1) {
            end = _text.length();
        }
        const auto entry = _text.mid(from, end - from);
        if (ScreenplayCharacterParser::name(entry) == _name) {
            int nameFrom = from;
            int nameEnd = entry.indexOf('(');
            nameEnd = nameEnd == -1 ? end : from + nameEnd;
            while (nameFrom < nameEnd && _text.at(nameFrom).isSpace()) {
                ++nameFrom;
            }
            while (nameEnd > nameFrom && _text.at(nameEnd - 1).isSpace()) {
                --nameEnd;
            }
            if (nameEnd > nameFrom) {
                names.append({ nameFrom, nameEnd - nameFrom });
            }
        }
        from = end + 1;
    }
    return names;
}
} // namespace

class ScreenplayTextModel::Implementation
{
public:
    explicit Implementation(ScreenplayTextModel* _q);

    /**
     * @brief Получить корневой элемент
     */
    TextModelItem* rootItem() const;

    /**
     * @brief Пересчитать счетчики элемента и всех детей
     */
    void updateChildrenCounters(const TextModelItem* _item, bool _force = false);


    /**
     * @brief Родительский элемент
     */
    ScreenplayTextModel* q = nullptr;

    /**
     * @brief Модель информации о проекте
     */
    ScreenplayInformationModel* informationModel = nullptr;

    /**
     * @brief Модель справочников
     */
    ScreenplayDictionariesModel* dictionariesModel = nullptr;

    /**
     * @brief Количество страниц
     */
    int treatmentPageCount = 0;
    int scriptPageCount = 0;

    /**
     * @brief Количество сцен
     */
    int scenesCount = 0;

    /**
     * @brief Последний сохранённый хэш документа
     */
    QByteArray lastContentHash;

    /**
     * @brief Запланировано ли обновление нумерации
     */
    bool isUpdateNumberingPlanned = false;
};

ScreenplayTextModel::Implementation::Implementation(ScreenplayTextModel* _q)
    : q(_q)
{
}

TextModelItem* ScreenplayTextModel::Implementation::rootItem() const
{
    return q->itemForIndex({});
}

void ScreenplayTextModel::Implementation::updateChildrenCounters(const TextModelItem* _item,
                                                                 bool _force)
{
    if (_item == nullptr) {
        return;
    }

    for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
        auto childItem = _item->childAt(childIndex);
        switch (childItem->type()) {
        case TextModelItemType::Folder:
        case TextModelItemType::Group: {
            updateChildrenCounters(childItem, _force);
            break;
        }

        case TextModelItemType::Text: {
            auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
            textItem->updateCounters(_force);
            break;
        }

        default:
            break;
        }
    }
}


// ****

ScreenplayTextModel::ScreenplayTextModel(QObject* _parent)
    : ScriptTextModel(_parent, ScreenplayTextModel::createFolderItem(TextFolderType::Root))
    , d(new Implementation(this))
{
    auto updateCounters = [this](const QModelIndex& _index = {}) {
        if (const auto hash = contentHash(); d->lastContentHash != hash) {
            updateNumbering();
            d->lastContentHash = hash;
        }

        d->updateChildrenCounters(itemForIndex(_index));
    };
    //
    // Обновляем счётчики после того, как операции вставки и удаления будут обработаны клиентами
    // модели (главным образом внутри прокси-моделей), т.к. обновление элемента модели может
    // приводить к падению внутри них
    //
    connect(this, &ScreenplayTextModel::afterRowsInserted, this, updateCounters);
    connect(this, &ScreenplayTextModel::afterRowsRemoved, this, updateCounters);
    connect(this, &ScreenplayTextModel::modelReset, this, updateCounters);
    //
    // Если модель планируем большое изменение, то планируем отложенное обновление нумерации
    //
    connect(this, &ScreenplayTextModel::rowsAboutToBeChanged, this,
            [this] { d->isUpdateNumberingPlanned = true; });
    connect(this, &ScreenplayTextModel::rowsChanged, this, [this] {
        if (d->isUpdateNumberingPlanned) {
            d->isUpdateNumberingPlanned = false;
            updateNumbering();
        }
    });

    connect(this, &ScreenplayTextModel::contentsChanged, this,
            &ScreenplayTextModel::markNeedUpdateRuntimeDictionaries);
}

ScreenplayTextModel::~ScreenplayTextModel() = default;

QString ScreenplayTextModel::documentName() const
{
    return QString("%1 | %2").arg(tr("Screenplay"), informationModel()->name());
}

TextModelFolderItem* ScreenplayTextModel::createFolderItem(TextFolderType _type) const
{
    return new ScreenplayTextModelFolderItem(this, _type);
}

TextModelGroupItem* ScreenplayTextModel::createGroupItem(TextGroupType _type) const
{
    switch (_type) {
    case TextGroupType::Scene: {
        return new ScreenplayTextModelSceneItem(this);
    }

    case TextGroupType::Beat: {
        return new ScreenplayTextModelBeatItem(this);
    }

    default: {
        Q_ASSERT(false);
        return nullptr;
    }
    }
}

TextModelTextItem* ScreenplayTextModel::createTextItem() const
{
    return new ScreenplayTextModelTextItem(this);
}

QStringList ScreenplayTextModel::mimeTypes() const
{
    return { kMimeType };
}

void ScreenplayTextModel::setInformationModel(ScreenplayInformationModel* _model)
{
    if (d->informationModel == _model) {
        return;
    }

    if (d->informationModel) {
        d->informationModel->disconnect(this);
    }

    d->informationModel = _model;

    if (d->informationModel) {
        connect(d->informationModel, &ScreenplayInformationModel::scenesNumberingStartAtChanged,
                this, &ScreenplayTextModel::updateNumbering);
        connect(d->informationModel, &ScreenplayInformationModel::scenesNumbersTemplateChanged,
                this, &ScreenplayTextModel::updateNumbering);
        connect(d->informationModel, &ScreenplayInformationModel::isSceneNumbersLockedChanged, this,
                &ScreenplayTextModel::setScenesNumbersLocked);
    }
}

ScreenplayInformationModel* ScreenplayTextModel::informationModel() const
{
    return d->informationModel;
}

void ScreenplayTextModel::setDictionariesModel(ScreenplayDictionariesModel* _model)
{
    d->dictionariesModel = _model;
}

ScreenplayDictionariesModel* ScreenplayTextModel::dictionariesModel() const
{
    return d->dictionariesModel;
}

void ScreenplayTextModel::updateCharacterName(const QString& _oldName, const QString& _newName,
                                              bool _withMentions)
{
    const auto oldName = TextHelper::smartToUpper(_oldName);
    std::function<void(const TextModelItem*)> updateCharacterBlock;
    updateCharacterBlock = [this, oldName, _newName, _withMentions,
                            &updateCharacterBlock](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                updateCharacterBlock(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                if (textItem->paragraphType() == TextParagraphType::SceneCharacters
                    || textItem->paragraphType() == TextParagraphType::Character) {
                    const auto names = characterCueNames(textItem->text(), oldName,
                                                         textItem->paragraphType()
                                                             == TextParagraphType::SceneCharacters);
                    if (!names.isEmpty()) {
                        textItem->beginChangeTransaction();
                        for (const auto& name : reversed(names)) {
                            textItem->replaceText(name.first, name.second, _newName);
                        }
                        textItem->finishChangeTransaction();
                        updateItem(textItem);
                    }
                } else if (_withMentions && isRunningText(textItem->paragraphType())) {
                    const auto mentions = characterMentions(textItem->text(), oldName);
                    if (mentions.isEmpty()) {
                        break;
                    }
                    //
                    // From the last mention to the first one, so the positions of the ones
                    // that aren't renamed yet stay valid. A name written in capitals stays
                    // in capitals, any other one is written as a name
                    //
                    textItem->beginChangeTransaction();
                    for (const auto& mention : reversed(mentions)) {
                        const auto capitalizeEveryWord = true;
                        const auto newName = mention.captured() == oldName
                            ? TextHelper::smartToUpper(_newName)
                            : TextHelper::toSentenceCase(_newName, capitalizeEveryWord);
                        textItem->replaceText(mention.capturedStart(), mention.capturedLength(),
                                              newName);
                    }

                    textItem->finishChangeTransaction();
                    updateItem(textItem);
                }
                break;
            }

            default:
                break;
            }
        }
    };

    beginChangeRows();
    updateCharacterBlock(d->rootItem());
    endChangeRows();
}

int ScreenplayTextModel::characterMentionsCount(const QString& _name) const
{
    const auto name = TextHelper::smartToUpper(_name);
    int mentionsCount = 0;
    std::function<void(const TextModelItem*)> countMentions;
    countMentions = [&name, &mentionsCount, &countMentions](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                countMentions(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                if (isRunningText(textItem->paragraphType())) {
                    mentionsCount += characterMentions(textItem->text(), name).size();
                }
                break;
            }

            default:
                break;
            }
        }
    };
    countMentions(d->rootItem());
    return mentionsCount;
}

QVector<QModelIndex> ScreenplayTextModel::characterDialogues(const QString& _name) const
{
    QVector<QModelIndex> modelIndexes;
    for (int row = 0; row < rowCount(); ++row) {
        modelIndexes.append(index(row, 0));
    }
    QString lastCharacter;
    QVector<QModelIndex> dialoguesIndexes;
    while (!modelIndexes.isEmpty()) {
        const auto itemIndex = modelIndexes.takeFirst();
        const auto item = itemForIndex(itemIndex);
        if (item->type() == TextModelItemType::Text) {
            const auto textItem = static_cast<TextModelTextItem*>(item);
            switch (textItem->paragraphType()) {
            case TextParagraphType::Character: {
                lastCharacter = ScreenplayCharacterParser::name(textItem->text());
                break;
            }

            case TextParagraphType::Parenthetical: {
                //
                // Не очищаем имя персонажа, идём до реплики
                //
                break;
            }

            case TextParagraphType::Dialogue:
            case TextParagraphType::Lyrics: {
                if (lastCharacter == _name) {
                    dialoguesIndexes.append(itemIndex);
                }
                break;
            }

            default: {
                lastCharacter.clear();
                break;
            }
            }
        }

        for (int childRow = 0; childRow < rowCount(itemIndex); ++childRow) {
            modelIndexes.append(index(childRow, 0, itemIndex));
        }
    }

    return dialoguesIndexes;
}

QVector<QString> ScreenplayTextModel::findCharactersFromText() const
{
    QVector<QString> characters;
    QHash<QString, int> charactersDialogues;
    std::function<void(const TextModelItem*)> findCharacters;
    findCharacters
        = [&characters, &charactersDialogues, &findCharacters](const TextModelItem* _item) {
              for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
                  auto childItem = _item->childAt(childIndex);
                  switch (childItem->type()) {
                  case TextModelItemType::Folder:
                  case TextModelItemType::Group: {
                      findCharacters(childItem);
                      break;
                  }

                  case TextModelItemType::Text: {
                      auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                      if (textItem->paragraphType() == TextParagraphType::SceneCharacters) {
                          const auto textCharacters
                              = ScreenplaySceneCharactersParser::characters(textItem->text());
                          for (const auto& character : textCharacters) {
                              if (!charactersDialogues.contains(character)) {
                                  characters.append(character);
                                  charactersDialogues.insert(character, 0);
                              }
                          }
                      } else if (textItem->paragraphType() == TextParagraphType::Character) {
                          const auto character = ScreenplayCharacterParser::name(textItem->text());
                          if (charactersDialogues.contains(character)) {
                              ++charactersDialogues[character];
                          } else {
                              characters.append(character);
                              charactersDialogues.insert(character, 1);
                          }
                      }
                      break;
                  }

                  default:
                      break;
                  }
              }
          };
    findCharacters(d->rootItem());
    std::sort(characters.begin(), characters.end(),
              [&charactersDialogues](const QString& _lhs, const QString& _rhs) {
                  return charactersDialogues.value(_lhs) > charactersDialogues.value(_rhs);
              });

    return characters;
}

QVector<QModelIndex> ScreenplayTextModel::locationScenes(const QString& _name) const
{
    QVector<QModelIndex> modelIndexes;
    for (int row = 0; row < rowCount(); ++row) {
        modelIndexes.append(index(row, 0));
    }
    QVector<QModelIndex> dialoguesIndexes;
    while (!modelIndexes.isEmpty()) {
        const auto itemIndex = modelIndexes.takeFirst();
        const auto item = itemForIndex(itemIndex);
        if (item->type() == TextModelItemType::Group) {
            const auto groupItem = static_cast<TextModelGroupItem*>(item);
            switch (groupItem->groupType()) {
            case TextGroupType::Scene: {
                if (_name == ScreenplaySceneHeadingParser::location(groupItem->heading())) {
                    dialoguesIndexes.append(itemIndex);
                }
                break;
            }

            default: {
                break;
            }
            }
        }

        for (int childRow = 0; childRow < rowCount(itemIndex); ++childRow) {
            modelIndexes.append(index(childRow, 0, itemIndex));
        }
    }

    return dialoguesIndexes;
}

QVector<QString> ScreenplayTextModel::findLocationsFromText() const
{
    QVector<QString> locations;
    QHash<QString, int> locationsCount;
    std::function<void(const TextModelItem*)> findLocations;
    findLocations = [&locations, &locationsCount, &findLocations](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                findLocations(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                if (textItem->paragraphType() == TextParagraphType::SceneHeading) {
                    const auto location = ScreenplaySceneHeadingParser::location(textItem->text());
                    if (locationsCount.contains(location)) {
                        ++locationsCount[location];
                    } else {
                        locations.append(location);
                        locationsCount.insert(location, 1);
                    }
                }
                break;
            }

            default:
                break;
            }
        }
    };
    findLocations(d->rootItem());
    std::sort(locations.begin(), locations.end(),
              [&locationsCount](const QString& _lhs, const QString& _rhs) {
                  return locationsCount.value(_lhs) > locationsCount.value(_rhs);
              });

    return locations;
}

void ScreenplayTextModel::updateLocationName(const QString& _oldName, const QString& _newName)
{
    const auto oldName = TextHelper::smartToUpper(_oldName);
    std::function<void(const TextModelItem*)> updateLocationBlock;
    updateLocationBlock
        = [this, oldName, _newName, &updateLocationBlock](const TextModelItem* _item) {
              for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
                  auto childItem = _item->childAt(childIndex);
                  switch (childItem->type()) {
                  case TextModelItemType::Folder:
                  case TextModelItemType::Group: {
                      updateLocationBlock(childItem);
                      break;
                  }

                  case TextModelItemType::Text: {
                      auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                      if (textItem->paragraphType() == TextParagraphType::SceneHeading
                          && ScreenplaySceneHeadingParser::location(textItem->text()) == oldName) {
                          const auto nameIndex
                              = TextHelper::smartToUpper(textItem->text()).indexOf(oldName);
                          textItem->replaceText(nameIndex, oldName.length(), _newName);
                          updateItem(textItem);
                      }
                      break;
                  }

                  default:
                      break;
                  }
              }
          };

    beginChangeRows();
    updateLocationBlock(d->rootItem());
    endChangeRows();
}

int ScreenplayTextModel::treatmentPageCount() const
{
    return d->treatmentPageCount;
}

void ScreenplayTextModel::setTreatmentPageCount(int _count)
{
    if (d->treatmentPageCount == _count) {
        return;
    }

    d->treatmentPageCount = _count;

    //
    // Создаём фейковое уведомление, чтобы оповестить клиентов
    //
    emit dataChanged(index(0, 0), index(0, 0));
}

int ScreenplayTextModel::scriptPageCount() const
{
    return d->scriptPageCount;
}

void ScreenplayTextModel::setScriptPageCount(int _count)
{
    if (d->scriptPageCount == _count) {
        return;
    }

    d->scriptPageCount = _count;

    //
    // Создаём фейковое уведомление, чтобы оповестить клиентов
    //
    emit dataChanged(index(0, 0), index(0, 0));
}

int ScreenplayTextModel::scenesCount() const
{
    return d->scenesCount;
}

int ScreenplayTextModel::wordsCount() const
{
    return static_cast<ScreenplayTextModelFolderItem*>(d->rootItem())->wordsCount();
}

QPair<int, int> ScreenplayTextModel::charactersCount() const
{
    return static_cast<ScreenplayTextModelFolderItem*>(d->rootItem())->charactersCount();
}

std::chrono::milliseconds ScreenplayTextModel::duration() const
{
    return static_cast<ScreenplayTextModelFolderItem*>(d->rootItem())->duration();
}

qreal ScreenplayTextModel::eighths() const
{
    return static_cast<ScreenplayTextModelFolderItem*>(d->rootItem())->eighths();
}

std::map<std::chrono::milliseconds, QColor> ScreenplayTextModel::itemsColors() const
{
    std::chrono::milliseconds lastItemDuration{ 0 };
    std::map<std::chrono::milliseconds, QColor> colors;
    std::function<void(const TextModelItem*)> collectChildColors;
    collectChildColors = [&collectChildColors, &lastItemDuration,
                          &colors](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder: {
                collectChildColors(childItem);
                break;
            }

            case TextModelItemType::Group: {
                if (static_cast<TextGroupType>(childItem->subtype()) != TextGroupType::Scene) {
                    break;
                }

                const auto sceneItem = static_cast<const ScreenplayTextModelSceneItem*>(childItem);
                colors.emplace(lastItemDuration, sceneItem->color());
                lastItemDuration += sceneItem->duration();
                break;
            }

            default:
                break;
            }
        }
    };
    collectChildColors(d->rootItem());
    return colors;
}

std::map<qreal, QColor> ScreenplayTextModel::itemsColorsForEighths() const
{
    qreal lastItemDuration{ 0 };
    std::map<qreal, QColor> colors;
    std::function<void(const TextModelItem*)> collectChildColors;
    collectChildColors = [&collectChildColors, &lastItemDuration,
                          &colors](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder: {
                collectChildColors(childItem);
                break;
            }

            case TextModelItemType::Group: {
                if (static_cast<TextGroupType>(childItem->subtype()) != TextGroupType::Scene) {
                    break;
                }

                const auto sceneItem = static_cast<const ScreenplayTextModelSceneItem*>(childItem);
                colors.emplace(lastItemDuration, sceneItem->color());
                lastItemDuration += sceneItem->eighths();
                break;
            }

            default:
                break;
            }
        }
    };
    collectChildColors(d->rootItem());
    return colors;
}

std::map<std::chrono::milliseconds, QColor> ScreenplayTextModel::itemsBookmarks() const
{
    std::chrono::milliseconds lastItemDuration{ 0 };
    std::map<std::chrono::milliseconds, QColor> colors;
    std::function<void(const TextModelItem*)> collectChildColors;
    collectChildColors = [&collectChildColors, &lastItemDuration,
                          &colors](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                collectChildColors(childItem);
                break;
            }

            case TextModelItemType::Text: {
                const auto textItem = static_cast<const ScreenplayTextModelTextItem*>(childItem);
                if (textItem->bookmark().has_value() && textItem->bookmark().value().isValid()) {
                    colors.emplace(lastItemDuration, textItem->bookmark()->color);
                }
                lastItemDuration += textItem->duration();
                break;
            }

            default:
                break;
            }
        }
    };
    collectChildColors(d->rootItem());
    return colors;
}

std::map<qreal, QColor> ScreenplayTextModel::itemsBookmarksForEighths() const
{
    qreal lastItemDuration{ 0 };
    std::map<qreal, QColor> colors;
    std::function<void(const TextModelItem*)> collectChildColors;
    collectChildColors = [&collectChildColors, &lastItemDuration,
                          &colors](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                collectChildColors(childItem);
                break;
            }

            case TextModelItemType::Text: {
                const auto textItem = static_cast<const ScreenplayTextModelTextItem*>(childItem);
                if (textItem->bookmark().has_value() && textItem->bookmark().value().isValid()) {
                    colors.emplace(lastItemDuration, textItem->bookmark()->color);
                }
                lastItemDuration += textItem->eighths();
                break;
            }

            default:
                break;
            }
        }
    };
    collectChildColors(d->rootItem());
    return colors;
}

void ScreenplayTextModel::updateNumbering()
{
    if (d->isUpdateNumberingPlanned) {
        return;
    }

    d->scenesCount = 0;
    int sceneNumber = d->informationModel->scenesNumberingStartAt();
    int dialogueNumber = 0;
    QString lastLockedSceneFullNumber;
    std::function<void(const TextModelItem*)> updateChildNumbering;
    updateChildNumbering = [this, &sceneNumber, &dialogueNumber, &lastLockedSceneFullNumber,
                            &updateChildNumbering](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder: {
                updateChildNumbering(childItem);
                break;
            }

            case TextModelItemType::Group: {
                updateChildNumbering(childItem);
                auto groupItem = static_cast<TextModelGroupItem*>(childItem);
                if (groupItem->groupType() == TextGroupType::Scene) {
                    ++d->scenesCount;

                    //
                    // Если у сцены номер заблокирован, то запоминаем последний заблокированный для
                    // работы с номерами незаблокированных сцен и сбрасываем счётчик номеров
                    //
                    if (groupItem->number().has_value() && groupItem->number()->isLocked) {
                        lastLockedSceneFullNumber
                            = groupItem->number()->followNumber + groupItem->number()->value;
                        sceneNumber = 0;
                    }
                    //
                    // Если у сцены задан кастомный номер, то не меняем его
                    //
                    else if (groupItem->number().has_value() && groupItem->number()->isCustom) {
                        //
                        // ... но при необходимости переводим счётчик номеров сцен
                        //
                        if (!groupItem->number()->isEatNumber) {
                            ++sceneNumber;
                        }
                    }
                    //
                    // А если номера назначаются автоматически, то задаём очередной номер
                    //
                    else {
                        if (groupItem->setNumber(sceneNumber, lastLockedSceneFullNumber)) {
                            updateItemForRoles(groupItem, { TextModelGroupItem::GroupNumberRole });
                        }
                        ++sceneNumber;
                    }

                    //
                    // После того, как номер сформирован, декорируем его
                    //
                    groupItem->prepareNumberText(d->informationModel->scenesNumbersTemplate());
                }
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);
                if (textItem->isCorrection()) {
                    break;
                }

                switch (textItem->paragraphType()) {
                case TextParagraphType::Character: {
                    ++dialogueNumber;
                    Q_FALLTHROUGH();
                }

                case TextParagraphType::Dialogue:
                case TextParagraphType::Lyrics: {
                    if (textItem->setNumber(dialogueNumber)) {
                        updateItemForRoles(textItem, { TextModelTextItem::TextNumberRole });
                    }
                    break;
                }

                default: {
                    break;
                }
                }

                break;
            }

            default:
                break;
            }
        }
    };
    updateChildNumbering(d->rootItem());
}

void ScreenplayTextModel::setScenesNumbersLocked(bool _locked)
{
    std::function<void(const TextModelItem*)> setSceneNumbersLockedImpl;
    setSceneNumbersLockedImpl
        = [this, _locked, &setSceneNumbersLockedImpl](const TextModelItem* _item) {
              for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
                  auto childItem = _item->childAt(childIndex);
                  switch (childItem->type()) {
                  case TextModelItemType::Folder: {
                      setSceneNumbersLockedImpl(childItem);
                      break;
                  }

                  case TextModelItemType::Group: {
                      auto groupItem = static_cast<TextModelGroupItem*>(childItem);
                      if (groupItem->groupType() == TextGroupType::Scene) {
                          if (_locked) {
                              groupItem->lockNumber();
                          } else {
                              groupItem->resetNumber();
                          }
                          updateItem(groupItem);
                      }
                      break;
                  }

                  default:
                      break;
                  }
              }
          };
    setSceneNumbersLockedImpl(d->rootItem());

    //
    // Если номера были разблокированы, то нужно сформировать их заново
    //
    if (!_locked) {
        updateNumbering();
    }
}

void ScreenplayTextModel::recalculateCounters()
{
    beginChangeRows();
    const auto forceUpdate = true;
    d->updateChildrenCounters(d->rootItem(), forceUpdate);
    endChangeRows();
}

void ScreenplayTextModel::updateRuntimeDictionaries()
{
    const bool showHintsForAllItems
        = settingsValue(DataStorageLayer::kComponentsScreenplayEditorShowHintsForAllItemsKey)
              .toBool();
    const bool showHintsForPrimaryItems
        = settingsValue(DataStorageLayer::kComponentsScreenplayEditorShowHintsForPrimaryItemsKey)
              .toBool();
    const bool showHintsForSecondaryItems
        = settingsValue(DataStorageLayer::kComponentsScreenplayEditorShowHintsForSecondaryItemsKey)
              .toBool();
    const bool showHintsForTertiaryItems
        = settingsValue(DataStorageLayer::kComponentsScreenplayEditorShowHintsForTertiaryItemsKey)
              .toBool();

    //
    // Формируем списки персонажей и локаций
    //
    QSet<QString> characters;
    QSet<QString> locations;

    //
    // Если нужно собирать персонажей и локации из текста
    //
    std::function<void(const TextModelItem*)> findInText;
    findInText = [&findInText, &characters, &locations](const TextModelItem* _item) {
        for (int childIndex = 0; childIndex < _item->childCount(); ++childIndex) {
            auto childItem = _item->childAt(childIndex);
            switch (childItem->type()) {
            case TextModelItemType::Folder:
            case TextModelItemType::Group: {
                findInText(childItem);
                break;
            }

            case TextModelItemType::Text: {
                auto textItem = static_cast<ScreenplayTextModelTextItem*>(childItem);

                switch (textItem->paragraphType()) {
                case TextParagraphType::SceneHeading: {
                    locations.insert(ScreenplaySceneHeadingParser::location(textItem->text()));
                    break;
                }
                case TextParagraphType::SceneCharacters: {
                    const auto sceneCharacters
                        = ScreenplaySceneCharactersParser::characters(textItem->text());
                    for (const auto& character : sceneCharacters) {
                        characters.insert(character);
                    }
                    break;
                }
                case TextParagraphType::Character: {
                    characters.insert(ScreenplayCharacterParser::name(textItem->text()));
                    break;
                }
                default: {
                    break;
                }
                }

                break;
            }

            default:
                break;
            }
        }
    };
    findInText(d->rootItem());
    characters.remove({});
    locations.remove({});

    //
    // ... не забываем приаттачить персонажей из общей модели
    //
    for (int row = 0; row < charactersModel()->rowCount(); ++row) {
        const auto character = charactersModel()->character(row);
        if (character == nullptr) {
            return;
        }

        //
        // ... фильтруем по ролям, если необходимо
        //
        if (!showHintsForAllItems) {
            bool skipCharacter = true;
            switch (character->storyRole()) {
            case CharacterStoryRole::Primary: {
                skipCharacter = !showHintsForPrimaryItems;
                break;
            }
            case CharacterStoryRole::Secondary: {
                skipCharacter = !showHintsForSecondaryItems;
                break;
            }
            case CharacterStoryRole::Tertiary: {
                skipCharacter = !showHintsForTertiaryItems;
                break;
            }
            default: {
                break;
            }
            }

            if (skipCharacter) {
                continue;
            }
        }

        characters.insert(character->name());
    }
    //
    // ... создаём (при необходимости) и наполняем модель персонажей
    //
    charactersModelFromText()->setStringList(characters.values());

    //
    // ... не забываем приаттачить персонажей из общей модели
    //
    for (int row = 0; row < locationsModel()->rowCount(); ++row) {
        const auto location = locationsModel()->location(row);
        if (location == nullptr) {
            continue;
        }

        //
        // ... фильтруем по ролям, если необходимо
        //
        if (!showHintsForAllItems) {
            bool skipLocation = true;
            switch (location->storyRole()) {
            case LocationStoryRole::Primary: {
                skipLocation = !showHintsForPrimaryItems;
                break;
            }
            case LocationStoryRole::Secondary: {
                skipLocation = !showHintsForSecondaryItems;
                break;
            }
            case LocationStoryRole::Tertiary: {
                skipLocation = !showHintsForTertiaryItems;
                break;
            }
            default: {
                break;
            }
            }

            if (skipLocation) {
                continue;
            }
        }

        locations.insert(location->name());
    }
    //
    // ... создаём (при необходимости) и наполняем модель локаций
    //
    locationsModelFromText()->setStringList(locations.values());
}

void ScreenplayTextModel::initEmptyDocument()
{
    auto sceneHeading = new ScreenplayTextModelTextItem(this);
    sceneHeading->setParagraphType(TextParagraphType::SceneHeading);
    auto scene = new ScreenplayTextModelSceneItem(this);
    scene->resetUuid();
    scene->appendItem(sceneHeading);
    appendItem(scene);
}

void ScreenplayTextModel::finalizeInitialization()
{
    beginChangeRows();
    updateNumbering();
    endChangeRows();
}

ChangeCursor ScreenplayTextModel::applyPatch(const QByteArray& _patch)
{
    const auto changeCursor = TextModel::applyPatch(_patch);

    updateNumbering();

    return changeCursor;
}

QByteArray ScreenplayTextModel::restoreAfterComparison(const QByteArray& _xml) const
{
    QByteArray xml = "<?xml version=\"1.0\"?>\n";
    xml += "<document mime-type=\"" + Domain::mimeTypeFor(document()->type())
        + "\" version=\"1.0\">\n";
    bool hasOpenedScene = false;
    bool hasOpenedBeat = false;
    auto closeScene = [&xml, &hasOpenedScene] {
        if (hasOpenedScene) {
            xml += "</content>\n"
                   "</scene>\n";
            hasOpenedScene = false;
        }
    };
    auto closeBeat = [&xml, &hasOpenedBeat] {
        if (hasOpenedBeat) {
            xml += "</content>\n"
                   "</beat>\n";
            hasOpenedBeat = false;
        }
    };
    const auto lines = _xml.split('\n');
    for (const auto& line : lines) {
        if (!line.startsWith("<")) {
            xml += line + "\n";
            continue;
        }

        if (line.startsWith("<act_heading")) {
            closeBeat();
            closeScene();
            xml += QString("<act uuid=\"%1\" plots=\"\">\n"
                           "<content>\n")
                       .arg(QUuid::createUuid().toString())
                       .toUtf8();
            xml += line + "\n";
        } else if (line.startsWith("</act_footer")) {
            closeBeat();
            closeScene();
            xml += line + "\n";
            xml += "</content>\n"
                   "</act>\n";
        } else if (line.startsWith("<sequence_heading")) {
            closeBeat();
            closeScene();
            xml += QString("<sequence uuid=\"%1\" plots=\"\">\n"
                           "<content>\n")
                       .arg(QUuid::createUuid().toString())
                       .toUtf8();
            xml += line + "\n";
        } else if (line.startsWith("</sequence_footer")) {
            closeBeat();
            closeScene();
            xml += line + "\n";
            xml += "</content>\n"
                   "</sequence>\n";
        } else if (line.startsWith("<scene_heading")) {
            closeBeat();
            closeScene();
            xml += QString("<scene uuid=\"%1\" plots=\"\">\n"
                           "<content>\n")
                       .arg(QUuid::createUuid().toString())
                       .toUtf8();
            hasOpenedScene = true;
            xml += line + "\n";
        } else if (line.startsWith("<beat_heading")) {
            closeBeat();
            xml += QString("<beat uuid=\"%1\" plots=\"\">\n"
                           "<content>\n")
                       .arg(QUuid::createUuid().toString())
                       .toUtf8();
            hasOpenedBeat = true;
            xml += line + "\n";
        } else {
            xml += line + "\n";
        }
    }
    xml += "</document";
    return xml;
}

} // namespace BusinessLayer
