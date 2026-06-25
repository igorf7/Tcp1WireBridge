#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tcpclient.h"
#include <QThread>
#include <QMessageBox>

/**
 * @brief MainWindow Class Constructor
 * @param parent_object
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName());

    /* Create TCP client thread */
    TcpClient *tcpClient = new TcpClient;
    QThread *threadTcpPort = new QThread;
    tcpClient->moveToThread(threadTcpPort);
    threadTcpPort->start();

    /* Connecting signals to slots */
    connect(threadTcpPort, &QThread::started, tcpClient, &TcpClient::onClientStart);
    connect(threadTcpPort, &QThread::finished, tcpClient, &TcpClient::deleteLater);
    connect(tcpClient, &TcpClient::quitTcpClient, threadTcpPort, &QThread::deleteLater);

    connect(tcpClient, &TcpClient::showResponse, this, &MainWindow::onTcpResponse);
    connect(tcpClient, &TcpClient::showError, this, &MainWindow::onShowTcpError);
    connect(tcpClient, &TcpClient::confirmTcpConnection, this, &MainWindow::onConfirmTcpConnection);

    connect(this, &MainWindow::tcpConnect, tcpClient, &TcpClient::onSetTcpConnection);
    connect(this, &MainWindow::tcpDisconnect, tcpClient, &TcpClient::onSetTcpDisconnection);
    connect(this, &MainWindow::sendToServer, tcpClient, &TcpClient::onSendToServer);

    connect(ui->tcpConnButton, SIGNAL(clicked()), this, SLOT(onTcpConnButton()));
    connect(ui->searchPushButton, SIGNAL(clicked()), this, SLOT(onSearchButtonClicked()));
    connect(ui->deviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onDeviceComboBoxChanged(int)));
    connect(ui->startPollingButton, SIGNAL(clicked()), this, SLOT(onStartButtonClicked()));
    connect(ui->clearPushButton, SIGNAL(clicked()), this, SLOT(onClearButtonClicked()));
    connect(ui->infoPushButton, SIGNAL(clicked()), this, SLOT(onInfoButtonClicked()));

    /* TCP connection settings by default */
    ui->tcpHostnameEdit->setText("192.168.1.7");
    ui->tcpPortEdit->setText("20108");

    /* Activate status bar */
    QFont font;
    font.setItalic(true);
    statusBar()->setFont(font);
}

/**
 * @brief MainWindow Class Destructor
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief intToByteArray
 * @param value
 * @return
 */
template<typename Type>
QByteArray intToByteArray(Type value)
{
    QByteArray array;

    while (array.size() != sizeof(value))
    {
        array.append((quint8)value);
        value >>= 8;
    }

    return array;
}

/**
 * @brief onShowStatusBar
 * @param str
 * @param timeout
 */
void MainWindow::onShowStatusBar(const QString &str, int timeout)
{
    statusBar()->showMessage(str, timeout);
}

/**
 * @brief MainWindow::onTcpConnButton
 */
void MainWindow::onTcpConnButton()
{
    if (isTcpConnected) {
        emit tcpDisconnect();
    }
    else {
        emit tcpConnect(ui->tcpHostnameEdit->text(),
                        ui->tcpPortEdit->text().toUShort());
    }
}

/**
 * @brief MainWindow::onConfirmTcpConnection
 * @param isConnected
 */
void MainWindow::onConfirmTcpConnection(bool connected)
{
    if (connected) {
        ui->tcpConnButton->setIcon(QPixmap(":/images/connect.png"));
        statusBar()->showMessage(tr("Connected"));
    }
    else {
        ui->tcpConnButton->setIcon(QPixmap(":/images/disconnect.png"));
        statusBar()->showMessage(tr("Disconnected"));
        if (isOwPollRunning) {
            this->onStartButtonClicked();
        }
    }
    isTcpConnected = connected;
}

/**
 * @brief MainWindow::timerEvent
 * @param event
 */
void MainWindow::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == owPollingEvent) {
        if (owPollingEvent != 0) {
            killTimer(owPollingEvent);
            owPollingEvent = 0;
            this->owDataRead(9);
        }
    }
}

/**
 * @brief MainWindow::onStartButtonClicked
 */
