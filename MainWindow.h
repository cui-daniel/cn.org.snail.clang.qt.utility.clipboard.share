#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QClipboard>
#include <QLabel>
#include <QTcpServer>
#include <QTextStream>
#include <QtNetwork>
#include <QCloseEvent>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    uint mUpdateTime;
    Ui::MainWindow *ui;
    QSystemTrayIcon *mSysTrayIcon;
    QClipboard *mClipboard;
    QLabel* mStatusOperationLabel = nullptr;
    QLabel* mServerStatusLabel = nullptr;
    QTcpServer* mQTcpServer = nullptr;
    QStringList mLogs;

    void log(QString log);


private slots:
    int write(QTcpSocket* socket,QByteArray* bytes);
    int read(QTcpSocket* socket, QByteArray* bytes);
    void onClipboardDataChanged();
    void onClipboardModeChanged(QClipboard::Mode);
    void onActivatedSystemTrayIcon(QSystemTrayIcon::ActivationReason);
    void onSocketAcceptConnection();
    void on_mSwitchButton_clicked();
    void on_mLogClear_clicked();
    void closeEvent(QCloseEvent *event);//由于要关闭窗口变为隐藏至托盘图标，所以要重写close事件
};

#endif // MAINWINDOW_H
