#include "filedownloader.h"

FileDownloader::FileDownloader(QUrl imageUrl, QObject *parent) : QObject(parent)
{

    QNetworkRequest request(imageUrl);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply* reply = m_WebCtrl.get(request);

    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), SLOT(updateDownloadProgress(qint64, qint64)));

    connect(&m_WebCtrl, SIGNAL(finished(QNetworkReply*)), this, SLOT (fileDownloaded(QNetworkReply*)));

    m_sFilename = imageUrl.fileName();
}

FileDownloader::~FileDownloader() {
}

void FileDownloader::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if ( bytesRead > 0 && totalBytes > 0 ) {
        m_iProgress = (qint64)((bytesRead * 100) / totalBytes);
        emit progress();
    }
}

qint64 FileDownloader::downloadedProgress() const {

    return m_iProgress;
}


void FileDownloader::fileDownloaded(QNetworkReply* pReply) {

    m_DownloadedData = pReply->readAll();

    //emit a signal
    pReply->deleteLater();
    emit downloaded();
}

QByteArray FileDownloader::downloadedData() const {

    return m_DownloadedData;
}

QString FileDownloader::filename() const {

    return m_sFilename ;
}
