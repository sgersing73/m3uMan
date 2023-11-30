#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qstyle.h>
#include <qpainter.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    qDebug() << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();

    m_AppDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    dir.mkpath(m_AppDataPath);

    m_SettingsFile = m_AppDataPath + "/settings.ini";

    _instance = new VlcInstance(VlcCommon::args(), this);

    _player = new VlcMediaPlayer(_instance);
    _equalizerDialog = new EqualizerDialog(this);    
    _videoControl = new VlcControlVideo (_player);
    _video = new VlcVideo(_player);

    _player->setVideoWidget(ui->widVideo);    
    _equalizerDialog->setMediaPlayer(_player);
    _error = new VlcError();

    ui->widSeek->setMediaPlayer(_player);
    ui->widVolume->setMediaPlayer(_player);
    ui->widVolume->setVolume(50);
    ui->widVideo->setMediaPlayer(_player);

    connect(ui->cmdPause, &QPushButton::clicked, _player, &VlcMediaPlayer::togglePause);
    connect(ui->cmdStop, &QPushButton::clicked, _player, &VlcMediaPlayer::stop);

    connect(_player, SIGNAL(playing()), this, SLOT(isPlaying()));
    connect(_player, SIGNAL(stopped()), this, SLOT(isStopped()));
    connect(_player, SIGNAL(buffering(int)), this, SLOT(isBuffering(int)));
    connect(_player, SIGNAL(error()), this, SLOT(showVlcError()));

    this->findAllButtons();

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    ui->splitter->restoreState(settings.value("splitter").toByteArray());
    ui->splitter_2->restoreState(settings.value("splitter_2").toByteArray());
    ui->splitter_3->restoreState(settings.value("splitter_3").toByteArray());
    ui->splitter_4->restoreState(settings.value("splitter_4").toByteArray());
    ui->edtUrl->setText(settings.value("iptvurl").toByteArray());
    ui->edtUrlEpg->setText(settings.value("iptvepgurl").toByteArray());
    ui->cmdImdb->setEnabled(false);

    if ( ! settings.value("stylsheet").toByteArray().isEmpty() ) {

        QFile f ( settings.value("stylsheet").toByteArray() );
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);

        qApp->setStyleSheet(ts.readAll());
        f.close();
    }

    if ( ! settings.value("fontname").toByteArray().isEmpty() ) {

        QFont font(settings.value("fontname").toByteArray());

        font.setPointSize( settings.value("fontsize").toByteArray().toInt() );
        font.setBold( settings.value("fontbold").toBool() );
        font.setItalic( settings.value("fontitalic").toBool() );

        QApplication::setFont(font);
    }

    ui->treeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(ShowContextMenu(const QPoint&)));

    createActions();
    createStatusBar();

    db.open(m_AppDataPath + "/m3uMan.sqlite");

    if (db.isOpen()) {
        if ( ! db.createTable() ) {
             qApp->exit();
        }
     } else {
        qDebug() << "Database is not open!";
     }

    fillComboPlaylists();
    fillComboGroupTitels();
    fillComboEPGChannels();
    fillTreeWidget();

    m_Process = new QProcess(this);

    connect(m_Process, SIGNAL(started()), this, SLOT(processStarted()));
    connect(m_Process, SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
    connect(m_Process, SIGNAL(finished(int)), this, SLOT(processFinished()));

#ifdef Q_OS_WIN
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(this->windowHandle());

    taskbarProgress = taskbarButton->progress();
#endif

    somethingchanged = false;
}

void MainWindow::showEvent(QShowEvent *e)
{
#ifdef Q_OS_WIN32
    taskbarButton->setWindow(windowHandle());
#endif
    e->accept();
}

void MainWindow::isPlaying() {

    qDebug() << "VLC is playing...";
}

void MainWindow::isStopped() {

    qDebug() << "VLC is stopped...";
    ui->pgbBuffer->setValue(0);
}

void MainWindow::isBuffering(int buffer) {

    //qDebug() << "VLC is buffering..." << buffer;

    ui->pgbBuffer->setValue(buffer);
}

