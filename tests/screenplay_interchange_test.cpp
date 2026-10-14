#include "explicit_screenplay_export.h"

#include <business_layer/export/screenplay/screenplay_export_options.h>
#include <business_layer/export/screenplay/screenplay_fdx_exporter.h>
#include <business_layer/export/screenplay/screenplay_fountain_exporter.h>
#include <business_layer/import/import_options.h>
#include <business_layer/import/screenplay/screenplay_fdx_importer.h>
#include <business_layer/import/screenplay/screenplay_fountain_importer.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_scene_item.h>
#include <business_layer/model/screenplay/text/screenplay_text_model_text_item.h>
#include <business_layer/model/structure/structure_model.h>
#include <business_layer/model/structure/structure_model_item.h>
#include <business_layer/templates/text_template.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <management_layer/content/export/export_manager.h>
#include <management_layer/content/import/import_manager.h>
#include <management_layer/content/project/project_manager.h>
#include <management_layer/content/projects/projects_model_project_item.h>
#include <management_layer/plugins_builder.h>
#include <ui/project/project_navigator.h>
#include <ui/widgets/task_bar/task_bar.h>
#include <ui/widgets/tree/tree.h>

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSortFilterProxyModel>

#include <cstdio>
#include <cstdlib>

namespace {
using namespace BusinessLayer;
using Type = Domain::DocumentObjectType;

void require(bool ok, const QString& label)
{
    std::printf("%s: %s\n", ok ? "ok" : "FAIL", qPrintable(label));
    if (!ok)
        std::exit(1);
}

QByteArray read(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read " + path);
    return file.readAll();
}

void write(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "write " + path);
}

// Expected emphasis is authored independently, by substring and B/I/U bit flags.
struct Emphasis { QString text; int bits; };
struct Row {
    QString type;
    QString text;
    QVector<Emphasis> emphasis = {};
    QString column = "none";
    QStringList comments = {};
    QVector<QPair<int, int>> reviewRanges = {};
};
struct Expected {
    QVector<Row> rows;
    QStringList numbers;
};

QJsonObject rowJson(const Row& row)
{
    QString styles(row.text.size(), '0');
    for (const auto& span : row.emphasis) {
        const auto from = row.text.indexOf(span.text);
        require(from >= 0, "expected emphasis substring");
        for (int i = from; i < from + span.text.size(); ++i)
            styles[i] = QChar('0' + span.bits);
    }
    QJsonArray ranges;
    for (const auto& range : row.reviewRanges)
        ranges.append(QJsonArray{ range.first, range.second });
    return { { "type", row.type }, { "text", row.text }, { "styles", styles },
             { "column", row.column }, { "comments", QJsonArray::fromStringList(row.comments) },
             { "reviewRanges", ranges } };
}

QJsonObject expectedJson(const Expected& expected)
{
    QJsonArray rows;
    for (const auto& row : expected.rows)
        rows.append(rowJson(row));
    return { { "rows", rows }, { "numbers", QJsonArray::fromStringList(expected.numbers) } };
}

// Interchange has physical lines. Fountain reimports each physical line as a separate model
// item. Normalize ONLY line boundaries and whitespace surrounding inline-note delimiters;
// keep type/order, every text character, per-character emphasis, comments and column ownership.
void collect(TextModelItem* item, QJsonArray& rows, QStringList& numbers)
{
    if (item->type() == TextModelItemType::Group) {
        auto group = static_cast<TextModelGroupItem*>(item);
        if (group->groupType() == TextGroupType::Scene) {
            require(group->number().has_value(), "scene has a number");
            numbers.append(group->number()->value);
        }
    }
    if (item->type() == TextModelItemType::Text) {
        auto text = static_cast<TextModelTextItem*>(item);
        require(!text->isCorrection(), "model contains no pagination correction");
        QString value = text->text();
        QString styles(value.size(), '0');
        for (const auto& format : text->formats()) {
            require(format.from >= 0 && format.length >= 0
                        && format.from + format.length <= value.size(), "valid emphasis range");
            const int bits = format.isBold + 2 * format.isItalic + 4 * format.isUnderline;
            for (int i = format.from; i < format.from + format.length; ++i)
                styles[i] = QChar('0' + (styles[i].digitValue() | bits));
        }
        QStringList comments;
        QJsonArray reviewRanges;
        // The editor stores one review thread as contiguous QTextBlock format
        // fragments. Normalize only identical complete review metadata across
        // adjacent fragments, retaining exact coverage and distinct comments.
        QVector<TextModelTextItem::ReviewMark> threads;
        for (const auto& mark : text->reviewMarks()) {
            require(mark.from >= 0 && mark.length >= 0
                        && mark.from + mark.length <= value.size(), "valid review range");
            if (!threads.isEmpty() && threads.last().end() == mark.from
                && threads.last().textColor == mark.textColor
                && threads.last().backgroundColor == mark.backgroundColor
                && threads.last().isDone == mark.isDone
                && threads.last().comments == mark.comments) {
                threads.last().length += mark.length;
            } else {
                threads.append(mark);
            }
        }
        for (const auto& mark : threads) {
            for (const auto& comment : mark.comments)
                comments.append(comment.text);
            reviewRanges.append(QJsonArray{ mark.from, mark.length });
        }
        const auto column = !text->isInFirstColumn().has_value() ? "none"
            : (*text->isInFirstColumn() ? "left" : "right");
        if (text->paragraphType() == TextParagraphType::InlineNote) {
            int start = 0;
            while (start < value.size() && value[start].isSpace())
                ++start;
            value = value.trimmed();
            styles = styles.mid(start, value.size());
        }
        value.replace(QChar::LineSeparator, '\n');
        int offset = 0;
        for (const auto& line : value.split('\n')) {
            rows.append(QJsonObject{ { "type", toString(text->paragraphType()) },
                { "text", line }, { "styles", styles.mid(offset, line.size()) },
                { "column", column }, { "comments", QJsonArray::fromStringList(comments) },
                { "reviewRanges", reviewRanges } });
            offset += line.size() + 1;
        }
    }
    for (int i = 0; i < item->childCount(); ++i)
        collect(item->childAt(i), rows, numbers);
}

void check(ScreenplayTextModel* model, const Expected& expected, const QString& path)
{
    QJsonArray rows;
    QStringList numbers;
    collect(model->itemForIndex({}), rows, numbers);
    const QJsonObject actual{ { "rows", rows }, { "numbers", QJsonArray::fromStringList(numbers) } };
    write(path, QJsonDocument(actual).toJson());
    if (actual != expectedJson(expected)) {
        std::fprintf(stderr, "Expected:\n%s\nActual:\n%s\n",
            QJsonDocument(expectedJson(expected)).toJson().constData(),
            QJsonDocument(actual).toJson().constData());
    }
    require(actual == expectedJson(expected), "independent screenplay semantics: " + path);
}

const Expected inlineNoteReview{
    { { "scene_heading", "INT. STUDIO - DAY" },
      { "action", "Before.", {}, "none", { "before" }, { { 0, 7 } } },
      { "inline_note", "private", {}, "none", { "about it" }, { { 0, 7 } } },
      { "inline_note", "first then Ω🙂 [[literal]] at C:\\temp",
        { { "first", 1 }, { "Ω🙂", 4 } }, "none",
        { "one", "reply", "two [[x]] * and _ and \\" }, { { 0, 5 }, { 5, 32 } } },
      { "action", "After.", {}, "none", { "after" }, { { 0, 6 } } },
      { "inline_note", "line one", {}, "none", { "line comment\nsecond */ inside review", "tail" },
        { { 0, 8 }, { 8, 9 } } },
      { "inline_note", "line two", {}, "none", { "line comment\nsecond */ inside review", "tail" },
        { { 0, 8 }, { 8, 9 } } },
      { "inline_note", "last", {}, "none", { "end" }, { { 0, 4 } } } },
    { "18A" }
};