void MainWindow::onStartButtonClicked()
{
    if (!isTcpConnected) return;

    if (!isOwPollRunning) {
        isOwPollRunning = true;
        ui->startPollingButton->setText(tr("Stop"));
        this->startPolling();
    }
    else {
        isOwPollRunning = false;
        ui->startPollingButton->setText(tr("Start"));
        if (owPollingEvent != 0) {
            killTimer(owPollingEvent);
            owPollingEvent = 0;
        }
    }
}

/**
 * @brief MainWindow::onClearButtonClicked
 */
void MainWindow::onClearButtonClicked()
{
    rxCounter = 0;
    ui->textEdit->clear();
}

/**
 * @brief MainWindow::onInfoButtonClicked
 */
void MainWindow::onInfoButtonClicked()
{
    quint8 family = OneWire::getFamily(ui->deviceComboBox->currentText());
    QMessageBox::information(this, tr("Description"), OneWire::getDescription(family));
}

/**
 * @brief MainWindow::onSearchButtonClicked
 */
void MainWindow::onSearchButtonClicked()
{
    if (!isTcpConnected) return;

    ui->startPollingButton->setEnabled(false);
    ui->infoPushButton->setEnabled(false);

    isOwSearchDone = false;
    owDeviceAddressList.clear();

    QByteArray txData;
    txData.append((quint8)eOwSearchRom);
    txData.append((quint8)0);
    emit sendToServer(txData);
}

/**
 * @brief MainWindow::initDeviceComboBox
 */
void MainWindow::initDeviceComboBox()
{
    ui->deviceComboBox->clear();

    QStringList ComboBoxItems;

    for (int i = 0; i < owDeviceAddressList.size(); ++i) {
        quint8 dev_family = owDeviceAddressList.at(i) & 0xFF;
        QString name = OneWire::getName(dev_family);
        if (!ComboBoxItems.contains(name)) {
            ComboBoxItems.append(name);
        }
    }
    if (!ComboBoxItems.isEmpty()) {
        ui->deviceComboBox->addItems(ComboBoxItems);
    }
}

/**
 * @brief onDeviceComboBoxChanged
 * @param index
 */
void MainWindow::onDeviceComboBoxChanged(int index)
{
    Q_UNUSED(index)

    if (!isTcpConnected) return;

    selDevices.clear();
    owDevIndex = 0;

    quint8 dev_family = OneWire::getFamily(ui->deviceComboBox->currentText());

    for (int i = 0, j = 0; i < owDeviceAddressList.size(); ++i) {
        if (dev_family == (owDeviceAddressList.at(i) & 0xFF)) {
            selDevices.insert(owDeviceAddressList.at(i), j++);
        }
    }

    owSelDeviceCount = selDevices.size();
    statusBar()->showMessage(ui->deviceComboBox->currentText() +
                            " device found: " + QString::number(owSelDeviceCount));
}

/**
 * @brief MainWindow::startPolling
 */
void MainWindow::startPolling()
{
    QByteArray packet;
    quint8 rom_cmd = (owSelDeviceCount > 1) ? OW_SKIPROM_CMD : OW_MATCHROM_CMD;

    if (owPacketQueue.isEmpty()) owPacketQueue.clear();

    /* Place MATCHROM/SKIPROM packet into packet queue */
    packet.append(rom_cmd);
    this->putPacketToQueue(eOwBusWrite, packet);

    /* Place ADDRESS packet into packet queue */
    if (owSelDeviceCount == 1) { // this for DD84
        packet.clear();
        packet.append(intToByteArray(selDevices.key(owDevIndex)));
        this->putPacketToQueue(eOwBusWrite, packet);
    }

    /* Place OW_CONVERT_CMD packet into packet queue */
    packet.clear();
    packet.append(OW_CONVERT_CMD);
    this->putPacketToQueue(eOwBusWrite, packet);

    /* Init Reset/Presence procedure */
    this->sendOwCmd(eOwBusReset);

    /* Start measure inyerval timer */
    owPollingEvent = startTimer(owPollingTime);
}

/**
 * @brief MainWindow::owDataRead
 * @param read_data_len
 */
void MainWindow::owDataRead(quint16 read_data_len)
{
    QByteArray packet;

    /* Place MATCHROM packet into packet queue */
    packet.append(OW_MATCHROM_CMD);
    this->putPacketToQueue(eOwBusWrite, packet);

    /* Place ADDRESS packet into packet queue */
    packet.clear();
    packet.append(intToByteArray(selDevices.key(owDevIndex)));
    this->putPacketToQueue(eOwBusWrite, packet);

    /* Place READ_CMD packet into packet queue */
    packet.clear();
    packet.append(OW_READ_CMD);
    this->putPacketToQueue(eOwBusWrite, packet);

    /* Place read_data_len packet into packet queue */
    packet.clear();
    packet.append(intToByteArray(read_data_len));
    this->putPacketToQueue(eOwBusRead, packet);

    /* Init Reset/Presence procedure */
    this->sendOwCmd(eOwBusReset);
}