void MainWindow::ShowContextMenu( const QPoint & pos )
{
    QTreeWidgetItem *item = ui->treeWidget->itemAt(pos);
    if (!item)
       return;

    QPoint globalPos = ui->treeWidget->mapToGlobal(pos);

    QMenu myMenu;
    myMenu.addAction("add to favorites");
    myMenu.addAction("remove from favorites");
    myMenu.addAction("make playlists from favorites");
    myMenu.addSeparator();
    myMenu.addAction("move all stations to selected playlist");
    // ...

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        if ( selectedItem->text().contains("add to favorites") ) {
            if ( item->childCount() != 0 ) { // Group Item
                if ( db.updateGroupFavorite(item->text(2).toInt(), 1) )  {
                    fillComboGroupTitels();
                    fillTreeWidget();
                }
            }
        }
        if ( selectedItem->text().contains("remove from favorites") ) {
            if ( item->childCount() != 0 ) { // Group Item
                if ( db.updateGroupFavorite(item->text(2).toInt(), 0) ) {
                    fillComboGroupTitels();
                    fillTreeWidget();
                }
            }
        }
        if ( selectedItem->text().contains("move all stations to selected playlist") ) {

            if ( item->childCount() != 0 ) { // Group Item

                QProgressDialog progress("Task in progress...", "Cancel", 0, item->childCount(), this);
                progress.setWindowModality(Qt::WindowModal);

                for( int i = 0; i < item->childCount(); ++i ) {

                    const int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();
                    const int extinf_id = item->child(i)->text(2).toInt();
                    const int pls_pos = ui->twPLS_Items->topLevelItemCount() +1;

                    db.insertPLS_Item( pls_id, extinf_id, pls_pos );

                    progress.setValue(i);
                }

                progress.close();
            }

            this->fillTwPls_Item();
        }

    }
    else
    {
        // nothing was chosen
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{    
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("splitter",ui->splitter->saveState());
    settings.setValue("splitter_2",ui->splitter_2->saveState());
    settings.setValue("splitter_3",ui->splitter_3->saveState());
    settings.setValue("splitter_4",ui->splitter_4->saveState());
    settings.setValue("iptvurl", ui->edtUrl->text());
    settings.setValue("iptvepgurl", ui->edtUrlEpg->text());
    settings.sync();

    _player->stop();

    if (maybeSave()) {
        //writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About Application"),
            tr("The <b>Application</b> example demonstrates how to "
               "write modern GUI applications using Qt, with a menu bar, "
               "toolbars, and a status bar."));
}

void MainWindow::createStatusBar()
{
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_progress);

    m_progressCancel = new QPushButton(this);
    m_progressCancel->setText("cancel");
    m_progressCancel->setVisible(false);

    connect(m_progressCancel, SIGNAL(clicked()), this, SLOT(progressCancel_clicked()));
    statusBar()->addPermanentWidget(m_progressCancel);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createActions() {

     QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

     QAction *aboutAct = helpMenu->addAction(this->style()->standardIcon(QStyle::SP_TitleBarMenuButton), tr("&About"), this, &MainWindow::about);
     aboutAct->setStatusTip(tr("Show the application's About box"));

     QAction *aboutQtAct = helpMenu->addAction(this->style()->standardIcon(QStyle::SP_TitleBarMenuButton), tr("About &Qt"), qApp, &QApplication::aboutQt);
     aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

     menuBar()->addSeparator();
}

bool MainWindow::maybeSave()
{
    if (!somethingchanged)
        return true;

    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return true;
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void MainWindow::on_edtLoad_clicked() {

    QString fileName = QFileDialog::getOpenFileName(this, ("Open m3u File"),
                                                     m_AppDataPath,
                                                     ("m3u Listen (*.m3u)"));

    if ( !fileName.isNull() ) {
        qDebug() << "selected file path : " << fileName.toUtf8();
        this->getFileData(fileName);
    }
}

void MainWindow::getFileData(const QString &filename)
{
    int counter = 0;
    int linecount = 0;
    bool ende = false;
    int obsolete = 0;
    int newfiles = 0;

    QString tags;
    QString station;
    QString url;
    QStringList parser;

    QString tvg_name;
    QString tvg_id;
    QString tvg_chno;
    QString group_title, last_group_title;
    int     group_id, pls_id, plsi_id, extinf_id;
    QString tvg_logo;

    QSqlQuery *query = nullptr;

    QFile file(filename);

    if(!file.exists()){
        qDebug() << "File <i>cannot</i> be found "<<filename;
    }

    QString line;

    db.deactivateEXTINFs();

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)){

        QTextStream stream(&file);
        stream.setCodec("UTF-8");

        statusBar()->showMessage(tr("preparing data..."));

        while (!stream.atEnd()){

            line = stream.readLine();

            if ( line.startsWith("http") or line.startsWith("rtp") ) {
              linecount++;
            }
        }

        qDebug() << "Lines" << linecount;

        statusBar()->repaint();

        m_progress->setMinimum(0);
        m_progress->setMaximum(linecount);
        m_progress->setVisible(true);
        m_progressCancel->setVisible(true);
        m_ProgressWasCanceled = false;

#ifdef Q_OS_WIN
        taskbarProgress->setMinimum(0);
        taskbarProgress->setMaximum(linecount);
        taskbarProgress->setVisible(true);
#endif

        stream.seek(0);

        while (!stream.atEnd() && !ende){

            line = stream.readLine();

            if (m_ProgressWasCanceled)
                ende = true;

            if ( line.contains("#EXTINF") ) {

                tags = line.split(",").at(0);
                station = line.split(",").at(1);

            } else if ( line.startsWith("http") or line.startsWith("rtp") ) {

                url = line;

                parser = this->splitCommandLine(tags);

                tvg_name = station;
                group_title = "NoGroup";
                tvg_logo = " ";
                tvg_chno = "0";

                foreach(QString item, parser) {

                    if ( item.contains("tvg-name") ) {
                        tvg_name = item.split("=").at(1);
                    }
                    else if ( item.contains("tvg-id") ) {
                        tvg_id = item.split("=").at(1);
                    }
                    else if ( item.contains("group-title") ) {
                        group_title = item.split("=").at(1);
                    }
                    else if ( item.contains("tvg-logo") ) {
                        tvg_logo = item.split("=").at(1);
                    }
                    else if ( item.contains("tvg-chno") ) {
                        tvg_chno = item.split("=").at(1);
                    }
                }

                // ------------------------------------------------
                query = db.selectGroup_byTitle(group_title);
                query->last();
                query->first();

                if ( query->isValid() ) {
                    group_id = query->value(0).toByteArray().toInt();
                } else {
                    group_id = db.addGroup(group_title);
                }

                delete query;

                // ------------------------------------------------
                query = db.selectEXTINF_byUrl(url);

                query->last();
                query->first();

                if ( query->isValid() ) {

                    extinf_id = query->value(0).toByteArray().toInt();

                    if ( ! db.updateEXTINF_byRef(extinf_id, tvg_name, group_id, tvg_logo, 1 ) ) {
                        qDebug() << "-E-" << "updateEXTINF_byRef" <<tvg_name<< tvg_id<< group_title<< tvg_logo<< url;
                    }
                } else {

                    extinf_id = db.insertEXTINF(tvg_name, tvg_id, group_id, tvg_logo, url)                    ;

                    if ( extinf_id == 0 ) {
                        qDebug() << "-E-" << "addEXTINF" << tvg_name<< tvg_id<< group_title<< tvg_logo<< url;
                    }
                    newfiles++;
                }

                delete query;

                // ------------------------------------------------
                // Playlist anlegen
                // ------------------------------------------------
                query = db.selectPLS_by_pls_name(group_title);

                query->last();
                query->first();

                if ( query->isValid() ) {
                    pls_id = query->value(0).toByteArray().toInt();
                } else {
                    pls_id = db.insertPLS(group_title);
                }

                delete query;

                // ------------------------------------------------
                // Playlist Item anlegen
                // ------------------------------------------------
                query = db.selectPLS_Items_by_key(pls_id, extinf_id);

                query->last();
                query->first();

                if ( ! query->isValid() ) {

                    plsi_id = db.insertPLS_Item( pls_id, extinf_id, tvg_chno.toInt() );

                    if ( plsi_id == 0 ) {
                        qDebug() << "-E-" << "insertPLS_Item" << pls_id<< extinf_id<< tvg_chno.toInt();
                    }
                }

                delete query;
                // ------------------------------------------------

                counter++;

                if ( counter % 100 == 0 ) {
                    statusBar()->showMessage(tr("%1 stations").arg(counter));                    
                }

                QCoreApplication::processEvents();

                m_progress->setValue(counter);
#ifdef Q_OS_WIN
                taskbarProgress->setValue(counter);
#endif
            }
        }
    }

    file.close();

#ifdef Q_OS_WIN
    taskbarProgress->setVisible(false);
#endif
    m_progress->setVisible(false);
    m_progressCancel->setVisible(false);

    QSqlQuery *test;

    test = db.countEXTINF_byState();

    while ( test->next() ) {

        qDebug() << "obsolete" << test->value(0).toByteArray().constData() << test->value(1).toByteArray().constData();

        if( test->value(0).toByteArray().toInt() == 0 ) {
            obsolete = test->value(1).toByteArray().toInt();
        }
    }

    delete test;

    if ( obsolete > 0 ) {

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "m3uMan", QString("Remove %1 obsolete stations?").arg(obsolete),
                                      QMessageBox::Yes|QMessageBox::No);

        if (reply == QMessageBox::Yes) {

            db.removeObsoleteEXTINFs();
        }
    }

    if ( newfiles > 0 ) {
        QMessageBox::information(this, "m3uMan", QString("%1 new stations added!").arg(newfiles), QMessageBox::Ok);
    }

    fillComboGroupTitels();
    fillTreeWidget();
    fillComboPlaylists();
}

