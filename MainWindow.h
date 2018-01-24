#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QClipboard>
#include <QLabel>
#include <QTcpServer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QSystemTrayIcon *mSysTrayIcon;
    QClipboard *mClipboard;
    QLabel* mStatusLabel = NULL;
    QLabel* mServerStatusLabel = NULL;
    QTcpServer* mQTcpServer = NULL;

private slots:
    void onClipboardDataChanged();
    void onActivatedSystemTrayIcon(QSystemTrayIcon::ActivationReason);
    void onSocketAcceptConnection();
    void on_mSwitchButton_clicked();
};

#endif // MAINWINDOW_H
