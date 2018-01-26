#include "MainWindow.h"
#include "ui_MainWindow.h"

int write(QTcpSocket* socket, char* buffer, int length)
{
    int count = 0;
    int sum = 0;
    int error = 0;

    while(length > 0) {
        QTcpSocket::SocketState state = socket->state();

        if(state != QTcpSocket::ConnectedState) {
            return -1;
        }

        count = socket->write(buffer, length);

        if(count < 0) {
            return sum;
        } else if(count == 0) {
            socket->waitForBytesWritten(1000);
            error++;

            if(error > 3) {
                return sum;
            } else {
                continue;
            }
        } else {
            error = 0;
            sum += count;
            length -= count;
            buffer += count;
        }

        socket->flush();
    }

    return sum;
}
int write(QTcpSocket* socket, QByteArray* bytes)
{
    int length = bytes->size();
    char buffer[4];
    buffer[0] = (char)((length >> 24) & 0xff);
    buffer[1] = (char)((length >> 16) & 0xff);
    buffer[2] = (char)((length >> 8) & 0xff);
    buffer[3] = (char)((length >> 0) & 0xff);
    length = write(socket, buffer, 4);

    if(length != 4) {
        return -1;
    }

    length = bytes->size();

    if(write(socket, bytes->data(), length) != length) {
        return -2;
    }

    return length;
}



int read(QTcpSocket* socket, char* buffer, int length)
{
    int count = 0;
    int sum = 0;
    int error = 0;

    while(length > 0) {
        QTcpSocket::SocketState state = socket->state();

        if(state != QTcpSocket::ConnectedState) {
            return -1;
        }

        int available = socket->bytesAvailable();

        if(available == 0) {
            socket->waitForReadyRead(1000);
            error++;

            if(error > 3) {
                return sum;
            } else {
                continue;
            }
        }

        if(available > length) {
            available = length;
        }

        count = socket->read(buffer, available);

        if(count < 0) {
            return sum;
        } else if(count == 0) {
            continue;
        } else {
            error = 0;
            sum += count;
            length -= count;
            buffer += count;
        }
    }

    return sum;
}

int read(QTcpSocket* socket, QByteArray* bytes)
{
    bytes->resize(4);
    char* buffer = bytes->data();
    int length = read(socket, buffer, 4);

    if(length != 4) {
        return -1;
    }

    length = 0;
    length |= (buffer[0] & 0xff) << 24;
    length |= (buffer[1] & 0xff) << 16;
    length |= (buffer[2] & 0xff) << 8;
    length |= (buffer[3] & 0xff) << 0;

    if(length < 0) {
        return -2;
    } else if(length == 0) {
        bytes->resize(0);
        return 0;
    } else if(length > 32 * 1024 * 1024) {
        return -3;
    }

    bytes->resize(length);
    buffer = bytes->data();

    if(read(socket, buffer, length) != length) {
        return -4;
    }

    return length;
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
    mLogs.append(log);
    ui->mClipboardLog->setText(mLogs.join("\n"));
}

void MainWindow::onClipboardDataChanged()
{
    log(QString("onClipboardDataChanged"));
    QString address = ui->mRemoteAddress->text().trimmed();
    int port = ui->mRemotePort->text().toInt();

    if(address.length() == 0) {
        mStatusOperationLabel->setText("Remote address is empty");
        return;
    }

    if(port <= 0 || port >= 65536) {
        mStatusOperationLabel->setText("Remote port is invalid");
        return;
    }


    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    const QMimeData* data = mClipboard->mimeData();

    if(data->hasText()) {
        log(QString("send text"));
        QString text = mClipboard->text();
        QString format("text");
        QByteArray buffer = text.toUtf8();
        out << format.toUtf8();
        out << buffer;
        log(QString("text length: %1").arg(buffer.size()));
        ui->mClipboardText->setText(mClipboard->text());
    } else if(data->hasImage()) {
        log(QString("send image"));
        QImage image = mClipboard->image();
        QBuffer buffer;
        image.save(&buffer, "bmp");
        QString format("image");
        out << format.toUtf8();
        out << buffer.data();
        log(QString("image length: %1").arg(buffer.size()));
        log(QString("image width: %1").arg(image.width()));
        log(QString("image height: %1").arg(image.height()));
        ui->mClipboardImage->setPixmap(mClipboard->pixmap());
    } else {
        mStatusOperationLabel->setText("Clipboard is empty");
        return;
    }

    QTcpSocket socket(this);
    socket.connectToHost(QHostAddress(address), port);

    if(socket.waitForConnected(3000) && write(&socket, &bytes) > 0 && socket.waitForDisconnected(3000)) {
        mStatusOperationLabel->setText("Send data succeed");
    } else {
        mStatusOperationLabel->setText("Send data failed");
    }

    socket.close();
}



void MainWindow::onSocketAcceptConnection()
{
    disconnect(mClipboard, 0, 0, 0);
    QTcpSocket* socket = mQTcpServer->nextPendingConnection();
    QByteArray bytes;

    if(read(socket, &bytes) > 0) {
        QDataStream in(&bytes, QIODevice::ReadOnly);
        QByteArray buffer;
        buffer.clear();
        in >> buffer;
        QString format = QString(buffer);
        buffer.clear();

        if(format == "text") {
            log(QString("receive text"));
            in >> buffer;
            log(QString("text length: %1").arg(buffer.size()));
            QString text = QString(buffer);
            mClipboard->setText(text);
            ui->mClipboardText->setText(text);
        } else if(format == "image") {
            log(QString("receive image"));
            in >> buffer;
            log(QString("image length: %1").arg(buffer.size()));
            QImage image;
            image.loadFromData(buffer, Q_NULLPTR);
            log(QString("image width: %1").arg(image.width()));
            log(QString("image height: %1").arg(image.height()));
            mClipboard->setImage(image);
            ui->mClipboardImage->setPixmap(mClipboard->pixmap());
        } else {
            log(QString("unsupported type: %1").arg(format));
        }

        mStatusOperationLabel->setText("Receive data succeed");
    } else {
        mStatusOperationLabel->setText("Receive data failed");
    }

    socket->close();
    delete socket;
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
    if(mQTcpServer == NULL) {
        int port = ui->mLocalPort->text().toInt();

        if(port <= 0 || port >= 35536) {
            QMessageBox::information(NULL, "Error", "Local port is invalid", QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        mQTcpServer = new QTcpServer(this);

        if(!mQTcpServer->listen(QHostAddress::Any, port)) {
            mStatusOperationLabel->setText("Create server failed");
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

void MainWindow::on_mLogClear_clicked()
{
    mLogs.clear();
    ui->mClipboardLog->clear();
}
