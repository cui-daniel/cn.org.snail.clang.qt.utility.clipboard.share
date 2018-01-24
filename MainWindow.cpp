#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QDebug>
#include <QtNetwork>
#include <QMessageBox>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mSysTrayIcon = new QSystemTrayIcon(this);
    //新建托盘要显示的icon
    QIcon icon = QIcon(":/icon/application.ico");
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
    qDebug() << "onClipboardDataChanged";
    ui->mClipboardContent->setText(mClipboard->text());
    QTcpSocket socket(this);
    QString address = ui->mRemoteAddress->text();
    int port = ui->mRemotePort->text().toInt();
    address = address.trimmed();

    if(address.length() == 0) {
        mStatusLabel->setText("Server address is empty");
        return;
    }

    if(port <= 0 || port >= 65536) {
        mStatusLabel->setText("Server address is empty");
        return;
    }

    socket.connectToHost(QHostAddress(address), port);

    if(socket.waitForConnected(3000)) {
        const QMimeData* data = mClipboard->mimeData();
        QStringList formats = data->formats();
        QDataStream out(&socket);

        int i = 0;
        int count = formats.size();
        qDebug() << "send clipboard item count: " << count;
        out << count;

        foreach(QString format, data->formats()) {
            {
                QByteArray buffer = format.toUtf8();
                qDebug() << "send clipboard item[" << count << "/" << i << "].format.length =" << buffer.size();
                qDebug() << "send clipboard item[" << count << "/" << i << "].format.data =" << format;
                out << buffer;
            }
            {
                QByteArray buffer = data->data(format);
                qDebug() << "send clipboard item[" << count << "/" << i << "].data.length =" << buffer.size();
                out << buffer;
            }
            i++;
        }

        socket.flush();

        while(socket.waitForBytesWritten(3000)) {
            qDebug() << "send ...";
        }
    } else {
        mStatusLabel->setText("Connect server failed");
    }

    socket.close();
    mStatusLabel->setText("Send data succeed");
}

void MainWindow::onSocketAcceptConnection()
{
    QTcpSocket* socket = mQTcpServer->nextPendingConnection();

    if(socket->waitForReadyRead(3000)) {
        mStatusLabel->setText("Read remote data ...");
        qDebug() << "onSocketAcceptConnection";
        QMimeData* data = new QMimeData();
        QDataStream in(socket);
        int count;
        in >> count;
        qDebug() << "receive clipboard item count: " << count;

        QByteArray buffer;

        for(int i = 0; i < count; i++) {
            socket->waitForReadyRead(3000);
            buffer.clear();
            in >> buffer;
            qDebug() << "receive clipboard item[" << count << "/" << i << "].format.length =" << buffer.size();
            QString format = QString(buffer);
            qDebug() << "receive clipboard item[" << count << "/" << i << "].format.data =" << format;
            buffer.clear();
            in >> buffer;
            qDebug() << "receive clipboard item[" << count << "/" << i << "].data.length =" << buffer.size();
            data->setData(format, buffer);
        }

        mStatusLabel->setText("Read remote data succeed");
        mClipboard->setMimeData(data);
    } else {
        mStatusLabel->setText("Read remote data failed");
    }

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

void MainWindow::on_mSwitchButton_clicked()
{
    if(mQTcpServer == NULL) {
        int port = ui->mLocalPort->text().toInt();

        if(port <= 0 || port >= 35536) {
            QMessageBox::information(NULL, "Error", "Local port is invalid", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        mQTcpServer = new QTcpServer(this);

        if(!mQTcpServer->listen(QHostAddress::Any, 12345)) {
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
