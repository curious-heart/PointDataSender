#include <QMessageBox>
#include <QRandomGenerator>
#include <QNetworkDatagram>
#include <QButtonGroup>
#include <QFileDialog>
#include <QTextStream>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "common_tools/common_tool_func.h"
#include "logger/logger.h"
#include "config_recorder/uiconfigrecorder.h"
#include "sysconfigs/sysconfigs.h"

const char* g_str_row_int = "行发送间隔时间";
const char* g_str_unit_ms = "ms";
const char* g_str_select_file = "选择文件";
const char* g_str_unsupported_file_type = "不支持此类文件";
const char* g_str_invalid_data_src = "无效的数据源";
const char* g_str_img_file_number_should_be = "图像文件数量必须为";
const char* g_str_plz_select_txt_file = "请选择文本格式的数据文件";
const char* g_str_plz_select_img_file = "请选择图像格式的数据文件";
const char* g_str_unit_ge = "个";
const char* g_str_open_file_failes = "打开文件失败";
const char* g_str_the_order = "第";
const char* g_str_row = "行";
const char* g_str_point = "点";
const char* g_str_contains_invalid_char = "包含非法字符";
const char* g_str_byte_cnts_of_rows_should_be_iden = "每行数据字节数应该相等";
const char* g_str_hex_digit_num_not_fill_pts = "HEX字符数为不满足整数点数需求";
const char* g_str_auto_append = "自动补充";
const char* g_str_every_mei = "每";
const char* g_str_imgs_should_be_of_same_size = "图像的宽高应该相等";

const QStringList g_slist_img_file_ext = {"png", "bmp", "jpg"};
const QStringList g_slist_txt_file_ext = {"txt"};
const QRegExp g_row_seperator("\\s"), g_non_hex_char("[^0-9a-fA-F]");

const char g_append_data_digit = 'F';

const char* g_str_fpn_sep = ";";
static int g_data_channel_number = 2;
static int g_byte_per_point = 3;

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
    ui->setupUi(this);

    m_start_req = QByteArray::fromRawData(gs_start_req_cmd, sizeof(gs_start_req_cmd));
    m_start_ack = QByteArray::fromRawData(gs_start_ack_cmd, sizeof(gs_start_ack_cmd));
    m_stop_req = QByteArray::fromRawData(gs_stop_req_cmd, sizeof(gs_stop_req_cmd));
    m_stop_ack = QByteArray::fromRawData(gs_stop_ack_cmd, sizeof(gs_stop_ack_cmd));

    m_cfg_filter_in.clear();
    m_cfg_filter_out << ui->rmtIPLEdit << ui->rmtPortLEdit << ui->rmtPortLEdit
                     << ui->infoDispEdit;

    /**/
    m_row_idx = g_sys_configs_block.start_row_idx;
    m_counter = 0; m_sent_succ_counter = 0;

    ui->ptPerRowSpinBox->setRange(1, g_sys_configs_block.max_pt_number);
    ui->rowIntSpinBox->setRange(g_sys_configs_block.min_row_interval_ms,
                                g_sys_configs_block.max_row_interval_ms);
    /*set ui default cfg-----------------------------------------------------------*/
    QButtonGroup *d_src_rbtn_grp = new QButtonGroup(this);
    d_src_rbtn_grp->addButton(ui->randDataRBtn);
    d_src_rbtn_grp->addButton(ui->fileDataRBtn);
    ui->fileDataRBtn->setChecked(true);
    ui->randDataRBtn->setChecked(true);

    QButtonGroup *d_file_type_rbtn_grp = new QButtonGroup(this);
    d_file_type_rbtn_grp->addButton(ui->txtFileTypeRBtn);
    d_file_type_rbtn_grp->addButton(ui->imgFileTypeRBtn);
    ui->txtFileTypeRBtn->setChecked(true);

    ui->infinDataCheckBox->setChecked(false);
    ui->stopSendBtn->setVisible(false);

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

    if(ui->randDataRBtn->isChecked())
    {
        m_data_source_type = RAND_DATA_BYTES;
    }
    else
    {
        m_data_source_type = ui->txtFileTypeRBtn->isChecked() ?
                             DATA_FROM_TXT_FILE : DATA_FROM_IMG_FILE;
    }
    m_data_fpn_list = ui->fileDescLEdit->text().split(g_str_fpn_sep);

    if(DATA_FROM_TXT_FILE == m_data_source_type || DATA_FROM_IMG_FILE == m_data_source_type)
    {
        display_data_size();
    }

    ui->stopSendBtn->setVisible(ui->randDataRBtn->isChecked()
                                && ui->infinDataCheckBox->isChecked());

    ui->sentCountLbl->setText(QString("%1/%2").arg(m_sent_succ_counter).arg(m_counter));

    m_init_ok = true;
}

