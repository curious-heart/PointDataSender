#include <QSettings>
#include <QList>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>

#include "uiconfigrecorder.h"
#include "logger/logger.h"

UiConfigRecorder::UiConfigRecorder(QObject *parent, QString cfg_file_fpn)
    : QObject{parent}, m_cfg_file_fpn(cfg_file_fpn)
{
    if(cfg_file_fpn.isEmpty())
    {
        DIY_LOG(LOG_WARN, "the name of config file for recording ui configrations is empty.\n");
        return;
    }
}

#define LIST_VAR_NAME(ctrl) ctrl##_ptr_list
#define DEF_CTRL(ctrl)  QList<ctrl*> LIST_VAR_NAME(ctrl);

#define CTRL_PTR_VAR_DEF \
    DEF_CTRL(QLineEdit)\
    DEF_CTRL(QTextEdit)\
    DEF_CTRL(QComboBox)\
    DEF_CTRL(QRadioButton)\
    DEF_CTRL(QCheckBox)\
    DEF_CTRL(QSpinBox)
#define BOX_CHECKED 1
#define BOX_UNCHECKED 0

#define CTRL_WRITE_TO_CFG(ctrl_type, value) \
    LIST_VAR_NAME(ctrl_type) = ui_widget->findChildren<ctrl_type*>();\
    do_it = true;\
    for(idx = 0; idx < LIST_VAR_NAME(ctrl_type).size(); ++idx)        \
    {                                                                 \
        if(check_in)\
            do_it = filter_in.contains(LIST_VAR_NAME(ctrl_type)[idx]);\
        else if(check_out)\
            do_it = !filter_out.contains(LIST_VAR_NAME(ctrl_type)[idx]);\
        if(!do_it) continue;\
                            \
        cfg_setting.setValue(key_pre_str \
                             + LIST_VAR_NAME(ctrl_type)[idx]->objectName() \
                             + key_post_str, \
                             LIST_VAR_NAME(ctrl_type)[idx]->value);  \
    }

void UiConfigRecorder::record_ui_configs(QWidget * ui_widget,
                             const qobj_ptr_set_t &filter_in, const qobj_ptr_set_t &filter_out,
                             QString sec_pre_str, QString sec_post_str,
                             QString key_pre_str, QString key_post_str,
                             QSettings::Format cfg_format)
{
    CTRL_PTR_VAR_DEF;
    int idx;
    QSettings cfg_setting(m_cfg_file_fpn, cfg_format);
    bool check_in = !filter_in.isEmpty(), check_out = !filter_out.isEmpty();
    bool do_it;

    if(!ui_widget)
    {
        DIY_LOG(LOG_ERROR, "ui_widget is NULL.\n");
        return;
    }

    cfg_setting.beginGroup(sec_pre_str + ui_widget->objectName() + sec_post_str);

    CTRL_WRITE_TO_CFG(QLineEdit, text())
    CTRL_WRITE_TO_CFG(QTextEdit, toPlainText())
    CTRL_WRITE_TO_CFG(QComboBox, currentIndex())
    CTRL_WRITE_TO_CFG(QRadioButton, isChecked() ? BOX_CHECKED : BOX_UNCHECKED)
    CTRL_WRITE_TO_CFG(QCheckBox, isChecked() ? BOX_CHECKED : BOX_UNCHECKED)
    CTRL_WRITE_TO_CFG(QSpinBox, value())

    cfg_setting.endGroup();
}

#define READ_FROM_CFG(ctrl_type, val, cond, op) \
    LIST_VAR_NAME(ctrl_type) = ui_widget->findChildren<ctrl_type *>();\
    do_it = true;\
    for(idx = 0; idx < LIST_VAR_NAME(ctrl_type).size(); ++idx)\
    {\
        if(check_in)\
            do_it = filter_in.contains(LIST_VAR_NAME(ctrl_type)[idx]);\
        else if(check_out)\
            do_it = !filter_out.contains(LIST_VAR_NAME(ctrl_type)[idx]);\
        if(!do_it) continue;\
                            \
        str_val = cfg_setting.value(key_pre_str \
                                    + LIST_VAR_NAME(ctrl_type)[idx]->objectName() \
                                    + key_post_str, \
                                    "").toString();\
        val;\
        if(cond)\
        {\
            LIST_VAR_NAME(ctrl_type)[idx]->op;\
        }\
    }

void UiConfigRecorder::load_configs_to_ui(QWidget * ui_widget,
                             const qobj_ptr_set_t &filter_in, const qobj_ptr_set_t &filter_out,
                             QString sec_pre_str, QString sec_post_str,
                             QString key_pre_str, QString key_post_str,
                             QSettings::Format cfg_format)
{
    CTRL_PTR_VAR_DEF;
    int idx;
    QSettings cfg_setting(m_cfg_file_fpn, cfg_format);
    QString str_val;
    int int_val;
    bool tr_ret;
    bool check_in = !filter_in.isEmpty(), check_out = !filter_out.isEmpty();
    bool do_it;

    if(!ui_widget)
    {
        DIY_LOG(LOG_ERROR, "ui_widget is NULL.\n");
        return;
    }

    cfg_setting.beginGroup(sec_pre_str + ui_widget->objectName() + sec_post_str);

    /* Be careful: Load radiobutton and checkbox firstly is more reasonable, because these
     * ones are normally used as config-switch, which may affect the content of editor widget.
    */

    READ_FROM_CFG(QRadioButton, int_val = str_val.toInt(&tr_ret),
                  (tr_ret && (BOX_CHECKED == int_val || BOX_UNCHECKED == int_val)),
                  setChecked(int_val));
    READ_FROM_CFG(QCheckBox, int_val = str_val.toInt(&tr_ret),
                  (tr_ret && (BOX_CHECKED == int_val || BOX_UNCHECKED == int_val)),
                  setChecked(int_val));
    READ_FROM_CFG(QComboBox, int_val = str_val.toInt(&tr_ret),
                  (tr_ret && (int_val < LIST_VAR_NAME(QComboBox)[idx]->count())),
                  setCurrentIndex(int_val));
    READ_FROM_CFG(QLineEdit, ,(!str_val.isEmpty()), setText(str_val));
    READ_FROM_CFG(QTextEdit, ,(!str_val.isEmpty()), setText(str_val));
    READ_FROM_CFG(QSpinBox, , true, setValue(str_val.toInt()));

    cfg_setting.endGroup();
}
