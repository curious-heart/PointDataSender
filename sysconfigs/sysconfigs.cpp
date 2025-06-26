#include <QSet>
#include <QSettings>

#include "logger/logger.h"
#include "common_tools/common_tool_func.h"
#include "sysconfigs/sysconfigs.h"

#undef ENUM_NAME_DEF
#define ENUM_NAME_DEF(e) <<e

static const char* gs_str_cfg_item = "配置项";
static const char* gs_str_should_be_one_val_of = "应为如下值之一：";
static const char* gs_str_actual_val = "实际值";
static const char* gs_str_value_invalid = "值无效";

extern const char* g_str_row_int;
extern const char* g_str_unit_ms;
static const char* gs_str_cfg_param_limit_error = "参数门限配置错误";
static const char* gs_str_plz_check = "请检查！";

static const char* gs_sysconfigs_file_fpn = "configs/configs.ini";

static const char* gs_ini_grp_sys_cfgs = "sys_cfgs";
static const char* gs_ini_key_log_level = "log_level";

static const char* gs_ini_grp_sc_data_cfg = "sc_data_cfg";
static const char* gs_ini_key_max_pt_number = "max_pt_number";
static const char* gs_ini_key_all_bytes_per_pt = "all_bytes_per_pt";
static const char* gs_ini_key_pkt_idx_byte_cnt = "pkt_idx_byte_cnt";
static const char* gs_ini_key_min_row_interval_ms = "min_row_interval_ms";
static const char* gs_ini_key_max_row_interval_ms= "max_row_interval_ms";
static const char* gs_ini_key_start_row_idx = "start_row_idx";
static const char* gs_ini_key_row_idx_byte_order = "row_idx_byte_order";
static const char* gs_ini_key_local_udp_port = "local_udp_port";

sys_configs_struct_t g_sys_configs_block;

static const int gs_def_log_level = LOG_ERROR;

static const int gs_def_max_pt_number = 200, gs_def_all_bytes_per_pt = 3,
                 gs_def_pkt_idx_byte_cnt = 2;
static const int gs_def_min_row_interval_ms = 10, gs_def_max_row_interval_ms = 1000;
static const int gs_def_start_row_idx = 0;
static const byte_sent_order_e_t gs_def_row_idx_byte_order = LOW_BYTE_FIRST;

static const quint16 gs_def_local_udp_port = 8020;

static RangeChecker<int> gs_cfg_file_log_level_ranger((int)LOG_DEBUG, (int)LOG_ERROR, "",
                     EDGE_INCLUDED, EDGE_INCLUDED);

static RangeChecker<int> gs_cfg_file_value_ge0_int_ranger(0, 0, "",
                           EDGE_INCLUDED, EDGE_INFINITE);

static RangeChecker<float> gs_cfg_file_value_ge0_float_ranger(0, 0, "",
                       EDGE_INCLUDED, EDGE_INFINITE);

static RangeChecker<int> gs_cfg_file_value_gt0_int_ranger(0, 0, "",
                       EDGE_EXCLUDED, EDGE_INFINITE);

static RangeChecker<int> gs_cfg_file_value_01_int_ranger(0, 1, "",
                       EDGE_INCLUDED, EDGE_INCLUDED);

static RangeChecker<float> gs_cfg_file_value_gt0_float_ranger(0, 0, "",
                       EDGE_EXCLUDED, EDGE_INFINITE);

/*the __VA_ARGS__ should be empty or a type converter like (cust_type).*/
#define GET_INF_CFG_NUMBER_VAL(settings, key, type_func, var, def, factor, checker, ...)\
{\
    (var) = __VA_ARGS__((factor) * ((settings).value((key), (def)).type_func()));\
    if((checker) && !((checker)->range_check((var))))\
    {\
        (var) = (def);\
    }\
}

/*check the validation of config parameters.*/
#define CHECK_LIMIT_RANGE(l_name, min_l, max_l, checker, unit_str) \
    cfg_ret = true; \
    if(((checker) && (!((checker)->range_check(min_l)) || !((checker)->range_check(max_l)))) \
        || ((min_l) > (max_l)))\
    {\
        cfg_ret = false;\
        ret_str += QString((l_name)) + \
                   " [" + QString::number((min_l)) + ", " + QString::number((max_l)) + "] " +\
                   (unit_str) + "\n";\
    }\
    ret = ret && cfg_ret;

