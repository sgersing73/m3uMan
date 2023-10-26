#include "dbmanager.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

DbManager::DbManager()
{
}

DbManager::~DbManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DbManager::open(const QString& path)
{
    QSqlQuery query;

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path);

    qDebug() << "open database" << path;

    return m_db.open();
}

bool DbManager::isOpen()
{
    return m_db.isOpen();
}

bool DbManager::createTable()
{
    bool success = false;

    QSqlQuery query;

    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << "set PRAGME fails!" <<  query.lastError();
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "extinf (id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "        tvg_name    TEXT, "
                  "        tvg_id      TEXT, "
                  "        group_title TEXT, "
                  "        tvg_logo    TEXT, "
                  "        url         TEXT, "
                  "        state       INTEGER)");

    if (!query.exec()) {
        qDebug() << "createTable extinf" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_group_title ON extinf(group_title);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_group_title " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE UNIQUE INDEX IF NOT EXISTS idx_url ON extinf(url);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_url " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "pls (id       INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "     pls_name TEXT);");

    if (!query.exec()) {
        qDebug() << "createTable" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "pls_item (id        INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "          pls_id    INTEGER, "
                  "          extinf_id INTEGER, "
                  "          pls_pos   INTEGER,"
                  "          FOREIGN KEY(extinf_id) REFERENCES extinf(id) ON DELETE CASCADE,"
                  "          FOREIGN KEY(pls_id)    REFERENCES pls(id) ON DELETE CASCADE"
                  ");");

    if (!query.exec()) {
        qDebug() << "createTable" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_extinf_id ON pls_item(extinf_id);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_extinf_id " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "program (id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "         start       TEXT, "
                  "         stop        TEXT, "
                  "         channel     TEXT, "
                  "         title       TEXT, "
                  "         desc        TEXT)");

    if (!query.exec()) {
        qDebug() << "createTable program" <<  query.lastError();
        success = false;
    }

    return success;
}

bool DbManager::addEXTINF(const QString& tvg_name, const QString& tvg_id, const QString& group_title, const QString& tvg_logo, const QString& url)
{
   bool success = false;

   // you should check if args are ok first...
   QSqlQuery query;
   query.prepare("INSERT INTO extinf (tvg_name, tvg_id, group_title, tvg_logo, url, state ) VALUES (:tvg_name, :tvg_id, :group_title, :tvg_logo, :url, :state)");
   query.bindValue(":tvg_name", tvg_name);
   query.bindValue(":tvg_id", tvg_id);
   query.bindValue(":group_title", group_title);
   query.bindValue(":tvg_logo", tvg_logo);
   query.bindValue(":url", url);
   query.bindValue(":state", 2);

   if ( query.exec() ) {
       success = true;
   } else {
        qDebug() << "addEXTINF" << query.lastError() << url;
   }

   return success;
}

bool DbManager::removeAllEXTINFs()
{
    bool success = false;

    QSqlQuery query;

    if ( query.exec("DELETE FROM extinf") ) {
        success = true;
    } else {
        qDebug() << "removeAllEXTINFs" << query.lastError();
    }

    return success;
}

bool DbManager::removeObsoleteEXTINFs()
{
    bool success = false;

    QSqlQuery query;

    if ( query.exec("DELETE FROM extinf WHERE state = 0") ) {
        success = true;
    } else {
        qDebug() << "removeObsoleteEXTINFs" << query.lastError();
    }

    return success;
}

QSqlQuery* DbManager::selectEXTINF(const QString& group, const QString& station, const QString& state)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare(QString("SELECT *, ( select count(*) from pls_item where pls_item.extinf_id = extinf.id ) FROM extinf WHERE (group_title LIKE '%%1%' OR '%1' = '') AND tvg_name LIKE '%%2%' AND (state = %3 OR %3 = 0) ORDER BY group_title").arg(group).arg(station).arg(state.toInt()));
    if ( ! select->exec() ) {
        qDebug() << "selectEXTINF" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectEXTINF_byUrl(const QString& url)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare(QString("SELECT * FROM extinf WHERE url = :url"));
    select->bindValue(":url", url);

    if ( ! select->exec() ) {
        qDebug() << "selectEXTINF_byUrl" << url << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectEXTINF_byRef(int id)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM extinf WHERE id = :id");
    select->bindValue(":id", id);
    if ( ! select->exec() ) {
         qDebug() << "selectEXTINF_byRef" << id << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::countEXTINF_byState()
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT state, count(*) FROM extinf group by state");

    if ( ! select->exec() ) {
         qDebug() << "countEXTINF_byState"  << select->lastError();
    }

    return select;
}

bool DbManager::updateEXTINF_byRef(int id, const QString& tvg_name, const QString& group_title, const QString& tvg_logo,int state)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET state = :state, group_title = :group_title, tvg_name = :tvg_name, tvg_logo =:tvg_logo WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":tvg_name", tvg_name);
    select->bindValue(":group_title", group_title);
    select->bindValue(":tvg_logo", tvg_logo);
    select->bindValue(":state", state);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_state_byRef" << select->lastError();
    }

    return retCode;
}

