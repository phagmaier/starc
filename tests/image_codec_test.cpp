#include <business_layer/import/import_options.h>
#include <business_layer/import/screenplay/screenplay_fountain_importer.h>
#include <data_layer/database.h>
#include <data_layer/storage/document_change_storage.h>
#include <data_layer/storage/document_image_storage.h>
#include <data_layer/storage/document_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <utils/helpers/image_helper.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>

#include <InputDCTDecodeStream.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
using DatabaseLayer::Database;
using DataStorageLayer::StorageFacade;

void require(bool _ok, const char* _label)
{
    std::printf("%s: %s\n", _ok ? "ok" : "FAIL", _label);
    if (!_ok) {
        std::exit(1);
    }
}

void write(const QString& _path, const QByteArray& _bytes)
{
    QFile file(_path);
    require(file.open(QIODevice::WriteOnly) && file.write(_bytes) == _bytes.size(),
            "retain authored image/script byte oracle");
}

QByteArray read(const QString& _path)
{
    QFile file(_path);
    require(file.open(QIODevice::ReadOnly), "read retained byte oracle");
    return file.readAll();
}

class ByteReader final : public IByteReader
{
public:
    explicit ByteReader(const QByteArray& _bytes)
        : m_bytes(_bytes)
    {
    }

    IOBasicTypes::LongBufferSizeType Read(IOBasicTypes::Byte* _buffer,
                                          IOBasicTypes::LongBufferSizeType _size) override
    {
        const auto count = std::min(
            _size, static_cast<IOBasicTypes::LongBufferSizeType>(m_bytes.size() - m_position));
        std::memcpy(_buffer, m_bytes.constData() + m_position, count);
        m_position += count;
        return count;
    }

    bool NotEnded() override
    {
        return m_position < m_bytes.size();
    }

private:
    QByteArray m_bytes;
    qsizetype m_position = 0;
};

void checkPixels(const QImage& _image, bool _alpha)
{
    require(_image.size() == QSize(32, 24), "decoded image keeps authored dimensions");
    bool correct = true;
    for (int y = 0; y < _image.height(); ++y) {
        for (int x = 0; x < _image.width(); ++x) {
            const auto color = _image.pixelColor(x, y);
            correct &= std::abs(color.red() - (_alpha ? 0 : 12)) <= (_alpha ? 0 : 4)
                && std::abs(color.green() - (_alpha ? 0 : 34)) <= (_alpha ? 0 : 4)
                && std::abs(color.blue() - (_alpha ? 255 : 56)) <= (_alpha ? 0 : 4)
                && color.alpha() == (_alpha ? 128 : 255);
        }
    }
    require(correct, "decoded pixels retain authored color and alpha");
}

void checkPdfDecoder(const QByteArray& _jpeg)
{
    // Exercise the production PDF reader's private DCT decoder as well as Qt.
    InputDCTDecodeStream decoder(new ByteReader(_jpeg));
    QByteArray decoded;
    IOBasicTypes::Byte chunk[127];
    while (decoder.NotEnded()) {
        const auto count = decoder.Read(chunk, sizeof(chunk));
        require(count > 0 && decoded.size() + count <= 32 * 24 * 3,
                "bundled PDF JPEG decoder makes bounded progress");
        decoded.append(reinterpret_cast<const char*>(chunk), count);
    }
    require(decoded.size() == 32 * 24 * 3, "PDF decoder retains every RGB sample");
    bool correct = true;
    const int expected[] = { 12, 34, 56 };
    for (qsizetype index = 0; index < decoded.size(); ++index) {
        correct &= std::abs(static_cast<unsigned char>(decoded[index]) - expected[index % 3]) <= 4;
    }
    require(correct, "bundled PDF JPEG decoder retains authored colors");
}

