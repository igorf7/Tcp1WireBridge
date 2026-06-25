#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "usertypes.h"
#include "onewire.h"
#include <QMainWindow>
#include <QLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void tcpConnect(const QString &hostName, quint16 port);
    void tcpDisconnect();
    void sendToServer(const QByteArray &msg);

protected:
    int owPollingEvent = 0;
    void timerEvent(QTimerEvent *event);

public slots:
    void onShowStatusBar(const QString &str, int timeout);
    void onConfirmTcpConnection(bool connected);
    void onTcpResponse(const QByteArray &response);
    void onShowTcpError(const QByteArray &err_msg);

private slots:
    void onTcpConnButton();
    void onSearchButtonClicked();
    void onStartButtonClicked();
    void onClearButtonClicked();
    void onDeviceComboBoxChanged(int index);
    void onInfoButtonClicked();

private:
    Ui::MainWindow *ui;

    QList<quint64> owDeviceAddressList;
    QList<QByteArray> owPacketQueue;
    QHash<quint64, int> selDevices;

    int owPollingTime = 1000; // ms
    int owTotalDeviceCount = 0;
    int owSelDeviceCount = 0;
    int owDevIndex = 0;

    quint32 rxCounter = 0;

    bool isTcpConnected = false;
    bool isOwSearchDone = false;
    bool isOwPollRunning = false;

    void startPolling();
    void owDataRead(quint16 read_data_len);
    void initDeviceComboBox();
    void sendOwCmd(TOpcode opcode);
    void putPacketToQueue(TOpcode opcode, const QByteArray &msg);
};
#endif // MAINWINDOW_H