QStringList MainWindow::splitCommandLine(const QString & cmdLine)
{
    QStringList list;
    QString arg;

    bool escape = false;
    enum { Idle, Arg, QuotedArg } state = Idle;

    foreach (QChar const c, cmdLine) {
        if (!escape && c == '\\') { escape = true; continue; }
        switch (state) {
        case Idle:
            if (!escape && c == '"') state = QuotedArg;
            else if (escape || !c.isSpace()) { arg += c; state = Arg; }
            break;
        case Arg:
            if (!escape && c == '"') state = QuotedArg;
            else if (escape || !c.isSpace()) arg += c;
            else { list << arg; arg.clear(); state = Idle; }
            break;
        case QuotedArg:
            if (!escape && c == '"') state = arg.isEmpty() ? Idle : Arg;
            else arg += c;
            break;
        }
        escape = false;
    }
    if (!arg.isEmpty()) list << arg;
    return list;
}

QTreeWidgetItem* MainWindow::addTreeRoot(const QString& name, const QString& description, const QString& id, int favorite)
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->treeWidget);

    treeItem->setText(0, name);
    treeItem->setText(1, description);
    treeItem->setText(2, id);

    if ( favorite == 1 ) {
         treeItem->setBackground(0, QColor("orange") );
    }

    return treeItem;
}

void MainWindow::addTreeChild(QTreeWidgetItem *parent, const QString& name, const QString& description, const QString& id, const QString& used, const QString& state, const QString& url, const QString& logo)
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem();

    treeItem->setText(0, name);
    treeItem->setText(1, description);

    if ( state.toInt() == 1 ) {
         treeItem->setBackground(2, QColor("#FFCCCB") ); // light red (active stream )
    } else if ( state.toInt() == 2 ) {
         treeItem->setBackground(2, QColor("#90ee90") ); // light green (new stream)
    }

    treeItem->setText(2, id);
    treeItem->setData(Qt::UserRole, 0, url);

    treeItem->setText(3, logo);

    treeItem->setStatusTip(0, tr("double click to add the station to the selected playlist"));

    if ( used.toInt() > 0 ) {
        treeItem->setBackground(0, QColor("#4CAF50") );
    }

    parent->addChild(treeItem);

}

void MainWindow::fillTreeWidget()
{
    QString group, lastgroup;
    QString title;
    QString id;
    QString tvg_id, tvg_logo;
    QString used;
    QString state;
    QString url;
    QString favorite;
    QString group_id;

    // Add root nodes
    QTreeWidgetItem *item = nullptr;
    QSqlQuery *select = nullptr;

    ui->treeWidget->clear();
    ui->treeWidget->setColumnCount(4);
    ui->treeWidget->setHeaderLabels(QStringList() << "Group" << "Station" << "ID" << "Logo");

    if ( ui->cboGroupTitels->currentText().isEmpty() ) {
        group = "EU |";
    } else {
        group = ui->cboGroupTitels->currentText();
    }

    if ( ! ui->edtFilter->text().isEmpty() ) {
        group = "";
    }

    if ( ui->radAll->isChecked() ) {
        state = "0";
    } else if ( ui->radNew->isChecked() ) {
        group = "";
        state = "2";
    }

    if ( ui->chkOnlyFavorites->isChecked() ) {
        group = "";
        favorite = "1";
    } else {
        favorite = "0";
    }

    select = db.selectEXTINF(group, ui->edtFilter->text(), state, favorite.toInt() );

    ui->treeWidget->blockSignals(true);

    while ( select->next() ) {

        group = select->value(8).toByteArray().constData();

        if (group.isEmpty()) {
            group = " ";
        }

        title = select->value(1).toByteArray().constData();
        tvg_id = select->value(2).toByteArray().constData();
        tvg_logo = select->value(4).toByteArray().constData();
        id = select->value(0).toByteArray().constData();
        group_id = select->value(3).toByteArray().constData();
        url = select->value(5).toByteArray().constData();
        used = select->value(10).toByteArray().constData();
        state = select->value(6).toByteArray().constData();
        favorite = select->value(9).toByteArray().constData();

        if ( group != lastgroup ) {
            item = addTreeRoot(group, "", group_id, favorite.toInt());
            lastgroup = group;
        }

        addTreeChild(item, title, tvg_id, id, used, state, url, tvg_logo);
    }

    ui->treeWidget->blockSignals(false);

    delete select;
}

void MainWindow::fillComboPlaylists()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;
    int      favorite = 0;

    ui->cboPlaylists->clear();

    if ( ui->chkPlaylistOnlyFavorits->isChecked() ) {
        favorite = 1;
    }

    select = db.selectPLS(favorite);
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        title = select->value(1).toByteArray().constData();

        ui->cboPlaylists->addItem(title, id);
    }

    delete select;

    ui->cmdDeletePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );
    ui->cmdRenamePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );   
    ui->cmdAddToFavorits->setEnabled( ui->cboPlaylists->count() != 0 );
}


void MainWindow::fillComboGroupTitels()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;
    int favorite = 0;

    ui->cboGroupTitels->clear();

    if ( ui->chkOnlyFavorites->isChecked() ) {
        favorite = 1;
    }

    select = db.selectGroups( favorite );
    while ( select->next() ) {

        id    = select->value(0).toByteArray().constData();
        title = select->value(1).toByteArray().constData();

        ui->cboGroupTitels->addItem(title, id);
    }

    delete select;
}

void MainWindow::on_cmdNewPlaylist_clicked()
{
    bool ok;

    QString text = QInputDialog::getText(this, "The Playlist",
                                         "What's the name of the new playlist?", QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty()) {

        db.insertPLS(text);
        fillComboPlaylists();

        QMessageBox::information (this, "The Playlist",
                                  QString("Playlist '%1' added...").arg(text));

        ui->cboPlaylists->setCurrentText(text);
    }
}

