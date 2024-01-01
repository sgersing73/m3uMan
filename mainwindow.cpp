#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    qDebug() << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();

    m_AppDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    dir.mkpath(m_AppDataPath);
    m_SettingsFile = m_AppDataPath + "/settings.ini";
    QSettings settings(m_SettingsFile, QSettings::IniFormat);

    if ( QString(settings.value("BackupOnStart").toByteArray()).toInt() == 1 )  {

        qDebug("Perform a database backup, now...");

        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMddhhmmss"));

        this->Zip(m_AppDataPath + "/m3uMan.sqlite", m_AppDataPath + "/m3uMan.sqlite_" + timestamp + ".bck");
    }

    settings.setValue("BackupOnStart", 0);

    db.open(m_AppDataPath + "/m3uMan.sqlite");

    if (db.isOpen()) {
        if ( ! db.createTable() ) {
            qApp->exit();
        }
    } else {
        qDebug() << "Database is not open!";
    }

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

    this->FindAndColorAllButtons();

    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    ui->splitter->restoreState(settings.value("splitter").toByteArray());
    ui->splitter_2->restoreState(settings.value("splitter_2").toByteArray());
    ui->splitter_3->restoreState(settings.value("splitter_3").toByteArray());
    ui->splitter_4->restoreState(settings.value("splitter_4").toByteArray());
    ui->edtUrl->setText(settings.value("iptvurl").toByteArray());
    ui->edtUrlEpg->setText(settings.value("EPG1").toByteArray());

    if ( settings.value("PlaylistOnlyFavorits").toInt() == Qt::Checked ) {
        ui->chkPlaylistOnlyFavorits->setCheckState( Qt::Checked );
    } else {
        ui->chkPlaylistOnlyFavorits->setCheckState( Qt::Unchecked );
        this->fillComboPlaylists();
    }

    if ( settings.value("AutoPlay").toInt() == Qt::Checked ) {
        ui->chkAutoPlay->setCheckState( Qt::Checked );
    } else {
        ui->chkAutoPlay->setCheckState( Qt::Unchecked );
    }

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
            this, SLOT(ShowContextMenuTreeWidget(const QPoint&)));

    ui->twPLS_Items->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->twPLS_Items, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(ShowContextMenuPlsItems(const QPoint&)));

    createActions();
    createStatusBar();

    fillComboGroupTitels();
    fillComboEPGChannels();

    ui->cboPlaylists->setCurrentText(settings.value("CurrentPlaylist").toByteArray());

    m_Process = new QProcess(this);

    connect(m_Process, SIGNAL(started()), this, SLOT(processStarted()));
    connect(m_Process, SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutput()));
    connect(m_Process, SIGNAL(finished(int)), this, SLOT(processFinished()));

    m_Process2 = new QProcess(this);

    connect(m_Process2, SIGNAL(started()), this, SLOT(processStarted()));
    connect(m_Process2, SIGNAL(readyReadStandardOutput()),this,SLOT(readyReadStandardOutputJson()));
    connect(m_Process2, SIGNAL(finished(int)), this, SLOT(processFinished()));

    ui->cboUrlEpgSource->addItems( QStringList() << "EPG1" << "EPG2" << "EPG3" << "EPG4" << "EPG5");

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

    ui->cmdPlayStream->setEnabled( false );
    qDebug() << "VLC is playing...";
}

void MainWindow::isStopped() {

    ui->cmdPlayStream->setEnabled( true );
    ui->pgbBuffer->setValue(0);
}

void MainWindow::isBuffering(int buffer) {

    ui->pgbBuffer->setValue(buffer);
}


void MainWindow::ShowContextMenuPlsItems( const QPoint & pos )
{
    QTreeWidgetItem *item = ui->twPLS_Items->itemAt(pos);
    if (!item)
       return;

    QPoint globalPos = ui->twPLS_Items->mapToGlobal(pos);

    QMenu myMenu;
    myMenu.addAction(QIcon(":/images/Ui/icons8-add-to-favorites-50.png"), "add to favorites");
    myMenu.addAction(QIcon(":/images/Ui/icons8-trash-can-50.png"),"remove from favorites");
    myMenu.addSeparator();
    myMenu.addAction(QIcon(":/images/Ui/icons8-add-50.png"),"add logo to store");

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        if ( selectedItem->text().contains("add to favorites") ) {
            if ( db.updatePLS_item_favorite ( item->data(0, Qt::UserRole).toInt(), 1) ) {
                qDebug() << "add";
            }
        }
        if ( selectedItem->text().contains("remove from favorites") ) {
            if ( db.updatePLS_item_favorite ( item->data(0, Qt::UserRole).toInt(), 0) ) {
                qDebug() << "removed";
            }
        }
        if ( selectedItem->text().contains("add logo to store") ) {

            dir.mkpath(m_AppDataPath + "/logos/");

            const QPixmap* pix = ui->lblLogo->pixmap();

            if ( pix ) {

                qDebug() << "add to store" << m_actualTitle;

                pix->save(m_AppDataPath + "/logos/" + m_actualTitle + ".png", "PNG");

                statusBar()->showMessage(tr("Logo added to store..."));
            }
        }

        this->fillTwPls_Item();
    }
}



void MainWindow::ShowContextMenuTreeWidget( const QPoint & pos )
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
                    const int pls_pos = i;

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
    settings.setValue(ui->cboUrlEpgSource->currentText(), ui->edtUrlEpg->text() + ";" + ui->edtUrlEpgHour->text());
    settings.setValue("CurrentPlaylist", ui->cboPlaylists->currentText());
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

