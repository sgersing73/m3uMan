#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QTreeWidgetItem>
#include <QObject>
#include <QDebug>
#include <QFileDialog>
#include <QCloseEvent>
#include <QSaveFile>
#include <QIcon>
#include <QAction>
#include <QStringList>
#include <QSqlQuery>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QColor>
#include <QSettings>
#include <QSqlRecord>
#include <QProcess>
#include <QFlags>
#include <QStandardPaths>

#include "dbmanager.h"
#include "filedownloader.h"
#include "gatherdata.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;

    void createActions();
    void createStatusBar();
    void about();
    bool maybeSave();
    bool save();
    bool saveAs();
    bool saveFile(const QString &);
    void setCurrentFile(const QString &);
    void getFileData(const QString &);
    void fillTreeWidget();
    void fillTwPls_Item();
    void fillComboPlaylists();
    void fillComboGroupTitels();

    QStringList splitCommandLine(const QString &);

    QTreeWidgetItem* addTreeRoot(const QString &, const QString &, const QString &);
    void addTreeChild(QTreeWidgetItem *parent, const QString &, const QString &, const QString &, const QString &, const QString &);

public slots:
    void on_edtLoad_clicked();

private slots:
    void on_cmdNewPlaylist_clicked();
    void on_cmdDeletePlaylist_clicked();
    void on_cmdRenamePlaylist_clicked();
    void on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_cboPlaylists_currentTextChanged(const QString &arg1);
    void on_twPLS_Items_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_cmdMoveUp_clicked();
    void on_cmdMoveDown_clicked();
    void on_cmdMakePlaylist_clicked();
    void on_edtFilter_returnPressed();
    void on_cboGroupTitels_currentTextChanged(const QString &arg1);
    void on_edtDownload_clicked();
    void on_cmdGatherData_clicked();
    void on_twPLS_Items_itemSelectionChanged();

    void readyReadStandardOutput();
    void processStarted();
    void processFinished();

    void SaveM3u();
    void loadImage();

    void on_cmdPlayStream_clicked();

private:
    QString         curFile;
    DbManager       db;
    bool            somethingchanged;
    FileDownloader  *m_pImgCtrl;
    QProcess        *m_Process;
    QString         m_OutputString;
    QStandardPaths  *path;
    QString         m_AppDataPath;
};

#endif // MAINWINDOW_H
