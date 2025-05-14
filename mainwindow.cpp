#include <QMessageBox>
#include <QRandomGenerator>
#include <QNetworkDatagram>
#include <QButtonGroup>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "common_tools/common_tool_func.h"
#include "logger/logger.h"
#include "config_recorder/uiconfigrecorder.h"
#include "sysconfigs/sysconfigs.h"

const char* g_str_row_int = "行发送间隔时间";
const char* g_str_unit_ms = "ms";

#define RECV_DATA_NOTE_E(e) e

#define RECV_DATA_NOTES \
    RECV_DATA_NOTE_E(NORMAL), \
    RECV_DATA_NOTE_E(START_ACK), \
    RECV_DATA_NOTE_E(STOP_ACK), \
    RECV_DATA_NOTE_E(UNEXPECTED_IN_START_WAIT), \
    RECV_DATA_NOTE_E(UNEXPECTED_IN_STOP_WAIT), \
    RECV_DATA_NOTE_E(IRRELEVANT_ADDR), \
    RECV_DATA_NOTE_E(RECV_IN_IDLE),

typedef enum
{
    RECV_DATA_NOTES
}recv_data_notes_e_t;

typedef struct
{
    recv_data_notes_e_t  notes;
    QByteArray data;
}recv_data_with_notes_s_t;

static const char gs_start_req_cmd[] = {'\x00', '\x00', '\x00', '\x02'};
static const char gs_start_ack_cmd[] = {'\x00', '\x00', '\x00', '\x02'};
static const char gs_stop_req_cmd[] = {'\x00', '\x00', '\x00', '\x03'};
static const char gs_stop_ack_cmd[] = {'\x00', '\x00', '\x00', '\x03'};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow), m_cfg_recorder(this),
      collectingState(ST_IDLE)
{
    QString ret_str;
    bool ret;

    ui->setupUi(this);

    m_start_req = QByteArray::fromRawData(gs_start_req_cmd, sizeof(gs_start_req_cmd));
    m_start_ack = QByteArray::fromRawData(gs_start_ack_cmd, sizeof(gs_start_ack_cmd));
    m_stop_req = QByteArray::fromRawData(gs_stop_req_cmd, sizeof(gs_stop_req_cmd));
    m_stop_ack = QByteArray::fromRawData(gs_stop_ack_cmd, sizeof(gs_stop_ack_cmd));

    m_cfg_filter_in.clear();
    m_cfg_filter_out << ui->rmtIPLEdit << ui->rmtPortLEdit << ui->fileDescLEdit << ui->infoDispEdit;

    ret = fill_sys_configs(&ret_str);
    if(!ret)
    {
        QMessageBox::critical(this, "", ret_str);
        return;
    }

    /**/
    m_row_idx = g_sys_configs_block.start_row_idx;
    m_counter = 0;

    ui->ptPerRowSpinBox->setRange(1, g_sys_configs_block.max_pt_number);
    ui->rowIntSpinBox->setRange(g_sys_configs_block.min_row_interval_ms,
                                g_sys_configs_block.max_row_interval_ms);
    /*set ui default cfg-----------------------------------------------------------*/
    QButtonGroup *d_src_rbtn_grp = new QButtonGroup(this);
    d_src_rbtn_grp->addButton(ui->randDataRBtn);
    d_src_rbtn_grp->addButton(ui->fileDataRBtn);
    ui->fileDataRBtn->setChecked(true);
    ui->randDataRBtn->setChecked(true);

    ui->infinDataCheckBox->setChecked(false);

    /*load saved ui cfg-----------------------------------------------------------*/
    m_cfg_recorder.load_configs_to_ui(this, m_cfg_filter_in, m_cfg_filter_out);

    /*-----------------------------------------------------------*/
    if (!udpSocket.bind(QHostAddress::AnyIPv4, g_sys_configs_block.local_udp_port))
    {
        QMessageBox::critical(this, "", QString("bind error"));
        return;
    }
    connect(&udpSocket, &QUdpSocket::readyRead, this, &MainWindow::data_ready_hdlr);
    connect(&m_send_timer, &QTimer::timeout, this, &MainWindow::send_int_timer_hdlr);
}

MainWindow::~MainWindow()
{
    m_cfg_recorder.record_ui_configs(this, m_cfg_filter_in, m_cfg_filter_out);
    m_cfg_filter_in.clear();
    m_cfg_filter_out.clear();

    m_send_timer.stop();

    delete ui;
}

void MainWindow::send_one_row()
{
    int byteCount = g_sys_configs_block.all_bytes_per_pt * ui->ptPerRowSpinBox->value();

    switch(m_data_source_type)
    {
    case RAND_DATA_BYTES:
    default:
        QByteArray data = generateRandomData(byteCount);

        // Append 2-byte counter (little-endian)
        data.append(static_cast<char>(m_row_idx & 0xFF));
        data.append(static_cast<char>((m_row_idx >> 8) & 0xFF));

        m_row_idx++;
        m_counter++;

        // Send data via UDP
        qint64 bytesSent = udpSocket.writeDatagram(data, m_rmt_addr, m_rmt_port);
        QString info_str = byteArrayToHexString(data);
        ui->infoDispEdit->append(log_disp_prepender_str() + "sent:" + info_str);
        if (bytesSent == -1)
        {
            info_str = QString("ERROR!!! Failed to send data: ") + udpSocket.errorString();
            ui->infoDispEdit->append(info_str);
        }

        break;
    }
}

