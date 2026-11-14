#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QQmlNetworkAccessManagerFactory>
#include <QUrl>

// A notepad has no business on the network: a rendered Markdown or HTML page
// may name remote images, and fetching them would tell a server the note was
// opened. Every request that isn't for a local file is emptied, so it fails.
class OfflineManager : public QNetworkAccessManager {
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *data) override {
        QNetworkRequest local(request);
        const QString scheme = request.url().scheme();
        if (scheme != "file" && scheme != "qrc")
            local.setUrl(QUrl());
        return QNetworkAccessManager::createRequest(operation, local, data);
    }
};

class OfflineFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override { return new OfflineManager(parent); }
};
