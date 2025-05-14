#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QTimer>

#include "config_recorder/uiconfigrecorder.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

enum CollectState
{
    ST_IDLE,
    ST_COLLECTING,
};

typedef enum
{
    RAND_DATA_BYTES,
    DATA_FROM_TXT_FILE,
    DATA_FROM_BIN_FILE,
}data_source_type_e_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    UiConfigRecorder m_cfg_recorder;
    qobj_ptr_set_t m_cfg_filter_in, m_cfg_filter_out;

    // UDP socket
    QUdpSocket udpSocket;

    // Counter for appending to data
    qint64 counter;

    CollectState collectingState = ST_IDLE;

    QHostAddress m_rmt_addr;
    quint16 m_rmt_port;
    QByteArray m_start_req, m_start_ack, m_stop_req, m_stop_ack;

    QByteArray m_curr_row_data;

    int m_send_int_ms;
    QTimer m_send_timer;

    data_source_type_e_t m_data_source_type;

    // Helper methods
    bool validateInputs();
    QByteArray generateRandomData(int byteCount);
    QString byteArrayToHexString(const QByteArray &data);
    void setupUI();

    QString log_disp_prepender_str();

    void send_one_row();

private slots:
    void data_ready_hdlr();
    void on_sendBtn_clicked();
    void on_randDataRBtn_toggled(bool checked);
    void on_fileDataRBtn_toggled(bool checked);
    void on_infinDataCheckBox_clicked(bool checked);
    void send_int_timer_hdlr();
    void on_resetBtn_clicked();
};
#endif // MAINWINDOW_H