MainWindow::~MainWindow()
{
    m_cfg_recorder.record_ui_configs(this, m_cfg_filter_in, m_cfg_filter_out);
    m_cfg_filter_in.clear();
    m_cfg_filter_out.clear();

    m_send_timer.stop();

    delete ui;
}

void MainWindow::display_data_size()
{
    int row_cnt = 0, pt_per_row = 0;
    if(DATA_FROM_TXT_FILE == m_data_source_type)
    {
        gen_data_from_txt_file(&row_cnt, &pt_per_row, true);
    }
    else
    {
        gen_data_from_img_file(&row_cnt, &pt_per_row, true);
    }
    ui->byteAndRowNumLbl->setText(QString("%1%2 %3 %4; %5 %6").arg(g_str_every_mei, g_str_row)
                                  .arg(pt_per_row).arg(g_str_point)
                                  .arg(row_cnt).arg(g_str_row));
}

bool MainWindow::is_send_finished()
{
    bool finished = false;

    switch(m_data_source_type)
    {
    case DATA_FROM_TXT_FILE:
    case DATA_FROM_IMG_FILE:
        finished = (m_counter >= m_data_rows_from_file.count());
        break;

    default: //RAND_DATA_BYTES
        finished = (!ui->infinDataCheckBox->isChecked()
                        && m_counter >= ui->rowsToSendSpinBox->value());
        break;
    }
    return finished;
}

void MainWindow::send_one_row()
{
    QByteArray a_row;

    if(RAND_DATA_BYTES == m_data_source_type)
    {
        int byteCount = g_sys_configs_block.all_bytes_per_pt * ui->ptPerRowSpinBox->value();
        a_row = generateRandomData(byteCount);
    }

    QByteArray &data = (DATA_FROM_TXT_FILE == m_data_source_type || DATA_FROM_IMG_FILE == m_data_source_type) ?
                m_data_rows_from_file[m_counter] : a_row;

    // Append 2-byte counter
    if(LOW_BYTE_FIRST == g_sys_configs_block.row_idx_byte_order)
    {
        data.append(static_cast<char>(m_row_idx & 0xFF));
        data.append(static_cast<char>((m_row_idx >> 8) & 0xFF));
    }
    else
    {
        data.append(static_cast<char>((m_row_idx >> 8) & 0xFF));
        data.append(static_cast<char>(m_row_idx & 0xFF));
    }

    m_row_idx++;
    m_counter++;

    // Send data via UDP
    qint64 bytesSent = udpSocket.writeDatagram(data, m_rmt_addr, m_rmt_port);
    QString info_str = data.toHex(' ').toUpper();
    ui->infoDispEdit->append(log_disp_prepender_str() + "sent:" + info_str);
    if (bytesSent == -1)
    {
        info_str = QString("ERROR!!! Failed to send data: ") + udpSocket.errorString();
        ui->infoDispEdit->append(info_str);
    }
    else
    {
        m_sent_succ_counter++;
        ui->sentCountLbl->setText(QString("%1/%2").arg(m_sent_succ_counter).arg(m_counter));
    }
}