void MainWindow::license()
{
    QString   info;

    QFile f ( ":/docs/license.txt" );
    f.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&f);
    info = ts.readAll();
    f.close();

    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(9);

    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("License Informations"));
    msgBox.setText(info);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setFont(font);

    msgBox.exec();
}

void MainWindow::about()
{
    QSysInfo  systemInfo;
    QString   info;

    QStorageInfo storage = QStorageInfo::root();

    info.append(tr("M3uMan V1.0\n"));
    info.append("\n");
    info.append(tr("Windows Version:\t%1\n").arg(systemInfo.windowsVersion()));
    info.append(tr("Build Cpu Architecture:\t%1\n").arg(systemInfo.buildCpuArchitecture()));
    info.append(tr("Current Cpu Architecture: %1\n").arg(systemInfo.currentCpuArchitecture()));
    info.append(tr("Kernel Type:\t\t%1\n").arg(systemInfo.kernelType()));
    info.append(tr("Kernel Version:\t\t%1\n").arg(systemInfo.kernelVersion()));
    info.append(tr("Machine Host Name:\t%1\n").arg(systemInfo.machineHostName()));
    info.append(tr("Product Type:\t\t%1\n").arg(systemInfo.productType()));
    info.append(tr("Product Version:\t%1\n").arg(systemInfo.productVersion()));
    info.append(tr("Byte Order:\t\t%1\n").arg(systemInfo.buildAbi()));
    info.append(tr("Pretty ProductName:\t%1\n").arg(systemInfo.prettyProductName()));
    info.append("\n");

    QHostInfo hostInfo = QHostInfo::fromName( QHostInfo::localHostName() );

    if (!hostInfo.addresses().isEmpty()) {
        QHostAddress address = hostInfo.addresses().first();
        info.append(tr("IP Address:\t\t%1\n").arg(address.toString()));
    }

    info.append("\n");

    qDebug() << storage.rootPath();
    if (storage.isReadOnly())
        qDebug() << "isReadOnly:" << storage.isReadOnly();

    info.append(tr("Storage Name:\t\t%1\n").arg(storage.displayName()));
    info.append(tr("File System Type:\t%1\n").arg(QString::fromStdString(storage.fileSystemType().toStdString())));
    info.append(tr("Size (GB):\t\t%1\n").arg(storage.bytesTotal()/1024/1024/1024));
    info.append(tr("Free (GB):\t\t%1\n").arg(storage.bytesAvailable()/1024/1024/1024));

    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(9);

    QMessageBox msgBox;
    msgBox.setWindowTitle(tr("About Application"));
    msgBox.setText(info);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setFont(font);

    msgBox.exec();

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

     QAction *aboutLic = helpMenu->addAction(this->style()->standardIcon(QStyle::SP_TitleBarMenuButton), tr("&License"), this, &MainWindow::license);
     aboutLic->setStatusTip(tr("Show the application's License Box"));

     QAction *aboutQtAct = helpMenu->addAction(this->style()->standardIcon(QStyle::SP_TitleBarMenuButton), tr("About &Qt"), qApp, &QApplication::aboutQt);
     aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

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

    bool overwrite = false;

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

                    if ( overwrite ) {

                        if ( ! db.updateEXTINF_byRef(extinf_id, tvg_name, group_id, tvg_logo, 1 ) ) {
                            qDebug() << "-E-" << "updateEXTINF_byRef" <<tvg_name<< tvg_id<< group_title<< tvg_logo<< url;
                        }
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
                    pls_id = db.insertPLS(group_title, 0);
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
/*
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
*/
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
        group = "%EU |%";
    } else {
        group = "%" + ui->cboGroupTitels->currentText() + "%";
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
    int     favorite = 0;

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

        db.insertPLS(text, 1);

        fillComboPlaylists();

        QMessageBox::information (this, "The Playlist",
                                  QString("Playlist '%1' added... don't forget to select the kind").arg(text));

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
    this->fillTwPls_Item();
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
    QAction *action;
    int     favorite;
    int     kind = 0;
    int     onlyEpg = 0;
    QString program;

    ui->radTv->setChecked(false);
    ui->radRadio->setChecked(false);
    ui->radMovie->setChecked(false);

    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    // Add root nodes
    QSqlQuery *select, *select2 = nullptr;

    select = db.selectPLS_by_id(pls_id);

    select->last();
    select->first();

    if ( select->isValid() ) {
        kind = select->value(3).toByteArray().toInt();
    }

    delete select;

    switch (kind) {
        case 1 : ui->radTv->setChecked(true);
                 ui->cmdImdb->setVisible(false);
                 ui->cboEPGChannels->setVisible(true);
                 ui->cmdEPG->setVisible(true);
                 ui->cmdWiki->setVisible(true);
                 ui->chkOnlyEpg->setVisible(true);
                 break;
        case 2 : ui->radRadio->setChecked(true);
                 ui->cmdImdb->setVisible(false);
                 ui->cboEPGChannels->setVisible(false);
                 ui->cmdEPG->setVisible(true);
                 ui->cmdWiki->setVisible(true);
                 ui->chkOnlyEpg->setChecked(false);
                 ui->chkOnlyEpg->setVisible(false);
                 break;
        case 3 : ui->radMovie->setChecked(true);
                 ui->cmdImdb->setVisible(true);
                 ui->cboEPGChannels->setVisible(false);
                 ui->cmdEPG->setVisible(true);
                 ui->cmdWiki->setVisible(true);
                 ui->chkOnlyEpg->setChecked(false);
                 ui->chkOnlyEpg->setVisible(false);
                 break;
    }

    ui->twPLS_Items->clear();
    ui->twPLS_Items->setColumnCount(4);
    ui->twPLS_Items->setHeaderLabels(QStringList() << "Place" << "Stations"  << "EPG" << "Program" );

    for(int i = 0; i < 4 ; i++)
        ui->twPLS_Items->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    ui->mainToolBar->clear();

    tvg_name = ui->edtFilter_2->text();
    if ( tvg_name.isEmpty() ) {
        tvg_name = "%%";
    }

    if ( ui->chkOnlyEpg->isChecked() ) {
        onlyEpg = 1;
    }

    select = db.selectPLS_Items(pls_id, tvg_name, onlyEpg);
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        extinf_id = select->value(2).toByteArray().constData();
        pls_pos = select->value(3).toByteArray().constData();
        favorite = select->value(5).toByteArray().toInt();
        logo = select->value(10).toByteArray().constData();
        tvg_name = select->value(7).toByteArray().constData();
        tvg_id = select->value(8).toByteArray().constData();
        url = select->value(11).toByteArray().constData();

        program.clear();
        select2 = db.selectActualProgramData(tvg_id);
        while ( select2->next() ) {
            program = select2->value(4).toByteArray().constData();
        }

        delete select2;

        QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->twPLS_Items);

        treeItem->setText(0, QString("%1").arg(pls_pos.toInt() + 1) );
        treeItem->setText(1, tvg_name);
        treeItem->setText(2, tvg_id);
        treeItem->setText(3, program);

        treeItem->setData(0, Qt::UserRole, id);
        treeItem->setData(0, Qt::UserRole+1, extinf_id);
        treeItem->setStatusTip(0, tr("double click to remove the station"));

        if ( QUrl(logo).fileName().isEmpty() ) {

            buttonImage = QPixmap(":/images/iptv.png");

        } else {

            if ( tvg_name.contains("|") ) {
                m_actualTitle = tvg_name.mid(4);
            }

            file.setFileName(m_AppDataPath + "/logos/" + m_actualTitle + ".PNG");
            if ( file.exists() && file.size() > 0 ) {

                qDebug() << "use logo from store" << m_actualTitle;

                file.open(QIODevice::ReadOnly);
                buttonImage.loadFromData(file.readAll());
                file.close();

            } else {

                file.setFileName(m_AppDataPath + "/pictures/" + QUrl(logo).fileName());
                if ( file.exists() && file.size() > 0 ) {

                    file.open(QIODevice::ReadOnly);

                    buttonImage.loadFromData(file.readAll());

                    file.close();

                    if ( ui->radTv->isChecked() ) {

                        if ( logo.contains( "lo1.in" ) ) {

                            buttonImage = buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation);

                        } else {

                            QPixmap backImage = QPixmap(":/images/template.png");

                            QPainter painter(&backImage);

                            if (buttonImage.height() > backImage.width() )
                                buttonImage = buttonImage.scaledToHeight(backImage.height()-20, Qt::SmoothTransformation);

                            if (buttonImage.width() > backImage.width() )
                                buttonImage = buttonImage.scaledToWidth(backImage.width()-20, Qt::SmoothTransformation);

                            painter.drawPixmap((backImage.width() - buttonImage.width()) / 2 ,
                                               (backImage.height() - buttonImage.height()) / 2 , buttonImage);

                            buttonImage = backImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                    } else {
                        buttonImage = buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                } else {
                    buttonImage = QPixmap(":/images/iptv.png");
                }
            }

            treeItem->setIcon(0, QIcon(buttonImage.scaled(16,16,Qt::KeepAspectRatio, Qt::SmoothTransformation)) );
        }

        // ----------------------------------------------------

        if ( favorite == 1 ) {
            action = new QAction(QIcon(buttonImage.scaled(100,100,Qt::KeepAspectRatio, Qt::SmoothTransformation)), tvg_name);
            action->setData(select->at());
            ui->mainToolBar->addAction(action);
        }

        added = true;
    }

    delete select;

    ui->cmdMoveUp->setEnabled( added );
    ui->cmdMoveDown->setEnabled( added );
    ui->edtStationUrl->setText("");
}