void MainWindow::on_cmdDeletePlaylist_clicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "The Playlist " + ui->cboPlaylists->currentText(), "Do you really want to delete the playlist?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {

        db.removePLS( ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt() );

        fillComboPlaylists();
    } else {
      qDebug() << "Yes was *not* clicked";
    }
}

void MainWindow::on_cmdRenamePlaylist_clicked()
{
    bool ok;
    int id;

    QString text = QInputDialog::getText(this, "The Playlist",
                                         "What's the name of the playlist?", QLineEdit::Normal,
                                         ui->cboPlaylists->currentText(), &ok);
    if (ok && !text.isEmpty()) {

        id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

        db.updatePLS(id, text);
        fillComboPlaylists();

        QMessageBox::information (this, "The Playlist",
                                  QString("Playlist %1 modified...").arg(text));
    }
}


void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    QList<QTreeWidgetItem*>items = ui->treeWidget->selectedItems();

    foreach( QTreeWidgetItem* mitem, items) {

        int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();
        int extinf_id = mitem->text(2).toInt();
        int pls_pos = ui->twPLS_Items->topLevelItemCount() +1;

        db.insertPLS_Item( pls_id, extinf_id, pls_pos );
    }

    ui->treeWidget->clearSelection();
    fillTwPls_Item();
}

void MainWindow::fillTwPls_Item()
{
    QString id;
    QString extinf_id;
    QString tvg_name, tvg_id, pls_pos;
    QString logo;
    QString url;
    bool    added = false;
    QFile   file;
    QPixmap buttonImage, topImage;

    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    // Add root nodes
    QSqlQuery *select = nullptr;

    ui->twPLS_Items->clear();
    ui->twPLS_Items->setColumnCount(3);
    ui->twPLS_Items->setHeaderLabels(QStringList() << "Place" << "Stations"  << "EPG");

    for(int i = 0; i < 3; i++)
        ui->twPLS_Items->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    ui->lvStations->clear();
    ui->lvStations->setFlow(QListView::Flow::LeftToRight);

    select = db.selectPLS_Items(pls_id);
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        extinf_id = select->value(2).toByteArray().constData();
        pls_pos = select->value(3).toByteArray().constData();
        logo = select->value(9).toByteArray().constData();
        tvg_name = select->value(6).toByteArray().constData();
        tvg_id = select->value(7).toByteArray().constData();
        url = select->value(10).toByteArray().constData();

        QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->twPLS_Items);

        treeItem->setText(0, QString("%1").arg(pls_pos.toInt() + 1) );
        treeItem->setText(1, tvg_name);
        treeItem->setText(2, tvg_id);

        treeItem->setData(0, 1, id);
        treeItem->setData(1, 1, extinf_id);
        treeItem->setStatusTip(0, tr("double click to remove the station"));

        if ( QUrl(logo).fileName().trimmed().isEmpty() ) {

            buttonImage = QPixmap(":/images/iptv.png");

        } else {

            file.setFileName(m_AppDataPath + "/logos/" + QUrl(logo).fileName());
            if ( file.exists() && file.size() > 0 ) {

                file.open(QIODevice::ReadOnly);

                buttonImage.loadFromData(file.readAll());

                file.close();
            } else {
                buttonImage = QPixmap(":/images/iptv.png");
            }
        }

        QListWidgetItem* item = new QListWidgetItem(buttonImage, "");

        item->setData(Qt::UserRole, url);
        item->setData(Qt::UserRole+1, tvg_id);
        item->setData(Qt::DecorationRole, buttonImage.scaled(50,50,Qt::KeepAspectRatio, Qt::SmoothTransformation));
        item->setToolTip(tvg_name);

        ui->lvStations->addItem(item);

        added = true;
    }

    delete select;

    ui->cmdMoveUp->setEnabled( added );
    ui->cmdMoveDown->setEnabled( added );
    ui->edtStationUrl->setText("");

/*
    buttonImage = QPixmap(":/images/template.png");

    topImage = QPixmap("C:/Users/Developer/Downloads/62fb8958ce481.svg");
    QPainter painter(&buttonImage);
    painter.drawPixmap(10, 10, buttonImage.width()-20, buttonImage.height()-20, topImage);

    ui->lblLogo->setPixmap(buttonImage.scaled(100,100,Qt::KeepAspectRatio, Qt::SmoothTransformation));
*/
}

void MainWindow::on_cboPlaylists_currentTextChanged(const QString &arg1)
{
    ui->lblLogo->clear();
    ui->cboEPGChannels->setCurrentText(" ");

    fillTwPls_Item();
}

void MainWindow::on_twPLS_Items_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    db.removePLS_Item( item->data(0, 1).toInt() );

    fillTwPls_Item();
}

void MainWindow::on_cmdMoveUp_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();
    int              row  = ui->twPLS_Items->currentIndex().row();

    if (item && row > 0)
    {
        ui->twPLS_Items->takeTopLevelItem(row);
        ui->twPLS_Items->insertTopLevelItem(row - 1, item);
        ui->twPLS_Items->setCurrentItem(item);

        somethingchanged = true;
    }
}

void MainWindow::on_cmdMoveDown_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();
    int              row  = ui->twPLS_Items->currentIndex().row();

    if (item && row >= 0)
    {
        ui->twPLS_Items->takeTopLevelItem(row);
        ui->twPLS_Items->insertTopLevelItem(row + 1, item);
        ui->twPLS_Items->setCurrentItem(item);

        somethingchanged = true;
    }
}

