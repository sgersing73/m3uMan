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
        qDebug() << "set PRAGME foreign_keys fails!" <<  query.lastError();
    }

    if (!query.exec("PRAGMA synchronous = OFF")) {
        qDebug() << "set PRAGME synchronous fails!" <<  query.lastError();
    }

    if (!query.exec("PRAGMA journal_mode = MEMORY")) {
        qDebug() << "set PRAGME journal_mode fails!" <<  query.lastError();
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "groups (id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "        group_title TEXT, "
                  "        favorite    INTEGER)");

    if (!query.exec()) {
        qDebug() << "createTable groups" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_group_favorite ON groups(favorite);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_group_favorite " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "extinf (id          INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "        tvg_name    TEXT, "
                  "        tvg_id      TEXT, "
                  "        group_id    INTEGER, "
                  "        tvg_logo    TEXT, "
                  "        url         TEXT, "
                  "        state       INTEGER, "
                  "FOREIGN KEY(group_id) REFERENCES groups(id) ON DELETE CASCADE)");

    if (!query.exec()) {
        qDebug() << "createTable extinf" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_group_id ON extinf(group_id);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_group_id " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE UNIQUE INDEX IF NOT EXISTS idx_url ON extinf(url);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_url " <<  query.lastError();
        success = false;
    }

    // Tabelle pls (Playlists)

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "pls (id       INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "     pls_name TEXT,"
                  "     favorite INTEGER DEFAULT 0,"
                  "     kind     INTEGER DEFAULT 0)");

    if (!query.exec()) {
        qDebug() << "createTable" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_pls_pls_name ON pls(pls_name);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_pls_pls_name" <<  query.lastError();
        success = false;
    }

    // Tabelle pls_item (Playlist Einträge)

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "pls_item (id        INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "          pls_id    INTEGER, "
                  "          extinf_id INTEGER, "
                  "          pls_pos   INTEGER DEFAULT 0, "
                  "          tmdb_id   INTEGER DEFAULT 0, "
                  "          favorite INTEGER DEFAULT 0,"
                  "          FOREIGN KEY(extinf_id) REFERENCES extinf(id) ON DELETE CASCADE,"
                  "          FOREIGN KEY(pls_id)    REFERENCES pls(id) ON DELETE CASCADE"
                  ")");

    if (!query.exec()) {
        qDebug() << "createTable" <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE INDEX IF NOT EXISTS idx_extinf_id ON pls_item(extinf_id);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_extinf_id " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE UNIQUE INDEX IF NOT EXISTS idx_extinf_id_pls_id ON pls_item(extinf_id, pls_id);");

    if (!query.exec()) {
        qDebug() << "createIndex idx_extinf_id_pls_id " <<  query.lastError();
        success = false;
    }

    // Tabelle program (EPG Daten)

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

    query.prepare("CREATE INDEX IF NOT EXISTS idx_program_channel ON program(channel)");

    if (!query.exec()) {
        qDebug() << "createIndex idx_program_channel " <<  query.lastError();
        success = false;
    }

    query.prepare("CREATE UNIQUE INDEX IF NOT EXISTS idx_program_uk1 ON program(start, stop, channel)");

    if (!query.exec()) {
        qDebug() << "createIndex idx_program_uk1 " <<  query.lastError();
        success = false;
    }

    // Tabelle settings (save the settings from ini file)

    query.prepare("CREATE TABLE IF NOT EXISTS "
                  "ini (id       INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "     key      TEXT,"
                  "     text     TEXT)");

    if (!query.exec()) {
        qDebug() << "createTable settings" <<  query.lastError() << query.lastQuery();
        success = false;
    }

    return success;
}