void MainWindow::on_cboPlaylists_currentTextChanged(const QString &arg1)
{
    ui->lblLogo->clear();
    ui->cboEPGChannels->setCurrentText(" ");

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    ui->edtFilter_2->setText( settings.value(QString("%1").arg(qHash(arg1))).toString() );

    fillTwPls_Item();
}

void MainWindow::on_twPLS_Items_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    db.removePLS_Item( item->data(0, Qt::UserRole).toInt() );

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
    } else {
        statusBar()->showMessage(tr("already on top of the list..."),2000);
    }
}

void MainWindow::on_cmdMoveDown_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();
    int              row  = ui->twPLS_Items->currentIndex().row();
    int              rows = ui->twPLS_Items->topLevelItemCount();

    qDebug() << row << rows;

    if (item && row + 1 < rows)
    {
        ui->twPLS_Items->takeTopLevelItem(row);
        ui->twPLS_Items->insertTopLevelItem(row + 1, item);
        ui->twPLS_Items->setCurrentItem(item);

        somethingchanged = true;
    } else {
        statusBar()->showMessage(tr("already on bottom of the list..."),2000);
    }
}


void MainWindow::ImportLogoUrlList()
{
    QString tvg_name, tvg_logo;

    QString filename = QFileDialog::getOpenFileName(this, ("Select logo url file"),
                                           dir.homePath(),
                                           ("txt file (*.txt)"));

    if ( filename.isNull() ) {
        return;
    }

    QFile file(filename);

    if(!file.exists()){
        qDebug() << "File cannot be found " << filename;
    }

    QString line;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)){

        QTextStream stream(&file);

        statusBar()->showMessage(tr("import logo list..."));

        stream.seek(0);

        while (!stream.atEnd() ) {

            line = stream.readLine();

            tvg_name = line.split(",").at(0);
            tvg_logo = line.split(",").at(1);

            if ( ! db.updateEXTINF_tvg_logo_by_tvg_name(tvg_name, tvg_logo) ) {
                qDebug() << "-E-" << tvg_name << tvg_logo;
            }
        }
    }

    file.close();

    QMessageBox::information(this, "m3uMan", tr("import logo list <b>%1</b> done...").arg(filename), QMessageBox::Ok);

    statusBar()->showMessage(tr("import logo list done..."));
}