/**/
#define CHECK_ENUM(title_str, e_v, e_set, str_func) \
    cfg_ret = true; ret_str += (ret_str.isEmpty() ? "" : "\n");\
    if(!e_set.contains(e_v))\
    {\
        cfg_ret = false;\
        ret_str += QString(gs_str_cfg_item) + (title_str) + "\n" + gs_str_should_be_one_val_of + "\n{";\
        auto it = e_set.constBegin();\
        while(it != e_set.constEnd()) {ret_str += str_func(*it) + ", "; ++it;}\
        ret_str.chop(2);\
        ret_str += "}\n";\
        ret_str += QString(gs_str_actual_val) + str_func(e_v) + "\n";\
    }\
    ret = ret && cfg_ret;

bool fill_sys_configs(QString * ret_str_ptr)
{
    bool ret = true, cfg_ret;
    QString ret_str;
    QSettings settings(gs_sysconfigs_file_fpn, QSettings::IniFormat);

    /*--------------------*/
    settings.beginGroup(gs_ini_grp_sys_cfgs);
    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_log_level, toInt,
                           g_sys_configs_block.log_level, gs_def_log_level,
                           1, &gs_cfg_file_log_level_ranger);
    settings.endGroup();

    /*--------------------*/
    settings.beginGroup(gs_ini_grp_sc_data_cfg);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_max_pt_number, toInt,
                           g_sys_configs_block.max_pt_number, gs_def_max_pt_number,
                           1, &gs_cfg_file_value_gt0_int_ranger);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_all_bytes_per_pt, toInt,
                           g_sys_configs_block.all_bytes_per_pt, gs_def_all_bytes_per_pt,
                           1, &gs_cfg_file_value_gt0_int_ranger);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_pkt_idx_byte_cnt, toInt,
                           g_sys_configs_block.pkt_idx_byte_cnt, gs_def_pkt_idx_byte_cnt,
                           1, &gs_cfg_file_value_gt0_int_ranger);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_min_row_interval_ms, toInt,
                           g_sys_configs_block.min_row_interval_ms, gs_def_min_row_interval_ms,
                           1, (RangeChecker<int>*)0);
    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_max_row_interval_ms, toInt,
                           g_sys_configs_block.max_row_interval_ms, gs_def_max_row_interval_ms,
                           1, (RangeChecker<int>*)0);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_start_row_idx, toInt,
                           g_sys_configs_block.start_row_idx, gs_def_start_row_idx,
                           1, &gs_cfg_file_value_ge0_int_ranger);

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_row_idx_byte_order, toInt,
                           g_sys_configs_block.row_idx_byte_order, gs_def_row_idx_byte_order,
                           1, (RangeChecker<int>*)0, (byte_sent_order_e_t));

    GET_INF_CFG_NUMBER_VAL(settings, gs_ini_key_local_udp_port, toInt,
                           g_sys_configs_block.local_udp_port, gs_def_local_udp_port,
                           1, &gs_cfg_file_value_gt0_int_ranger, (quint16));
    settings.endGroup();
    /*--------------------*/

    CHECK_LIMIT_RANGE(g_str_row_int,
                g_sys_configs_block.min_row_interval_ms, g_sys_configs_block.max_row_interval_ms,
                &gs_cfg_file_value_gt0_int_ranger, g_str_unit_ms);

    if(!ret)  ret_str = QString(gs_str_cfg_param_limit_error) + "," + gs_str_plz_check + "\n" + ret_str;


    /*--------------------*/
    QSet<byte_sent_order_e_t> byte_orders;
    byte_orders BYTE_SENT_ORDER_E;
    CHECK_ENUM(QString(gs_ini_key_row_idx_byte_order)  + gs_str_value_invalid + "," + gs_str_plz_check,
               g_sys_configs_block.row_idx_byte_order,
               byte_orders, QString::number)

    /*--------------------*/
    ret = ret && cfg_ret;
    if(ret_str_ptr) *ret_str_ptr = ret_str;
    return ret;
}
