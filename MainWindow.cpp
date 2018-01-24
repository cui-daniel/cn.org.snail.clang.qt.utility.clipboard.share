#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QtNetwork>
#include <QMessageBox>

#define qDebug() (*mOut)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    mOut=new QTextStream(stdout);
    qDebug()<<"1"<<endl;

    mSupportedTextMimeTypes.append("text/plain");
    mSupportedTextMimeTypes.append("text/html");
    mSupportedTextMimeTypes.append("text/uri-list");

    mSupportedImageMimeTypes.append("image/bmp");
    mSupportedImageMimeTypes.append("image/png");
    mSupportedImageMimeTypes.append("image/jpeg");

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

    mStatusLabel = new QLabel();
    mStatusLabel->setMinimumSize(mStatusLabel->sizeHint());
    mStatusLabel->setAlignment(Qt::AlignHCenter);
    ui->mStatusBar->addWidget(mStatusLabel);
    mServerStatusLabel = new QLabel();
    mServerStatusLabel->setMinimumSize(mServerStatusLabel->sizeHint());
    mServerStatusLabel->setAlignment(Qt::AlignHCenter);
    ui->mStatusBar->addWidget(mServerStatusLabel);
    mStatusLabel->setText("Ready");
    mServerStatusLabel->setText("Server is stopped");



    QStringList addresses;

    foreach(const QHostAddress& hostAddress, QNetworkInterface::allAddresses()) {
        if(hostAddress != QHostAddress::LocalHost && hostAddress.toIPv4Address()) {
            addresses.append(hostAddress.toString());
        }
    }

    ui->mLocalAddress->setText(addresses.join(", "));
}

void MainWindow::onClipboardDataChanged()
{
    qDebug() << "onClipboardDataChanged"<<endl;;
    ui->mClipboardContent->setText(mClipboard->text());
    QTcpSocket socket(this);
    QString address = ui->mRemoteAddress->text().trimmed();
    int port = ui->mRemotePort->text().toInt();

    if(address.length() == 0) {
        mStatusLabel->setText("Remote address is empty");
        return;
    }

    if(port <= 0 || port >= 65536) {
        mStatusLabel->setText("Remote port is invalid");
        return;
    }
    QByteArray bytes;
    {
        const QMimeData* data = mClipboard->mimeData();
        QStringList formats;
        QDataStream out(&bytes,QIODevice::WriteOnly);
        foreach(QString format, mSupportedTextMimeTypes) {
            if(data->hasFormat(format)){
                formats.append(format);
            }
        }
        foreach(QString format, mSupportedImageMimeTypes) {
            if(data->hasFormat(format)){
                formats.append(format);
                break;
            }
        }
        int i = 0;
        int count = formats.size();

        qDebug() << "send clipboard item count: " << count<<endl;;
        out << count;

        foreach(QString format, formats) {
            {
                QByteArray buffer = format.toUtf8();
                qDebug() << "send clipboard item[" << count << "/" << i << "].format.length =" << buffer.size()<<endl;;
                qDebug() << "send clipboard item[" << count << "/" << i << "].format.data =" << format<<endl;;
                out << buffer;
            }
            {
                QByteArray buffer = data->data(format);
                qDebug() << "send clipboard item[" << count << "/" << i << "].data.length =" << buffer.size()<<endl;;
                out << buffer;
            }
            i++;
        }
    }

    socket.connectToHost(QHostAddress(address), port);

    if(socket.waitForConnected(3000)) {
        int count=bytes.size();

        char* raw=bytes.data();
        while (count>0) {
            int length=socket.write(raw,count);
            qDebug()<<length<<endl;
            if(length==-1){
                break;
            }else if(length<count){
                socket.waitForBytesWritten(3000);
            }
            count-=length;
            if(!socket.waitForBytesWritten(3000)){
                qDebug()<<"write..."<<endl;
            }
        }
        socket.waitForDisconnected(10000);
        socket.flush();
        mStatusLabel->setText("Send data succeed");
    } else {
        mStatusLabel->setText("Connect server failed");
    }
    socket.close();
}

void writeLength(QTcpSocket* socket,int value){
    char buffer[4];
    buffer[0]=(char)((value>>24)&0xff);
    buffer[1]=(char)((value>>16)&0xff);
    buffer[2]=(char)((value>>8)&0xff);
    buffer[3]=(char)((value>>0)&0xff);
    socket->write(&buffer);
}
int readLength(QTcpSocket* socket){
    char buffer[4]={0,0,0,0};
    if(socket->read(&buffer,4)!=4){
        return -1;
    }
    int result=0;
    result|=(buffer[0]&0xff)<<24;
    result|=(buffer[0]&0xff)<<16;
    result|=(buffer[0]&0xff)<<8;
    result|=(buffer[0]&0xff)<<0;
    return result;
}
void MainWindow::onSocketAcceptConnection()
{
    disconnect(mClipboard, 0, 0, 0);
    QMimeData* data = new QMimeData();
    QTcpSocket* socket = mQTcpServer->nextPendingConnection();
    QByteArray bytes;
    while(socket->state()==QTcpSocket::SocketState::ConnectedState){
        if(!socket->waitForReadyRead(3000)){
            continue;
        }
        bytes.append(socket->readAll());
    }
    socket->close();
    qDebug() << "receive clipboard item length: " << bytes.size()<<endl;;
    QDataStream in(&bytes,QIODevice::ReadOnly);
    QByteArray buffer;
    int count;
    in >> count;
    qDebug() << "receive clipboard item count: " << count<<endl;;
    for(int i = 0; i < count; i++) {
        buffer.clear();
        in >> buffer;
        qDebug() << "receive clipboard item[" << count << "/" << i << "].format.length =" << buffer.size()<<endl;;
        QString format = QString(buffer);
        qDebug() << "receive clipboard item[" << count << "/" << i << "].format.data =" << format<<endl;;
        buffer.clear();
        in >> buffer;
        qDebug() << "receive clipboard item[" << count << "/" << i << "].data.length =" << buffer.size()<<endl;;
        data->setData(format, buffer);
    }
    mClipboard->setMimeData(data);
    connect(mClipboard, SIGNAL(dataChanged()), this, SLOT(onClipboardDataChanged()));
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

void MainWindow::on_mSwitchButton_clicked()
{
    if(mQTcpServer == NULL) {
        int port = ui->mLocalPort->text().toInt();

        if(port <= 0 || port >= 35536) {
            QMessageBox::information(NULL, "Error", "Local port is invalid", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        mQTcpServer = new QTcpServer(this);

        if(!mQTcpServer->listen(QHostAddress::Any, port)) {
            mStatusLabel->setText("Create server failed");
            mQTcpServer->close();
            delete mQTcpServer;
            mQTcpServer = NULL;
            QMessageBox::information(NULL, "Error", "Create server failed", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        //newConnection()用于当有客户端访问时发出信号，acceptConnection()信号处理函数
        connect(mQTcpServer, SIGNAL(newConnection()), this, SLOT(onSocketAcceptConnection()));
        ui->mSwitchButton->setText("Stop");
        ui->mLocalPort->setReadOnly(true);
        mServerStatusLabel->setText("Server is running");
    } else {
        disconnect(mQTcpServer, 0, 0, 0) ;
        mQTcpServer->close();
        delete mQTcpServer;
        mQTcpServer = NULL;
        ui->mLocalPort->setReadOnly(false);
        ui->mSwitchButton->setText("Start");
        mServerStatusLabel->setText("Server is stopped");
    }
}
