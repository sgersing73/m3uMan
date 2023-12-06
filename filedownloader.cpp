#include "filedownloader.h"

FileDownloader::FileDownloader(QUrl imageUrl, QObject *parent) : QObject(parent)
{
    m_sFilename = imageUrl.fileName();

    QNetworkRequest request(imageUrl);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    connect(&m_WebCtrl, SIGNAL(finished(QNetworkReply*)), this, SLOT (fileDownloaded(QNetworkReply*)));
    QNetworkReply* reply = m_WebCtrl.get(request);

    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), SLOT(updateDownloadProgress(qint64, qint64)));

    qDebug() << "FileDownloader" << imageUrl;
}

FileDownloader::~FileDownloader() {
}

void FileDownloader::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if ( bytesRead > 0 && totalBytes > 0 ) {
        m_iProgress = (qint64)((bytesRead * 100) / totalBytes);
        emit progress();
    }
    qDebug() << "updateDownloadProgress" << bytesRead << totalBytes;
}

qint64 FileDownloader::downloadedProgress() const {

    return m_iProgress;

    qDebug() << "downloadedProgress";
}


void FileDownloader::fileDownloaded(QNetworkReply* pReply) {

    m_DownloadedData = pReply->readAll();

    //emit a signal
    pReply->deleteLater();
    emit downloaded();

    qDebug() << "fileDownloaded";
}

QByteArray FileDownloader::downloadedData() const {

    return m_DownloadedData;
    qDebug() << "fileDownloaded";
}

QString FileDownloader::filename() const {

    return m_sFilename ;
    qDebug() << "fileDownloaded";
}