void create(const QString& _root)
{
    require(ImageHelper::bytesFromImage(QPixmap()).isEmpty(), "null image stays empty");
    write(_root + "/source.fountain",
          "INT. ARCHIVE - DAY\n\nThe folder is C:\\\\temp. **Keep** this.[[review]]\n");
    BusinessLayer::ImportOptions options;
    options.filePath = _root + "/source.fountain";
    const auto scripts = BusinessLayer::ScreenplayFountainImporter().importScreenplays(options);
    require(scripts.size() == 1, "import disposable annotated screenplay");
    const auto scriptXml = scripts.first().text.toUtf8();
    write(_root + "/script.xml", scriptXml);

    Database::setCurrentFile(_root + "/story.starc");
    require(!Database::hasError(), "create disposable native database");
    auto documents = StorageFacade::documentStorage();
    auto script = documents->createDocument(QUuid::createUuid(),
                                            Domain::DocumentObjectType::ScreenplayText);
    script->setContent(scriptXml);
    DataStorageLayer::DocumentImageStorage images;
    QJsonArray identities;
    require(Database::transaction() && documents->saveDocument(script), "save screenplay");
    identities.append(QJsonObject{ { "uuid", script->uuid().toString() },
                                   { "id", script->id().value() },
                                   { "file", "script.xml" } });
    for (const bool alpha : { false, true }) {
        QImage image(32, 24, alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
        image.fill(alpha ? qRgba(0, 0, 255, 128) : qRgb(12, 34, 56));
        const auto pixmap = QPixmap::fromImage(image);
        const auto bytes = ImageHelper::bytesFromImage(pixmap);
        require(bytes.startsWith(alpha ? QByteArray::fromHex("89504e470d0a1a0a")
                                       : QByteArray::fromHex("ffd8")),
                "opaque images keep JPEG; alpha images keep PNG");
        checkPixels(QImage::fromData(bytes), alpha);
        if (!alpha) {
            checkPdfDecoder(bytes);
        }
        const auto uuid = images.save(pixmap);
        require(!uuid.isNull(), "production pixmap image-storage path succeeds");
        images.save(uuid, pixmap);
        const auto stored = documents->document(uuid);
        require(stored && stored->content() == bytes, "image storage retains exact encoded bytes");
        images.saveChanges();
        const auto name = alpha ? "alpha.png" : "opaque.jpg";
        write(_root + '/' + name, bytes);
        identities.append(QJsonObject{
            { "uuid", uuid.toString() }, { "id", stored->id().value() }, { "file", name } });
    }
    auto changes = StorageFacade::documentChangeStorage();
    changes->appendDocumentChange(script->uuid(), QUuid::createUuid(), "authored-undo",
                                  "authored-redo", "", "Writer");
    changes->store();
    require(Database::commit() && !Database::hasError(), "commit images, screenplay and history");
    write(_root + "/identities.json", QJsonDocument(identities).toJson());
    StorageFacade::clearStorages();
    Database::closeCurrentFile();
}

void reopen(const QString& _root)
{
    Database::setCurrentFile(_root + "/story.starc");
    require(!Database::hasError(), "fresh process opens native story");
    const auto identities = QJsonDocument::fromJson(read(_root + "/identities.json")).array();
    require(identities.size() == 3, "script and both image identities recorded");
    DataStorageLayer::DocumentImageStorage images;
    for (const auto& value : identities) {
        const auto row = value.toObject();
        const QUuid uuid(row["uuid"].toString());
        const auto document = StorageFacade::documentStorage()->document(uuid);
        const auto name = row["file"].toString();
        require(document && document->uuid() == uuid && document->id().value() == row["id"].toInt()
                    && document->content() == read(_root + '/' + name),
                "fresh native process retains exact bytes, UUID and database identity");
        if (name != "script.xml") {
            checkPixels(images.load(uuid).toImage(), name == "alpha.png");
        }
    }
    StorageFacade::clearStorages();
    Database::closeCurrentFile();
}
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationVersion("0.8.4");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    require(argc == 3, "disposable root and create/reopen mode");
    const QString root = QString::fromLocal8Bit(argv[1]);
    require(QDir().mkpath(root), "create disposable image fixture directory");
    if (QByteArray(argv[2]) == "create") {
        create(root);
    } else {
        require(QByteArray(argv[2]) == "reopen", "fresh reopen mode");
        reopen(root);
    }
    std::puts("PASS: image codecs and native preservation");
    return 0;
}