const Expected fountain{
    { { "scene_heading", "INT. CAFÉ - DAY" },
      { "action", "A bold quiet signal & <lamp> greets Zoë.",
        { { "bold", 1 }, { "quiet", 2 }, { "signal", 4 } } },
      { "action", "A second action line." },
      { "character", "MARA" }, { "parenthetical", "under her breath" },
      { "dialogue", "First line: café & tea." }, { "dialogue", "Second line: Ω waits." },
      { "transition", "CUT TO:" }, { "scene_heading", "ROOFTOP AT DAWN" },
      { "action", "THE WIND RISES" }, { "character", "MCKAY" },
      { "parenthetical", "softly" }, { "dialogue", "Keep the *literal* star." },
      { "transition", "FADE AWAY." } }, { "12A", "27" }
};
const Expected handwrittenMarkers{
    { { "scene_heading", "INT. CALCULATOR - DAY" }, { "character", "MARA" },
      { "dialogue", "It costs 2 * 4." }, { "dialogue", "5 * 3 = 15" },
      { "dialogue", "He writes 5 * 3 = 15 and a footnote* here." },
      { "dialogue", "An _unpaired name and a lone **marker." },
      { "dialogue", "Words * italic* and _ underlined_ stay." },
      { "dialogue", "The path is C:\\temp and D:\\notes." },
      { "dialogue", "A trailing path C:\\" },
      { "dialogue", "A valid bold quiet word remains.",
        { { "bold", 1 }, { "quiet", 2 }, { "word", 4 } } },
      { "dialogue", "Escaped *literal* _name_ and \\ still." },
      { "dialogue", "A clue.", {}, "none",
        { "see file_name and other_name, 2 * 3 * 4; **literal** _literal_ at C:\\temp" },
        { { 0, 7 } } },
      { "dialogue", "A bold clue. tail.", { { "bold", 1 }, { "tail", 4 } }, "none",
        { "**comment** _text_ *stars*" }, { { 0, 12 } } },
      { "inline_note", "note 2 * 3 and _lone with bold.", { { "bold", 1 } }, "none",
        { "file_name and 2 * 3 * 4" }, { { 0, 31 } } },
      { "action", "The calculation ends." } }, { "19A" }
};
const Expected fountainNames{
    { { "scene_heading", "INT. LAB [[A]] - DAY", { { "LAB", 1 } } },
      { "character", "MARA", { { "MARA", 1 } } }, { "dialogue", "First." },
      { "character", "JO_ANN" }, { "dialogue", "Ready." },
      { "character", "MCKAY", {}, "none", { "cue note" }, { { 0, 5 } } },
      { "dialogue", "Answer." },
      { "scene_heading", "EXT. CAFÉ & <YARD> *NORTH* - NIGHT", { { "CAFÉ", 4 } } },
      { "character", "RENÉ (V.O.)", {}, "none", { "réview" }, { { 0, 11 } } },
      { "dialogue", "Still." }, { "action", "FAKE_CHARACTER" },
      { "action", "INT. FALSE - NIGHT" }, { "inline_note", "INT. HIDDEN - DAY" },
      { "inline_note", "@GHOST" },
      { "scene_heading", "INT. OFFICE - DAY", { { "OFFICE", 1 } } },
      { "character", "JO_ANN" }, { "dialogue", "Again." } }, { "20A", "20B", "20C" }
};

const QStringList namesCharacters = { "JO_ANN", "MARA", "MCKAY", "RENÉ" };
const QStringList namesLocations = { "CAFÉ & <YARD> *NORTH*", "LAB [[A]]", "OFFICE" };

QJsonObject importedNames(const AbstractImporter::Documents& documents)
{
    QStringList characters, locations;
    for (const auto& document : documents.characters)
        characters.append(document.name);
    for (const auto& document : documents.locations)
        locations.append(document.name);
    characters.sort();
    locations.sort();
    return { { "characters", QJsonArray::fromStringList(characters) },
             { "locations", QJsonArray::fromStringList(locations) } };
}

void checkRawNames(const QString& root, const QString& fixtures)
{
    ImportOptions options;
    options.filePath = fixtures + "/fountain-names.fountain";
    const auto original = read(options.filePath);
    ScreenplayFountainImporter importer;
    for (const bool characters : { true, false }) {
        for (const bool locations : { true, false }) {
            options.importCharacters = characters;
            options.importLocations = locations;
            options.importText = false; // Dictionary-only import must still parse the source.
            const auto actual = importedNames(importer.importDocuments(options));
            const auto label = QString("names-options-%1-%2").arg(characters).arg(locations);
            write(root + "/" + label + ".json", QJsonDocument(actual).toJson());
            require(actual == QJsonObject{
                { "characters", QJsonArray::fromStringList(characters ? namesCharacters : QStringList()) },
                { "locations", QJsonArray::fromStringList(locations ? namesLocations : QStringList()) } },
                "clean imported names and independent option flags: " + label);
        }
    }
    options.importText = true;
    const auto scripts = importer.importScreenplays(options);
    require(scripts.size() == 1, "same importer parses screenplay after dictionary calls");
    write(root + "/fountain-names-raw.xml", scripts.first().text.toUtf8());
    options.importCharacters = options.importLocations = true;
    require(importedNames(importer.importDocuments(options)) == QJsonObject{
        { "characters", QJsonArray::fromStringList(namesCharacters) },
        { "locations", QJsonArray::fromStringList(namesLocations) } },
        "same importer parses dictionaries after screenplay call");
    write(root + "/names-unclosed.fountain", "INT. OLD - DAY\n\n**old**[[unfinished\n");
    options.filePath = root + "/names-unclosed.fountain";
    importer.importDocuments(options); // A following import must not inherit syntax/format state.
    options.filePath = fixtures + "/fountain-names.fountain";
    require(importedNames(importer.importDocuments(options)) == QJsonObject{
        { "characters", QJsonArray::fromStringList(namesCharacters) },
        { "locations", QJsonArray::fromStringList(namesLocations) } },
        "unfinished prior source cannot contaminate names");
    require(read(options.filePath) == original, "dictionary import retains exact source bytes");
}
const Expected fdx{
    { { "scene_heading", "EXT. HARBOR - NIGHT" },
      { "action", "A bright  blue beacon & <buoy> guides René.",
        { { "bright", 1 }, { "blue", 6 } } },
      { "action", "Another line." }, { "character", "LENA" },
      { "parenthetical", "almost silent", { { "silent", 2 } } },
      { "dialogue", "Say \"go\" & wait." }, { "dialogue", "Ω answers." },
      { "transition", "DISSOLVE TO:" }, { "scene_heading", "INT. CABIN - LATER" },
      { "action", "RAIN HAMMERS THE ROOF" }, { "character", "NOAH" },
      { "parenthetical", "steady" }, { "dialogue", "It's safe now." } }, { "8B", "19" }
};
const Expected notes{
    { { "scene_heading", "INT. STUDIO - DAY" },
      { "action", "A clue.", {}, "none", { "Check café & <tag>" }, { { 0, 6 } } },
      { "inline_note", "Private note" },
      { "character", "ALICE", {}, "left" }, { "dialogue", "Left voice.", {}, "left" },
      { "character", "BOB", {}, "right" }, { "dialogue", "Right voice.", {}, "right" },
      { "action", "The voices stop." } }, { "1" }
};
const Expected authoredModel{
    { { "scene_heading", "INT. WORKSHOP - DAY" },
      { "action", "A copper & <glass> bell.", { { "copper", 1 } } },
      { "character", "ADA" }, { "parenthetical", "listening" },
      { "dialogue", "One café." }, { "dialogue", "Two Ω." },
      { "transition", "FADE TO:" } }, { "41C" }
};
const Expected emphasisReview{
    { { "scene_heading", "INT. ROOM - DAY" },
      { "action", "A bold clue.", { { "bold", 1 } } },
      { "action", "A clue.", {}, "none", { "Review only" }, { { 0, 7 } } },
      { "action", "A bold clue.", { { "bold", 1 } }, "none", { "Check clue" }, { { 0, 12 } } },
      { "action", "The red box and blue key waits.", { { "red", 1 }, { "blue", 2 } }, "none",
        { "First note", "Second café & <tag>" }, { { 0, 24 }, { 24, 6 } } },
      { "character", "MARA" },
      { "dialogue", "A quiet bold answer.", { { "quiet", 4 }, { "bold", 1 } }, "none",
        { "Check answer" }, { { 0, 20 } } },
      { "action", "Final urgent clue.", { { "urgent", 3 }, { "clue", 4 } }, "none",
        { "Final note" }, { { 0, 18 } } } }, { "1" }
};
const Expected reviewOffsets{
    { { "scene_heading", "INT. OFFICE - DAY" },
      { "action", "A *literal* clue.", {}, "none", { "Check clue" }, { { 0, 17 } } },
      { "action", "The _plain_ symbol and *star* stay.", {}, "none",
        { "Check symbols" }, { { 0, 35 } } },
      { "action", "A red box and blue key.", { { "red", 1 }, { "blue", 2 } }, "none",
        { "First note", "Second note" }, { { 0, 9 }, { 9, 13 } } },
      { "action", "A *literal* and bold clue.", { { "bold", 1 } }, "none",
        { "Mixed note" }, { { 0, 26 } } },
      { "character", "MARA" },
      { "dialogue", "A bold answer then quiet.", { { "bold", 1 }, { "quiet", 4 } }, "none",
        { "First dialogue note", "Second dialogue note" }, { { 0, 13 }, { 13, 12 } } },
      { "action", "A bold italic clue.", { { "bold italic", 3 }, { "clue", 4 } }, "none",
        { "Combined note" }, { { 0, 19 } } },
      { "action", "A bold clue.", { { "bold", 1 } }, "none",
        { "First reply", "Second reply" }, { { 0, 6 } } },
      { "action", "A bold *star* still tail.", { { "bold *star* still", 1 } }, "none",
        { "Across style", "Final style" }, { { 0, 13 }, { 13, 12 } } },
      { "action", "A lead *literal*.", {}, "none", { "Before escape" }, { { 0, 7 } } },
      { "action", "A leadbold tail.", { { "bold", 1 } }, "none", { "Before bold" }, { { 0, 6 } } },
      { "action", "A 🙂 bold *literal* Ω clue.", { { "bold", 1 } }, "none",
        { "Unicode note" }, { { 0, 27 } } } }, { "1" }
};

