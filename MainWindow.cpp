#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>
#define CHAR(X) static_cast<char>(X)
#define INT(X) static_cast<int>(X)
#define USHORT(X) static_cast<ushort>(X)

int MainWindow::write(QTcpSocket* socket,QByteArray* bytes){
    long long count = 0;
    int sum = 0;
    int length=bytes->size();
    int timeout=0;
    char* data=bytes->data();
    char size[4];
    size[0]=CHAR((length>>24)&0xff);
    size[1]=CHAR((length>>16)&0xff);
    size[2]=CHAR((length>>8)&0xff);
    size[3]=CHAR((length>>0)&0xff);
    socket->write(size,4);
    char str[1024];
    sprintf(str,"write size: %d  %02x %02x %02x %02x",length, INT(size[0]&0xff),INT(size[0]&0xff),INT(size[0]&0xff),INT(size[0]&0xff));
    qDebug()<<QString(str);
    while(length>0) {
        if(socket->state() != QTcpSocket::ConnectedState) {
            qDebug()<<"socket status not is connected";
            return -1;
        }
        if(timeout>5){
             qDebug()<<"socket write timeout";
             return -1;
        }
        count = socket->write(data, length);
        qDebug()<<"socket write data count: "<<length<<"/"<<count;
        if(count < 0) {
            qDebug()<<"socket write failure ";
            break;
        } else if(count == 0) {
            socket->waitForBytesWritten(1000);
            timeout++;
            continue;
        } else {
           data+=count;
           sum+=count;
           length-=count;
           socket->flush();
        }
    }
    qDebug()<<"socket write success ";
    return sum;
}

int MainWindow::read(QTcpSocket* socket, QByteArray* bytes){
   char buffer[4096];
   bytes->clear();
   socket->waitForReadyRead(1000);
   char size[4];
   socket->read(size,4);
   int length=0;
   int timeout=0;
   length|=(size[0]&0xff)<<24;
   length|=(size[1]&0xff)<<16;
   length|=(size[2]&0xff)<<8;
   length|=(size[3]&0xff)<<0;
   char str[1024];
   sprintf(str,"read size: %d  %02x %02x %02x %02x",length, INT(size[0]&0xff),INT(size[0]&0xff),INT(size[0]&0xff),INT(size[0]&0xff));
   qDebug()<<QString(str);
   while(length>0) {
       if(socket->state() != QTcpSocket::ConnectedState) {
            qDebug()<<"socket status not is connected";
         return -1;
       }
       if(timeout>5){
          qDebug()<<"socket read timeout";
           return -1;
       }
       long long count = socket->bytesAvailable();

       if(count == 0) {
           socket->waitForReadyRead(1000);
       }

       if(count > 4096) {
           count = 4096;
       }

       count = socket->read(buffer, count);
       qDebug()<<"socket read data count: "<<length<<"/"<<count;
       if(count < 0) {
           qDebug()<<"socket read failure ";
          return -1;
       } else if(count == 0) {
           timeout++;
           continue;
       } else {
           timeout=0;
           length-=count;
           bytes->append(buffer,INT(count));
       }
   }
    qDebug()<<"socket read success ";
    return bytes->size();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mSysTrayIcon = new QSystemTrayIcon(this);
    //新建托盘要显示的icon
    QIcon icon = QIcon(":/icon/application.png");
    //将icon设到QSystemTrayIcon对象中
    mSysTrayIcon->setIcon(icon);
    connect(mSysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onActivatedSystemTrayIcon(QSystemTrayIcon::ActivationReason)));

    //在系统托盘显示此对象
    mSysTrayIcon->show();

    mClipboard = QApplication::clipboard(); //获取系统剪贴板指针
    connect(mClipboard, SIGNAL(dataChanged()), this, SLOT(onClipboardDataChanged()));
    //connect(mClipboard, SIGNAL(changed(QClipboard::Mode)), this, SLOT(onClipboardModeChanged(QClipboard::Mode)));

    mStatusOperationLabel = new QLabel();
    mStatusOperationLabel->setMinimumSize(mStatusOperationLabel->sizeHint());
    mStatusOperationLabel->setAlignment(Qt::AlignHCenter);
    ui->mStatusBar->addWidget(mStatusOperationLabel);
    mServerStatusLabel = new QLabel();
    mServerStatusLabel->setMinimumSize(mServerStatusLabel->sizeHint());
    mServerStatusLabel->setAlignment(Qt::AlignHCenter);
    ui->mStatusBar->addWidget(mServerStatusLabel);
    mStatusOperationLabel->setText("Ready");
    mServerStatusLabel->setText("Server is stopped");
    ui->mClipboardImage->setAlignment(Qt::AlignCenter);
    ui->mClipboardImage->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    ui->mClipboardImage->setScaledContents(true);


    QStringList addresses;

    foreach(const QHostAddress& hostAddress, QNetworkInterface::allAddresses()) {
        if(hostAddress != QHostAddress::LocalHost && hostAddress.toIPv4Address()) {
            addresses.append(hostAddress.toString());
        }
    }

    ui->mLocalAddress->setText(addresses.join(", "));
}

