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
#include <QDir>
#include <QListWidgetItem>
#include <QXmlStreamReader>

#include <VLCQtCore/Common.h>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>

#include "dbmanager.h"
#include "filedownloader.h"
#include "EqualizerDialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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
    void getEPGFileData(const QString &);
    void fillTreeWidget();
    void fillTwPls_Item();
    void fillComboPlaylists();
    void fillComboGroupTitels();

    QStringList splitCommandLine(const QString &);

    QTreeWidgetItem* addTreeRoot(const QString &, const QString &, const QString &);
    void addTreeChild(QTreeWidgetItem *parent, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &);
    void get_media_sub_items( const libvlc_media_t& media );

private slots:
    void on_edtLoad_clicked();
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
    void on_cmdPlayStream_clicked();
    void on_edtStationUrl_textChanged(const QString &arg1);
    void on_lvStations_itemClicked(QListWidgetItem *item);

    void readyReadStandardOutput();
    void processStarted();
    void processFinished();

    void SaveM3u();
    void SaveXML();
    void loadImage();

    void on_cmdSavePosition_clicked();
    void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_cmdImportEpg_clicked();

    void on_edtEPGDownload_clicked();

private:
    QString         curFile;
    QDir            dir;
    DbManager       db;
    bool            somethingchanged;
    FileDownloader  *m_pImgCtrl;
    QProcess        *m_Process;
    QString         m_OutputString;
    QStandardPaths  *path;
    QString         m_AppDataPath;
    QString         m_SettingsFile;

    VlcInstance     *_instance;
    VlcMedia        *_media;
    VlcMediaPlayer  *_player;

    EqualizerDialog *_equalizerDialog;
};

#endif // MAINWINDOW_H