void MainWindow::on_sendBtn_clicked()
{
    if (!validateInputs())
    {
        return;
    }

    m_rmt_addr = QHostAddress((ui->rmtIPLEdit->text()));
    m_rmt_port = ui->rmtPortLEdit->text().toInt();

    m_data_source_type = RAND_DATA_BYTES;
    send_one_row();

    if(m_counter < ui->rowsToSendSpinBox->value())
    {
        m_send_timer.start(ui->rowIntSpinBox->value());
    }
}

void MainWindow::send_int_timer_hdlr()
{
    send_one_row();

    if((RAND_DATA_BYTES == m_data_source_type)
        && !ui->infinDataCheckBox->isChecked() && m_counter >= ui->rowsToSendSpinBox->value())
    {
        m_send_timer.stop();
        m_counter = 0;
    }
}

bool MainWindow::validateInputs()
{
    bool tr_ret;
    // Validate IP address
    if (ui->rmtIPLEdit->text().isEmpty())
    {
        QMessageBox::warning(this, "Validation Error", "Please enter a valid IP address.");
        return false;
    }
    int port = ui->rmtPortLEdit->text().toInt(&tr_ret);
    if(!tr_ret || port > 65535 || port <= 0)
    {
        QMessageBox::warning(this, "Validation Error", "invalid rmt port");
        return false;
    }

    return true;
}

QByteArray MainWindow::generateRandomData(int byteCount)
{
    QByteArray data;
    data.resize(byteCount);

    // Fill the byte array with random values
    QRandomGenerator *generator = QRandomGenerator::global();
    for (int i = 0; i < byteCount; i++) {
        data[i] = static_cast<char>(generator->bounded(256)); // 0-255
    }

    return data;
}

QString MainWindow::byteArrayToHexString(const QByteArray &data)
{
    QString result;

    for (int i = 0; i < data.size(); i++) {
        // Format each byte as a 2-digit hex value
        result += QString("%1 ").arg(static_cast<quint8>(data.at(i)), 2, 16, QChar('0')).toUpper();
    }

    return result.trimmed();
}

void MainWindow::data_ready_hdlr()
{
    QString log_str;

    while (udpSocket.hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket.receiveDatagram();

        recv_data_with_notes_s_t data_with_notes = {NORMAL, datagram.data()};
        QByteArray &data = data_with_notes.data;

        switch(collectingState)
        {
        case ST_IDLE:
            if (data == m_start_req)
            {
                m_rmt_addr = datagram.senderAddress();
                m_rmt_port = datagram.senderPort();

                ui->rmtIPLEdit->setText(m_rmt_addr.toString());
                ui->rmtPortLEdit->setText(QString::number(m_rmt_port));

                udpSocket.writeDatagram(m_start_ack, m_rmt_addr, m_rmt_port);
                collectingState = ST_COLLECTING;

                log_str = "receive start cmd. acked. enter ST_COLLECTING";
            }
            else
            {
                log_str = "receive unkonwn data in ST_IDLE\n";
                log_str += data.toHex() + "\n";
            }
            break;

        case ST_COLLECTING:
        default:
            if (data == m_stop_req)
            {
                udpSocket.writeDatagram(m_stop_ack, m_rmt_addr, m_rmt_port);
                collectingState = ST_IDLE;

                log_str = "receive stop cmd. acked. enter ST_IDLE";
            }
            else
            {
                log_str = "receive unexpected data in ST_COLLECTIN\n";
                log_str += data.toHex() + "\n";
            }
            log_str += data.toHex() + "\n";
            break;
        }
        ui->infoDispEdit->append(log_disp_prepender_str() + log_str);
    }
}

QString MainWindow::log_disp_prepender_str()
{
    return (common_tool_get_curr_date_str() + "," + common_tool_get_curr_time_str() + ",");
}

void MainWindow::on_randDataRBtn_toggled(bool checked)
{
    ui->ptPerRowSpinBox->setEnabled(checked);
    ui->infinDataCheckBox->setEnabled(checked);

    if(checked) ui->rowsToSendSpinBox->setEnabled(!(ui->infinDataCheckBox->isChecked()));
}

void MainWindow::on_fileDataRBtn_toggled(bool checked)
{
    ui->fileFpnLEdit->setEnabled(checked);
    ui->selFileBtn->setEnabled(checked);
    ui->fileDescLEdit->setEnabled(checked);
}


void MainWindow::on_infinDataCheckBox_clicked(bool checked)
{
    ui->rowsToSendSpinBox->setEnabled(!checked);
}


void MainWindow::on_resetBtn_clicked()
{
    m_send_timer.stop();

    m_row_idx = g_sys_configs_block.start_row_idx;
    m_counter = 0;
    collectingState = ST_IDLE;
}

