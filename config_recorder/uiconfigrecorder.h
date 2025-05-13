#ifndef UICONFIGRECORDER_H
#define UICONFIGRECORDER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QSet>

typedef QSet<QObject *> qobj_ptr_set_t;
class UiConfigRecorder : public QObject
{
    Q_OBJECT
private:
    static constexpr const char * const m_def_cfg_file_fpn = ".user_settings.ini";
    QString m_cfg_file_fpn;

public:
    explicit UiConfigRecorder(QObject *parent = nullptr,
                              QString cfg_file_fpn = m_def_cfg_file_fpn);

    void record_ui_configs(QWidget * ui_widget,
                           const qobj_ptr_set_t &filter_in = qobj_ptr_set_t(),
                           const qobj_ptr_set_t &filter_out = qobj_ptr_set_t(),
                           QString sec_pre_str = "", QString sec_post_str = "",
                           QString key_pre_str = "", QString key_post_str = "",
                           QSettings::Format cfg_format = QSettings::IniFormat);
    void load_configs_to_ui(QWidget * ui_widget,
                            const qobj_ptr_set_t &filter_in = qobj_ptr_set_t(),
                            const qobj_ptr_set_t &filter_out = qobj_ptr_set_t(),
                            QString sec_pre_str = "", QString sec_post_str = "",
                            QString key_pre_str = "", QString key_post_str = "",
                            QSettings::Format cfg_format = QSettings::IniFormat);

signals:

};

#endif // UICONFIGRECORDER_H
