#include "gatherdata.h"

#include <QDebug>

GatherData::GatherData()
{

}

void GatherData::run()
{
    for(int i = 0; i <= 100; i++)
    {
        qDebug() << m_url << i;
    }
}

void GatherData::setUrl(const QString& url)
{
    this->m_url = url;
}