void MainWindow::log(QString log)
{
    qDebug()<<log;
    mLogs.append(log);
    ui->mClipboardLog->setText(mLogs.join("\n"));
}
void MainWindow::onClipboardModeChanged(QClipboard::Mode mode){
    log(QString("onClipboardModeChanged(%1)").arg(mode));
}
void MainWindow::onClipboardDataChanged()
{
    uint now = QDateTime::currentDateTime().toTime_t();
    log(QString("Clipboard last update time:%1 at %2").arg(mUpdateTime).arg(now));
    if(mUpdateTime>now){
       mUpdateTime=now;
    }
    if(now-mUpdateTime<=5){
        log(QString("Ignore clipboard data change event"));
        return;
    }

    QString address = ui->mRemoteAddress->text().trimmed();
    int port = ui->mRemotePort->text().toInt();

    if(address.length() == 0) {
        log(QString("Remote address is empty"));
        return;
    }

    if(port <= 0 || port >= 65536) {
        log(QString("Remote port is invalid"));
        return;
    }

    QByteArray bytes;
    const QMimeData* data = mClipboard->mimeData();
    if(data->hasText()) {
        log(QString("send text"));
        bytes.append('t');
        bytes.append(data->text().toUtf8());
        ui->mClipboardText->setText(data->text());
    } else if(data->hasImage()) {
        log(QString("send image"));
        QPixmap image= qvariant_cast<QPixmap>(data->imageData());
        QByteArray array;
        QBuffer buffer(&array);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer,"jpg",100);
        bytes.append('i');
        bytes.append(array);
        ui->mClipboardImage->setPixmap(image);
        ui->mClipboardImage->resize(image.size());
        ui->mScrollArea->widget()->resize(image.size());
    }else{
        return;
    }

    qDebug()<<"write socket open";
    QTcpSocket socket(this);
    socket.connectToHost(QHostAddress(address),USHORT(port));
    socket.waitForConnected(3000);
    if(write(&socket, &bytes) >0) {
        socket.waitForDisconnected();
        log(QString("Sent data:%1 succeed").arg(bytes.size()));
    } else {
        log(QString("Sent data:%1 failed").arg(bytes.size()));
    }
    qDebug()<<"write socket close";
    socket.close();
}



void MainWindow::onSocketAcceptConnection()
{
    qDebug()<<"read socket open";
    QTcpSocket* socket = mQTcpServer->nextPendingConnection();
    QByteArray bytes;

    if(read(socket, &bytes) > 0) {
        disconnect(mClipboard,nullptr,nullptr,nullptr);
        log(QString("Received data size:%1").arg(bytes.size()));
        if(bytes.at(0)=='t'){
            log(QString("receive text"));
            bytes.remove(0,1);
            QString text(bytes);
            ui->mClipboardText->setText(text);
            mClipboard->setText(text);
        }else  if(bytes.at(0)=='i'){
            log(QString("receive image"));
            QPixmap pixmap;
            bytes.remove(0,1);
            pixmap.loadFromData(bytes,"jpg");
            ui->mClipboardImage->setPixmap(pixmap);
            ui->mClipboardImage->resize(pixmap.size());
            ui->mScrollArea->widget()->resize(pixmap.size());
            mClipboard->setPixmap(pixmap);
        }
        connect(mClipboard, SIGNAL(dataChanged()), this, SLOT(onClipboardDataChanged()));
        mUpdateTime = QDateTime::currentDateTime().toTime_t();
        log(QString("Clipboard update at %1").arg(mUpdateTime));
        mStatusOperationLabel->setText("Receive data succeed");
    } else {
        mStatusOperationLabel->setText("Receive data failed");
    }
    qDebug()<<"read socket close";
    socket->close();
    delete socket;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onActivatedSystemTrayIcon(QSystemTrayIcon::ActivationReason reason)
{
    switch(reason) {
        case QSystemTrayIcon::Trigger:
            if(this->isHidden()) {
                this->show();
            } else {
                this->hide();
            }

            break;

        default:
            break;
    }
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if(QMessageBox::question(this, "Quit", "Are you sure you want to quit?", QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape) == QMessageBox::Yes) {
        event->accept();
    } else {
        this->hide();
        event->ignore();
    }
}

void MainWindow::on_mSwitchButton_clicked()
{
    if(mQTcpServer == nullptr) {
        int port = ui->mLocalPort->text().toInt();

        if(port <= 0 || port >= 35536) {
            QMessageBox::information(nullptr, "Error", "Local port is invalid", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        mQTcpServer = new QTcpServer(this);

        if(!mQTcpServer->listen(QHostAddress::Any, USHORT(port))) {
            mStatusOperationLabel->setText("Create server failed");
            mQTcpServer->close();
            delete mQTcpServer;
            mQTcpServer = nullptr;
            QMessageBox::information(nullptr, "Error", "Create server failed", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        //newConnection()用于当有客户端访问时发出信号，acceptConnection()信号处理函数
        connect(mQTcpServer, SIGNAL(newConnection()), this, SLOT(onSocketAcceptConnection()));
        ui->mSwitchButton->setText("Stop");
        ui->mLocalPort->setReadOnly(true);
        ui->mRemoteAddress->setReadOnly(true);
        ui->mRemotePort->setReadOnly(true);
        mServerStatusLabel->setText("Server is running");
    } else {
        disconnect(mQTcpServer, nullptr, nullptr, nullptr) ;
        mQTcpServer->close();
        delete mQTcpServer;
        mQTcpServer = nullptr;
        ui->mLocalPort->setReadOnly(false);
        ui->mRemoteAddress->setReadOnly(false);
        ui->mRemotePort->setReadOnly(false);
        ui->mSwitchButton->setText("Start");
        mServerStatusLabel->setText("Server is stopped");
    }
}

void MainWindow::on_mLogClear_clicked()
{
    mLogs.clear();
    ui->mClipboardLog->clear();
}