void MainWindow::MakePlaylist()
{
    QTreeWidgetItem *item;
    int             extinf_id;
    QSqlQuery       *select;
    QString         group;
    QString         title;
    QString         id;
    QString         tvg_id;
    QString         logo;
    QString         url;
    QDir            dir;

    QString fileName = dir.homePath() + "/"+ ui->cboPlaylists->currentText() + ".m3u";

    QFile file( fileName );

    if(!file.open(QFile::WriteOnly |
                  QFile::Text))
    {
        qDebug() << " Could not open file '" + fileName + "' for writing";
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    QTextStream out(&file);

    out << "#EXTM3U\n";

    for(int i=0;i<ui->twPLS_Items->topLevelItemCount();++i) {

        item = ui->twPLS_Items->topLevelItem(i);
        extinf_id = item->data(1, 1).toInt();

        select = db.selectEXTINF_byRef(extinf_id);
        while ( select->next() ) {

            id = select->value(0).toByteArray().constData();
            title = select->value(1).toByteArray().constData();
            tvg_id = select->value(2).toByteArray().constData();
            group = select->value(8).toByteArray().constData();
            logo = select->value(4).toByteArray().constData();
            url = select->value(5).toByteArray().constData();

            group = ui->cboPlaylists->currentText();

            out << QString("#EXTINF:-1 tvg-chno=\"%6\" tvg-name=\"%1\" tvg-id=\"%2\" group-title=\"%3\" tvg-logo=\"%4\",%1\n%5\n").arg(title).arg(tvg_id).arg(group).arg(logo).arg(url).arg(i+1);
        }

        delete select;

        QGuiApplication::restoreOverrideCursor();
    }

    file.flush();
    file.close();

    somethingchanged = false;

    statusBar()->showMessage(tr("File saved"), 2000);
}

void MainWindow::on_edtFilter_returnPressed()
{
    ui->cboGroupTitels->blockSignals(true);
    ui->cboGroupTitels->setCurrentText("");
    ui->cboGroupTitels->blockSignals(false);
    fillTreeWidget();
}

void MainWindow::on_cboGroupTitels_currentTextChanged(const QString &arg1)
{
    ui->edtFilter->setText("");
    fillTreeWidget();
}

void MainWindow::on_edtDownload_clicked()
{
    if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Downloader", "start the download?", QMessageBox::Yes|QMessageBox::No).exec())  {

        QUrl imageUrl(ui->edtUrl->text());
        m_pImgCtrl = new FileDownloader(imageUrl, this);

        connect(m_pImgCtrl, SIGNAL (downloaded()), this, SLOT (SaveM3u()));
        connect(m_pImgCtrl, SIGNAL (progress()), this, SLOT (ShowDownloadProgress()));
    }
}

void MainWindow::ShowDownloadProgress(){

    statusBar()->showMessage(tr("file download %1%").arg(m_pImgCtrl->downloadedProgress()), 2000);
}

void MainWindow::SaveM3u()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString timestamp = now.toString(QLatin1String("yyyyMMddhhmmss"));

    const QString filename = m_AppDataPath + "/" + QString::fromLatin1("/iptv-%1.m3u").arg(timestamp);

    QFile newDoc(filename);

    if ( m_pImgCtrl->downloadedData().size() > 0 ) {
        if(newDoc.open(QIODevice::WriteOnly)){

            newDoc.write(m_pImgCtrl->downloadedData());
            newDoc.close();

            statusBar()->showMessage(tr("File %1 download success!").arg(filename), 5000);

            if (ui->chkImport->isChecked() ) {
                this->getFileData(filename);
            } else {
                QMessageBox(QMessageBox::Information, "Downloader", tr("File %1 download success!").arg(filename), QMessageBox::Ok).exec() ;
            }
        }
    } else {
        QMessageBox(QMessageBox::Critical, "Downloader", tr("File %1 download fails!").arg(filename), QMessageBox::Ok).exec() ;
    }   
}

void MainWindow::SaveXML()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString timestamp = now.toString(QLatin1String("yyyyMMddhhmmss"));

    const QString filename = m_AppDataPath + "/" + QString::fromLatin1("/epg-%1.xml").arg(timestamp);

    QFile newDoc(filename);

    if ( m_pImgCtrl->downloadedData().size() > 0 ) {
        if(newDoc.open(QIODevice::WriteOnly)){

            newDoc.write(m_pImgCtrl->downloadedData());
            newDoc.close();

            statusBar()->showMessage(tr("File %1 download success!").arg(filename), 5000);

            if (ui->chkEPGImport->isChecked() ) {
                this->getEPGFileData(filename);
            } else {
                QMessageBox(QMessageBox::Information, "Downloader", tr("File %1 download success!").arg(filename), QMessageBox::Ok).exec() ;
            }
        }
    } else {
        QMessageBox(QMessageBox::Critical, "Downloader", tr("File %1 download fails!").arg(filename), QMessageBox::Ok).exec() ;
    }
}


void MainWindow::on_cmdGatherData_clicked()
{
    QString program = "ffprobe";
    QStringList arguments;

    arguments << "-hide_banner" << ui->edtStationUrl->text();

    m_Process->setProcessChannelMode(QProcess::MergedChannels);
    m_Process->start(program, arguments);
}

void MainWindow::on_twPLS_Items_itemSelectionChanged()
{
    QSqlQuery *select;
    QString   group;
    QString   title;
    QString   id;
    QString   tvg_id;
    QString   logo;
    QString   url;
    QString   desc;
    QString   tmdb_id;
    QPixmap   buttonImage;

    QList<QTreeWidgetItem*>items = ui->twPLS_Items->selectedItems();

    foreach( QTreeWidgetItem* mitem, items) {

        int extinf_id = mitem->data(1,1).toInt();

        select = db.selectEXTINF_byRef(extinf_id);
        while ( select->next() ) {

            id = select->value(0).toByteArray().constData();
            title = select->value(1).toByteArray().constData();
            tvg_id = select->value(2).toByteArray().constData();
            group = select->value(3).toByteArray().constData();
            logo = select->value(4).toByteArray().constData();
            url = select->value(5).toByteArray().constData();
        }

        delete select;

        select = db.selectPLS_Items_by_extinf_id(extinf_id);
        while ( select->next() ) {

            tmdb_id = select->value(4).toByteArray().constData();
        }

        delete select;

        ui->cboEPGChannels->setCurrentText(tvg_id);
        ui->edtStationUrl->setText(url);
        ui->edtStationName->setText(title);

        m_actualTitle = title;

        if ( m_actualTitle.contains("|") ) {
            m_actualTitle = m_actualTitle.mid(4);
        }

        setWindowTitle(m_actualTitle);

        if ( url.contains("mkv") || url.contains("mp4") ) {

            ui->cmdImdb->setEnabled(true);

            qDebug() << "getTMDBdate" << QUrl::toPercentEncoding(m_actualTitle, "-/:") << extinf_id << tmdb_id;

            this->getTMDBdate(QUrl::toPercentEncoding(m_actualTitle, "-/:"), tmdb_id.toInt(), extinf_id);

        } else {

            ui->cmdImdb->setEnabled(false);

            select = db.selectProgramData(tvg_id);

            while ( select->next() ) {

                title = select->value(4).toByteArray().constData();
                desc = select->value(5).toByteArray().constData();
            }

            ui->edtOutput->clear();
            ui->edtOutput->append(title);
            ui->edtOutput->append("");
            ui->edtOutput->append(desc);

            delete select;
        }

        if ( ! logo.trimmed().isEmpty() ) { // z.B.: https://lo1.in/ger/dsr.png

            QFileInfo fi(logo.trimmed());

            QFile file(m_AppDataPath + "/logos/" + fi.fileName());

            if ( ( ! file.exists() ) || ( file.size() == 0 ) ) {

                m_pImgCtrl = new FileDownloader(logo, this);
                connect(m_pImgCtrl, SIGNAL(downloaded()), SLOT(loadImage()));

                qDebug() << "requesting logo..." << file.fileName() << id << logo;

            } else {

                file.open(QIODevice::ReadOnly);
                QTextStream in(&file);
                buttonImage.loadFromData(file.readAll());
                file.close();

                ui->lblLogo->setPixmap(buttonImage.scaledToWidth(ui->lblLogo->maximumWidth()));
            }

        } else {
            const QPixmap buttonImage (":/images/iptv.png");
            ui->lblLogo->setPixmap(buttonImage.scaledToWidth(ui->lblLogo->maximumWidth()));
        }

        ui->lvStations->setCurrentRow( ui->twPLS_Items->indexOfTopLevelItem( mitem ) );
        //ui->lvStations->setCurrentRow( ui->twPLS_Items->currentIndex().row() );
    }
}