// Every Action below would be another paragraph type, or lose leading characters,
// if its text were written without the forcing "!". The controls stay unforced.
const Expected forcedAction{
    { { "scene_heading", "INT. STUDIO - DAY" },
      { "action", "# A private instruction" }, { "action", "@address" },
      { "action", ">A direction" }, { "action", "= A hint" },
      { "action", "~ A hummed sound" }, { "action", "INT. The camera opens - Day" },
      { "action", ".A dotted remark" }, { "action", "...and then silence." },
      { "action", "!Twice forced" }, { "action", "EXT is short for exterior." },
      { "action", "ESTHER walks in." }, { "action", "I/E reads as both." },
      { "character", "MARA" }, { "dialogue", "A spoken line." },
      { "action", "(Later that night)" }, { "action", ">Centered words<" },
      { "action", "===" }, { "action", "# A bold marker", { { "# A bold marker", 1 } } },
      { "action", "@noted address", {}, "none", { "Check address" }, { { 0, 14 } } },
      { "action", "THE WIND RISES" }, { "action", "A plain action stays plain." },
      { "action", "Into the room, a # sign and an @ sign." },
      { "action", "# The final instruction" } }, { "1" }
};
// An unforced title page key on the first line of a file would begin a title page, which
// is not script text. The exporter forces such a line wherever it stands.
const Expected forcedActionFirst{
    { { "action", "Source: a live broadcast." }, { "action", "Title: the door is ajar." },
      { "action", "Titles roll over black." }, { "scene_heading", "INT. STUDIO - DAY" },
      { "action", "Credit: none is given." }, { "action", "source: lowercase is not a key." } },
    { "1" }
};
// Physical lines of the model-authored Action paragraphs after a Fountain round trip.
const Expected forcedActionModel{
    { { "scene_heading", "INT. LAB - NIGHT" },
      { "action", "The sign reads:" }, { "action", "# 1 in sales" },
      { "action", "INT. means interior." },
      { "action", "BOB" }, { "action", "walks in." },
      { "action", "A marked line", {}, "none", { "two\nlines" }, { { 0, 13 } } },
      { "action", "# 2 in sales" },
      { "action", "BANG", {}, "none", { "SEE\nthis" }, { { 0, 4 } } },
      { "character", "ADA" }, { "dialogue", "Yes." },
      { "action", "(She waits)" }, { "action", "~ humming" } }, { "7" }
};