int DbManager::insertEXTINF(const QString& tvg_name, const QString& tvg_id, int group_id, const QString& tvg_logo, const QString& url)
{
   int id = 0;

   QSqlQuery query;

   query.prepare("INSERT INTO extinf (tvg_name, tvg_id, group_id, tvg_logo, url, state ) VALUES (:tvg_name, :tvg_id, :group_id, :tvg_logo, :url, :state)");
   query.bindValue(":tvg_name", tvg_name);
   query.bindValue(":tvg_id", tvg_id);
   query.bindValue(":group_id", group_id);
   query.bindValue(":tvg_logo", tvg_logo);
   query.bindValue(":url", url);
   query.bindValue(":state", 2);

   if ( query.exec() ) {
       id = query.lastInsertId().toInt();
   } else {
       qDebug() << "addEXTINF" << query.lastError() << url;
   }

   return id;
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

QSqlQuery* DbManager::selectEXTINF(const QString& group_title, const QString& tvg_name, const QString& state, int favorite)
{
    QSqlQuery *select = new QSqlQuery();

    //qDebug() << group_title <<tvg_name<<favorite<<state;

    QString query = QString("SELECT *, "
                            "( select count(*) from pls_item where pls_item.extinf_id = extinf.id ) "
                            "FROM  extinf, "
                            "      groups "
                            "WHERE groups.id = extinf.group_id "
                            "AND  (groups.favorite = :favorite OR :favorite = 0) "
                            "AND  (groups.group_title LIKE :group_title OR :group_title = '') "
                            "AND  (tvg_name LIKE :tvg_name OR :tvg_name = '') "
                            "AND  (state = :state OR :state = '0') "
                            "ORDER BY groups.group_title");

    select->prepare( query );
    select->bindValue(":state", state);
    select->bindValue(":favorite", favorite);
    select->bindValue(":group_title", group_title);
    select->bindValue(":tvg_name", tvg_name);

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

    select->prepare("SELECT * FROM extinf, groups WHERE extinf.id = :id and groups.id = extinf.group_id");
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

bool DbManager::updateEXTINF_byRef(int id, const QString& tvg_name, int group_id, const QString& tvg_logo,int state)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET state = :state, group_id = :group_id, tvg_name = :tvg_name, tvg_logo =:tvg_logo WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":tvg_name", tvg_name);
    select->bindValue(":group_id", group_id);
    select->bindValue(":tvg_logo", tvg_logo);
    select->bindValue(":state", state);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_byRef" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}

bool DbManager::updateEXTINF_tvg_name_byRef(int id, const QString& tvg_name)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET tvg_name =:tvg_name WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":tvg_name", tvg_name);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_tvg_name_byRef" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}

bool DbManager::updateEXTINF_tvg_logo_byRef(int id, const QString& tvg_logo)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET tvg_logo =:tvg_logo WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":tvg_logo", tvg_logo);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_tvg_logo_byRef" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}

bool DbManager::updateEXTINF_tvg_logo_by_tvg_name(const QString& tvg_name, const QString& tvg_logo)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET tvg_logo =:tvg_logo WHERE tvg_name = :tvg_name");
    select->bindValue(":tvg_name", tvg_name);
    select->bindValue(":tvg_logo", tvg_logo);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_tvg_logo_by_tvg_name" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}

bool DbManager::updateEXTINF_tvg_id_byRef(int id, const QString& tvg_id)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET tvg_id =:tvg_id WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":tvg_id", tvg_id);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_tvg_id_byRef" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}

bool DbManager::updateEXTINF_url_byRef(int id, const QString& url)
{
    int retCode = true;

    QSqlQuery *select = new QSqlQuery();

    select->prepare("UPDATE extinf SET url =:url WHERE id = :id");
    select->bindValue(":id", id);
    select->bindValue(":url", url);

    if ( ! select->exec() ) {
        qDebug() << "updateEXTINF_url_byRef" << select->lastError();
        retCode = false;
    }

    delete select;

    return retCode;
}


bool DbManager::deactivateEXTINFs()
{
    QSqlQuery *select = new QSqlQuery();

    select->exec("UPDATE extinf SET state = 0");

    delete select;

    return true;
}

QSqlQuery* DbManager::selectEXTINF_group_titles(int state)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("select distinct group_title from extinf WHERE (state = :state OR :state = 0) order by group_title");
    select->bindValue(":state", state);

    if ( ! select->exec() ) {
        qDebug() << "selectEXTINF_group_titles" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectPLS_by_pls_name(const QString& pls_name)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls WHERE pls_name = :pls_name");
    select->bindValue(":pls_name", pls_name);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_by_pls_name" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectPLS(int favorite)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls WHERE (favorite = :favorite OR :favorite = 0) ORDER BY pls_name");
    select->bindValue(":favorite", favorite);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectPLS_by_id(int id)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls WHERE id = :id");
    select->bindValue(":id", id);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_by_id" << select->lastError();
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