void MainWindow::processStarted()
{
    qDebug() << "processStarted()";

    m_OutputString.clear();
    ui->edtOutput->setText(m_OutputString);
}

void MainWindow::readyReadStandardOutput()
{
    m_OutputString.append(m_Process->readAllStandardOutput());
    ui->edtOutput->setText(m_OutputString);
}

void MainWindow::processFinished()
{
    qDebug() << "processFinished()";
}

void MainWindow::loadImage()
{
    QListWidgetItem *item;

    QPixmap buttonImage;
    dir.mkpath(m_AppDataPath + "/logos/");

    QString filename = m_AppDataPath + "/logos/" + m_pImgCtrl->filename();

    QFile file(filename);

    if ( ( ! file.exists() ) || ( file.size() == 0 ) ) {

        file.open(QIODevice::WriteOnly);
        QTextStream out(&file);
        file.write(m_pImgCtrl->downloadedData());
        file.close();
    }

    if ( file.exists() && file.size() > 0 ) {

        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        buttonImage.loadFromData(file.readAll());
        file.close();

        ui->lblLogo->setPixmap(buttonImage.scaledToWidth(ui->lblLogo->maximumWidth()));

        item = ui->lvStations->currentItem();
        item->setData(Qt::DecorationRole, buttonImage.scaled(50,50,Qt::KeepAspectRatio, Qt::SmoothTransformation));

    } else {
        ui->lblLogo->setText("no data!");
    }
}

void MainWindow::on_cmdPlayStream_clicked()
{
    if (ui->edtStationUrl->text().isEmpty())
        return;

    _media = new VlcMedia(ui->edtStationUrl->text(), _instance);

    _player->open(_media);
}

void MainWindow::on_edtStationUrl_textChanged(const QString &arg1)
{
     ui->cmdGatherData->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdPlayExtern->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdPlayStream->setEnabled(arg1.trimmed().length() > 0);
}

void MainWindow::on_lvStations_itemClicked(QListWidgetItem *item)
{
    QVariant url      =  item->data(Qt::UserRole);
    QVariant tvg_id   =  item->data(Qt::UserRole+1); // zdf.de

    ui->twPLS_Items->clearSelection();
    ui->twPLS_Items->topLevelItem( ui->lvStations->currentRow() )->setSelected(true);

    ui->twPLS_Items->scrollToItem( ui->twPLS_Items->selectedItems().at(0) );
}

void MainWindow::on_cmdSavePosition_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();

    if ( item != nullptr ) {

        int extinf_id = item->data(1,1).toInt();

        db.updateEXTINF_tvg_id_byRef(extinf_id, ui->cboEPGChannels->currentText());
        db.updateEXTINF_url_byRef(extinf_id, ui->edtStationUrl->text());
    }

    for (int i = 0; i < ui->twPLS_Items->topLevelItemCount(); i++) {

        item = ui->twPLS_Items->topLevelItem(i);

        db.updatePLS_item_pls_pos(item->data(0, 1).toInt() , i );
    }

    fillTwPls_Item();

    MakePlaylist();

    statusBar()->showMessage("positions set...");
}

void MainWindow::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    QVariant url ;

    if ( current != nullptr ) {
        url = current->data(Qt::UserRole,0).toString();
    }

    if ( ! url.toString().isEmpty() and ui->chkAutoPlay->isChecked() ) {

        _media = new VlcMedia(url.toString(), _instance);

        _player->open(_media);
    }
}

void MainWindow::on_cmdImportEpg_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, ("Open xmp program File"),
                                                     m_AppDataPath,
                                                     ("xml program file (*.xml)"));

    if ( !fileName.isNull() ) {
        qDebug() << "selected file path : " << fileName.toUtf8();
        this->getEPGFileData(fileName);
    }
}

void MainWindow::getEPGFileData(const QString &sFileName)
{
    QFile   *xmlFile;
    QXmlStreamReader *xmlReader;
    int     linecount=0;
    QString line;
    bool    ende = false;

    QString start, stop, channel, title, desc;

    db.removeAllPrograms();

    m_progress->setMinimum(0);
    m_progress->setMaximum(0);
    m_progress->setVisible(false);
    m_progressCancel->setVisible(true);
    m_ProgressWasCanceled = false;

#ifdef Q_OS_WIN
    taskbarProgress->setMinimum(0);
    taskbarProgress->setMaximum(0);
    taskbarProgress->setVisible(true);
#endif

    xmlFile = new QFile(sFileName);

    if (!xmlFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(this,"Load XML File Problem",
            tr("Couldn't open %1 to load settings for download").arg(sFileName),
            QMessageBox::Ok);
            return;
    }

    xmlReader = new QXmlStreamReader(xmlFile);


    //Parse the XML until we reach end of it
    while ( !xmlReader->atEnd() && !xmlReader->hasError() && !ende ) {

            if (m_ProgressWasCanceled)
               ende = true;

            // Read next element
            QXmlStreamReader::TokenType token = xmlReader->readNext();

            //If token is StartElement - read it
            if ( token == QXmlStreamReader::StartElement ) {

                    if ( xmlReader->name() == "programme" ) {
                        start = xmlReader->attributes().value("start").toString();
                        stop = xmlReader->attributes().value("stop").toString();
                        channel = xmlReader->attributes().value("channel").toString();
                    } else if ( xmlReader->name() == "title" ) {
                        title = xmlReader->readElementText();
                    } else if ( xmlReader->name() == "desc" ) {
                        desc = xmlReader->readElementText();
                    }
            }

            if(token == QXmlStreamReader::EndElement) {

                if(xmlReader->name() == "programme") {
                    db.addProgram(start, stop, channel, title, desc);
                    start = stop = channel = title = desc = "";
                }
            }

            linecount++;

            if ( linecount % 100 == 0 ) {

                statusBar()->showMessage(tr("%1 programs").arg(linecount));
            }

            QCoreApplication::processEvents();
    }

    if(xmlReader->hasError()) {
        QMessageBox::critical(this, "xmlFile.xml Parse Error", xmlReader->errorString(), QMessageBox::Ok);
        return;
    }

    //close reader and flush file
    xmlReader->clear();
    xmlFile->close();

#ifdef Q_OS_WIN
    taskbarProgress->setVisible(false);
#endif
    m_progress->setVisible(false);
    m_progressCancel->setVisible(false);
}

