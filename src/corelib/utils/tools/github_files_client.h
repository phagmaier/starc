#pragma once

#include <QDateTime>
#include <QObject>
#include <QVector>

#include <corelib_global.h>

#include <functional>


/**
 * @brief Client for keeping files in the root of a GitHub repository
 */
class CORE_LIBRARY_EXPORT GitHubFilesClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Result of a request
     */
    struct CORE_LIBRARY_EXPORT Error {
        enum class Type {
            None,
            //
            // GitHub can't be reached
            //
            Network,
            //
            // The access token was rejected, or it doesn't allow the request
            //
            Unauthorized,
            NotFound,
            //
            // The file in the repository isn't the one which should be replaced
            //
            Conflict,
            Other,
        };

        bool isValid() const
        {
            return type != Type::None;
        }

        Type type = Type::None;
        QString text;
    };

    /**
     * @brief Information about the repository
     */
    struct CORE_LIBRARY_EXPORT RepositoryInfo {
        bool isPrivate = false;
        bool canPush = false;
    };

    /**
     * @brief File in the repository
     */
    struct CORE_LIBRARY_EXPORT FileInfo {
        QString name;
        QByteArray sha;
        qint64 size = 0;
    };

    /**
     * @brief Last change of a file
     */
    struct CORE_LIBRARY_EXPORT ChangeInfo {
        QString message;
        QDateTime dateTime;
    };

public:
    explicit GitHubFilesClient(QObject* _parent = nullptr);
    ~GitHubFilesClient() override;

    /**
     * @brief Repository in the form "owner/name"
     */
    void setRepository(const QString& _repository);

    /**
     * @brief Personal access token
     */
    // Returns whether it changed; a changed credential cancels outstanding requests.
    bool setToken(const QString& _token);

    /**
     * @brief Address of the API, the STARC_GITHUB_API_URL environment variable overrides
     *        the default one
     */
    void setApiUrl(const QString& _url);

    /**
     * @brief Check that the repository is available
     */
    void loadRepositoryInfo(std::function<void(const Error&, const RepositoryInfo&)> _callback);

    /**
     * @brief Load the list of files placed in the root of the repository
     */
    void loadFiles(std::function<void(const Error&, const QVector<FileInfo>&)> _callback);

    /**
     * @brief Load the content of a file
     */
    void download(const QString& _name,
                  std::function<void(const Error&, const QByteArray&)> _callback);

    /**
     * @brief Save a file to the repository
     * @param _replacedSha - sha of the file that should be replaced, empty for a new file.
     *        If the repository contains another version, nothing is changed and the request
     *        ends with the conflict error
     * @note The callback receives the sha of the saved file
     */
    void upload(const QString& _name, const QByteArray& _content, const QByteArray& _replacedSha,
                const QString& _message,
                std::function<void(const Error&, const QByteArray&)> _callback);

    /**
     * @brief Load the information about the last change of a file
     */
    void loadLastChange(const QString& _name,
                        std::function<void(const Error&, const ChangeInfo&)> _callback);

    /**
     * @brief Stop all requests, their callbacks will not be called
     */
    void abort();

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};
