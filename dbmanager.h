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

    bool addEXTINF(const QString&, const QString&, int, const QString&, const QString&);
    bool removeAllEXTINFs();
    bool removeObsoleteEXTINFs();
    bool updateEXTINF_byRef(int, const QString&, int, const QString&, int);
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
    bool updatePLS_item_pls_pos(int, int);
    bool insertPLS(const QString &);
    bool removePLS(int);
    QSqlQuery* selectPLS();
    QSqlQuery* selectEXTINF_byRef(int);

    bool insertPLS_Item(int, int, int);
    QSqlQuery* selectPLS_Items(int);
    bool removePLS_Item(int);
    bool removePLS_Items(int);

    bool removeAllPrograms();
    bool addProgram(const QString&, const QString&, const QString&, const QString&, const QString&);
    QSqlQuery* selectProgramData(const QString &);

private:
    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
