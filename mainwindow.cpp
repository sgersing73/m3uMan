#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_AppDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    QSettings settings("Freeware", "m3uMan");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    ui->splitter->restoreState(settings.value("splitter").toByteArray());
    ui->edtUrl->setText(settings.value("iptvurl").toByteArray());

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

    somethingchanged = false;

    qDebug() << QDir::homePath();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("Freeware", "m3uMan");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("splitter",ui->splitter->saveState());
    settings.setValue("iptvurl", ui->edtUrl->text());

    settings.sync(); // forces to write the settings to storage

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
                                                     QDir::homePath(),
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

    QString tags;
    QString station;
    QString url;
    QStringList parser;

    QString tvg_name;
    QString tvg_id;
    QString group_title;
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

        stream.seek(0);

        while (!stream.atEnd() && !ende){

            line = stream.readLine();

            if (progress.wasCanceled())
                ende = true;

            if ( line.contains("#EXTINF") ) {
                tags = line.split(",").at(0);
                station = line.split(",").at(1);
            } else if ( line.startsWith("http") ) {
                url = line;

                parser = this->splitCommandLine(tags);

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

                query = db.selectEXTINF_byUrl(url);

                query->last();
                query->first();

                if ( query->isValid() ) {
                    db.updateEXTINF_state_byRef( query->value(0).toByteArray().toInt(), 1 );
                } else {
                    db.addEXTINF(tvg_name, tvg_id, group_title, tvg_logo, url);
                }

                query->clear();

                counter++;

                if ( counter % 100 == 0 ) {

                    statusBar()->showMessage(tr("%1 stations").arg(counter));                    
                }

                QCoreApplication::processEvents();

                progress.setValue(counter);
            }
        }
    }

    file.close();

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "m3uMan", "Remove all obsolete stations?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {

        db.removeObsoleteEXTINFs();
    }

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

QTreeWidgetItem* MainWindow::addTreeRoot(const QString& name, const QString& description, const QString& id)
{
    // QTreeWidgetItem(QTreeWidget * parent, int type = Type)
    QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->treeWidget);

    // QTreeWidgetItem::setText(int column, const QString & text)
    treeItem->setText(0, name);
    treeItem->setText(1, description);
    treeItem->setText(2, id);

    return treeItem;
}

void MainWindow::addTreeChild(QTreeWidgetItem *parent, const QString& name, const QString& description, const QString& id, const QString& used, const QString& state)
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem();

    treeItem->setText(0, name);
    treeItem->setText(1, description);

    if ( state.toInt() == 0 ) {
         treeItem->setBackgroundColor(2, QColor("#FFCCCB") );
    }
    treeItem->setText(2, id);

    treeItem->setStatusTip(0, tr("double click to remove the station to the playlist"));

    if ( used.toInt() > 0 ) {
        treeItem->setBackgroundColor(0, QColor("#4CAF50") );
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

    // Add root nodes
    QTreeWidgetItem *item = nullptr;
    QSqlQuery *select = nullptr;

    ui->treeWidget->clear();
    ui->treeWidget->setColumnCount(3);
    ui->treeWidget->setHeaderLabels(QStringList() << "Group" << "Station" << "ID");

    //select = db.selectEXTINF(ui->edtFilter->text());

    if ( ui->cboGroupTitels->currentText().isEmpty() ) {
        group = "EU |";
    } else {
        group = ui->cboGroupTitels->currentText();
    }

    if ( ! ui->edtFilter->text().isEmpty() ) {
        group = "";
    }

    select = db.selectEXTINF(group, ui->edtFilter->text());

    ui->treeWidget->blockSignals(true);

    while ( select->next() ) {

        group = select->value(3).toByteArray().constData();

        if (group.isEmpty()) {
            group = " ";
        }

        title = select->value(1).toByteArray().constData();
        tvg_id = select->value(2).toByteArray().constData();
        id = select->value(0).toByteArray().constData();
        used = select->value(7).toByteArray().constData();
        state = select->value(6).toByteArray().constData();

        if ( group != lastgroup ) {
            item = addTreeRoot(group, "", "");
            lastgroup = group;
        }

        addTreeChild(item, title, tvg_id, id, used, state);
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

    ui->cmdDeletePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );
    ui->cmdRenamePlaylist->setEnabled( ui->cboPlaylists->count() != 0 );

    delete select;
}


void MainWindow::fillComboGroupTitels()
{
    QSqlQuery *select = nullptr;
    QString title;
    QString id;

    ui->cboGroupTitels->clear();

    select = db.selectEXTINF_group_titles();
    while ( select->next() ) {

        title = select->value(0).toByteArray().constData();

        ui->cboGroupTitels->addItem(title);
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
                                  QString("Playlist %1 added...").arg(text));
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

    int pls_id = ui->cboPlaylists->itemData(ui->cboPlaylists->currentIndex()).toString().toInt();

    // Add root nodes
    QSqlQuery *select = nullptr;

    ui->twPLS_Items->clear();
    ui->twPLS_Items->setColumnCount(1);
    ui->twPLS_Items->setHeaderLabels(QStringList() << "Stations");

    select = db.selectPLS_Items(pls_id);
    while ( select->next() ) {

        id = select->value(0).toByteArray().constData();
        extinf_id = select->value(2).toByteArray().constData();
        tvg_name = select->value(5).toByteArray().constData();

        QTreeWidgetItem *treeItem = new QTreeWidgetItem(ui->twPLS_Items);

        treeItem->setText(0, tvg_name);
        treeItem->setData(0, 1, id);
        treeItem->setData(1, 1, extinf_id);
        treeItem->setStatusTip(0, tr("double click to remove the station"));
    }

    delete select;
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

    QFile file(ui->cboPlaylists->currentText() + ".m3u");

    if(!file.open(QFile::WriteOnly |
                  QFile::Text))
    {
        qDebug() << " Could not open file for writing";
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
    }
}

void MainWindow::SaveM3u()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString timestamp = now.toString(QLatin1String("yyyyMMddhhmmss"));

    const QString filename = QString::fromLatin1("iptv-%1.m3u").arg(timestamp);

    QFile newDoc(filename);

    if ( m_pImgCtrl->downloadedData().size() > 0 ) {
        if(newDoc.open(QIODevice::WriteOnly)){

            newDoc.write(m_pImgCtrl->downloadedData());
            newDoc.close();

            statusBar()->showMessage(tr("file %1 saved").arg(filename), 2000);

            QMessageBox(QMessageBox::Information, "Downloader", tr("File %1 download success!").arg(filename), QMessageBox::Ok).exec() ;

            if (ui->chkImport->isChecked() ) {
                this->getFileData(filename);
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

    qDebug() << ui->twPLS_Items->selectedItems().count();

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

            qDebug() << url;
        }

        delete select;

        ui->edtStationUrl->setText(url);

        qDebug() << "logo" << logo;

        m_pImgCtrl = new FileDownloader(logo, this);

        connect(m_pImgCtrl, SIGNAL(downloaded()), SLOT(loadImage()));
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
    buttonImage.loadFromData(m_pImgCtrl->downloadedData());

    ui->lblLogo->setPixmap(buttonImage);
}

void MainWindow::on_cmdPlayStream_clicked()
{
    QString program = "vlc";
    QStringList arguments;

    arguments << ui->edtStationUrl->text();

    m_Process->setProcessChannelMode(QProcess::MergedChannels);
    m_Process->start(program, arguments);
}
