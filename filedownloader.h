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

signals:
    void downloaded();

private slots:
    void fileDownloaded(QNetworkReply* pReply);

private:
    QNetworkAccessManager m_WebCtrl;
    QByteArray            m_DownloadedData;
    QString               m_sFilename;
};

#endif // FILEDOWNLOADER_H
