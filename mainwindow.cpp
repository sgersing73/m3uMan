#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_AppDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    dir.mkpath(m_AppDataPath);

    m_SettingsFile = m_AppDataPath + "/settings.ini";

    _instance = new VlcInstance(VlcCommon::args(), this);
    _player = new VlcMediaPlayer(_instance);
    _equalizerDialog = new EqualizerDialog(this);    

    _player->setVideoWidget(ui->widVideo);
    _equalizerDialog->setMediaPlayer(_player);
    _error = new VlcError();

    ui->widSeek->setMediaPlayer(_player);
    ui->widVolume->setMediaPlayer(_player);
    ui->widVolume->setVolume(50);
    ui->widVideo->setMediaPlayer(_player);

    connect(ui->actionPause, &QAction::toggled, _player, &VlcMediaPlayer::togglePause);
    connect(ui->actionStop, &QAction::triggered, _player, &VlcMediaPlayer::stop);
    connect(ui->cmdPause, &QPushButton::clicked, _player, &VlcMediaPlayer::togglePause);
    connect(ui->cmdStop, &QPushButton::clicked, _player, &VlcMediaPlayer::stop);    
    connect(ui->actionEqualizer, &QAction::triggered, _equalizerDialog, &EqualizerDialog::show);

    connect(_player, SIGNAL(error()), this, SLOT(showVlcError()));

    this->findAllButtons();

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    ui->splitter->restoreState(settings.value("splitter").toByteArray());
    ui->edtUrl->setText(settings.value("iptvurl").toByteArray());
    ui->edtUrlEpg->setText(settings.value("iptvepgurl").toByteArray());
    ui->edtFtpConnect->setText(settings.value("ftpconnect").toByteArray());

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
    fillTreeWidget();

    m_Process = new QProcess(this);

    connect(m_Process, SIGNAL(started()), this, SLOT(processStarted()));
    connect(m_Process, SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
    connect(m_Process, SIGNAL(finished(int)), this, SLOT(processFinished()));

#ifdef Q_OS_WIN
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(this->windowHandle());
    //taskbarButton->setOverlayIcon(QIcon(":/overlay"));

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
    settings.setValue("iptvurl", ui->edtUrl->text());
    settings.setValue("iptvepgurl", ui->edtUrlEpg->text());
    settings.setValue("ftpconnect", ui->edtFtpConnect->text());
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
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createActions() {

     QToolBar *fileToolBar = addToolBar(tr("File"));

     QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

     const QIcon exitIcon = QIcon::fromTheme("application-exit");
     QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
     exitAct->setShortcuts(QKeySequence::Quit);
     exitAct->setStatusTip(tr("Exit the application"));
     fileToolBar->addAction(exitAct);
     fileToolBar->setObjectName("toolbarFile");

     QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

     const QIcon helpIcon = QIcon::fromTheme("help-contents");
     QAction *aboutAct = helpMenu->addAction(helpIcon, tr("&About"), this, &MainWindow::about);
     aboutAct->setStatusTip(tr("Show the application's About box"));

     const QIcon aboutIcon = QIcon::fromTheme("help-about");
     QAction *aboutQtAct = helpMenu->addAction(aboutIcon, tr("About &Qt"), qApp, &QApplication::aboutQt);
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
    QString group_title;
    int     group_id;
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

            if ( line.startsWith("http") ) {
              linecount++;
            }
        }

        qDebug() << "Lines" << linecount;

        statusBar()->repaint();

        QProgressDialog progress("Task in progress...", "Cancel", 0, linecount, this);

#ifdef Q_OS_WIN
        taskbarProgress->setMinimum(0);
        taskbarProgress->setMaximum(linecount);
        taskbarProgress->setVisible(true);
#endif

        stream.seek(0);

        while (!stream.atEnd() && !ende){

            line = stream.readLine();

            if (progress.wasCanceled())
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
                }

                query = db.selectGroup_byTitle(group_title);
                query->last();
                query->first();

                if ( query->isValid() ) {
                    group_id = query->value(0).toByteArray().toInt();
                } else {
                    group_id = db.addGroup(group_title);
                }
                query->clear();

                // ------------------------------------------------
                query = db.selectEXTINF_byUrl(url);

                query->last();
                query->first();

                if ( query->isValid() ) {
                  //  qDebug() << "-U-" <<tvg_name<< tvg_id<< group_title<< tvg_logo<< url;
                    db.updateEXTINF_byRef( query->value(0).toByteArray().toInt(), tvg_name, group_id, tvg_logo, 1 );
                } else {
                  //  qDebug() << "-I-" <<tvg_name<< tvg_id<< group_title<< tvg_logo<< url;
                    db.addEXTINF(tvg_name, tvg_id, group_id, tvg_logo, url);
                    newfiles++;
                }
                query->clear();
                // ------------------------------------------------

                counter++;

                if ( counter % 100 == 0 ) {
                    statusBar()->showMessage(tr("%1 stations").arg(counter));                    
                }

                QCoreApplication::processEvents();

                progress.setValue(counter);
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

    QSqlQuery *test;

    test = db.countEXTINF_byState();
    while ( test->next() ) {

        qDebug() << "obsolete" << test->value(0).toByteArray().constData() << test->value(1).toByteArray().constData();

        if( test->value(0).toByteArray().toInt() == 0 ) {
            obsolete = test->value(1).toByteArray().toInt();
        }
    }

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

void MainWindow::addTreeChild(QTreeWidgetItem *parent, const QString& name, const QString& description, const QString& id, const QString& used, const QString& state, const QString& url)
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
    QString tvg_id;
    QString used;
    QString state;
    QString url;
    QString favorite;
    QString group_id;

    // Add root nodes
    QTreeWidgetItem *item = nullptr;
    QSqlQuery *select = nullptr;

    ui->treeWidget->clear();
    ui->treeWidget->setColumnCount(3);
    ui->treeWidget->setHeaderLabels(QStringList() << "Group" << "Station" << "ID");

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

        addTreeChild(item, title, tvg_id, id, used, state, url);
    }

    ui->treeWidget->blockSignals(false);

    delete select;
}