void MainWindow::MakeLogoUrlList()
{
    QTreeWidgetItem *item;
    int             extinf_id;
    QSqlQuery       *select;
    QString         logo,title;
    QDir            dir;
    bool            ok = false;

    QString fileName = dir.homePath() + "/"+ ui->cboPlaylists->currentText() + "_logo.txt";

    fileName = QInputDialog::getText(this, tr("Make Logo Url List"),
                                     tr("Please enter the name of the list"), QLineEdit::Normal,
                                     fileName, &ok);
    if (!ok || fileName.isEmpty()) {
        qDebug() << "Invalid input " << fileName << ok;
        return;
    }

    QFile file( fileName );

    if(!file.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() << "Could not open file '" + fileName + "' for writing";
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    QTextStream out(&file);

    for(int i=0;i<ui->twPLS_Items->topLevelItemCount();++i) {

        item = ui->twPLS_Items->topLevelItem(i);
        extinf_id = item->data(0, Qt::UserRole+1).toInt();

        select = db.selectEXTINF_byRef(extinf_id);
        while ( select->next() ) {

            title = select->value(1).toByteArray().constData();
            logo = select->value(4).toByteArray().constData();

            if ( ! logo.isEmpty() ) {
                out << QString("%1,%2\n").arg(title, logo);
            }
        }

        delete select;

        QGuiApplication::restoreOverrideCursor();
    }

    file.flush();
    file.close();

    somethingchanged = false;

    QMessageBox::information(this, "m3uMan", tr("Export logo list <b>%1</b> done...").arg(fileName), QMessageBox::Ok);

    statusBar()->showMessage(tr("Export logo list done..."));
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
    bool            ok = false;

    QString fileName = dir.homePath() + "/"+ ui->cboPlaylists->currentText() + ".m3u";

    fileName = QInputDialog::getText(this, tr("Make playlist"),
                                         tr("Please enter the name of the playlist"), QLineEdit::Normal,
                                         fileName, &ok);
    if (!ok || fileName.isEmpty()) {
        qDebug() << "Invalid input " << fileName << ok;
        return;
    }

    QFile file( fileName );

    if(!file.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() << "Could not open file '" + fileName + "' for writing";
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    QTextStream out(&file);

    out << "#EXTM3U\n";

    for(int i=0;i<ui->twPLS_Items->topLevelItemCount();++i) {

        item = ui->twPLS_Items->topLevelItem(i);
        extinf_id = item->data(0, Qt::UserRole+1).toInt();

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

    QMessageBox::information(this, "m3uMan", tr("Export playlist <b>%1</b> done...").arg(fileName), QMessageBox::Ok);

    statusBar()->showMessage(tr("File saved"));
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

    statusBar()->showMessage(tr("file download %1%").arg(m_pImgCtrl->downloadedProgress()));
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

            QString sHourCorrection = m_pImgCtrl->getData();

            statusBar()->showMessage(tr("File %1 download success!").arg(filename), 5000);

            if (ui->chkEPGImport->isChecked() ) {
                this->getEPGFileData(filename, sHourCorrection);
            } else {
                QMessageBox(QMessageBox::Information, "Downloader", tr("File %1 download success!").arg(filename), QMessageBox::Ok).exec() ;
            }
        }
    } else {
        QMessageBox(QMessageBox::Critical, "Downloader", tr("File %1 download fails!").arg(filename), QMessageBox::Ok).exec() ;
    }
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
    QPixmap   backImage;

    ui->cmdWiki->setEnabled(true);

    QList<QTreeWidgetItem*>items = ui->twPLS_Items->selectedItems();

    foreach( QTreeWidgetItem* mitem, items) {

        m_ActTreeItem = mitem;

        int extinf_id = mitem->data(0, Qt::UserRole+1).toInt();

        select = db.selectEXTINF_byRef(extinf_id);
        while ( select->next() ) {

            id = select->value(0).toByteArray().constData();
            title = select->value(1).toByteArray().constData();
            tvg_id = select->value(2).toByteArray().constData();
            group = select->value(3).toByteArray().constData();
            logo = select->value(4).toByteArray();
            url = select->value(5).toByteArray().constData();
        }

        delete select;

        select = db.selectPLS_Items_by_extinf_id(extinf_id);
        while ( select->next() ) {

            tmdb_id = select->value(4).toByteArray().constData();
        }

        delete select;

        if ( tvg_id.trimmed().isEmpty() ) {
            ui->cboEPGChannels->setCurrentText(" ");
            ui->cmdEPG->setEnabled(false);
        } else {
            ui->cboEPGChannels->setCurrentText(tvg_id);
            ui->cmdEPG->setEnabled(true);
        }

        ui->edtStationUrl->setText(url);
        ui->edtStationName->setText(title);

        m_actualTitle = title;

        if ( m_actualTitle.contains("|") ) {
            m_actualTitle = m_actualTitle.mid(4);
        }

        if ( url.contains("mkv") || url.contains("mp4") ) {

            ui->cmdImdb->setEnabled(true);

            qDebug() << "getTMDBdate" << QUrl::toPercentEncoding(m_actualTitle, "-/:") << extinf_id << tmdb_id;

            this->getTMDBdate(QUrl::toPercentEncoding(m_actualTitle, "-/:"), tmdb_id.toInt(), extinf_id);

        } else {

            ui->cmdImdb->setEnabled(false);

            select = db.selectActualProgramData(tvg_id);

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

            QUrl url(logo);

            QFileInfo fi(url.fileName());

            QFile file(m_AppDataPath + "/pictures/" + fi.fileName());

            if ( ( ! file.exists() ) || ( file.size() == 0 ) ) {

                // qDebug() << "File does not exists..." << m_AppDataPath + "/pictures/" + fi.fileName();

                m_pImgCtrl = new FileDownloader(logo, this);
                connect(m_pImgCtrl, SIGNAL(downloaded()), SLOT(loadImage()));

                statusBar()->showMessage("Requesting image...");

                // qDebug() << "requesting logo..." << file.fileName() << id << logo;

            } else {

                if ( file.exists() && file.size() > 0 ) {

                    file.open(QIODevice::ReadOnly);
                    QTextStream in(&file);
                    buttonImage.loadFromData(file.readAll());
                    file.close();

                    if ( ui->radTv->isChecked() ) {

                        if ( logo.contains( "lo1.in" ) ) {

                            ui->lblLogo->setPixmap(buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));

                        } else {

                            backImage = QPixmap(":/images/template.png");

                            QPainter painter(&backImage);

                            if (buttonImage.height() > backImage.width() )
                              buttonImage = buttonImage.scaledToHeight(backImage.height()-20, Qt::SmoothTransformation);

                            if (buttonImage.width() > backImage.width() )
                              buttonImage = buttonImage.scaledToWidth(backImage.width()-20, Qt::SmoothTransformation);

                            painter.drawPixmap((backImage.width() - buttonImage.width()) / 2 ,
                                               (backImage.height() - buttonImage.height()) / 2 , buttonImage);

                            ui->lblLogo->setPixmap(backImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        }

                    } else if ( ui->radRadio->isChecked()  || ui->radMovie->isChecked() ) {

                        ui->lblLogo->setPixmap(buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                }
            }

        } else {
            const QPixmap buttonImage (":/images/iptv.png");
            ui->lblLogo->setPixmap(buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MainWindow::processStarted()
{
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
}

void MainWindow::loadImage()
{
    QPixmap buttonImage, backImage;

    dir.mkpath(m_AppDataPath + "/pictures/");

    QString filename = m_AppDataPath + "/pictures/" + m_pImgCtrl->getFilename();

    qDebug() << "LoadImage" << filename;

    QFile file(filename);

    if ( ( ! file.exists() ) || ( file.size() == 0 ) ) {

        file.open(QIODevice::WriteOnly);
        QTextStream out(&file);
        file.write(m_pImgCtrl->downloadedData());
        file.close();

        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        buttonImage.loadFromData(file.readAll());
        file.close();

        if ( ui->radTv->isChecked() ) {

            if ( m_pImgCtrl->getUrl().contains( "lo1.in" ) ) {

                ui->lblLogo->setPixmap(buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));

            } else {

                backImage = QPixmap(":/images/template.png");

                QPainter painter(&backImage);

                if (buttonImage.height() > backImage.width() )
                  buttonImage = buttonImage.scaledToHeight(backImage.height()-20, Qt::SmoothTransformation);

                if (buttonImage.width() > backImage.width() )
                  buttonImage = buttonImage.scaledToWidth(backImage.width()-20, Qt::SmoothTransformation);

                painter.drawPixmap((backImage.width() - buttonImage.width()) / 2 ,
                                   (backImage.height() - buttonImage.height()) / 2 , buttonImage);

                ui->lblLogo->setPixmap(backImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }

        } else if ( ui->radRadio->isChecked()  || ui->radMovie->isChecked() ) {

            ui->lblLogo->setPixmap(buttonImage.scaled(ui->lblLogo->maximumWidth(),ui->lblLogo->maximumHeight(),Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        m_ActTreeItem->setIcon(0, QIcon(buttonImage.scaled(16,16,Qt::KeepAspectRatio, Qt::SmoothTransformation)) );

        statusBar()->showMessage("Image successful retrieved...");
    }
}

void MainWindow::on_cmdPlayStream_clicked()
{
    if (ui->edtStationUrl->text().isEmpty())
        return;

    setWindowTitle(m_actualTitle);

    _media = new VlcMedia(ui->edtStationUrl->text(), _instance);

    _player->open(_media);

    _mediaManager = new VlcMetaManager(_media);
}

void MainWindow::on_edtStationUrl_textChanged(const QString &arg1)
{
     ui->cmdGatherStream->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdGatherStreamData->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdPlayExtern->setEnabled(arg1.trimmed().length() > 0);
     ui->cmdPlayStream->setEnabled(arg1.trimmed().length() > 0);
}

void MainWindow::on_cmdSavePosition_clicked()
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();

    if ( item != nullptr ) {

        int extinf_id = item->data(0, Qt::UserRole+1).toInt();

        db.updateEXTINF_tvg_id_byRef(extinf_id, ui->cboEPGChannels->currentText());
        db.updateEXTINF_url_byRef(extinf_id, ui->edtStationUrl->text());
        db.updateEXTINF_tvg_name_byRef(extinf_id,ui->edtStationName->text());
    }

    if ( ui->edtFilter_2->text().isEmpty() ) {

        for (int i = 0; i < ui->twPLS_Items->topLevelItemCount(); i++) {

            item = ui->twPLS_Items->topLevelItem(i);

            db.updatePLS_item_pls_pos(item->data(0, Qt::UserRole).toInt(), i );
        }
    }

    fillTwPls_Item();

    statusBar()->showMessage("settings saved...");
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
    bool    ok=false;
    QRegExp re("\\d*");

    QString sFileName = QFileDialog::getOpenFileName(this, ("Open xmp program File"),
                                                     m_AppDataPath,
                                                     ("xml program file (*.xml)"));

    if ( !sFileName.isNull() ) {
        qDebug() << "selected file path : " << sFileName.toUtf8();

        QString sHourCorrection = QInputDialog::getText(this, tr("Hour Correction"),
                                                         "Stundenkorrektur", QLineEdit::Normal,
                                                         "", &ok);
        if ( ok && !sHourCorrection.isEmpty() && re.exactMatch(sHourCorrection) ) {
            this->getEPGFileData(sFileName, sHourCorrection);
        }
    }
}

void MainWindow::getEPGFileData(const QString &sFileName, const QString &sHourCorrection )
{
    QFile   *xmlFile;
    QXmlStreamReader *xmlReader;
    int     linecount=0;
    QString line;
    bool    ende = false;

    QString start, stop, channel, title, desc;

    db.removeOldPrograms();

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

            if (token == QXmlStreamReader::EndElement) {

                if (xmlReader->name() == "programme") {

                    start.replace(8, 2, QString("%1").arg(start.mid(8, 2).toInt() + sHourCorrection.toInt(), 2, 10, QLatin1Char('0')));
                    stop.replace(8, 2, QString("%1").arg(stop.mid(8, 2).toInt() + sHourCorrection.toInt(), 2, 10, QLatin1Char('0')));

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

    this->fillComboEPGChannels();

    QMessageBox::information(this, "m3uMan", QString("EPG data import done..."), QMessageBox::Ok);
}

void MainWindow::on_edtEPGDownload_clicked()
{
    if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Downloader", "start the download?", QMessageBox::Yes|QMessageBox::No).exec())  {

        QUrl imageUrl(ui->edtUrlEpg->text());
        m_pImgCtrl = new FileDownloader(imageUrl, this);

        m_pImgCtrl->setData( ui->edtUrlEpgHour->text() );

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

void MainWindow::FindAndColorAllButtons() {

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

        QSettings settings(m_SettingsFile, QSettings::IniFormat);
        settings.setValue("iconcolor", color.name());
        settings.sync();

        FindAndColorAllButtons();
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
    }
}

void MainWindow::serviceRequestFinished(QNetworkReply* reply)
{
    if(reply->error() == QNetworkReply::NoError) {

        QString strReply = reply->readAll();

        this->displayMovieInfo(reply->property("extinf_id").toInt(), strReply, true);

    } else {
        ui->edtOutput->clear();
        statusBar()->showMessage(tr("Netzwerk-Fehler auf themoviedb.org Zugriff...%1").arg(reply->errorString()));
    }

   // delete reply;
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

    QString poster =  doc_obj.value("poster_path").toString();
    if ( ! poster.isEmpty() ) {

        QString poster_url = QString("https://image.tmdb.org/t/p/w185/%1").arg(poster);

        db.updateEXTINF_tvg_logo_byRef(extinf_id, poster_url);
    }

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

        int extinf_id = item->data(0, Qt::UserRole+1).toInt();

        QString url = QInputDialog::getText(this, tr("Set channel logo"),
                                             tr("Please enter URL from station logo:"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !url.isEmpty()) {

            qDebug() << "set logo" << url;

            db.updateEXTINF_tvg_logo_byRef(extinf_id, url);
        }
    }
}

void MainWindow::fillComboEPGChannels()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    QString EpgPreSelection = settings.value("EpgPreSelection").toString();

    ui->cboEPGChannels->blockSignals(true);

    ui->cboEPGChannels->clear();
    ui->cboEPGChannels->addItem(" ");

    select = db.selectEPGChannels(EpgPreSelection);
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
    // xdotool search --name http key --window %1 0xff52

    QString program = "ffplay";
    QStringList arguments;

    QRect test = ui->widVideo->geometry();

    QPoint topLeft = ui->widVideo->mapToGlobal(test.topLeft());
    QPoint bottomRight = ui->widVideo->mapToGlobal(test.bottomRight());

    arguments << "-alwaysontop"
              << "-noborder"
              << "-top" << QString("%1").arg(topLeft.y())
              << "-left" << QString("%1").arg(topLeft.x())
              << "-x" << QString("%1").arg(bottomRight.x() - topLeft.x())
              << "-y" << QString("%1").arg(bottomRight.y() - topLeft.y())
              << "-loglevel" << "warning"
              << "-nostats"
              << ui->edtStationUrl->text();

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
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue("PlaylistOnlyFavorits", arg1);
    settings.sync();

    this->fillComboPlaylists();
}

void MainWindow::on_chkAutoPlay_stateChanged(int arg1)
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue("AutoPlay", arg1);
    settings.sync();
}

void MainWindow::on_actionDB_Browser_triggered()
{
    QString program = "DBBrowser";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

        arguments << m_AppDataPath + "/m3uMan.sqlite";

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);
        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select ftpclient program file"),
                                               m_AppDataPath,
                                               ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("DBBrowser", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as sqlite client...").arg(program) );
        }
    }
}


void MainWindow::on_actionFTP_Client_triggered()
{
    QString program = "FTPClient";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

        arguments << dir.homePath();

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);
        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select ftpclient program file"),
                                                      m_AppDataPath,
                                                      ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("FTPClient", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as ftp client...").arg(program) );
        }
    }
}


void MainWindow::on_actionEdit_settings_ini_triggered()
{
    QString program = "Editor";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

        arguments << m_SettingsFile;

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);
        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select editor program file"),
                                               m_AppDataPath,
                                               ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("Editor", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as editor...").arg(program) );
        }
    }
}

void MainWindow::on_actionExplore_application_folder_triggered()
{
    QString program = "Explorer";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

#ifdef Q_OS_WIN
        arguments <<  dir.homePath().replace("/","\\");
#else
        arguments <<  dir.homePath();
#endif

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);

        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select explorer program file"),
                                               m_AppDataPath,
                                               ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("Explorer", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as explorer...").arg(program) );
        }
    }
}

void MainWindow::on_actionExplorer_storage_folder_triggered()
{
    QString program = "Explorer";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

#ifdef Q_OS_WIN
        arguments <<  m_AppDataPath.replace("/","\\");
#else
        arguments <<  m_AppDataPath;
#endif
        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);
        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select explorer program file"),
                                               m_AppDataPath,
                                               ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("Explorer", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as explorer...").arg(program) );
        }
    }
}