bool DbManager::deactivateEXTINFs()
{
    QSqlQuery *select = new QSqlQuery();

    return select->exec("UPDATE extinf SET state = 0");
}

QSqlQuery* DbManager::selectEXTINF_group_titles()
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("select distinct group_title from extinf order by group_title");
    if ( select->exec() ) {
    }

    return select;
}


QSqlQuery* DbManager::selectPLS()
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls ORDER BY pls_name");
    if ( ! select->exec() ) {
        qDebug() << "selectPLS" << select->lastError();
    }

    return select;
}

bool DbManager::removePLS(int id)
{
    bool success = false;

    QSqlQuery query;
    query.prepare("DELETE FROM pls WHERE id = :id");
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "removePLS" << query.lastError();
    }

    return success;
}


bool DbManager::updatePLS(int id, const QString & pls_name )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls SET pls_name = :pls_name WHERE id = :id");
    query.bindValue(":pls_name", pls_name);
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS" << query.lastError();
    }

    return success;
}

bool DbManager::updatePLS_item_pls_pos(int id, int pls_pos )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls_item SET pls_pos = :pls_pos WHERE id = :id");
    query.bindValue(":pls_pos", pls_pos);
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS_pls_pos" << query.lastError();
    }

    return success;
}

bool DbManager::insertPLS(const QString & pls_name )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("INSERT INTO pls (pls_name) VALUES (:pls_name)");
    query.bindValue(":pls_name", pls_name);

    if ( query.exec() ) {
        success = true;
    } else {
         qDebug() << "insertPLS" << query.lastError();
    }

    return success;
}


bool DbManager::insertPLS_Item(int pls_id, int extinf_id, int pls_pos )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("INSERT INTO pls_item (pls_id, extinf_id, pls_pos) VALUES (:pls_id, :extinf_id, :pls_pos )");
    query.bindValue(":pls_id", pls_id);
    query.bindValue(":extinf_id", extinf_id);
    query.bindValue(":pls_pos", pls_pos);

    if ( query.exec() ) {
        success = true;
    } else {
         qDebug() << "insertPLS_Item" << query.lastError();
    }

    return success;
}

QSqlQuery* DbManager::selectPLS_Items(int pls_id)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls_item, extinf WHERE pls_id = :pls_id AND extinf.id = pls_item.extinf_id ORDER BY pls_pos");
    select->bindValue(":pls_id", pls_id);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_Item" << select->lastError();
    }

    return select;
}

bool DbManager::removePLS_Item(int id)
{
    bool success = false;

    QSqlQuery query;
    query.prepare("DELETE FROM pls_item WHERE id = :id");
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "removePLS_Item" << query.lastError();
    }

    return success;
}

bool DbManager::removePLS_Items(int pls_id)
{
    bool success = false;

    QSqlQuery query;
    query.prepare("DELETE FROM pls_item WHERE pls_id = :pls_id");
    query.bindValue(":pls_id", pls_id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "removePLS_Items" << query.lastError();
    }

    return success;
}

bool DbManager::addProgram(const QString& start, const QString& stop,
                           const QString& channel, const QString& title,
                           const QString& desc)
{
   bool success = false;

   // you should check if args are ok first...
   QSqlQuery query;
   query.prepare("INSERT INTO program (start, stop, channel, title, desc ) VALUES (:start, :stop, :channel, :title, :desc)");
   query.bindValue(":start", start);
   query.bindValue(":stop", stop);
   query.bindValue(":channel", channel);
   query.bindValue(":title", title);
   query.bindValue(":desc", desc);

   if ( query.exec() ) {
       success = true;
   } else {
        qDebug() << "addProgram" << query.lastError();
   }

   return success;
}

bool DbManager::removeAllPrograms()
{
    bool success = false;

    QSqlQuery query;

    if ( query.exec("DELETE FROM program") ) {
        success = true;
    } else {
        qDebug() << "removeAllPrograms" << query.lastError();
    }

    return success;
}

QSqlQuery* DbManager::selectProgramData(const QString &channel)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM program WHERE strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime', '-2 hours') > start AND "
                    "                            strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime', '-2 hours') < stop AND "
                    "                            channel = :channel");
    select->bindValue(":channel", channel);

    if ( ! select->exec() ) {
        qDebug() << "selectProgramData" << select->lastError();
    }

    return select;
}
