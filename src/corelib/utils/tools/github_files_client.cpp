#include "github_files_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>


namespace {
const QLatin1String kDefaultApiUrl("https://api.github.com");
const QByteArray kJsonMediaType = "application/vnd.github+json";
const QByteArray kRawMediaType = "application/vnd.github.raw+json";
constexpr int kTransferTimeoutMs = 60000;

QString encoded(const QString& _text)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(_text));
}
} // namespace


class GitHubFilesClient::Implementation
{
public:
    explicit Implementation(GitHubFilesClient* _q);

    /**
     * @brief Build the request to the repository API
     */
    QNetworkRequest request(const QString& _path, const QByteArray& _mediaType) const;

    /**
     * @brief Determine the error of a finished request
     */
    static Error error(QNetworkReply* _reply, const QByteArray& _body);

    /**
     * @brief Call the callback with the error and the body when the request is finished
     */
    void handle(QNetworkReply* _reply,
                std::function<void(const Error&, const QByteArray&)> _callback);


    GitHubFilesClient* q = nullptr;

    QString apiUrl = kDefaultApiUrl;
    QString repository;
    QString token;
    QNetworkAccessManager network;
};

GitHubFilesClient::Implementation::Implementation(GitHubFilesClient* _q)
    : q(_q)
{
    if (const auto url = qEnvironmentVariable("STARC_GITHUB_API_URL"); !url.isEmpty()) {
        apiUrl = url;
    }
}

QNetworkRequest GitHubFilesClient::Implementation::request(const QString& _path,
                                                           const QByteArray& _mediaType) const
{
    QNetworkRequest request(QUrl(apiUrl + QLatin1String("/repos/") + repository + _path));
    request.setRawHeader("Accept", _mediaType);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("starc-github-sync"));
    request.setTransferTimeout(kTransferTimeoutMs);
    return request;
}

GitHubFilesClient::Error GitHubFilesClient::Implementation::error(QNetworkReply* _reply,
                                                                  const QByteArray& _body)
{
    const auto status = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (_reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        return {};
    }

    //
    // There is no answer from the server at all
    //
    if (status == 0) {
        return { Error::Type::Network,
                 GitHubFilesClient::tr("Can't connect to GitHub: %1").arg(_reply->errorString()) };
    }

    const auto message = QJsonDocument::fromJson(_body).object()["message"].toString();
    switch (status) {
    case 401: {
        return { Error::Type::Unauthorized,
                 GitHubFilesClient::tr("GitHub rejected the access token. Check that it is "
                                       "entered correctly and hasn't expired.") };
    }

    case 403: {
        return { Error::Type::Unauthorized,
                 GitHubFilesClient::tr("GitHub refused the request: %1").arg(message) };
    }

    case 404: {
        return { Error::Type::NotFound,
                 GitHubFilesClient::tr("GitHub can't find the repository or the file.") };
    }

    case 409: {
        return { Error::Type::Conflict, message };
    }

    case 422: {
        //
        // That's how GitHub answers when a file is being created, but it already exists
        //
        if (message.contains(QLatin1String("sha"))) {
            return { Error::Type::Conflict, message };
        }
        break;
    }

    default: {
        break;
    }
    }

    return {
        Error::Type::Other,
        GitHubFilesClient::tr("GitHub answered with the error %1: %2").arg(status).arg(message)
    };
}

void GitHubFilesClient::Implementation::handle(
    QNetworkReply* _reply, std::function<void(const Error&, const QByteArray&)> _callback)
{
    QObject::connect(_reply, &QNetworkReply::finished, q, [_reply, _callback] {
        _reply->deleteLater();
        const auto body = _reply->readAll();
        _callback(error(_reply, body), body);
    });
}


// ****


GitHubFilesClient::GitHubFilesClient(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
}

GitHubFilesClient::~GitHubFilesClient() = default;

void GitHubFilesClient::setRepository(const QString& _repository)
{
    //
    // Accept the address of the repository page as well as the short form
    //
    auto repository = _repository.trimmed();
    for (const auto& prefix :
         { QLatin1String("https://github.com/"), QLatin1String("http://github.com/"),
           QLatin1String("github.com/") }) {
        if (repository.startsWith(prefix, Qt::CaseInsensitive)) {
            repository.remove(0, prefix.size());
            break;
        }
    }
    while (repository.endsWith('/')) {
        repository.chop(1);
    }
    if (repository.endsWith(QLatin1String(".git"))) {
        repository.chop(4);
    }

    d->repository = repository;
}