/**
 * @brief MainWindow::sendOwCmd
 */
void MainWindow::sendOwCmd(TOpcode opcode)
{
    QByteArray packet;
    packet.append((quint8)opcode);
    packet.append((quint8)0);
    emit sendToServer(packet);
}

/**
 * @brief MainWindow::putPacketToQueue
 */
void MainWindow::putPacketToQueue(TOpcode opcode, const QByteArray &msg)
{
    QByteArray txData;

    txData.append((quint8)opcode);

    if (msg == nullptr) {
        txData.append((quint8)0);
    }
    else {
        txData.append((quint8)msg.size());
        txData.append(msg);
    }

    owPacketQueue << txData;
}

/**
 * @brief MainWindow::onShowResponse
 * @param response
 */
void MainWindow::onTcpResponse(const QByteArray &response)
{
    TAppLayerPacket *rx_packet = (TAppLayerPacket*)response.data();
    quint64 dev_addr = 0;
    float ftemper = 0.0f;
    int dev_cnt = 0;
    quint16 utemper = 0, sign = 0;
    quint8 dev_family = 0;

    switch (rx_packet->opcode)
    {
    case eOwBusReset:
        break;

    case eOwSearchRom:
        dev_addr = *((quint64*)rx_packet->data);
        if (dev_addr != 0) {
            owDeviceAddressList.append(dev_addr);
            this->sendOwCmd(eOwSearchRom);
        }
        else {
            isOwSearchDone = true;
            this->initDeviceComboBox();
            dev_cnt = owDeviceAddressList.size();
            if (dev_cnt > 0) {
                ui->startPollingButton->setEnabled(true);
                ui->infoPushButton->setEnabled(true);
            }
            else {
                ui->startPollingButton->setEnabled(false);
                ui->infoPushButton->setEnabled(false);
            }

            statusBar()->showMessage(tr("Total 1-Wire devices found: ") +
                                     QString::number(dev_cnt));
        }
        break;

    case eOwBusRead:
        dev_family = OneWire::getFamily(ui->deviceComboBox->currentText());
        ui->textEdit->append("Packet " + QString::number(++rxCounter));
        ui->textEdit->append("Device " + QString::number(owDevIndex + 1));
        ui->textEdit->append("Address: " +
                             QString::number(selDevices.key(owDevIndex), 16).toUpper());

        switch (dev_family) {
        case 0x28: // DS18B20
            if (rx_packet->data[8] != OneWire::calcCrc8(rx_packet->data, 8)) {
                statusBar()->showMessage(tr("!!! CRC8 Error !!!"));
            }
            else {
                utemper = (rx_packet->data[1] << 8) | rx_packet->data[0];
                sign = utemper & DS18B20_SIGN_MASK;
                if (sign != 0) utemper = (0xFFFF - utemper + 1);
                ftemper = (float)utemper * 0.0625f;
                ui->textEdit->append("Temperature: " + QString::number(ftemper, 'f', 1) + " °C");
                ui->textEdit->append("Alarm High: " + QString::number((qint8)rx_packet->data[2]));
                ui->textEdit->append("Alarm Low: " + QString::number((qint8)rx_packet->data[3]));
                ui->textEdit->append("Resolution code: " + QString::number((qint8)rx_packet->data[4]));
            }
            break;
        default: // other device
            ui->textEdit->append(OneWire::getDescription(dev_family));
            break;
        }

        ui->textEdit->append("");

        if (owDevIndex < (owSelDeviceCount - 1))
            owDevIndex++;
        else
            owDevIndex = 0;

        if (isOwPollRunning)
            this->startPolling();
        break;

    case eOwBusWrite:
        break;

    default:
        return;
    }

    if (!owPacketQueue.isEmpty()) {
        emit sendToServer(owPacketQueue.takeFirst());
    }
}

/**
 * @brief MainWindow::onShowTcpError
 * @param response
 */
void MainWindow::onShowTcpError(const QByteArray &err_msg)
{
    statusBar()->showMessage(err_msg);
}