bool DbManager::updatePLS_favorite(int id, int favorite )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls SET favorite = :favorite WHERE id = :id");
    query.bindValue(":favorite", favorite);
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS_favorite" << query.lastError();
    }

    return success;
}

bool DbManager::updatePLS_kind(int id, int kind )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls SET kind = :kind WHERE id = :id");
    query.bindValue(":kind", kind);
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS_kind" << query.lastError();
    }

    return success;
}

bool DbManager::updatePLS_item_favorite(int id, int favorite )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls_item SET favorite = :favorite WHERE id = :id");
    query.bindValue(":favorite", favorite);
    query.bindValue(":id", id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS_favorite" << query.lastError();
    }

    return success;
}
bool DbManager::updatePLS_item_tmdb_by_extinf_id(int extinf_id, double tmdb_id )
{
    bool success = false;

    QSqlQuery query;
    query.prepare("UPDATE pls_item SET tmdb_id = :tmdb_id WHERE extinf_id = :extinf_id");
    query.bindValue(":tmdb_id", tmdb_id);
    query.bindValue(":extinf_id", extinf_id);

    if ( query.exec() ) {
        success = true;
    } else {
        qDebug() << "updatePLS_item_tmdb" << query.lastError();
    }

    return success;
}

int DbManager::insertPLS(const QString & pls_name, int favorite)
{
    int id = 0;

    QSqlQuery query;
    query.prepare("INSERT INTO pls (pls_name, favorite) VALUES (:pls_name, :favorite)");
    query.bindValue(":pls_name", pls_name);
    query.bindValue(":favorite", favorite);

    if ( query.exec() ) {
        id = query.lastInsertId().toInt();
    } else {
        qDebug() << "insertPLS" << query.lastError();
    }

    return id;
}


int DbManager::insertPLS_Item(int pls_id, int extinf_id, int pls_pos )
{
    int id = 0;

    QSqlQuery query;
    query.prepare("INSERT INTO pls_item (pls_id, extinf_id, pls_pos) VALUES (:pls_id, :extinf_id, :pls_pos )");
    query.bindValue(":pls_id", pls_id);
    query.bindValue(":extinf_id", extinf_id);
    query.bindValue(":pls_pos", pls_pos);

    if ( query.exec() ) {
        id = query.lastInsertId().toInt();
    } else {
        qDebug() << "insertPLS_Item" << query.lastError();
    }

    return id;
}

QSqlQuery* DbManager::selectPLS_Items(int pls_id, const QString& tvg_name, int onlyepg )
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * "
                    "FROM   pls_item, extinf "
                    "WHERE  pls_id = :pls_id "
                    "AND    extinf.id = pls_item.extinf_id "
                    "AND    extinf.tvg_name like :tvg_name "
                    "AND    ( ( extinf.tvg_id <> ' ' AND :onlyepg = 1 ) OR ( :onlyepg = 0 ) ) "
                    "ORDER BY pls_pos");

    select->bindValue(":pls_id", pls_id);
    select->bindValue(":tvg_name", tvg_name);
    select->bindValue(":onlyepg", onlyepg);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_Item" << select->lastError() << select->lastQuery();
    }

    return select;
}

QSqlQuery* DbManager::selectPLS_Items_by_extinf_id(int extinf_id)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls_item WHERE extinf_id = :extinf_id");
    select->bindValue(":extinf_id", extinf_id);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_Items_by_extinf_id" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectPLS_Items_by_key(int pls_id, int extinf_id)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM pls_item WHERE pls_id = :pls_id and extinf_id = :extinf_id");
    select->bindValue(":extinf_id", extinf_id);
    select->bindValue(":pls_id", pls_id);

    if ( ! select->exec() ) {
        qDebug() << "selectPLS_Items_by_key" << select->lastError();
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
       if ( query.lastError().nativeErrorCode().toInt() != 19 ) {
           qDebug() << "addProgram" << query.lastError();
       } else {
           // Program already there...
       }
   }

   return success;
}

bool DbManager::removeOldPrograms()
{
    bool success = false;

    QSqlQuery query;

    if ( query.exec("DELETE FROM program WHERE stop < strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime')") ) {
        success = true;
    } else {
        qDebug() << "removeOldPrograms" << query.lastError();
    }

    return success;
}