void MainWindow::on_cboEPGChannels_currentTextChanged(const QString &arg1)
{
    QTreeWidgetItem* item = ui->twPLS_Items->currentItem();

    if ( item != nullptr ) {

        int extinf_id = item->data(0, Qt::UserRole+1).toInt();

        db.updateEXTINF_tvg_id_byRef(extinf_id, arg1);
    }
}

void MainWindow::on_mainToolBar_actionTriggered(QAction *action)
{
    ui->twPLS_Items->clearSelection();

    ui->twPLS_Items->topLevelItem( action->data().toInt() )->setSelected(true);
    ui->twPLS_Items->scrollToItem( ui->twPLS_Items->selectedItems().at(0) );
}

void MainWindow::readyReadStandardOutputJson()
{
    ui->edtOutput->clear();

    m_JsonString.append(m_Process2->readAllStandardOutput());

    QJsonDocument jsonResponse = QJsonDocument::fromJson(m_JsonString.toUtf8());

    QJsonObject jsonObj = jsonResponse.object();

    QJsonObject format = jsonObj.value("format").toObject();
    QJsonObject tags = format.value("tags").toObject();

    ui->edtOutput->append( "title:\t" + tags.value("StreamTitle").toString());
    ui->edtOutput->append( "" ) ;
    ui->edtOutput->append( "genre:\t" + tags.value("icy-genre").toString());
    ui->edtOutput->append( "name:\t" + tags.value("icy-name").toString());
    ui->edtOutput->append( "description:\t" + tags.value("icy-description").toString());
    ui->edtOutput->append( "audio-info:\t" + tags.value("icy-audio-info").toString());
}