void MainWindow::on_edtEPGDownload_clicked()
{
    if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Downloader", "start the download?", QMessageBox::Yes|QMessageBox::No).exec())  {

        QUrl imageUrl(ui->edtUrlEpg->text());
        m_pImgCtrl = new FileDownloader(imageUrl, this);

        connect(m_pImgCtrl, SIGNAL (downloaded()), this, SLOT (SaveXML()));
    }
}


void MainWindow::on_radAll_clicked()
{
    fillComboGroupTitels();
    fillTreeWidget();
}

void MainWindow::on_radNew_clicked()
{
    fillComboGroupTitels();
    fillTreeWidget();
}

void MainWindow::showVlcError()
{    
    qDebug() << "*******" << _error->errmsg();
}

void MainWindow::on_cmdPlayMoveDown_clicked()
{
    ui->twPLS_Items->clearSelection();
    ui->twPLS_Items->setCurrentItem(ui->twPLS_Items->itemBelow(ui->twPLS_Items->currentItem() ));
    ui->twPLS_Items->setItemSelected( ui->twPLS_Items->currentItem(), true );
}

void MainWindow::on_cmdPlayMoveUp_clicked()
{
    ui->twPLS_Items->clearSelection();
    ui->twPLS_Items->setCurrentItem(ui->twPLS_Items->itemAbove(ui->twPLS_Items->currentItem() ));
    ui->twPLS_Items->setItemSelected( ui->twPLS_Items->currentItem(), true );
}

void MainWindow::on_cmdMoveForward_clicked()
{
    _player->setTime( _player->time() + 60 * 1000 );
}

void MainWindow::on_cmdMoveBackward_clicked()
{
    _player->setTime( _player->time() - 60 * 1000 );
}

void MainWindow::findAllButtons() {

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    m_IconColor = settings.value("iconcolor", "black").toByteArray();

    QList<QPushButton *> buttons = this->findChildren<QPushButton *>();

    QList<QPushButton *>::const_iterator iter;
    for (iter = buttons.constBegin(); iter != buttons.constEnd(); ++iter) {

          QPushButton *button = *iter;

          if( ! button->icon().isNull() ) {

              button->setIcon( this->changeIconColor( button->icon(), QColor(m_IconColor) ) );
          }
    }
}

QPixmap MainWindow::changeIconColor(QIcon icon, QColor color) {

    QImage tmpImage = icon.pixmap(20).toImage();

    // Loop all the pixels
    for(int y = 0; y < tmpImage.height(); y++) {
      for(int x= 0; x < tmpImage.width(); x++) {

        // Read the alpha value each pixel, keeping the RGB values of your color
        color.setAlpha(tmpImage.pixelColor(x,y).alpha());

        // Apply the pixel color
        tmpImage.setPixelColor(x,y,color);
      }
    }

    return QPixmap::fromImage(tmpImage);
}

void MainWindow::on_actionIcon_color_triggered()
{
    QColor color = QColorDialog::getColor(Qt::black, this );

    if ( color.isValid() ) {

        qDebug() << m_SettingsFile;

        QSettings settings(m_SettingsFile, QSettings::IniFormat);
        settings.setValue("iconcolor", color.name());
        settings.sync();

        this->findAllButtons();
    }
}

void MainWindow::on_chkOnlyFavorites_stateChanged(int arg1)
{
    fillComboGroupTitels();
    fillTreeWidget();
}

void MainWindow::on_cmdEqualizer_clicked()
{
    _equalizerDialog->show();

}

void MainWindow::on_cmdMute_clicked()
{
    ui->widVolume->setMute( ! ui->widVolume->mute() );

    if ( ui->widVolume->mute() ) {
        ui->cmdMute->setIcon( this->changeIconColor( QIcon(":/images/Ui/icons8-mute-50.png"), QColor(m_IconColor) ) );
    } else {
        ui->cmdMute->setIcon( this->changeIconColor( QIcon(":/images/Ui/icons8-audio-50.png"), QColor(m_IconColor) ) );
    }
}

void MainWindow::getTMDBdate(const QString &title, int tmdb_id, int extinf_id)
{
    QString filename = m_AppDataPath + "/tmdb/" + QString("%1.txt").arg(tmdb_id);
    QFile   file(filename);

    if ( tmdb_id > 0 && file.exists() && file.size() > 0 ) {

        qDebug() << "use local tmdb data..." << filename;

        if ( file.open(QIODevice::ReadOnly) ) {

            QString strReply = file.readAll();

            file.close();

            this->displayMovieInfo(extinf_id, strReply, true);
        }

    } else {

        m_nam = new QNetworkAccessManager(this);
        QObject::connect(m_nam, SIGNAL(finished(QNetworkReply*)),
                 this, SLOT(serviceRequestFinished(QNetworkReply*)));

        QUrl url(QString("https://api.themoviedb.org/3/search/movie?query=%1&language=de-DE&api_key=%2").arg(title).arg("6c125ca74f059b4c88bc49e1b09e241e"));

        QNetworkReply* reply = m_nam->get(QNetworkRequest(url));

        reply->setProperty("extinf_id", extinf_id);

        qDebug() << url;
    }
}

void MainWindow::getTMDBdataById(int tmdb_id, int extinf_id)
{
    QString filename = m_AppDataPath + "/tmdb/" + QString("%1.txt").arg(tmdb_id);
    QFile   file(filename);

    if ( tmdb_id > 0 && file.exists() && file.size() > 0 ) {

        qDebug() << "use local tmdb data..." << filename;

        if ( file.open(QIODevice::ReadOnly) ) {

            QString strReply = file.readAll();

            file.close();

            this->displayMovieInfo(extinf_id, strReply, true);
        }

    } else {

        m_nam = new QNetworkAccessManager(this);
        QObject::connect(m_nam, SIGNAL(finished(QNetworkReply*)),
                 this, SLOT(serviceRequestFinished(QNetworkReply*)));

        QUrl url(QString("https://api.themoviedb.org/3/movie/%1?language=de-DE&api_key=%2").arg(tmdb_id).arg("6c125ca74f059b4c88bc49e1b09e241e"));

        QNetworkReply* reply = m_nam->get(QNetworkRequest(url));

        reply->setProperty("extinf_id", extinf_id);

        qDebug() << url;
    }
}

