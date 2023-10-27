#include "filedownloader.h"

FileDownloader::FileDownloader(QUrl imageUrl, QObject *parent) : QObject(parent)
{
    connect( &m_WebCtrl, SIGNAL(finished(QNetworkReply*)), this, SLOT (fileDownloaded(QNetworkReply*)));

    QNetworkRequest request(imageUrl);
    QNetworkReply* reply = m_WebCtrl.get(request);

    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), SLOT(updateDownloadProgress(qint64, qint64)));
    connect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), SLOT(errorOccurred(QNetworkReply::NetworkError)));

    m_sFilename = imageUrl.fileName();
}

FileDownloader::~FileDownloader() {

}

void FileDownloader::errorOccurred(QNetworkReply::NetworkError code) {

    qDebug() << code;
}

void FileDownloader::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{

    m_iProgress = (qint64)((bytesRead * 100) / totalBytes);
    emit progress();
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