void MainWindow::on_cmdGatherStream_clicked()
{
    QString program = "ffprobe";
    QStringList arguments;

    arguments << "-hide_banner" << ui->edtStationUrl->text();

    m_Process->kill();
    m_Process->setProcessChannelMode(QProcess::MergedChannels);
    m_Process->start(program, arguments);
}

void MainWindow::on_cmdGatherStreamData_clicked()
{
    QString program = "ffprobe";
    QStringList arguments;

    m_JsonString = "";

    arguments << "-v" << "quiet" << "-print_format" << "json=compact=1" << "-show_format" << ui->edtStationUrl->text();

    m_Process2->kill();
    m_Process2->setProcessChannelMode(QProcess::MergedChannels);
    m_Process2->start(program, arguments);
}

void MainWindow::on_edtFilter_2_returnPressed()
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue(QString("%1").arg(qHash(ui->cboPlaylists->currentText())), ui->edtFilter_2->text());
    settings.sync();

    this->fillTwPls_Item();
}

void MainWindow::on_radTv_clicked()
{
    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    db.updatePLS_kind(pls_id, 1 );

    ui->radTv->setChecked(true);
    ui->cmdImdb->setVisible(false);
    ui->cboEPGChannels->setVisible(true);
    ui->cmdEPG->setVisible(true);
    ui->cmdWiki->setVisible(true);
}