void GitHubFilesClient::setToken(const QString& _token)
{
    d->token = _token.trimmed();
}

void GitHubFilesClient::setApiUrl(const QString& _url)
{
    d->apiUrl = _url;
}

void GitHubFilesClient::loadRepositoryInfo(
    std::function<void(const Error&, const RepositoryInfo&)> _callback)
{
    d->handle(d->network.get(d->request({}, kJsonMediaType)),
              [_callback](const Error& _error, const QByteArray& _body) {
                  RepositoryInfo info;
                  if (!_error.isValid()) {
                      const auto json = QJsonDocument::fromJson(_body).object();
                      info.isPrivate = json["private"].toBool();
                      info.canPush = json["permissions"].toObject()["push"].toBool();
                  }
                  _callback(_error, info);
              });
}

void GitHubFilesClient::loadFiles(
    std::function<void(const Error&, const QVector<FileInfo>&)> _callback)
{
    d->handle(d->network.get(d->request(QStringLiteral("/contents/"), kJsonMediaType)),
              [_callback](const Error& _error, const QByteArray& _body) {
                  //
                  // GitHub answers that nothing is found for a repository without any files,
                  // availability of the repository itself is checked by loadRepositoryInfo()
                  //
                  if (_error.type == Error::Type::NotFound) {
                      _callback({}, {});
                      return;
                  }

                  QVector<FileInfo> files;
                  if (!_error.isValid()) {
                      const auto json = QJsonDocument::fromJson(_body).array();
                      for (const auto& fileJsonValue : json) {
                          const auto fileJson = fileJsonValue.toObject();
                          if (fileJson["type"].toString() != QLatin1String("file")) {
                              continue;
                          }

                          files.append({ fileJson["name"].toString(),
                                         fileJson["sha"].toString().toLatin1(),
                                         fileJson["size"].toInteger() });
                      }
                  }
                  _callback(_error, files);
              });
}

void GitHubFilesClient::download(const QString& _name,
                                 std::function<void(const Error&, const QByteArray&)> _callback)
{
    d->handle(
        d->network.get(d->request(QStringLiteral("/contents/") + encoded(_name), kRawMediaType)),
        [_callback](const Error& _error, const QByteArray& _body) {
            _callback(_error, _error.isValid() ? QByteArray() : _body);
        });
}

void GitHubFilesClient::upload(const QString& _name, const QByteArray& _content,
                               const QByteArray& _replacedSha, const QString& _message,
                               std::function<void(const Error&, const QByteArray&)> _callback)
{
    QJsonObject json;
    json["message"] = _message;
    json["content"] = QString::fromLatin1(_content.toBase64());
    if (!_replacedSha.isEmpty()) {
        json["sha"] = QString::fromLatin1(_replacedSha);
    }

    auto request = d->request(QStringLiteral("/contents/") + encoded(_name), kJsonMediaType);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    d->handle(d->network.put(request, QJsonDocument(json).toJson(QJsonDocument::Compact)),
              [_callback](const Error& _error, const QByteArray& _body) {
                  QByteArray sha;
                  if (!_error.isValid()) {
                      sha = QJsonDocument::fromJson(_body)
                                .object()["content"]
                                .toObject()["sha"]
                                .toString()
                                .toLatin1();
                  }
                  _callback(_error, sha);
              });
}

void GitHubFilesClient::loadLastChange(
    const QString& _name, std::function<void(const Error&, const ChangeInfo&)> _callback)
{
    d->handle(d->network.get(d->request(
                  QStringLiteral("/commits?per_page=1&path=") + encoded(_name), kJsonMediaType)),
              [_callback](const Error& _error, const QByteArray& _body) {
                  ChangeInfo info;
                  if (!_error.isValid()) {
                      const auto commit = QJsonDocument::fromJson(_body)
                                              .array()
                                              .first()
                                              .toObject()["commit"]
                                              .toObject();
                      info.message = commit["message"].toString();
                      info.dateTime = QDateTime::fromString(
                          commit["committer"].toObject()["date"].toString(), Qt::ISODate);
                  }
                  _callback(_error, info);
              });
}

void GitHubFilesClient::abort()
{
    const auto replies = d->network.findChildren<QNetworkReply*>();
    for (auto reply : replies) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}