void MainWindow::serviceRequestFinished(QNetworkReply* reply)
{
    if(reply->error() == QNetworkReply::NoError) {

        qDebug() << reply->property("extinf_id");

        QString strReply = reply->readAll();

        this->displayMovieInfo(reply->property("extinf_id").toInt(), strReply, true);

    } else {
        ui->edtOutput->clear();
        statusBar()->showMessage(tr("Netzwerk-Fehler auf themoviedb.org Zugriff..."));
    }

    delete reply;
}

void MainWindow::displayMovieInfo(int extinf_id, QString moviedata, bool storedata)
{
    dir.mkpath(m_AppDataPath + "/tmdb/");

    QJsonDocument jsonResponse = QJsonDocument::fromJson(moviedata.toUtf8());

    QJsonObject doc_obj = jsonResponse.object();

    QJsonArray doc_array = doc_obj.value("results").toArray();
    if ( ! doc_array.isEmpty() ) {
        doc_obj =  doc_array.at(0).toObject();
    }

    ui->edtOutput->setText( doc_obj.value("original_title").toString() + " (" + doc_obj.value("release_date").toString() + ") / " + QString("ID: %1").arg(doc_obj.value("id").toDouble()) );
    ui->edtOutput->append ( " " );
    ui->edtOutput->append ( QString("%1 - votes: %2").arg(doc_obj.value("vote_average").toDouble()).arg(doc_obj.value("vote_count").toDouble()));
    ui->edtOutput->append ( " " );
    ui->edtOutput->append ( doc_obj.value("overview").toString() );

    if ( storedata ) {

        QString filename = m_AppDataPath + "/tmdb/" + QString("%1.txt").arg(doc_obj.value("id").toDouble());

        const QString qPath(filename);
        QFile qFile(qPath);
        if (qFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&qFile);
            out.setCodec("utf-8");
            out << moviedata.toUtf8();
            qFile.close();
        }
    }

    db.updatePLS_item_tmdb_by_extinf_id(extinf_id, doc_obj.value("id").toDouble() );
}

void MainWindow::progressCancel_clicked()
{
    m_ProgressWasCanceled = true;
    qDebug() << "cancel";
}


void MainWindow::on_actionload_stylsheet_triggered()
{

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    QString path = settings.value("stylsheetpath").toByteArray();

    QString fileName = QFileDialog::getOpenFileName(this, ("Open qss stylsheet File"),
                                                     path,
                                                     ("qss stylsheet file (*.qss)"));
    QFile f(fileName);
    if (!f.exists())   {
        printf("Unable to set stylesheet, file not found\n");
    } else {

        QFileInfo fi(fileName);

        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);

        qApp->setStyleSheet(ts.readAll());

        QSettings settings(m_SettingsFile, QSettings::IniFormat);
        settings.setValue("stylsheet", fileName);
        settings.setValue("stylsheetpath", fi.path());
        settings.sync();
    }
}

void MainWindow::on_cmdSetPos_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();
    int              row  = ui->twPLS_Items->currentIndex().row();
    bool             ok;

    int pos = 0;

    QString text = QInputDialog::getText(this, tr("Set channel position"),
                                         tr("Please enter the position in the playlist"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty())
        pos = text.toInt()-1;

    if (item && row >= 0 && pos >= 0) {

        ui->twPLS_Items->takeTopLevelItem(row);
        ui->twPLS_Items->insertTopLevelItem(pos, item);
        ui->twPLS_Items->setCurrentItem(item);

        somethingchanged = true;
    }
}

void MainWindow::on_cmdSetLogo_clicked()
{
    bool ok;

    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();

    if ( item != nullptr ) {

        int extinf_id = item->data(1,1).toInt();

        QString url = QInputDialog::getText(this, tr("Set channel logo"),
                                             tr("Please enter URL from station logo:"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !url.isEmpty()) {

            db.updateEXTINF_tvg_logo_byRef(extinf_id, url);
        }
    }
}

void MainWindow::fillComboEPGChannels()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;

    ui->cboEPGChannels->blockSignals(true);

    ui->cboEPGChannels->clear();
    ui->cboEPGChannels->addItem(" ");

    select = db.selectEPGChannels(".de");
    while ( select->next() ) {

        title = select->value(3).toByteArray().constData();

        ui->cboEPGChannels->addItem(title);
    }

    delete select;

    ui->cboEPGChannels->blockSignals(false);
}

void MainWindow::on_actionselect_application_font_triggered()
{
    bool ok;

    QFont font = QFontDialog::getFont( &ok, QApplication::font(), this, tr("Pick a font") );

    if( ok ) {

        QApplication::setFont(font);

        QSettings settings(m_SettingsFile, QSettings::IniFormat);
        settings.setValue("fontname", font.toString());
        settings.setValue("fontsize", font.pointSize());
        settings.setValue("fontbold", font.bold());
        settings.setValue("fontitalic", font.italic());
        settings.sync();

    }
}

void MainWindow::on_cmdImdb_clicked()
{
    bool ok = false;

    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();

    if ( item != nullptr ) {

        int extinf_id = item->data(1,1).toInt();

        QString tmdb_id = QInputDialog::getText(this, tr("Set tmdb id"),
                                             tr("Please enter a valid id from Themoviedb.org"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !tmdb_id.isEmpty()) {

            this->getTMDBdataById(tmdb_id.toInt(), extinf_id);
        }
    }
}

void MainWindow::on_cmdPlayExtern_clicked()
{
    QString program = "ffplay";
    QStringList arguments;

    arguments << "-x" << "640" << "-y" << "480" << "-loglevel" << "warning" << "-nostats" << ui->edtStationUrl->text();

    m_Process->setProcessChannelMode(QProcess::MergedChannels);
    m_Process->start(program, arguments);
}

void MainWindow::on_actionimport_m3u_file_triggered()
{
    this->on_edtLoad_clicked();
}

void MainWindow::on_cmdAddToFavorits_clicked()
{

    const int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    QMessageBox msgBox;
    msgBox.setWindowTitle("title");
    msgBox.setText("Add actual Playliste to favorites?");
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if(msgBox.exec() == QMessageBox::Yes){
        db.updatePLS_favorite(pls_id, 1 );
    } else {
        db.updatePLS_favorite(pls_id, 0 );
    }
}

void MainWindow::on_chkPlaylistOnlyFavorits_stateChanged(int arg1)
{
    this->fillComboPlaylists();
}