void MainWindow::fillComboPlaylists()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;

    ui->cboPlaylists->clear();

    select = db.selectPLS();
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        title = select->value(1).toByteArray().constData();

        ui->cboPlaylists->addItem(title, id);
    }

    delete select;

    ui->cmdDeletePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );
    ui->cmdRenamePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );   
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
                                         "", &ok);
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
    QString tvg_name;
    QString logo;
    QString url;
    bool    added = false;
    QFile   file;
    QPixmap buttonImage;

    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    // Add root nodes
    QSqlQuery *select = nullptr;

    ui->twPLS_Items->clear();
    ui->twPLS_Items->setColumnCount(1);
    ui->twPLS_Items->setHeaderLabels(QStringList() << "Stations");

    ui->lvStations->clear();
    ui->lvStations->setFlow(QListView::Flow::LeftToRight);

    select = db.selectPLS_Items(pls_id);
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        extinf_id = select->value(2).toByteArray().constData();
        logo = select->value(8).toByteArray().constData();
        tvg_name = select->value(5).toByteArray().constData();
        url = select->value(9).toByteArray().constData();

        QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->twPLS_Items);

        treeItem->setText(0, tvg_name);
        treeItem->setData(0, 1, id);
        treeItem->setData(1, 1, extinf_id);
        treeItem->setStatusTip(0, tr("double click to remove the station"));

        file.setFileName(m_AppDataPath + "/logos/" + QUrl(logo).fileName());
        if ( file.exists() && file.size() > 0 ) {

            file.open(QIODevice::ReadOnly);

            if ( QUrl(logo).fileName().trimmed().isEmpty() ) {
                buttonImage = QPixmap(":/images/iptv.png");
            } else {
                buttonImage.loadFromData(file.readAll());
            }

            QListWidgetItem* item = new QListWidgetItem(buttonImage, "");

            item->setData(Qt::UserRole, url);
            item->setData(Qt::DecorationRole, buttonImage.scaled(50,50,Qt::KeepAspectRatio, Qt::SmoothTransformation));      
            item->setToolTip(tvg_name);
            ui->lvStations->addItem(item);

            file.close();       
        }

        added = true;
    }

    delete select;

    ui->cmdMoveUp->setEnabled( added );
    ui->cmdMoveDown->setEnabled( added );
    ui->cmdMakePlaylist->setEnabled( added );
    ui->edtStationUrl->setText("");

}

void MainWindow::on_cboPlaylists_currentTextChanged(const QString &arg1)
{
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

void MainWindow::on_cmdMakePlaylist_clicked()
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
            group = select->value(3).toByteArray().constData();
            logo = select->value(4).toByteArray().constData();
            url = select->value(5).toByteArray().constData();

            out << QString("#EXTINF:-1 tvg-name=\"%1\" tvg-id=\"%2\" group-title=\"%3\" tvg-logo=\"%4\",%1\n%5\n").arg(title).arg(tvg_id).arg(group).arg(logo).arg(url);
        }

        delete select;

        QGuiApplication::restoreOverrideCursor();
    }

    file.flush();
    file.close();

    somethingchanged = false;

    statusBar()->showMessage(tr("File saved"), 2000);

    this->ftpUploadFile(fileName);
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

        select->clear();

        ui->edtStationUrl->setText(url);

        select = db.selectProgramData(tvg_id);

        while ( select->next() ) {

            title = select->value(4).toByteArray().constData();
            desc = select->value(5).toByteArray().constData();
        }

        ui->edtOutput->clear();
        ui->edtOutput->append(title);
        ui->edtOutput->append("");
        ui->edtOutput->append(desc);

        select->clear();

        if ( ! logo.trimmed().isEmpty() ) {
            m_pImgCtrl = new FileDownloader(logo, this);
            connect(m_pImgCtrl, SIGNAL(downloaded()), SLOT(loadImage()));
        } else {
            const QPixmap buttonImage (":/images/iptv.png");
            ui->lblLogo->setPixmap(buttonImage.scaledToWidth(ui->lblLogo->maximumWidth()));
        }
    }
}

