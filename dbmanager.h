#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>

class DbManager
{
public:
    DbManager();

    ~DbManager();

    bool open(const QString& path);
    bool isOpen();

    bool createTable();

    int  insertEXTINF(const QString&, const QString&, int, const QString&, const QString&);
    bool removeAllEXTINFs();
    bool removeObsoleteEXTINFs();
    bool updateEXTINF_byRef(int, const QString&, int, const QString&, int);
    bool updateEXTINF_tvg_logo_byRef(int, const QString&);
    bool updateEXTINF_tvg_id_byRef(int, const QString&);
    bool updateEXTINF_url_byRef(int, const QString&);

    bool deactivateEXTINFs();
    QSqlQuery* selectEXTINF(const QString&, const QString&, const QString&, int);
    QSqlQuery* selectEXTINF_group_titles(int);
    QSqlQuery* selectEXTINF_byUrl(const QString&);
    QSqlQuery* countEXTINF_byState();

    int addGroup(const QString&);
    bool updateGroup(int, const QString&, int);
    bool updateGroupFavorite(int, int);

    QSqlQuery* selectGroup_byTitle(const QString&);
    QSqlQuery* selectGroups(int favorite);

    bool updatePLS(int, const QString &);
    bool updatePLS_favorite(int, int);

    bool updatePLS_item_pls_pos(int, int);
    bool updatePLS_item_tmdb_by_extinf_id(int, double);
    bool updatePLS_item_favorite(int, int);

    int insertPLS(const QString &);
    bool removePLS(int);
    QSqlQuery* selectPLS(int);
    QSqlQuery* selectPLS_by_pls_name(const QString& );

    QSqlQuery* selectEXTINF_byRef(int);
    QSqlQuery* selectPLS_Items_by_extinf_id(int);
    QSqlQuery* selectPLS_Items_by_key(int, int);

    int insertPLS_Item(int, int, int);
    QSqlQuery* selectPLS_Items(int);
    bool removePLS_Item(int);
    bool removePLS_Items(int);

    bool removeAllPrograms();
    bool addProgram(const QString&, const QString&, const QString&, const QString&, const QString&);
    QSqlQuery* selectProgramData(const QString &);

    QSqlQuery* selectEPGChannels(const QString&);

private:
    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