void MainWindow::on_sendBtn_clicked()
{
    stop_data_send(false);

    ui->sentCountLbl->setText(QString("%1/%2").arg(m_sent_succ_counter).arg(m_counter));

    if (!validateInputs())
    {
        return;
    }

    m_rmt_addr = QHostAddress((ui->rmtIPLEdit->text()));
    m_rmt_port = ui->rmtPortLEdit->text().toInt();

    if(DATA_SRC_INVALID == m_data_source_type)
    {
        QMessageBox::critical(this, "Error", g_str_invalid_data_src);
        return;
    }

    bool data_gened = true;
    switch(m_data_source_type)
    {
    case DATA_FROM_TXT_FILE:
        data_gened = gen_data_from_txt_file();
        break;

    case DATA_FROM_IMG_FILE:
        data_gened = gen_data_from_img_file();
        break;

    default: //RAND_DATA_BYTES
        break;
    }

    if(data_gened)
    {
        send_one_row();
        if(!is_send_finished())
        {
            m_send_timer.start(ui->rowIntSpinBox->value());
        }
    }
}

void MainWindow::send_int_timer_hdlr()
{
    send_one_row();

    if(is_send_finished())
    {
        m_send_timer.stop();
    }
}

void MainWindow::stop_data_send(bool reset_st)
{
    m_send_timer.stop();
    m_counter = 0; m_sent_succ_counter = 0;
    m_row_idx = g_sys_configs_block.start_row_idx;

    if(reset_st) collectingState = ST_IDLE;
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

bool MainWindow::gen_data_from_txt_file(int *row_num, int *pt_per_row, bool only_get_row_and_byte_per_row)
{
    if(m_data_fpn_list.count() <= 0)
    {
        QMessageBox::critical(this, "", g_str_plz_select_txt_file);
        return false;
    }
    QString fpn = m_data_fpn_list[0];
    QFile txt_file(fpn);
    if (!txt_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "", fpn + "\n" + QString(g_str_open_file_failes) + ":"
                              + txt_file.errorString());
        return false;
    }
    m_data_rows_from_file.clear();
    QTextStream txt_stream(&txt_file);
    QString line;
    int row_cnt = 0, hf_byte_per_row, padded_hex_digit_cnt;
    static int ls_lcm_of_2_and_byte_per_pt = lcm(2, g_byte_per_point);
    bool ret = true;
    bool the_1st_row = true;;
    while (txt_stream.readLineInto(&line))
    {
        line.remove(g_row_seperator);
        if(line.contains(g_non_hex_char))
        {
            QMessageBox::critical(this, "", QString("%1:%2%3%4%5").arg(fpn, g_str_the_order).
                                  arg(row_cnt).arg(g_str_row, g_str_contains_invalid_char));
            ret = false; break;
        }
        if(line.isEmpty()) continue;

        if(the_1st_row)
        {
            hf_byte_per_row = line.length();
        }

        if(line.length() != hf_byte_per_row)
        {
            QMessageBox::critical(this, "", fpn + "\n" + g_str_byte_cnts_of_rows_should_be_iden);
            ret = false; break;
        }

        padded_hex_digit_cnt = hf_byte_per_row %
                               (ls_lcm_of_2_and_byte_per_pt ? ls_lcm_of_2_and_byte_per_pt : 2);
        if(padded_hex_digit_cnt != 0)
        {
            padded_hex_digit_cnt = ls_lcm_of_2_and_byte_per_pt - padded_hex_digit_cnt;
            line += QString(padded_hex_digit_cnt, g_append_data_digit);
            DIY_LOG(LOG_WARN, QString("%1, %2 %3 %4 %5").arg(g_str_hex_digit_num_not_fill_pts, g_str_auto_append)
                                                        .arg(padded_hex_digit_cnt).arg(g_str_unit_ge)
                                                        .arg(g_append_data_digit));
        }

        if(the_1st_row)
        {
            if(pt_per_row) *pt_per_row =  line.length() / g_byte_per_point;
            the_1st_row = false;
        }

        if(!only_get_row_and_byte_per_row)
        {
            m_data_rows_from_file.append(QByteArray::fromHex(line.toUtf8()));
        }
        ++row_cnt;
    }

    txt_file.close();
    if(row_num) *row_num = row_cnt;

    return ret;
}

