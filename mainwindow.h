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
#include <QColorDialog>
#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressBar>
#include <QPushButton>
#include <QFontDialog>
#include <QStyle>
#include <QPainter>
#include <QRect>
#include <QPoint>
#include <QHostInfo>
#include <QStorageInfo>

#ifdef Q_OS_WIN
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif

#include <VLCQtCore/Common.h>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include <VLCQtCore/Error.h>
#include <VLCQtCore/Video.h>
#include <VLCQtCore/MetaManager.h>
#include <VLCQtCore/Stats.h>

#include <VLCQtWidgets/WidgetSeek.h>
#include <VLCQtWidgets/ControlVideo.h>

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
    void showEvent(QShowEvent *e) override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;

    void createActions();
    void createStatusBar();
    void about();
    void license();
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
    void getTMDBdate(const QString &, int, int);
    void getTMDBdataById(int, int);

    void displayMovieInfo(int, QString, bool);

    QTreeWidgetItem* addTreeRoot(const QString &, const QString &, const QString &, int);
    void addTreeChild(QTreeWidgetItem *parent, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, const QString& logo);
    void get_media_sub_items( const libvlc_media_t& media );

    void FindAndColorAllButtons();
    void MakePlaylist();
    void MakeLogoUrlList();
    void ImportLogoUrlList();

    QPixmap changeIconColor(QIcon, QColor);
    void fillComboEPGChannels();

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
    void on_edtFilter_returnPressed();
    void on_cboGroupTitels_currentTextChanged(const QString &arg1);
    void on_edtDownload_clicked();
    void on_twPLS_Items_itemSelectionChanged();
    void on_cmdPlayStream_clicked();
    void on_edtStationUrl_textChanged(const QString &arg1);

    void readyReadStandardOutput();
    void readyReadStandardOutputJson();
    void processStarted();
    void processFinished();
    void showVlcError();

    void SaveM3u();
    void SaveXML();
    void loadImage();
    void ShowDownloadProgress();
    void serviceRequestFinished(QNetworkReply*);

    void ShowContextMenuTreeWidget( const QPoint & );
    void ShowContextMenuPlsItems( const QPoint & );

    void on_cmdSavePosition_clicked();
    void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_cmdImportEpg_clicked();

    void on_edtEPGDownload_clicked();

    void on_radAll_clicked();
    void on_radNew_clicked();

    void on_cmdPlayMoveDown_clicked();
    void on_cmdPlayMoveUp_clicked();
    void on_cmdMoveForward_clicked();
    void on_cmdMoveBackward_clicked();
    void on_actionIcon_color_triggered();
    void on_chkOnlyFavorites_stateChanged(int arg1);
    void on_cmdEqualizer_clicked();
    void on_cmdMute_clicked();
    void on_cmdImdb_clicked();
    void progressCancel_clicked();
    void on_actionload_stylsheet_triggered();
    void on_cmdSetPos_clicked();
    void on_cmdSetLogo_clicked();
    void on_actionselect_application_font_triggered();

    void isPlaying();
    void isStopped();
    void isBuffering(int);

    void on_cmdPlayExtern_clicked();
    void on_actionimport_m3u_file_triggered();
    void on_cmdAddToFavorits_clicked();
    void on_chkPlaylistOnlyFavorits_stateChanged(int arg1);

    void on_chkAutoPlay_stateChanged(int arg1);

    void on_actionDB_Browser_triggered();
    void on_actionFTP_Client_triggered();
    void on_actionEdit_settings_ini_triggered();
    void on_actionExplore_application_folder_triggered();
    void on_actionExplorer_storage_folder_triggered();

    void on_cboEPGChannels_currentTextChanged(const QString &arg1);
    void on_mainToolBar_actionTriggered(QAction *);

    void on_cmdGatherStream_clicked();

    void on_cmdGatherStreamData_clicked();

    void on_edtFilter_2_returnPressed();

    void on_radTv_clicked();
    void on_radRadio_clicked();
    void on_radMovie_clicked();

    void on_actionExport_M3U_file_triggered();

    void on_actionExport_logo_links_triggered();

    void on_actionImport_logo_links_triggered();

    void on_cmdEPG_clicked();

    void on_chkOnlyEpg_clicked();

    void on_cmdWiki_clicked();

    void Zip (const QString&, const QString&);

private:
    QString         curFile;
    QDir            dir;
    DbManager       db;
    bool            somethingchanged;
    FileDownloader  *m_pImgCtrl;
    QProcess        *m_Process;
    QProcess        *m_Process2;
    QString         m_OutputString;
    QStandardPaths  *path;
    QString         m_AppDataPath;
    QString         m_SettingsFile;
    bool            m_ProgressWasCanceled;
    QTreeWidgetItem *m_ActTreeItem;

    QString               m_IconColor;
    QNetworkAccessManager *m_nam;
    QString         m_actualTitle;
    QString         m_JsonString;

    VlcInstance     *_instance;
    VlcMedia        *_media;
    VlcMediaPlayer  *_player;
    VlcError        *_error;
    VlcWidgetSeek   *_seek;
    VlcControlVideo *_videoControl;
    VlcVideo        *_video;
    VlcMetaManager  *_mediaManager;
    VlcStats        *_stats;
    VlcMetaManager  *_meta;

    EqualizerDialog *_equalizerDialog;

    QProgressBar    *m_progress;
    QPushButton     *m_progressCancel;

#ifdef Q_OS_WIN
    QWinTaskbarButton *taskbarButton;
    QWinTaskbarProgress *taskbarProgress;
#endif
};

#endif // MAINWINDOW_H