void MainWindow::on_radRadio_clicked()
{
    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    db.updatePLS_kind(pls_id, 2 );

    ui->radRadio->setChecked(true);
    ui->cmdImdb->setVisible(false);
    ui->cboEPGChannels->setVisible(false);
    ui->cmdEPG->setVisible(false);
    ui->cmdWiki->setVisible(true);
}

void MainWindow::on_radMovie_clicked()
{
    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    db.updatePLS_kind(pls_id, 3 );

    ui->radMovie->setChecked(true);
    ui->cmdImdb->setVisible(true);
    ui->cboEPGChannels->setVisible(false);
    ui->cmdEPG->setVisible(false);
    ui->cmdWiki->setVisible(true);
}

void MainWindow::on_actionExport_M3U_file_triggered()
{
    this->MakePlaylist();
}

void MainWindow::on_actionExport_logo_links_triggered()
{
    this->MakeLogoUrlList();
}

void MainWindow::on_actionImport_logo_links_triggered()
{
    this->ImportLogoUrlList();
}

void MainWindow::on_cmdEPG_clicked()
{
    QSqlQuery *select;
    QString   start, stop, title, descr;

    ui->edtOutput->clear();

    select = db.selectProgramData(ui->cboEPGChannels->currentText());
    while ( select->next() ) {

        start = select->value(0).toByteArray().constData();
        stop = select->value(1).toByteArray().constData();
        title = select->value(2).toByteArray().constData();
        descr = select->value(3).toByteArray().constData();

        ui->edtOutput->append( start + " - " + stop + " - <font color=\"lightgreen\">" + title + "</font><br><br>" + descr + "<br><br>");
    }

    delete select;

    ui->edtOutput->moveCursor(QTextCursor::Start);
}

