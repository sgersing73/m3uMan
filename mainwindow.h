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

#ifdef Q_OS_WIN
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif

#include <VLCQtCore/Common.h>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include <VLCQtCore/Error.h>
#include <VLCQtWidgets/WidgetSeek.h>

#include "dbmanager.h"
#include "filedownloader.h"
#include "EqualizerDialog.h"
#include "qftp.h"

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

    QTreeWidgetItem* addTreeRoot(const QString &, const QString &, const QString &, int);
    void addTreeChild(QTreeWidgetItem *parent, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &);
    void get_media_sub_items( const libvlc_media_t& media );

    void findAllButtons();
    QPixmap changeIconColor(QIcon, QColor);
    bool ftpUploadFile(const QString&);

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
    void showVlcError();

    void SaveM3u();
    void SaveXML();
    void loadImage();
    void ShowDownloadProgress();

    void ShowContextMenu( const QPoint & );

    void on_cmdSavePosition_clicked();
    void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_cmdImportEpg_clicked();

    void on_edtEPGDownload_clicked();

    void on_radAll_clicked();
    void on_radNew_clicked();

    void on_cmdPlayMoveDown_clicked();

    void on_pushButton_clicked();

    void on_cmdMoveForward_clicked();

    void on_cmdMoveBackward_clicked();

    void on_actionIcon_color_triggered();

    void on_chkOnlyFavorites_stateChanged(int arg1);

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
    VlcError        *_error;
    VlcWidgetSeek   *_seek;

    EqualizerDialog *_equalizerDialog;

#ifdef Q_OS_WIN
    QWinTaskbarButton *taskbarButton;
    QWinTaskbarProgress *taskbarProgress;
#endif
};

#endif // MAINWINDOW_H
