#ifndef GATHERDATA_H
#define GATHERDATA_H

#include <QThread>

class GatherData : public QThread
{
public:
    GatherData();

    void    setUrl(const QString&);
    void    run();

private:
    QString m_url;

};

#endif // GATHERDATA_H