// Literal brackets, backslashes and slashes that Fountain would read as a review note, an
// escape or an inline note. The text below is what the model must hold, with no escapes.
const Expected literalSyntax{
    { { "scene_heading", "INT. ARCHIVE - DAY" },
      { "action", "Screen shows [[message]] then continues." },
      { "action", "Triple [[[x]]] too." }, { "action", "The folder is C:\\temp." },
      { "action", "\\\\server\\share opens." },
      { "action", "A lone [bracket], a path/name and 2 + 2 stay plain." },
      { "action", "He types /* remark */ and moves on." },
      { "action", "C:\\bold follows a backslash.", { { "bold", 1 } } },
      { "action", "Either and/or is fine.", { { "or", 2 } } },
      { "action", "The strong/ ending.", { { "strong/", 1 } } },
      { "action", "A [ opens.", {}, "none", { "Bracket note" }, { { 0, 3 } } },
      { "action", "Path C:\\ here.", {}, "none", { "Drive note" }, { { 0, 8 } } },
      { "action", "A clue.", {}, "none",
        { "Check [[tag]] in C:\\dir, 2 * 3 and file_name]" }, { { 0, 7 } } },
      { "character", "MARA" }, { "parenthetical", "reading [[aloud]]" },
      { "dialogue", "Read [[this]] from D:\\notes." },
      { "action", "Stressed first word.", { { "Stressed", 6 } } },
      { "action", "@home [[x]] waits." }, { "action", "The last line keeps a\\b." } }, { "1" }
};
// The same kinds of text typed into the application model, which no importer has shaped.
const Expected literalModel{
    { { "scene_heading", "INT. VAULT - NIGHT" },
      { "action", "Screen shows [[message]] then continues." },
      { "action", "The folder is C:\\temp." },
      { "action", "C:\\bold after it.", { { "bold", 1 } } },
      { "action", "Either and/or works.", { { "or", 2 } } },
      { "action", "Trailing ", { { "Trailing", 2 }, { " ", 1 } } },
      { "action", "An odd [ here.", {}, "none", { "a]] b\\c ]" }, { { 0, 8 } } },
      { "action", "Marked _name_ here.", {}, "none", { "2 * 3 * 4 and [[x]]" }, { { 0, 13 } } },
      { "character", "ADA" }, { "dialogue", "Say [[it]] at C:\\x." },
      { "inline_note", "Keep [[this]] */ and a\\b" } }, { "9" }
};
const Expected reservedDialogue{
    { { "scene_heading", "INT. RECORDING - DAY" },
      { "character", "MARA" }, { "dialogue", "#1 is the best." },
      { "character", "MARA" }, { "dialogue", "@home, he says." },
      { "character", "MARA" }, { "dialogue", "...and then nothing." },
      { "character", "MARA" }, { "dialogue", ".A dotted remark." },
      { "character", "MARA" }, { "dialogue", "!Bang." },
      { "character", "MARA" }, { "dialogue", ">Onward." },
      { "character", "MARA" }, { "dialogue", "= Equal." },
      { "character", "MARA" }, { "dialogue", "~ Hum." },
      { "character", "MARA" }, { "dialogue", "===" },
      { "character", "MARA" }, { "dialogue", ">Centered words<" },
      { "character", "MARA" }, { "dialogue", "(I said that.)" },
      { "character", "MARA" }, { "dialogue", "INT. is short for interior." },
      { "character", "MARA" }, { "dialogue", "EXT. is short for exterior." },
      { "character", "MARA" }, { "dialogue", "EST. is another prefix." },
      { "character", "MARA" }, { "dialogue", "INT./EXT. stays spoken." },
      { "character", "MARA" }, { "dialogue", "INT/EXT. stays spoken." },
      { "character", "MARA" }, { "dialogue", "EXT./INT. stays spoken." },
      { "character", "MARA" }, { "dialogue", "EXT/INT. stays spoken." },
      { "character", "MARA" }, { "dialogue", "I/E stays spoken." },
      { "character", "MARA" },
      { "dialogue", "#1 café 🙂.", { { "#1", 1 } }, "none",
        { "Check start" }, { { 0, 11 } } },
      { "character", "MARA" }, { "dialogue", "INT. stays spoken.", { { "INT.", 2 } } },
      { "character", "MCKAY" }, { "dialogue", "#First line." },
      { "dialogue", "@Second line." }, { "dialogue", "INT. last line." },
      { "character", "ADA" }, { "dialogue", "An ordinary answer with # and @ inside." },
      { "action", "The recording stops." } }, { "17A" }
};
// Fountain has no Shot, Cast List or Unformatted Text element. Keep all their text as
// Action, with the original native types retained in the independently authored model.
const Expected shotFdx{
    { { "scene_heading", "INT. STUDY - DAY" },
      { "shot", "CLOSE ON THE LETTER" },
      { "action", "The seal breaks." },
      { "scene_characters", "MARA, JO_ANN" },
      { "shot", "HOLD ON CAFÉ Ω", { { "CAFÉ", 1 } } },
      { "action", "A plain action stays plain." } }, { "16A" }
};
const Expected shotFdxFountain{
    { { "scene_heading", "INT. STUDY - DAY" },
      { "action", "CLOSE ON THE LETTER" },
      { "action", "The seal breaks." },
      { "action", "MARA, JO_ANN" },
      { "action", "HOLD ON CAFÉ Ω", { { "CAFÉ", 1 } } },
      { "action", "A plain action stays plain." } }, { "16A" }
};
const Expected shotModel{
    { { "scene_heading", "INT. STUDY - DAY" },
      { "shot", "CLOSE ON THE LETTER" },
      { "action", "The seal breaks." },
      { "scene_characters", "MARA, JO_ANN" },
      { "shot", "HOLD ON CAFÉ Ω", { { "CAFÉ", 1 } }, "none",
        { "Check angle" }, { { 0, 14 } } },
      { "shot", "# INSERT" }, { "shot", "INT. THE LETTER - DAY" },
      { "shot", "> TO:" }, { "shot", "~ QUIET" },
      { "unformatted_text", "Title: A literal heading" },
      { "unformatted_text", "The file is C:\\temp *literal* _name_." },
      { "character", "MARA" }, { "dialogue", "Look here." },
      { "shot", "CUT TO:" },
      { "action", "A plain action stays plain." } }, { "16B" }
};
const Expected shotModelFountain{
    { { "scene_heading", "INT. STUDY - DAY" },
      { "action", "CLOSE ON THE LETTER" },
      { "action", "The seal breaks." },
      { "action", "MARA, JO_ANN" },
      { "action", "HOLD ON CAFÉ Ω", { { "CAFÉ", 1 } }, "none",
        { "Check angle" }, { { 0, 14 } } },
      { "action", "# INSERT" }, { "action", "INT. THE LETTER - DAY" },
      { "action", "> TO:" }, { "action", "~ QUIET" },
      { "action", "Title: A literal heading" },
      { "action", "The file is C:\\temp *literal* _name_." },
      { "character", "MARA" }, { "dialogue", "Look here." },
      { "action", "CUT TO:" },
      { "action", "A plain action stays plain." } }, { "16B" }
};