void MainWindow::on_chkOnlyEpg_clicked()
{
     fillTwPls_Item();
}

void MainWindow::on_cmdWiki_clicked()
{
    QString program = "Browser";
    QStringList arguments;

    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    program = settings.value(program).toByteArray();

    QFile exe(program);

    if ( exe.exists() ) {

        arguments << QString("https://www.google.de/search?q=%1+Wikipedia").arg(ui->edtStationName->text());

        m_Process->setProcessChannelMode(QProcess::MergedChannels);
        m_Process->start(program, arguments);
        qDebug() << program << arguments;

    } else {

        program = QFileDialog::getOpenFileName(this, ("Select browser program file"),
                                               m_AppDataPath,
                                               ("exe program file (*.exe)"));

        if ( ! program.isNull() ) {

            settings.setValue("Browser", program);
            settings.sync();

            QMessageBox::information(this, "m3uMan", QString("%1 set as browser...").arg(program) );
        }
    }
}

bool MainWindow::Zip (const QString& filename , const QString& zipfilename){

    bool retVal = false;

    QFile infile(filename);
    QFile outfile(zipfilename);

    if ( infile.open(QIODevice::ReadOnly) ) {
        if ( outfile.open(QIODevice::WriteOnly) ) {

            QByteArray uncompressedData = infile.readAll();
            QByteArray compressedData = qCompress(uncompressedData,9);

            if ( outfile.write(compressedData) ) {
                retVal = true;
            }
            infile.close();
        }
        outfile.close();
    }
    return retVal;
}

bool MainWindow::UnZip (const QString& zipfilename , const QString& filename){

    bool retVal = false;

    QFile infile(zipfilename);
    QFile outfile(filename);

    if ( infile.open(QIODevice::ReadOnly) ) {
        if ( outfile.open(QIODevice::WriteOnly) ) {

            QByteArray compressedData = infile.readAll();
            QByteArray uncompressedData = qUncompress(compressedData);

            if ( outfile.write(uncompressedData) ) {
                retVal = true;
            }
            infile.close();
        }
        outfile.close();
    }

    return retVal;
}

void MainWindow::on_cboUrlEpgSource_currentTextChanged(const QString &arg1)
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);

    if ( ! QString( settings.value(arg1).toByteArray() ).isEmpty() &&
           QString( settings.value(arg1).toByteArray() ).contains(";") ) {
        ui->edtUrlEpg->setText( QString(settings.value(arg1).toByteArray()).split(";").at(0) );
        ui->edtUrlEpgHour->setText( QString(settings.value(arg1).toByteArray()).split(";").at(1) );
    } else {
        ui->edtUrlEpg->clear();
        ui->edtUrlEpgHour->clear();
    }
}

void MainWindow::on_edtUrlEpgHour_returnPressed()
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue(ui->cboUrlEpgSource->currentText(), ui->edtUrlEpg->text() + ";" + ui->edtUrlEpgHour->text());
    settings.sync();
}

void MainWindow::on_actionMake_backup_on_next_run_triggered()
{
    QSettings settings(m_SettingsFile, QSettings::IniFormat);
    settings.setValue("BackupOnStart", 1);
    settings.sync();

    QMessageBox::information(this, "m3uMan", QString("Backup will be done on next program start!" ) );
}

void MainWindow::on_actionWrite_INI_to_database_triggered()
{
    QFile file(m_SettingsFile);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)){

        db.removeINI();

        QTextStream stream(&file);

        while (!stream.atEnd()){

            const QString line = stream.readLine();

            if ( line.contains("=") ) {
                db.insertINI(line.mid( 0, line.indexOf("=", 0 )), line.mid( line.indexOf("=", 0 ) + 1 ));
            } else {
                db.insertINI(line, "" );
            }
        }

        file.close();
    }

    QMessageBox::information(this, "m3uMan", QString("INI Settings written to database!" ) );
}

void MainWindow::on_actionRestore_backup_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, ("Open Backup File"),
                                                    m_AppDataPath,
                                                    ("bck File (*.bck)"));

    if ( !fileName.isNull() ) {

        if ( this->UnZip( fileName, m_AppDataPath + "/m3uMan.sqlite_restored") ) {
            QMessageBox::information(this, "m3uMan", QString("Restore to <b>" +  m_AppDataPath + "/m3uMan.sqlite_restored") + "</b> successful done..." );
        }
    }
}


void MainWindow::on_actionRestore_INI_file_triggered()
{
    QSqlQuery *select ;

    QString saveFile = QFileDialog::getSaveFileName(this, tr("INI Data Save As..."), m_AppDataPath + "/settings_restore.ini", tr("Settings Files (*.ini)"));

    if ( saveFile.isEmpty() ) {
        return;
    }

    select = db.selectINI();

    QFile outfile(saveFile);

    if ( outfile.open(QIODevice::WriteOnly | QIODevice::Text) ) {

        while ( select->next() ) {

            if ( QString( select->value(2).toByteArray().constData() ).isEmpty() ) {
                outfile.write( QString("%1\n").arg(select->value(1).toByteArray().constData() ).toUtf8() ) ;
            } else {
                outfile.write( QString("%1=%2\n").arg(select->value(1).toByteArray().constData(),
                                                      select->value(2).toByteArray().constData()).toUtf8() );
            }
        }
        outfile.close();
    }

    delete select;

    QMessageBox::information(this, "m3uMan", QString("Restore to <b>" + saveFile + "</b> successful done..." ) ) ;
}