QSqlQuery* DbManager::selectActualProgramData(const QString &channel)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM program WHERE strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime') > start AND "
                    "                            strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime') < stop AND "
                    "                            channel = :channel");

    select->bindValue(":channel", channel);

    if ( ! select->exec() ) {
        qDebug() << "selectActualProgramData" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectProgramData(const QString &channel)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT SUBSTR (start, 7, 2) || ' ' ||SUBSTR (start, 9, 2) || ':' || SUBSTR (start, 11, 2) || ':' || SUBSTR(start, 13, 2), "
                    "       SUBSTR (start, 7, 2) || ' ' ||SUBSTR (stop,  9, 2) || ':' || SUBSTR (stop,  11, 2) || ':' || SUBSTR(stop,  13, 2), "
                    "       title, "
                    "       desc "
                    "FROM   program "
                    "WHERE  strftime('%Y%m%d%H%M%S +0000', 'now', 'localtime') < stop "
                    "AND    channel = :channel");

    select->bindValue(":channel", channel);

    if ( ! select->exec() ) {
        qDebug() << "selectProgramData" << select->lastError();
    }

    return select;
}

int DbManager::addGroup(const QString& group_title)
{
   int id = 0;

   QSqlQuery query;

   query.prepare("INSERT INTO groups (group_title, favorite ) VALUES (:group_title, :favorite)");
   query.bindValue(":group_title", group_title);
   query.bindValue(":favorite", 0);

   if ( query.exec() ) {
        id = query.lastInsertId().toInt();
   } else {
        qDebug() << "addGroup" << query.lastError() << group_title;
   }

   return id;
}

bool DbManager::updateGroup(int id, const QString& group_title, int favorite)
{
   bool success = false;

   QSqlQuery query;

   query.prepare("UPDATE groups SET group_title = :group_title, favorite = :favorite WHERE id = :id");
   query.bindValue(":group_title", group_title);
   query.bindValue(":favorite", favorite);
   query.bindValue(":id", id);

   if ( query.exec() ) {
        success = true;
   } else {
        qDebug() << "updateGroup" << query.lastError() << id << group_title << favorite;
   }

   return success;
}

bool DbManager::updateGroupFavorite(int id, int favorite)
{
   bool success = false;

   QSqlQuery query;

   query.prepare("UPDATE groups SET favorite = :favorite WHERE id = :id");
   query.bindValue(":favorite", favorite);
   query.bindValue(":id", id);

   if ( query.exec() ) {
        success = true;
   } else {
        qDebug() << "updateGroupFavorite" << query.lastError() << id <<  favorite;
   }

   return success;
}


QSqlQuery* DbManager::selectGroup_byTitle(const QString& group_title)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM groups WHERE group_title = :group_title");
    select->bindValue(":group_title", group_title);

    if ( ! select->exec() ) {
        qDebug() << "selectGroup_by_title" << select->lastError() << group_title;
    }

    return select;
}

QSqlQuery* DbManager::selectGroups(int favorite)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare("SELECT * FROM groups WHERE (favorite = :favorite OR :favorite = 0) ORDER BY group_title");
    select->bindValue(":favorite", favorite);

    if ( ! select->exec() ) {
        qDebug() << "selectGroups" << select->lastError();
    }

    return select;
}

QSqlQuery* DbManager::selectEPGChannels(const QString& region)
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare(QString("select * from program where channel like '%%1%' group by channel").arg(region));

    if ( ! select->exec() ) {
        qDebug() << "selectEPGChannels" << select->lastError();
    }

    return select;
}

int DbManager::insertINI(const QString& key, const QString& text)
{
    int id = 0;

    QSqlQuery query;
    query.prepare("INSERT INTO ini (key, text) VALUES (:key, :text)");
    query.bindValue(":key", key);
    query.bindValue(":text", text);

    if ( query.exec() ) {
        id = query.lastInsertId().toInt();
    } else {
        qDebug() << "insertINI" << query.lastError();
    }

    return id;
}

bool DbManager::removeINI()
{
    bool success = false;

    QSqlQuery query;

    if ( query.exec("DELETE FROM ini") ) {
        success = true;
    } else {
        qDebug() << "removeINI" << query.lastError();
    }

    return success;
}

QSqlQuery* DbManager::selectINI()
{
    QSqlQuery *select = new QSqlQuery();

    select->prepare(QString("select * from ini") );

    if ( ! select->exec() ) {
        qDebug() << "selectINI" << select->lastError();
    }

    return select;
}