void MainWindow::processStarted()
{
    qDebug() << "processStarted()";

    m_OutputString.clear();
    ui->edtOutput->setText("mOutputString");
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
    QPixmap buttonImage;
    dir.mkpath(m_AppDataPath + "/logos/");

    QString filename = m_AppDataPath + "/logos/" + m_pImgCtrl->filename();

    QFile file(filename);

    if ( ( ! file.exists() ) or ( file.size() == 0 ) ) {
        file.open(QIODevice::WriteOnly);
        file.write(m_pImgCtrl->downloadedData());
        file.close();
    }

    if ( file.exists() ) {

        file.open(QIODevice::ReadOnly);
        buttonImage.loadFromData(file.readAll());

        ui->lblLogo->setPixmap(buttonImage.scaledToWidth(ui->lblLogo->maximumWidth()));
    } else {
        ui->lblLogo->setText("no data!");
    }
}

void MainWindow::on_cmdPlayStream_clicked()
{
    if ( ui->chkExtern->isChecked() ) {

        QString program = "vlc";
        QStringList arguments;

        arguments << ui->edtStationUrl->text();

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);

    } else {

        if (ui->edtStationUrl->text().isEmpty())
            return;

        _media = new VlcMedia(ui->edtStationUrl->text(), _instance);

        _player->open(_media);
    }
}

void MainWindow::on_edtStationUrl_textChanged(const QString &arg1)
{
     ui->cmdGatherData->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdPlayStream->setEnabled(arg1.trimmed().length() > 0);
}


void MainWindow::on_lvStations_itemClicked(QListWidgetItem *item)
{
    QVariant url =  item->data(Qt::UserRole);

    if ( ! url.toString().isEmpty() ) {

        _media = new VlcMedia(url.toString(), _instance);

        _player->open(_media);
    }
}

void MainWindow::on_cmdSavePosition_clicked()
{
    QTreeWidgetItem *item;

    for (int i = 0; i < ui->twPLS_Items->topLevelItemCount(); i++) {

        item = ui->twPLS_Items->topLevelItem(i);

        db.updatePLS_item_pls_pos(item->data(0, 1).toInt() , i );
    }

    fillTwPls_Item();

    statusBar()->showMessage("positions set...", 2000);
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

    xmlFile = new QFile(sFileName);
    if (!xmlFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(this,"Load XML File Problem",
            tr("Couldn't open %1 to load settings for download").arg(sFileName),
            QMessageBox::Ok);
            return;
    }

    db.removeAllPrograms();

    QProgressDialog progress("Task in progress...", "Cancel", 0, linecount, this);

#ifdef Q_OS_WIN
    taskbarProgress->setMinimum(0);
    taskbarProgress->setMaximum(linecount);
    taskbarProgress->setVisible(true);
#endif

    xmlReader = new QXmlStreamReader(xmlFile);

    //Parse the XML until we reach end of it
    while ( !xmlReader->atEnd() && !xmlReader->hasError() && !ende ) {

            if (progress.wasCanceled())
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

            progress.setValue(linecount);

#ifdef Q_OS_WIN
            taskbarProgress->setValue(linecount);
#endif
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

    on_cmdPlayStream_clicked();
}

void MainWindow::on_pushButton_clicked()
{
    ui->twPLS_Items->clearSelection();
    ui->twPLS_Items->setCurrentItem(ui->twPLS_Items->itemAbove(ui->twPLS_Items->currentItem() ));
    ui->twPLS_Items->setItemSelected( ui->twPLS_Items->currentItem(), true );

    on_cmdPlayStream_clicked();
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
    QString iconcolor = settings.value("iconcolor", "black").toByteArray();

    QList<QPushButton *> buttons = this->findChildren<QPushButton *>();

    QList<QPushButton *>::const_iterator iter;
    for (iter = buttons.constBegin(); iter != buttons.constEnd(); ++iter) {

          QPushButton *button = *iter;

          if( ! button->icon().isNull() ) {

              button->setIcon( this->changeIconColor( button->icon(), QColor(iconcolor) ) );
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

bool MainWindow::ftpUploadFile(const QString& filePath)
{
    QStringList values = ui->edtFtpConnect->text().split(",");
    if ( values.count() != 4 ) {
        return false;
    }

    QFile file(filePath);

    file.open(QIODevice::ReadOnly);

    QByteArray contents = file.readAll();
    file.close();

    qDebug() << "Uploading file to" << values.at(3);

    QFtp ftp;
    ftp.connectToHost( values.at(0) );
    ftp.login( values.at(1), values.at(2) );

    ftp.put( contents, values.at(3), QFtp::Binary );

    ftp.close();

    while ( ftp.hasPendingCommands() )
        QCoreApplication::instance()->processEvents();

    return true;
}
