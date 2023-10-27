#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class FileDownloader : public QObject
{
 Q_OBJECT
public:
    explicit FileDownloader(QUrl imageUrl, QObject *parent = nullptr);
    virtual ~FileDownloader();
    QByteArray downloadedData() const;
    QString filename() const ;
    qint64  downloadedProgress() const;

signals:
    void downloaded();
    void progress();

private slots:
    void fileDownloaded(QNetworkReply* pReply);
    void updateDownloadProgress(qint64,qint64);
    void errorOccurred(QNetworkReply::NetworkError);

private:
    QNetworkAccessManager m_WebCtrl;
    QByteArray            m_DownloadedData;
    QString               m_sFilename;
    QNetworkReply*        m_reply;
    qint64                m_iProgress;
};

#endif // FILEDOWNLOADER_H