bool MainWindow::gen_data_from_img_file(int *row_num, int *pt_per_row, bool only_get_row_and_byte_per_row)
{
    QString file_sel_hint_str = QString("%1(%2%3)").arg(g_str_plz_select_img_file)
                              .arg(g_data_channel_number).arg(g_str_unit_ge);
    if(m_data_fpn_list.count() < g_data_channel_number)
    {
        QMessageBox::critical(this, "", file_sel_hint_str);
        return false;
    }
    QImage img1(m_data_fpn_list[0]), img2(m_data_fpn_list[1]);
    if(img1.isNull() || img2.isNull())
    {
        QString err_str;
        if(img1.isNull()) err_str += m_data_fpn_list[0] + "\n";
        if(img2.isNull()) err_str += m_data_fpn_list[1] + "\n";
        err_str += g_str_open_file_failes;
        QMessageBox::critical(this, "", err_str);

        return false;
    }

    if((img1.width() != img2.width()) || (img1.height() != img2.height()))
    {
        QMessageBox::critical(this, "", g_str_imgs_should_be_of_same_size);
        return false;
    }
    m_data_rows_from_file.clear();

    if(only_get_row_and_byte_per_row)
    {
        if(row_num) *row_num = img1.height();
        if(pt_per_row) *pt_per_row = img1.width();
        return true;
    }

    img1 = img1.convertToFormat(QImage::Format_Grayscale8, Qt::MonoOnly);
    img2 = img2.convertToFormat(QImage::Format_Grayscale8, Qt::MonoOnly);

    for(int y = 0; y < img1.height(); ++y)
    {
        QByteArray data_row;
        const quint8* img1_row = img1.constScanLine(y);
        const quint8* img2_row = img2.constScanLine(y);
        data_row.clear();
        for(int x = 0; x < img1.width(); ++x)
        {
            quint16 pt1, pt2;
            quint8 byte;
            pt1 = (img1_row[x] << 4) | (img1_row[x] >> 4); //8bit -> 12 bit;
            pt2 = (img2_row[x] << 4) | (img2_row[x] >> 4); //8bit -> 12 bit;

            byte = (pt1 & 0x0FF0) >> 4;
            data_row.append(byte);

            byte = ((pt1 & 0x000F) << 4) | ((pt2 & 0x0F00) >> 8);
            data_row.append(byte);

            byte = (pt2 & 0x00FF);
            data_row.append(byte);
        }
        m_data_rows_from_file.append(data_row);
    }

    return true;
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

        m_rmt_addr = datagram.senderAddress();
        m_rmt_port = datagram.senderPort();
        ui->rmtIPLEdit->setText(m_rmt_addr.toString());
        ui->rmtPortLEdit->setText(QString::number(m_rmt_port));
        ui->localPortLEdit->setText(QString::number(udpSocket.localPort()));

        if (data == m_start_req)
        {
            udpSocket.writeDatagram(m_start_ack, m_rmt_addr, m_rmt_port);
        }
        else if(data == m_stop_req)
        {
            udpSocket.writeDatagram(m_stop_ack, m_rmt_addr, m_rmt_port);
        }

        switch(collectingState)
        {
        case ST_IDLE:
            if (data == m_start_req)
            {
                collectingState = ST_COLLECTING;
                log_str = "receive start cmd. acked. enter ST_COLLECTING";
            }
            else
            {
                log_str = "receive unkonwn data in ST_IDLE\n";
                log_str += data.toHex(' ').toUpper() + "\n";
            }
            break;

        case ST_COLLECTING:
        default:
            if (data == m_stop_req)
            {
                collectingState = ST_IDLE;
                stop_data_send();
                log_str = "receive stop cmd. acked. enter ST_IDLE\n";
            }
            else
            {
                log_str = "receive unexpected data in ST_COLLECTIN\n";
                log_str += data.toHex(' ').toUpper() + "\n";
            }
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
    ui->rowsToSendSpinBox->setEnabled(checked);
    ui->infinDataCheckBox->setEnabled(checked);

    if(checked)
    {
        ui->rowsToSendSpinBox->setEnabled(!(ui->infinDataCheckBox->isChecked()));
        m_data_source_type = RAND_DATA_BYTES;

        ui->stopSendBtn->setVisible(ui->infinDataCheckBox->isChecked());
    }
}

void MainWindow::on_fileDataRBtn_toggled(bool checked)
{
    ui->selFileBtn->setEnabled(checked);
    ui->fileDescLEdit->setEnabled(checked);
    ui->txtFileTypeRBtn->setEnabled(checked);
    ui->imgFileTypeRBtn->setEnabled(checked);

    if(checked)
    {
        m_data_source_type = ui->txtFileTypeRBtn->isChecked() ?
                             DATA_FROM_TXT_FILE : DATA_FROM_IMG_FILE;

        ui->stopSendBtn->setVisible(false);
    }
}


void MainWindow::on_infinDataCheckBox_clicked(bool checked)
{
    ui->rowsToSendSpinBox->setEnabled(!checked);
}


void MainWindow::on_resetBtn_clicked()
{
    stop_data_send();
}


void MainWindow::on_clearDispBtn_clicked()
{
    ui->infoDispEdit->clear();
}

void MainWindow::on_selFileBtn_clicked()
{
    static QString ls_txt_f_filter_str = QString("Text files (*.") + g_slist_txt_file_ext.join(" *.") + ")";
    static QString ls_img_f_filter_str = QString("Images (*.") + g_slist_img_file_ext.join(" *.") + ")";

    QString ls_f_filter_str = (DATA_FROM_TXT_FILE == m_data_source_type) ?
                                ls_txt_f_filter_str : ls_img_f_filter_str;
    QString def_dir_s = ui->fileDescLEdit->text();
    QStringList dirList = def_dir_s.split(g_str_fpn_sep, Qt::SkipEmptyParts);
    def_dir_s = dirList.isEmpty() ? "" : dirList[0];
    def_dir_s = QFileInfo(def_dir_s).absolutePath();
    if(def_dir_s.isEmpty()) def_dir_s = ".";

    if(DATA_FROM_TXT_FILE == m_data_source_type)
    {
        QString file_fpn_str = QFileDialog::getOpenFileName(this, g_str_select_file, def_dir_s, ls_f_filter_str);
        if(file_fpn_str.isEmpty()) return;

        m_data_fpn_list.clear();
        m_data_fpn_list.append(file_fpn_str);
    }
    else
    {
        QStringList file_fpn_strs;
        do
        {
            file_fpn_strs = QFileDialog::getOpenFileNames(this, g_str_select_file, def_dir_s, ls_f_filter_str);
            if(file_fpn_strs.isEmpty()) return;
            if(file_fpn_strs.count() == g_data_channel_number) break;

            QMessageBox::critical(this, "Error",
                                  QString("%1 %2").arg(g_str_img_file_number_should_be).arg(g_data_channel_number));
        }while(true);
        m_data_fpn_list.clear();
        m_data_fpn_list = file_fpn_strs;
    }

    if(!m_data_fpn_list.isEmpty())
    {
        ui->fileDescLEdit->setText(m_data_fpn_list.join(g_str_fpn_sep));
        m_cfg_recorder.record_ui_configs(this, m_cfg_filter_in, m_cfg_filter_out);

        display_data_size();
    }
}

void MainWindow::on_txtFileTypeRBtn_toggled(bool checked)
{
    if(checked) m_data_source_type = DATA_FROM_TXT_FILE;
}

void MainWindow::on_imgFileTypeRBtn_toggled(bool checked)
{
    if(checked) m_data_source_type = DATA_FROM_IMG_FILE;
}


void MainWindow::on_infinDataCheckBox_toggled(bool checked)
{
    ui->stopSendBtn->setVisible(checked);
}


void MainWindow::on_stopSendBtn_clicked()
{
     stop_data_send();
}