void inputFailures(const QString& root, const QString& fixtures)
{
    ScreenplayFountainImporter fountainImporter;
    ScreenplayFdxImporter fdxImporter;
    ImportOptions options;
    options.filePath = root + "/missing.fdx";
    require(fountainImporter.importScreenplays(options).isEmpty(), "unreadable Fountain: no script");
    require(fdxImporter.importScreenplays(options).isEmpty(), "unreadable FDX: no script");
    int invalidIndex = 0;
    for (const auto& bytes : { QByteArray(), QByteArray("<FinalDraft><Content><Paragraph Type=\"Action\"><Text>Partial</Text></Paragraph>"),
                              QByteArray("<other><Content/></other>"), QByteArray("<FinalDraft/>") }) {
        options.filePath = root + "/invalid-" + QString::number(invalidIndex++) + ".fdx";
        write(options.filePath, bytes);
        require(fdxImporter.importScreenplays(options).isEmpty(), "invalid FDX: no partial script");
        const auto documents = fdxImporter.importDocuments(options);
        require(documents.characters.isEmpty() && documents.locations.isEmpty(),
                "invalid FDX: no partial dictionaries");
    }
    write(root + "/empty.fountain", " \r\n\t\n");
    options.filePath = root + "/empty.fountain";
    const auto empty = fountainImporter.importScreenplays(options);
    require(empty.size() == 1 && empty.first().text.isEmpty(), "empty Fountain is an empty script");
    write(root + "/empty.fdx", "<FinalDraft DocumentType=\"Script\"><Content/></FinalDraft>");
    options.filePath = root + "/empty.fdx";
    const auto emptyFdx = fdxImporter.importScreenplays(options);
    require(emptyFdx.size() == 1 && !emptyFdx.first().text.isEmpty(), "valid empty FDX has internal document XML");
    options.importText = false;
    require(fountainImporter.importScreenplays(options).isEmpty()
                && fdxImporter.importScreenplays(options).isEmpty(), "importText=false skips script");
    for (const auto& extension : { QString("fountain"), QString("fdx") }) {
        options = ImportOptions();
        options.filePath = fixtures + "/authored." + extension;
        options.importCharacters = false;
        options.importLocations = false;
        const auto documents = extension == "fdx" ? fdxImporter.importDocuments(options)
            : fountainImporter.importDocuments(options);
        require(documents.characters.isEmpty() && documents.locations.isEmpty(),
                "dictionary import options honored: " + extension);
        const auto blocked = root + "/unreadable." + extension;
        write(blocked, read(options.filePath));
        require(QFile::setPermissions(blocked, {}), "deny fixture read permissions");
        options.filePath = blocked;
        const auto unreadable = extension == "fdx" ? fdxImporter.importScreenplays(options)
            : fountainImporter.importScreenplays(options);
        require(unreadable.isEmpty(), "actual unreadable file: no script " + extension);
        require(QFile::setPermissions(blocked, QFile::ReadOwner | QFile::WriteOwner), "restore fixture permissions");
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);
    app.setApplicationName("Screenplay interchange regression");
    app.setOrganizationName("Disposable tests");
    app.setApplicationVersion("0.8.4");
    require(argc == 3 || (argc == 4 && QByteArray(argv[3]) == "reopen"),
            "evidence and fixture directory arguments, optional fresh-process reopen");
    const QString root = QString::fromLocal8Bit(argv[1]);
    const QString fixtures = QString::fromLocal8Bit(argv[2]);
    if (argc == 3) {
        checkRawNames(root, fixtures);
    }
    for (const auto font : { ":/fonts/roboto-regular", ":/fonts/materialdesignicons",
                             ":/fonts/font-awesome-brands" })
        require(QFontDatabase::addApplicationFont(font) >= 0, "embedded font");

    ManagementLayer::PluginsBuilder plugins;
    QWidget host; // Existing plugin instances retain raw view pointers: host outlives managers.
    host.resize(1200, 800);
    host.show();
    TaskBar::registerTaskBar(&host, Qt::black, Qt::white, Qt::blue);
    ManagementLayer::ProjectManager manager(nullptr, &host, plugins);
    ManagementLayer::ImportManager importer(nullptr, &host, plugins);
    QObject::connect(&importer, &ManagementLayer::ImportManager::screenplayImported,
                     &manager, &ManagementLayer::ProjectManager::storeScreenplay);
    QObject::connect(&importer, &ManagementLayer::ImportManager::characterImported,
                     &manager, &ManagementLayer::ProjectManager::storeCharacter);
    QObject::connect(&importer, &ManagementLayer::ImportManager::locationImported,
                     &manager, &ManagementLayer::ProjectManager::storeLocation);
    StructureModel* structure = nullptr;
    QObject::connect(&manager, &ManagementLayer::ProjectManager::structureModelChanged,
        [&](AbstractModel* model) { structure = qobject_cast<StructureModel*>(model); });
    const QString projectPath = root + "/round-trips.starc";
    ProjectsModelProjectItem project;
    project.setProjectType(ProjectType::Local);
    project.setPath(projectPath);
    project.setName("Interchange checks");
    auto storage = DataStorageLayer::StorageFacade::documentStorage();
    DatabaseLayer::Database::setCurrentFile(projectPath);
    manager.loadCurrentProject(&project);
    auto navigator = qobject_cast<Ui::ProjectNavigator*>(manager.navigator());
    auto proxy = qobject_cast<QSortFilterProxyModel*>(navigator->findChild<Tree*>()->model());
    const auto select = [&](const QUuid& uuid) {
        navigator->setCurrentIndex(proxy->mapFromSource(structure->indexForItem(structure->itemForUuid(uuid))));
        QApplication::processEvents();
        const auto models = manager.currentModelsForExport();
        auto model = models.isEmpty() ? nullptr
            : qobject_cast<ScreenplayTextModel*>(models.first().second);
        require(model && model->document()->uuid() == uuid, "select screenplay by UUID");
        return model;
    };
    const auto scriptUuid = [&] {
        auto story = structure->itemForIndex(proxy->mapToSource(navigator->currentIndex()));
        if (story->type() == Type::ScreenplayText)
            return story->uuid();
        for (int i = 0; i < story->childCount(); ++i)
            if (story->childAt(i)->type() == Type::ScreenplayText)
                return story->childAt(i)->uuid();
        require(false, "imported story has screenplay text");
        return QUuid();
    };
    if (argc == 4) {
        const auto identities = QJsonDocument::fromJson(read(root + "/emphasis-review-identities.json")).array();
        require(identities.size() == 4, "four retained emphasis/review screenplays");
        for (const auto& identity : identities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact serialized bytes and UUID: " + label);
            const auto& expected = label.startsWith("review-offsets") ? reviewOffsets : emphasisReview;
            check(select(uuid), expected, root + "/" + label + "-process-reopened.json");
        }
        const auto shotIdentities = QJsonDocument::fromJson(read(root + "/shot-identities.json")).array();
        require(shotIdentities.size() == 5, "five retained Shot/other paragraph screenplays");
        for (const auto& identity : shotIdentities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact serialized bytes and UUID: " + label);
            auto expected = label == "shot-fdx" ? shotFdx
                : label == "shot-fdx-fresh" ? shotFdxFountain
                : label == "model-shot" ? shotModel : shotModelFountain;
            if (label == "manager-shot-fresh")
                expected.numbers = { "1" };
            check(select(uuid), expected, root + "/" + label + "-process-reopened.json");
        }
        const auto dialogueIdentities
            = QJsonDocument::fromJson(read(root + "/reserved-dialogue-identities.json")).array();
        require(dialogueIdentities.size() == 5, "five retained reserved-start dialogue screenplays");
        for (const auto& identity : dialogueIdentities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact serialized bytes and UUID: " + label);
            auto expected = reservedDialogue;
            if (label == "manager-reserved-dialogue-fresh")
                expected.numbers = { "1" };
            check(select(uuid), expected, root + "/" + label + "-process-reopened.json");
        }
        const auto inlineIdentities
            = QJsonDocument::fromJson(read(root + "/inline-note-review-identities.json")).array();
        require(inlineIdentities.size() == 5, "five retained reviewed inline-note screenplays");
        for (const auto& identity : inlineIdentities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact inline-note XML and UUID: " + label);
            auto expected = inlineNoteReview;
            if (label.startsWith("model-") || label.startsWith("manager-")) {
                expected.rows.remove(5, 2);
                expected.rows[3].comments.removeAt(1); // Model control has two distinct threads.
            }
            if (label.startsWith("manager-"))
                expected.numbers = { "1" };
            check(select(uuid), expected, root + "/" + label + "-process-reopened.json");
        }
        const auto markerIdentities
            = QJsonDocument::fromJson(read(root + "/handwritten-markers-identities.json")).array();
        require(markerIdentities.size() == 2, "two retained handwritten-marker screenplays");
        for (const auto& identity : markerIdentities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact handwritten-marker XML and UUID: " + label);
            check(select(uuid), handwrittenMarkers, root + "/" + label + "-process-reopened.json");
        }
        const auto namesIdentities
            = QJsonDocument::fromJson(read(root + "/fountain-names-identities.json")).array();
        require(namesIdentities.size() == 2, "two retained Fountain names screenplays");
        for (const auto& identity : namesIdentities) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto label = record.value("label").toString();
            require(storage->document(uuid)->content() == read(root + "/" + label + "-serialized.xml"),
                    "fresh-process exact Fountain names screenplay XML and UUID: " + label);
            check(select(uuid), fountainNames, root + "/" + label + "-process-reopened.json");
        }
        const auto nameDocuments
            = QJsonDocument::fromJson(read(root + "/fountain-names-documents.json")).array();
        require(nameDocuments.size() == 7, "four named characters and three named locations");
        for (const auto& identity : nameDocuments) {
            const auto record = identity.toObject();
            const QUuid uuid(record.value("uuid").toString());
            const auto item = structure->itemForUuid(uuid);
            require(item && item->name() == record.value("name").toString()
                        && static_cast<int>(item->type()) == record.value("type").toInt()
                        && storage->document(uuid)->content() == read(root + "/" + record.value("file").toString()),
                    "fresh-process exact imported name, document XML, type and UUID");
        }
        manager.closeCurrentProject(projectPath);
        storage->clear();
        DatabaseLayer::Database::closeCurrentFile();
        std::puts("PASS: fresh-process emphasis/review and Shot reopening retain exact XML, UUIDs, types, text, styles, notes and anchors");
        return 0;
    }
    QJsonArray emphasisReviewIdentities;
    QJsonArray shotIdentities;
    QJsonArray dialogueIdentities;
    QJsonArray inlineIdentities;
    QJsonArray markerIdentities;
    QJsonArray namesIdentities;
    const auto persist = [&](const QUuid& uuid, const Expected& expected, const QString& label) {
        auto model = select(uuid);
        model->saveChanges(); // Explicit model -> serialized DocumentObject boundary.
        structure->saveChanges();
        const auto bytes = model->document()->content();
        require(!bytes.isEmpty(), "serialized screenplay before saving");
        write(root + "/" + label + "-serialized.xml", bytes);
        manager.saveChanges();
        DatabaseLayer::Database::commit();
        manager.closeCurrentProject(projectPath);
        storage->clear();
        DatabaseLayer::Database::closeCurrentFile();
        DatabaseLayer::Database::setCurrentFile(projectPath);
        manager.loadCurrentProject(&project);
        require(storage->document(uuid)->content() == bytes, "exact serialized bytes after database reopen");
        check(select(uuid), expected, root + "/" + label + "-reopened.json");
        if (label == "emphasis-review" || label == "emphasis-review-fresh"
            || label == "review-offsets" || label == "review-offsets-fresh")
            emphasisReviewIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
        if (label == "shot-fdx" || label == "shot-fdx-fresh" || label == "model-shot"
            || label == "model-shot-fresh" || label == "manager-shot-fresh")
            shotIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
        if (label.contains("reserved-dialogue"))
            dialogueIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
        if (label.contains("inline-note-review"))
            inlineIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
        if (label.startsWith("handwritten-markers"))
            markerIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
        if (label.startsWith("fountain-names"))
            namesIdentities.append(QJsonObject{ { "uuid", uuid.toString() }, { "label", label } });
    };
    const auto run = [&](const QString& name, const QString& input, const Expected& expected,
                         bool asFdx, bool includeReviewMarks = true,
                         const Expected* freshExpected = nullptr, bool importNames = false) {
        const auto original = read(input);
        importer.importScreenplay(input, importNames);
        const auto uuid = scriptUuid();
        check(select(uuid), expected, root + "/" + name + "-imported.json");
        persist(uuid, expected, name);
        auto model = select(uuid);
        ScreenplayExportOptions options;
        options.templateId = "world_cp";
        options.includeTitlePage = false;
        options.includeInlineNotes = true;
        options.includeReviewMarks = includeReviewMarks;
        options.showScenesNumbers = true;
        options.filePath = root + "/" + name + (asFdx ? "-export.fdx" : "-export.fountain");
        if (asFdx)
            ScreenplayFdxExporter().exportTo(model, options);
        else
            ScreenplayFountainExporter().exportTo(model, options);
        require(read(input) == original, "fixture bytes unchanged");
        importer.importScreenplay(options.filePath, importNames);
        const auto fresh = scriptUuid();
        require(fresh != uuid, "fresh imported model identity");
        auto exportedExpected = freshExpected ? *freshExpected : expected;
        if (!includeReviewMarks) {
            for (auto& row : exportedExpected.rows) {
                row.comments.clear();
                row.reviewRanges.clear();
            }
        }
        check(select(fresh), exportedExpected, root + "/" + name + "-fresh.json");
        persist(fresh, exportedExpected, name + "-fresh");
        return uuid;
    };
    run("fountain-names", fixtures + "/fountain-names.fountain", fountainNames, false, true,
        nullptr, true);
    QJsonArray namesDocuments;
    QJsonObject projectNameLists;
    for (const auto type : { Type::Characters, Type::Locations }) {
        const auto group = structure->itemForType(type);
        require(group, "import manager creates the dictionary group");
        QStringList names;
        for (int i = 0; i < group->childCount(); ++i) {
            const auto item = group->childAt(i);
            names.append(item->name());
            namesDocuments.append(QJsonObject{
                { "name", item->name() }, { "type", static_cast<int>(item->type()) },
                { "uuid", item->uuid().toString() },
                { "file", QString("fountain-name-document-%1.xml").arg(namesDocuments.size()) } });
        }
        names.sort();
        require(names == (type == Type::Characters ? namesCharacters : namesLocations),
                "actual project dictionaries match cleaned cues/headings without phantom names");
        projectNameLists.insert(type == Type::Characters ? "characters" : "locations",
                               QJsonArray::fromStringList(names));
    }
    write(root + "/fountain-names-project.json", QJsonDocument(projectNameLists).toJson());
    ImportOptions markerOptions;
    markerOptions.filePath = fixtures + "/handwritten-markers.fountain";
    const auto rawMarkers = ScreenplayFountainImporter().importScreenplays(markerOptions);
    require(rawMarkers.size() == 1, "handwritten-marker raw import");
    write(root + "/handwritten-markers-raw.xml", rawMarkers.first().text.toUtf8());
    run("handwritten-markers", markerOptions.filePath, handwrittenMarkers, false);
    ImportOptions inlineOptions;
    inlineOptions.filePath = fixtures + "/inline-note-review.fountain";
    const auto rawInline = ScreenplayFountainImporter().importScreenplays(inlineOptions);
    require(rawInline.size() == 1, "reviewed inline-note raw import");
    write(root + "/inline-note-review-raw.xml", rawInline.first().text.toUtf8());
    run("inline-note-review", fixtures + "/inline-note-review.fountain", inlineNoteReview, false);
    run("fountain", fixtures + "/authored.fountain", fountain, false);
    ImportOptions emphasisOptions;
    emphasisOptions.filePath = fixtures + "/emphasis-review.fountain";
    const auto rawEmphasis = ScreenplayFountainImporter().importScreenplays(emphasisOptions);
    require(rawEmphasis.size() == 1, "combined emphasis/review raw import");
    write(root + "/emphasis-review-raw.xml", rawEmphasis.first().text.toUtf8());
    emphasisOptions.filePath = fixtures + "/review-offsets.fountain";
    const auto rawOffsets = ScreenplayFountainImporter().importScreenplays(emphasisOptions);
    require(rawOffsets.size() == 1, "review offsets raw import");
    write(root + "/review-offsets-raw.xml", rawOffsets.first().text.toUtf8());
    emphasisOptions.filePath = fixtures + "/literal-syntax.fountain";
    const auto rawLiteral = ScreenplayFountainImporter().importScreenplays(emphasisOptions);
    require(rawLiteral.size() == 1, "literal syntax raw import");
    write(root + "/literal-syntax-raw.xml", rawLiteral.first().text.toUtf8());
    run("emphasis-review", fixtures + "/emphasis-review.fountain", emphasisReview, false);
    run("review-offsets", fixtures + "/review-offsets.fountain", reviewOffsets, false);
    run("forced-action", fixtures + "/forced-action.fountain", forcedAction, false);
    run("forced-action-first", fixtures + "/forced-action-first.fountain", forcedActionFirst, false);
    run("literal-syntax", fixtures + "/literal-syntax.fountain", literalSyntax, false);
    run("reserved-dialogue", fixtures + "/reserved-dialogue.fountain", reservedDialogue, false);
    const auto fdxUuid = run("fdx", fixtures + "/authored.fdx", fdx, true);
    run("shot-fdx", fixtures + "/shot.fdx", shotFdx, false, true, &shotFdxFountain);
    // Ordinary saves of a shadow project use this convenience route. The destination's
    // extension must select FDX, rather than replacing an FDX file with Fountain text.
    ManagementLayer::ExportManager exportManager(nullptr, &host);
    const auto managerFdx = root + "/manager-export.fdx";
    require(explicitScreenplayExport(exportManager, select(fdxUuid), managerFdx, "FDX"),
            "explicit manager FDX export succeeds");
    require(read(managerFdx).startsWith("<?xml"), "manager selects FDX for FDX destination");
    importer.importScreenplay(managerFdx, false);
    auto managerExpected = fdx;
    managerExpected.numbers = { "1", "2" }; // Ordinary information settings hide scene numbers.
    const auto managerFresh = scriptUuid();
    check(select(managerFresh), managerExpected, root + "/manager-fdx-fresh.json");
    persist(managerFresh, managerExpected, "manager-fdx");
    const auto notesUuid = run("notes-dual", fixtures + "/notes-dual.fountain", notes, false);
    // The production defaults omit inline notes. Explicitly disabling review marks leaves
    // the script text and dual-dialogue ownership intact, without the comments.
    ScreenplayExportOptions filtered;
    filtered.templateId = "world_cp";
    filtered.includeTitlePage = false;
    filtered.includeReviewMarks = false;
    filtered.filePath = root + "/notes-filtered.fountain";
    ScreenplayFountainExporter().exportTo(select(notesUuid), filtered);
    auto filteredExpected = notes;
    filteredExpected.rows.removeAt(2);
    filteredExpected.rows[1].comments.clear();
    filteredExpected.rows[1].reviewRanges.clear();
    importer.importScreenplay(filtered.filePath, false);
    check(select(scriptUuid()), filteredExpected, root + "/notes-filtered.json");
    // FDX currently exports two-column dialogue as ordinary sequential paragraphs, and does
    // not serialize review comments. Pin the loss explicitly rather than claiming equivalence.
    filtered.filePath = root + "/dual-flattened.fdx";
    filtered.includeReviewMarks = true; // FDX's ordinary default still cannot carry comments.
    ScreenplayFdxExporter().exportTo(select(notesUuid), filtered);
    for (auto& row : filteredExpected.rows)
        row.column = "none";
    importer.importScreenplay(filtered.filePath, false);
    const auto flattened = scriptUuid();
    check(select(flattened), filteredExpected, root + "/dual-flattened.json");
    persist(flattened, filteredExpected, "dual-flattened");

    // The dialog's ordinary import option discards supplied scene numbers; the manager's
    // convenience import above preserves them. Both format implementations must honor it.
    for (const bool asFdx : { false, true }) {
        ImportOptions options;
        options.filePath = fixtures + (asFdx ? "/authored.fdx" : "/authored.fountain");
        const auto scripts = asFdx ? ScreenplayFdxImporter().importScreenplays(options)
            : ScreenplayFountainImporter().importScreenplays(options);
        require(scripts.size() == 1, "renumbered import produces one screenplay");
        manager.storeScreenplay("Renumbered", {}, {}, {}, scripts.first().text);
        auto expected = asFdx ? fdx : fountain;
        expected.numbers = { "1", "2" };
        const auto uuid = scriptUuid();
        check(select(uuid), expected, root + (asFdx ? "/fdx-renumbered.json" : "/fountain-renumbered.json"));
        ScreenplayExportOptions unnumbered;
        unnumbered.templateId = "world_cp";
        unnumbered.includeTitlePage = false;
        unnumbered.showScenesNumbers = false;
        unnumbered.filePath = root + (asFdx ? "/unnumbered.fdx" : "/unnumbered.fountain");
        if (asFdx)
            ScreenplayFdxExporter().exportTo(select(uuid), unnumbered);
        else
            ScreenplayFountainExporter().exportTo(select(uuid), unnumbered);
        importer.importScreenplay(unnumbered.filePath, false);
        check(select(scriptUuid()), expected, root + (asFdx ? "/fdx-unnumbered-fresh.json" : "/fountain-unnumbered-fresh.json"));
    }

    // Construct independently in the application model: no importer or exported seed.
    const auto author = [&](const QString& name, const QVector<Row>& rows, const QString& number) {
        auto story = structure->itemForIndex(structure->addDocument(Type::Screenplay, name));
        QUuid uuid;
        for (int i = 0; i < story->childCount(); ++i)
            if (story->childAt(i)->type() == Type::ScreenplayText)
                uuid = story->childAt(i)->uuid();
        auto model = select(uuid);
        const auto rootItem = model->itemForIndex({});
        auto scene = new ScreenplayTextModelSceneItem(model);
        scene->setGroupType(TextGroupType::Scene);
        model->appendItem(scene);
        while (rootItem->childAt(0) != scene)
            model->removeItem(rootItem->childAt(0));
        for (const auto& row : rows) {
            auto item = new ScreenplayTextModelTextItem(model);
            item->setParagraphType(textParagraphTypeFromString(row.type));
            item->setText(row.text);
            QVector<QTextLayout::FormatRange> formats;
            for (const auto& span : row.emphasis) {
                QTextCharFormat format;
                if (span.bits & 1)
                    format.setFontWeight(QFont::Bold);
                format.setFontItalic(span.bits & 2);
                format.setFontUnderline(span.bits & 4);
                formats.append({ int(row.text.indexOf(span.text)), int(span.text.size()), format });
            }
            item->setFormats(formats);
            QVector<TextModelTextItem::ReviewMark> marks;
            for (int i = 0; i < row.reviewRanges.size(); ++i) {
                TextModelTextItem::ReviewMark mark;
                mark.from = row.reviewRanges[i].first;
                mark.length = row.reviewRanges[i].second;
                mark.backgroundColor = QColor("#FFD302");
                mark.comments = { { "model author", {}, "2026-10-13T00:00:00", row.comments[i] } };
                marks.append(mark);
            }
            if (!marks.isEmpty())
                item->setReviewMarks(marks);
            model->appendItem(item, scene);
        }
        scene->setCustomNumber(number, false);
        return uuid;
    };
    // One model dialogue paragraph with an embedded line break; Fountain splits it, FDX keeps it.
    auto modelRows = authoredModel.rows;
    modelRows[4].text += QChar::LineSeparator + modelRows[5].text;
    modelRows.removeAt(5);
    const auto uuid = author("Model authored", modelRows, "41C");
    check(select(uuid), authoredModel, root + "/model-authored.json");
    persist(uuid, authoredModel, "model");
    for (const bool asFdx : { false, true }) {
        ScreenplayExportOptions options;
        options.templateId = "world_cp";
        options.includeTitlePage = false;
        options.filePath = root + (asFdx ? "/model-export.fdx" : "/model-export.fountain");
        if (asFdx)
            ScreenplayFdxExporter().exportTo(select(uuid), options);
        else
            ScreenplayFountainExporter().exportTo(select(uuid), options);
        importer.importScreenplay(options.filePath, false);
        const auto fresh = scriptUuid();
        check(select(fresh), authoredModel, root + (asFdx ? "/model-fdx-fresh.json" : "/model-fountain-fresh.json"));
        persist(fresh, authoredModel, asFdx ? "model-fdx" : "model-fountain");
    }

    // Action paragraphs with embedded line breaks: every physical line is its own Fountain
    // paragraph, so each one needs its own protection. Notes carry line breaks of their own.
    const QChar lineBreak = QChar::LineSeparator;
    const auto forcedUuid = author("Model forced actions", {
        { "scene_heading", "INT. LAB - NIGHT" },
        { "action", QString("The sign reads:") + lineBreak + "# 1 in sales" + lineBreak
                        + "INT. means interior." },
        { "action", QString("BOB") + lineBreak + "walks in." },
        { "action", QString("A marked line") + lineBreak + "# 2 in sales", {}, "none",
          { "two\nlines" }, { { 0, 13 } } },
        { "action", "BANG", {}, "none", { "SEE\nthis" }, { { 0, 4 } } },
        { "character", "ADA" }, { "dialogue", "Yes." },
        { "action", QString("(She waits)") + lineBreak + "~ humming" } }, "7");
    // collect() reports an item's review on each of its physical lines.
    auto forcedOriginal = forcedActionModel;
    forcedOriginal.rows[7].comments = forcedOriginal.rows[6].comments;
    forcedOriginal.rows[7].reviewRanges = forcedOriginal.rows[6].reviewRanges;
    check(select(forcedUuid), forcedOriginal, root + "/model-forced-authored.json");
    persist(forcedUuid, forcedOriginal, "model-forced");
    {
        ScreenplayExportOptions options;
        options.templateId = "world_cp";
        options.includeTitlePage = false;
        options.filePath = root + "/model-forced-export.fountain";
        ScreenplayFountainExporter().exportTo(select(forcedUuid), options);
        importer.importScreenplay(options.filePath, false);
        const auto fresh = scriptUuid();
        check(select(fresh), forcedActionModel, root + "/model-forced-fresh.json");
        persist(fresh, forcedActionModel, "model-forced-fresh");
    }
    // Literal syntax typed into the model goes out through the exporter and through the
    // manager's source Save route, and comes back as the same text, styles and notes.
    // One paragraph ends in spaces of two different styles, the last of them bold only.
    // Fountain keeps one of the spaces; every other row comes back exactly as authored.
    auto literalAuthored = literalModel;
    literalAuthored.rows[5] = { "action", QString("Trailing  \t"),
                                { { "Trailing  ", 2 }, { "\t", 1 } } };
    const auto literalUuid = author("Model literal syntax", literalAuthored.rows, "9");
    check(select(literalUuid), literalAuthored, root + "/model-literal-authored.json");
    persist(literalUuid, literalAuthored, "model-literal");
    {
        ScreenplayExportOptions options;
        options.templateId = "world_cp";
        options.includeTitlePage = false;
        options.includeInlineNotes = true;
        options.filePath = root + "/model-literal-export.fountain";
        ScreenplayFountainExporter().exportTo(select(literalUuid), options);
        importer.importScreenplay(options.filePath, false);
        const auto fresh = scriptUuid();
        check(select(fresh), literalModel, root + "/model-literal-fresh.json");
        persist(fresh, literalModel, "model-literal-fresh");
        const auto managerFountain = root + "/manager-literal.fountain";
        require(explicitScreenplayExport(exportManager, select(literalUuid), managerFountain,
                                         "Fountain"),
                "explicit manager export of literal syntax");
        importer.importScreenplay(managerFountain, false);
        auto managerLiteral = literalModel;
        managerLiteral.numbers = { "1" }; // Ordinary information settings hide scene numbers.
        check(select(scriptUuid()), managerLiteral, root + "/manager-literal-fresh.json");
    }
    // A multiline Shot needs Action forcing on every physical line, just like Action itself.
    auto shotRows = shotModel.rows;
    for (int i = 6; i <= 8; ++i)
        shotRows[5].text += QChar::LineSeparator + shotRows[i].text;
    shotRows.remove(6, 3);
    const auto shotUuid = author("Model Shot paragraphs", shotRows, "16B");
    check(select(shotUuid), shotModel, root + "/model-shot-authored.json");
    persist(shotUuid, shotModel, "model-shot");
    ScreenplayExportOptions shotOptions;
    shotOptions.templateId = "world_cp";
    shotOptions.includeTitlePage = false;
    shotOptions.filePath = root + "/model-shot-export.fountain";
    require(ScreenplayFountainExporter().exportTo(select(shotUuid), shotOptions),
            "direct Fountain export of Shot/other content paragraphs");
    importer.importScreenplay(shotOptions.filePath, false);
    auto freshShot = scriptUuid();
    check(select(freshShot), shotModelFountain, root + "/model-shot-fresh.json");
    persist(freshShot, shotModelFountain, "model-shot-fresh");
    const auto managerShot = root + "/manager-shot.fountain";
    require(explicitScreenplayExport(exportManager, select(shotUuid), managerShot, "Fountain"),
            "explicit manager export of Shot/other content paragraphs");
    importer.importScreenplay(managerShot, false);
    auto managerShotExpected = shotModelFountain;
    managerShotExpected.numbers = { "1" };
    freshShot = scriptUuid();
    check(select(freshShot), managerShotExpected, root + "/manager-shot-fresh.json");
    persist(freshShot, managerShotExpected, "manager-shot-fresh");
    auto originalShot = select(shotUuid);
    originalShot->saveChanges();
    require(originalShot->document()->content() == read(root + "/model-shot-serialized.xml"),
            "export retains exact original native types, text, styles and reviews");
    // Consecutive physical lines within one model speech must each keep their type and text.
    auto dialogueRows = reservedDialogue.rows;
    const int multiline = dialogueRows.size() - 6;
    dialogueRows[multiline].text += QChar::LineSeparator + dialogueRows[multiline + 1].text
        + QChar::LineSeparator + dialogueRows[multiline + 2].text;
    dialogueRows.remove(multiline + 1, 2);
    const auto dialogueUuid = author("Model reserved-start dialogue", dialogueRows, "17A");
    check(select(dialogueUuid), reservedDialogue, root + "/model-reserved-dialogue-authored.json");
    persist(dialogueUuid, reservedDialogue, "model-reserved-dialogue");
    ScreenplayExportOptions dialogueOptions;
    dialogueOptions.templateId = "world_cp";
    dialogueOptions.includeTitlePage = false;
    dialogueOptions.filePath = root + "/model-reserved-dialogue-export.fountain";
    require(ScreenplayFountainExporter().exportTo(select(dialogueUuid), dialogueOptions),
            "export model reserved-start dialogue");
    importer.importScreenplay(dialogueOptions.filePath, false);
    auto freshDialogue = scriptUuid();
    check(select(freshDialogue), reservedDialogue, root + "/model-reserved-dialogue-fresh.json");
    persist(freshDialogue, reservedDialogue, "model-reserved-dialogue-fresh");
    const auto managerDialogue = root + "/manager-reserved-dialogue.fountain";
    require(explicitScreenplayExport(exportManager, select(dialogueUuid), managerDialogue, "Fountain"),
            "explicit manager export retains reserved-start dialogue");
    importer.importScreenplay(managerDialogue, false);
    freshDialogue = scriptUuid();
    auto managerDialogueExpected = reservedDialogue;
    managerDialogueExpected.numbers = { "1" };
    check(select(freshDialogue), managerDialogueExpected, root + "/manager-reserved-dialogue-fresh.json");
    persist(freshDialogue, managerDialogueExpected, "manager-reserved-dialogue-fresh");
    auto originalDialogue = select(dialogueUuid);
    originalDialogue->saveChanges();
    require(originalDialogue->document()->content()
                == read(root + "/model-reserved-dialogue-serialized.xml"),
            "Fountain escaping leaves the original dialogue XML and annotations intact");
    auto modelInline = inlineNoteReview;
    modelInline.rows.remove(5, 2);
    modelInline.rows[3].comments.removeAt(1);
    const auto inlineUuid = author("Model reviewed inline notes", modelInline.rows, "18A");
    check(select(inlineUuid), modelInline, root + "/model-inline-note-review-authored.json");
    persist(inlineUuid, modelInline, "model-inline-note-review");
    ScreenplayExportOptions inlineExport;
    inlineExport.templateId = "world_cp";
    inlineExport.includeTitlePage = false;
    inlineExport.includeInlineNotes = true;
    inlineExport.showScenesNumbers = true;
    inlineExport.filePath = root + "/model-inline-note-review-export.fountain";
    require(ScreenplayFountainExporter().exportTo(select(inlineUuid), inlineExport),
            "direct export of independently authored reviewed inline notes");
    importer.importScreenplay(inlineExport.filePath, false);
    auto freshInline = scriptUuid();
    check(select(freshInline), modelInline, root + "/model-inline-note-review-fresh.json");
    persist(freshInline, modelInline, "model-inline-note-review-fresh");
    const auto managerInline = root + "/manager-inline-note-review.fountain";
    require(explicitScreenplayExport(exportManager, select(inlineUuid), managerInline, "Fountain"),
            "explicit Export dialog preserves inline-note reviews");
    importer.importScreenplay(managerInline, false);
    auto managerInlineExpected = modelInline;
    managerInlineExpected.numbers = { "1" };
    freshInline = scriptUuid();
    check(select(freshInline), managerInlineExpected, root + "/manager-inline-note-review-fresh.json");
    persist(freshInline, managerInlineExpected, "manager-inline-note-review-fresh");
    auto originalInline = select(inlineUuid);
    originalInline->saveChanges();
    require(originalInline->document()->content() == read(root + "/model-inline-note-review-serialized.xml"),
            "export preserves original native inline-note text and annotations");
    inlineExport.includeReviewMarks = false;
    inlineExport.filePath = root + "/inline-note-review-filtered.fountain";
    require(ScreenplayFountainExporter().exportTo(originalInline, inlineExport),
            "review filtering succeeds for inline notes");
    auto filteredInline = modelInline;
    for (auto& row : filteredInline.rows) {
        row.comments.clear();
        row.reviewRanges.clear();
    }
    importer.importScreenplay(inlineExport.filePath, false);
    check(select(scriptUuid()), filteredInline, root + "/inline-note-review-filtered.json");
    inputFailures(root, fixtures);
    write(root + "/emphasis-review-identities.json", QJsonDocument(emphasisReviewIdentities).toJson());
    write(root + "/shot-identities.json", QJsonDocument(shotIdentities).toJson());
    write(root + "/reserved-dialogue-identities.json", QJsonDocument(dialogueIdentities).toJson());
    write(root + "/inline-note-review-identities.json", QJsonDocument(inlineIdentities).toJson());
    write(root + "/handwritten-markers-identities.json", QJsonDocument(markerIdentities).toJson());
    write(root + "/fountain-names-identities.json", QJsonDocument(namesIdentities).toJson());
    manager.saveChanges();
    DatabaseLayer::Database::commit();
    for (const auto& identity : namesDocuments) {
        const auto record = identity.toObject();
        const QUuid uuid(record.value("uuid").toString());
        const auto item = structure->itemForUuid(uuid);
        require(item && item->name() == record.value("name").toString(),
                "subsequent interchange cases retain imported names and identities");
        write(root + "/" + record.value("file").toString(), storage->document(uuid)->content());
    }
    write(root + "/fountain-names-documents.json", QJsonDocument(namesDocuments).toJson());
    manager.closeCurrentProject(projectPath);
    storage->clear();
    DatabaseLayer::Database::closeCurrentFile();
    std::puts("PASS: Fountain/FDX independent fixtures, model construction, serialization/save/reopen and fresh imports");
}
